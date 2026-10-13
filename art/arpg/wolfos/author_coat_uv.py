"""Give an existing master usable, consistently scaled coat UVs.

Projects in each face's native limb rest space, preserving eye and mouth maps.
The saved revision remains editable; this does not reconstruct anatomy.
"""
import argparse
import json
from pathlib import Path
import sys

import bpy

parser = argparse.ArgumentParser()
parser.add_argument('--master', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
assert bpy.app.background and not bpy.data.filepath
assert not args.output.exists(), 'Choose a new authoring revision'
bpy.ops.wm.open_mainfile(filepath=str(args.master))
report = []
for ob in bpy.context.scene.objects:
    if ob.get('source_kind') != 'editable master':
        continue
    armature = next(mod.object for mod in ob.modifiers if mod.type == 'ARMATURE')
    mesh = ob.data
    uv = mesh.uv_layers.active.data
    owners = mesh.attributes['native_limb'].data
    coat_materials = set()
    minimum_area = float('inf')
    count = 0
    for face in mesh.polygons:
        mat = mesh.materials[face.material_index]
        if not mat['native_texture'].endswith('FurTex'):
            continue
        owner = owners[face.index].value
        inverse = armature.data.bones[f'limb_{owner:02}'].matrix_local.inverted()
        points = [inverse @ mesh.vertices[mesh.loops[i].vertex_index].co for i in face.loop_indices]
        assert len(points) == 3, 'Native baseline is triangulated'
        normal = (points[1]-points[0]).cross(points[2]-points[0]).normalized()
        dominant = max(range(3), key=lambda axis: abs(normal[axis]))
        # [TIME] | [INFO] | Wolfos.CoatUV | Limb-local box projection prevents collapsed cap UVs and keeps hair flow along limb length on side surfaces.
        if dominant == 0:
            values = [(p.z/10, p.y/20) for p in points]
        elif dominant == 1:
            values = [(p.z/10, -p.x/20) for p in points]
        else:
            values = [(p.y/10, -p.x/20) for p in points]
        for loop, value in zip(face.loop_indices, values):
            uv[loop].uv = value
        a, b, c = values
        area = abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))
        assert area > 1e-7, 'Coat projection collapsed a face'
        minimum_area = min(minimum_area, area)
        coat_materials.add(mat)
        count += 1
    for mat in coat_materials:
        mat['export_wrap'] = [0, 0]
        split = next(n for n in mat.node_tree.nodes if n.type == 'SEPXYZ')
        combine = next(n for n in mat.node_tree.nodes if n.type == 'COMBXYZ')
        for axis in (0, 1):
            mat.node_tree.links.new(split.outputs[axis], combine.inputs[axis])
    report.append(dict(object=ob.name, faces=count, minimum_uv_area=minimum_area,
                       method='Limb-local box projection, 10 by 20 authoring units per tile', wrap='Repeat S and T'))
bpy.ops.wm.save_as_mainfile(filepath=str(args.output), compress=True)
args.output.with_suffix('.uv.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
