"""Export the editable Blender master as a native SoH alternate-asset pack.

The archive replaces only exported Wolfos display lists and painted textures.
The original skeleton, animation clips and actor eye-segment selection remain in
charge. Reference collections are never exported. No game rebuild is required.
"""
import argparse
from collections import defaultdict
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import zipfile

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from native_asset import NativeWolfos

parser = argparse.ArgumentParser()
parser.add_argument('--master', type=Path, default=ROOT/'workspace/wolfos-authoring.blend')
parser.add_argument('--output', type=Path, default=ROOT/'workspace/export/wolfos-master.o2r')
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
assert bpy.app.background and not bpy.data.filepath
assert not args.output.exists(), 'Choose a new output path; an existing asset pack will not be overwritten'
bpy.ops.wm.open_mainfile(filepath=str(args.master))
native = NativeWolfos()
payloads, groups, painted_images = {}, defaultdict(list), {}
meshes = [ob for ob in bpy.context.scene.objects if ob.get('source_kind') == 'editable master']
assert meshes, 'No editable master mesh found'
maximum_weight_error = 0

for ob in meshes:
    assert ob.get('export_schema') == 'native-wolfos-pack-v1', 'Run prepare_master.py once to separate paintable images'
    species = ob['source_species']
    assert species in native.rigs
    armatures = [mod for mod in ob.modifiers if mod.type == 'ARMATURE' and mod.object]
    assert len(armatures) == 1, 'One native armature is required'
    armature = armatures[0].object
    axis = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
    bind, _ = native.pose('gWolfosWaitingAnim', 0, species)
    for joint, matrix in bind.items():
        expected = axis @ Matrix(matrix)
        actual = armature.data.bones[f'limb_{joint:02}'].matrix_local
        assert max(abs(expected[r][c]-actual[r][c]) for r in range(4) for c in range(4)) < .001, 'Native rest skeleton was changed'
    visible = [limb['joint'] for limb in native.rigs[species] if limb['dl']]
    armatures[0].show_viewport = False
    bpy.context.view_layer.update()
    evaluated = ob.evaluated_get(bpy.context.evaluated_depsgraph_get())
    data = evaluated.to_mesh(preserve_all_data_layers=True, depsgraph=bpy.context.evaluated_depsgraph_get())
    data.calc_loop_triangles()
    assert data.uv_layers.active and 'native_limb' in data.attributes, 'Mesh needs UVs and native_limb face assignments'
    owner_attribute = data.attributes['native_limb']
    assert owner_attribute.domain == 'FACE' and owner_attribute.data_type == 'INT'
    mesh_to_armature = armature.matrix_world.inverted() @ ob.matrix_world
    normal_to_armature = mesh_to_armature.to_3x3().inverted().transposed()
    vertex_joints = []
    for vertex in data.vertices:
        weights = [(group.weight, ob.vertex_groups[group.group].name) for group in vertex.groups
                   if group.weight > 1e-6 and ob.vertex_groups[group.group].name in armature.data.bones]
        assert len(weights) == 1, f'{ob.name} vertex {vertex.index}: native export needs one bone per vertex; do not silently discard blended weights'
        weight, name = weights[0]
        maximum_weight_error = max(maximum_weight_error, abs(weight-1))
        assert abs(weight-1) < 1e-5 and name.startswith('limb_')
        joint = int(name.removeprefix('limb_'))
        assert joint in visible, f'Joint {joint} has no native display matrix'
        vertex_joints.append(joint)
    for triangle in data.loop_triangles:
        owner = owner_attribute.data[triangle.polygon_index].value
        assert owner in visible
        mat = data.materials[triangle.material_index]
        texture, tile = mat['native_texture'], tuple(mat['native_tile'])
        wrap = tuple(mat.get('export_wrap', tile[:2]))
        assert len(wrap) == 2 and all(v in (0, 1, 2, 3) for v in wrap), 'Invalid native texture wrap override'
        assert (texture, tile) in native.material_setups(species, owner), f'{ob.name}: material needs a native template for limb {owner}'
        dimensions = native.texture(texture)
        for node in mat.node_tree.nodes:
            if node.type == 'TEX_IMAGE' and node.image:
                image = node.image
                assert image.get('paintable_master') and image.get('native_path'), 'Export only isolated master paint maps'
                path = image['native_path']
                assert path.startswith('objects/object_wf/gWolfos' + species)
                assert path not in painted_images or painted_images[path] == image, f'Two different paint maps claim {path}'
                painted_images[path] = image
        corners = []
        for index, loop in zip(triangle.vertices, triangle.loops):
            joint = vertex_joints[index]
            inverse = armature.data.bones[f'limb_{joint:02}'].matrix_local.inverted()
            position = inverse @ mesh_to_armature @ data.vertices[index].co * 100
            normal = (inverse.to_3x3() @ normal_to_armature @ data.corner_normals[loop].vector).normalized()
            uv = data.uv_layers.active.data[loop].uv
            position = tuple(round(v) for v in position)
            tc = (round(uv.x*32*dimensions['width']), round((1-uv.y)*32*dimensions['height']))
            assert all(-32768 <= v <= 32767 for v in (*position, *tc)), 'Native position/UV range exceeded'
            assert all(math.isfinite(v) for v in normal)
            corners.append((joint, position, tc, tuple(round(v*127) for v in normal)))
        groups[species, owner, texture, tile, wrap].append(corners)
    evaluated.to_mesh_clear()
    armatures[0].show_viewport = True

def command(a, b):
    return struct.pack('<II', a, b)

def material_commands(species, owner, texture, tile, wrap):
    data = bytearray(native.material_setups(species, owner)[texture, tile])
    if wrap == tile[:2]:
        return data
    offset = 0
    render_tile = None
    while offset < len(data):
        a, b = struct.unpack_from('<II', data, offset)
        opcode = a >> 24
        if opcode == 0xf5 and ((b >> 24) & 7) == 0:
            render_tile = offset
        offset += 16 if opcode in (0x20, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37) else 8
    assert render_tile is not None, 'Material has no explicit render tile to override'
    # [TIME] | [INFO] | Wolfos.MasterExport | Change only the final active render tile; previous texture-loading state remains byte-identical.
    b = struct.unpack_from('<I', data, render_tile+4)[0]
    b = (b & ~((3 << 8) | (3 << 18))) | (wrap[0] << 8) | (wrap[1] << 18)
    struct.pack_into('<I', data, render_tile+4, b)
    return data

draw_lists, vertex_count = {}, 0
for species, owner, texture, tile, wrap in sorted(groups):
    key = (species, owner)
    if key not in draw_lists:
        dl = native.rigs[species][owner-1]['dl']
        draw_lists[key] = dict(dl=dl, commands=bytearray(native.read(dl)[:72]), vertices=bytearray())
    target = draw_lists[key]
    target['commands'] += material_commands(species, owner, texture, tile, wrap)
    path = f'arpg/wolfos/master/{species.lower()}/limb-{owner:02}.vtx'
    crc = native.crc(path)
    visible = [limb['joint'] for limb in native.rigs[species] if limb['dl']]
    triangles = groups[species, owner, texture, tile, wrap]
    # [TIME] | [INFO] | Wolfos.MasterExport | Ten triangles fit the native 32-vertex cache, including mixed-joint seams.
    for first in range(0, len(triangles), 10):
        batch = triangles[first:first+10]
        by_joint = defaultdict(list)
        for face, triangle in enumerate(batch):
            for corner, vertex in enumerate(triangle):
                by_joint[vertex[0]].append((face, corner, vertex))
        indices, cache_index = {}, 0
        for joint, entries in sorted(by_joint.items()):
            target['commands'] += command(0xda380003, 0x0d000001 + visible.index(joint)*64)
            offset = len(target['vertices']); count = len(entries)
            target['commands'] += command(0x32000000 | (count << 12) | ((cache_index+count)*2), offset)
            target['commands'] += command(crc >> 32, crc & 0xffffffff)
            for face, corner, (_, position, tc, normal) in entries:
                target['vertices'] += struct.pack('<hhhHhhbbbB', *position, 0, *tc, *normal, 255)
                indices[face, corner] = cache_index; cache_index += 1; vertex_count += 1
        for face in range(len(batch)):
            i, j, k = [indices[face, corner]*2 for corner in range(3)]
            target['commands'] += command(0x05000000 | i << 16 | j << 8 | k, 0)

array_header = native.read('objects/object_wf/object_wfVtx_008878')[:64]
for (species, owner), target in draw_lists.items():
    target['commands'] += command(0xdf000000, 0)
    payloads['alt/' + target['dl']] = bytes(target['commands'])
    path = f'arpg/wolfos/master/{species.lower()}/limb-{owner:02}.vtx'
    payloads[path] = array_header + struct.pack('<II', 25, len(target['vertices'])//16) + target['vertices']

image_reports = {}
for path, image in sorted(painted_images.items()):
    width, height = image.size
    assert width > 0 and height > 0 and image.channels == 4
    logical = native.texture(path)
    values = list(image.pixels)
    pixels = bytes(round(min(1, max(0, values[(y*width+x)*4+c]))*255)
                   for y in reversed(range(height)) for x in range(width) for c in range(4))
    header = bytearray(native.read(path)[:64]); struct.pack_into('<I', header, 8, 1)
    # [TIME] | [INFO] | Wolfos.MasterExport | High-resolution RGBA32 maps retain the original tile dimensions and UV scale.
    body = struct.pack('<IIIIffI', 1, width, height, 2, 2*width/logical['width'], height/logical['height'], len(pixels))
    payloads['alt/' + path] = bytes(header) + body + pixels
    image_reports[path] = dict(width=width, height=height, logical_width=logical['width'], logical_height=logical['height'],
                               rgba_sha256=hashlib.sha256(pixels).hexdigest())

assert draw_lists and len(draw_lists) == 19*len({s for s, _ in draw_lists}), 'Export every visible limb of each edited species'
for species in {s for s, _ in draw_lists}:
    assert sum(len(v) for (s, *_), v in groups.items() if s == species) < 40000, 'Per-actor triangle budget exceeded'
args.output.parent.mkdir(parents=True, exist_ok=True)
temporary = args.output.with_suffix('.o2r.partial')
assert not temporary.exists()
with zipfile.ZipFile(temporary, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
    for name, data in sorted(payloads.items()):
        info = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0)); info.compress_type = zipfile.ZIP_DEFLATED
        archive.writestr(info, data)
temporary.rename(args.output)
report = dict(master=str(args.master), master_sha256=hashlib.sha256(args.master.read_bytes()).hexdigest(),
              pack=str(args.output), pack_sha256=hashlib.sha256(args.output.read_bytes()).hexdigest(),
              display_lists=len(draw_lists), triangles=sum(len(triangles) for triangles in groups.values()),
              vertex_records=vertex_count, texture_maps=image_reports, maximum_weight_error=maximum_weight_error,
              resources={name: hashlib.sha256(data).hexdigest() for name, data in sorted(payloads.items())},
              contract=['Native skeleton and actor behaviors are unchanged', 'One bone per vertex, mixed-bone triangles supported',
                        'Paint maps and UVs are authoritative; Blender PBR settings are studio-only',
                        'Native material templates preserve lighting, texture wrapping and eye texture selection'])
args.output.with_suffix('.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps({k: v for k, v in report.items() if k not in ('resources', 'texture_maps')}, indent=2))
