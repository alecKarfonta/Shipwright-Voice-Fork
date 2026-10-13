"""Map the saved coat mesh to facial, body, chest and muzzle paint regions.

The atlas contains four equal vertical strips. No mesh positions or rig data
change; the integer coat_region face attribute keeps the selections editable.
"""
import argparse
from collections import Counter, defaultdict
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector

parser = argparse.ArgumentParser()
parser.add_argument('--master', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--limbs', type=int, nargs='+', help='Re-map only these coat regions, preserving all other saved UVs')
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
assert bpy.app.background and not bpy.data.filepath
assert not args.output.exists()
bpy.ops.wm.open_mainfile(filepath=str(args.master))
ob = bpy.data.objects['EDIT THIS Wolfos mesh']; mesh = ob.data
rig = next(m.object for m in ob.modifiers if m.type == 'ARMATURE')
inverse = {int(b.name.removeprefix('limb_')): b.matrix_local.inverted() for b in rig.data.bones}
owners = mesh.attributes['native_limb'].data
coat = [f for f in mesh.polygons if mesh.materials[f.material_index]['native_texture'].endswith('FurTex')
        and (args.limbs is None or owners[f.index].value in args.limbs)]
assert coat
points = {f.index: [inverse[owners[f.index].value] @ mesh.vertices[i].co for i in f.vertices] for f in coat}
limb_points = defaultdict(list)
for f in coat:
    limb_points[owners[f.index].value].extend(points[f.index])
bounds = {j: [(min(p[i] for p in ps), max(p[i] for p in ps)) for i in range(3)] for j, ps in limb_points.items()}
regions = mesh.attributes.get('coat_region') or mesh.attributes.new('coat_region', 'INT', 'FACE')
counts = Counter()
uv = mesh.uv_layers.active.data

def normalized(value, limits):
    a, b = limits
    return min(1, max(0, (value-a)/max(b-a, .01)))

for face in coat:
    j = owners[face.index].value
    ps = points[face.index]; center = sum(ps, Vector())/len(ps)
    world_center = sum((mesh.vertices[i].co for i in face.vertices), Vector())/len(face.vertices)
    region = 0 if j == 17 else 1
    if j == 17 and center.x > 18 and center.y > -5.5:
        region = 3
    elif ((j == 12 and center.y > 5 and abs(center.z) < 9)
          or (j == 7 and face.normal.y < -.5 and abs(world_center.x) < 4)):
        region = 2
    regions.data[face.index].value = region
    counts[region] += 1
    # [TIME] | [INFO] | Wolfos.CoatRegions | One cylindrical projection per limb keeps fur direction continuous across triangle edges.
    long_axis = 1 if j == 7 else 0
    def angle(p):
        if j == 7:
            return math.atan2(p.x, p.z)
        return math.atan2(p.z, -p.y if j == 17 else p.y)
    angles = [angle(p) for p in ps]
    if max(angles)-min(angles) > math.pi:
        center_angle = angle(center)
        angles = [a + math.tau if a-center_angle < -math.pi else a-math.tau if a-center_angle > math.pi else a for a in angles]
    for loop, p, a in zip(face.loop_indices, ps, angles):
        u = .04+.92*min(1, max(0, .5+a/math.tau))
        v = .04+.92*(1-normalized(p[long_axis], bounds[j][long_axis]))
        local_normal = inverse[j].to_3x3() @ face.normal
        if abs(local_normal[long_axis]) > .65:
            cap_axes = [i for i in range(3) if i != long_axis]
            u = .04+.92*normalized(p[cap_axes[0]], bounds[j][cap_axes[0]])
            v = .04+.92*normalized(p[cap_axes[1]], bounds[j][cap_axes[1]])
        if region == 3:
            # [TIME] | [INFO] | Wolfos.CoatRegions | The atlas muzzle darkens toward its top; map the nose tip there and its root into the gray transition.
            v = .04+.92*normalized(p.x, (17, 34))
        uv[loop].uv = ((region+u)/4, v)

# [TIME] | [INFO] | Wolfos.CoatRegions | Preserve UV area after the native renderer rounds texture coordinates to its logical texel grid.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_asset import NativeWolfos
native = NativeWolfos()
logical = native.texture(mesh.materials[coat[0].material_index]['native_texture'])
scale = (32*logical['width'], 32*logical['height'])
mesh.calc_loop_triangles()
coat_ids = {f.index for f in coat}
triangles = [t for t in mesh.loop_triangles if t.polygon_index in coat_ids]
quantized = {i: [round(uv[i].uv.x*scale[0]), round((1-uv[i].uv.y)*scale[1])]
             for t in triangles for i in t.loops}
neighbors = defaultdict(list)
for triangle in triangles:
    for loop in triangle.loops:
        neighbors[loop].append(triangle)

def area(triangle):
    a, b, c = [quantized[i] for i in triangle.loops]
    return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])

repairs = 0
for triangle in triangles:
    if area(triangle):
        continue
    region = regions.data[triangle.polygon_index].value
    fixed = False
    for radius in range(1, 9):
        for loop in triangle.loops:
            old = quantized[loop][:]
            initially_valid = [t for t in neighbors[loop] if area(t)]
            for dx, dy in ((radius, 0), (-radius, 0), (0, radius), (0, -radius),
                           (radius, radius), (-radius, radius), (radius, -radius), (-radius, -radius)):
                value = [old[0]+dx, old[1]+dy]
                if not ((region+.03)/4*scale[0] < value[0] < (region+.97)/4*scale[0]
                        and .03*scale[1] < value[1] < .97*scale[1]):
                    continue
                quantized[loop] = value
                if area(triangle) and all(area(t) for t in initially_valid):
                    fixed = True
                    break
            if fixed:
                break
            quantized[loop] = old
        if fixed:
            break
    if not fixed:
        # [TIME] | [INFO] | Wolfos.CoatRegions | Tiny sculpt triangles can quantize all three corners to one point, requiring two independent coordinate adjustments.
        loops = list(triangle.loops)
        original = {loop: quantized[loop][:] for loop in loops}
        affected = {t.index: t for loop in loops for t in neighbors[loop]}
        initially_valid = [t for t in affected.values() if area(t)]
        for radius in range(1, 9):
            for first, second in ((0,1),(1,2),(2,0)):
                for dx, dy in ((radius,radius),(-radius,radius),(radius,-radius),(-radius,-radius)):
                    for loop, value in original.items():
                        quantized[loop] = value[:]
                    quantized[loops[first]][0] += dx
                    quantized[loops[second]][1] += dy
                    if (all((region+.03)/4*scale[0] < quantized[loop][0] < (region+.97)/4*scale[0]
                            and .03*scale[1] < quantized[loop][1] < .97*scale[1] for loop in loops)
                            and area(triangle) and all(area(t) for t in initially_valid)):
                        fixed = True
                        break
                if fixed:
                    break
            if fixed:
                break
    assert fixed, f'Unable to preserve exported UV area for triangle {triangle.index}'
    repairs += 1
assert all(area(t) for t in triangles)
for loop, value in quantized.items():
    uv[loop].uv = (value[0]/scale[0], 1-value[1]/scale[1])
for mat in {mesh.materials[f.material_index] for f in coat}:
    mat['export_wrap'] = [2, 2]
    mat['coat_atlas'] = 'Face, body, chest, muzzle; four vertical strips'
if args.limbs is None:
    assert set(counts) == {0, 1, 2, 3}
bpy.ops.wm.save_as_mainfile(filepath=str(args.output), compress=True)
report = dict(regions={['face', 'body', 'chest', 'muzzle'][i]: counts[i] for i in range(4)},
              atlas='Four equal vertical strips; continuous cylindrical limb projections; UVs inset four percent inside each region',
              geometry_changed=False, native_wrap='Clamp S and T',
              quantized_uv_repairs=repairs, collapsed_quantized_coat_uv_triangles=0)
args.output.with_suffix('.regions.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
