"""Retopologize the four native claw wedges around their saved landmarks.

Each native component supplies its two root anchors, knuckle and tip. The
existing hand joints, attack reach, surrounding mesh and animations stay fixed.
"""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import sys

import bpy
import bmesh
from mathutils import Vector

parser = argparse.ArgumentParser()
parser.add_argument('--master', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
assert bpy.app.background and not bpy.data.filepath and not args.output.exists()
bpy.ops.wm.open_mainfile(filepath=str(args.master))
ob = bpy.data.objects['EDIT THIS Wolfos mesh']; mesh = ob.data
rig = next(m.object for m in ob.modifiers if m.type == 'ARMATURE')
claw_joints = {15, 21}
def face_key(face):
    values = [tuple(mesh.vertices[i].co) for i in face.vertices]
    return face.material_index, min(tuple(values[i:]+values[:i]) for i in range(len(values)))
normals = {face_key(f): {tuple(mesh.vertices[mesh.loops[i].vertex_index].co): tuple(mesh.corner_normals[i].vector)
                       for i in f.loop_indices} for f in mesh.polygons
           if mesh.attributes['native_limb'].data[f.index].value not in claw_joints}
protected = Counter(normals.keys())
bm = bmesh.new(); bm.from_mesh(mesh)
owner = bm.faces.layers.int['native_limb']; deform = bm.verts.layers.deform.active
uv = bm.loops.layers.uv.active
landmarks = []
for joint in sorted(claw_joints):
    matrix = rig.data.bones[f'limb_{joint:02}'].matrix_local
    inverse = matrix.inverted()
    faces = [f for f in bm.faces if f[owner] == joint]
    material = faces[0].material_index
    assert all(f.material_index == material for f in faces)
    candidates = {v for f in faces for v in f.verts}
    group = ob.vertex_groups[f'limb_{joint:02}'].index
    assert all(len(v[deform]) == 1 and abs(v[deform][group]-1) < 1e-6 for v in candidates)
    components = []
    while candidates:
        pending = [next(iter(candidates))]; component = set()
        while pending:
            v = pending.pop()
            if v not in candidates:
                continue
            candidates.remove(v); component.add(v)
            pending.extend(e.other_vert(v) for e in v.link_edges)
        components.append(component)
    assert len(components) == 2, 'Expected two native claws on each hand'
    shapes = []
    for component in components:
        points = [inverse @ v.co for v in component]
        roots = sorted(points, key=lambda p: p.x)[:2]
        tip = max(points, key=lambda p: p.x)
        middle = [p for p in points if all((p-r).length > .01 for r in roots+[tip])]
        assert len(middle) == 3
        start = (roots[0]+roots[1])*.5
        knuckle = sum(middle, Vector())/3
        control = knuckle*2-(start+tip)*.5
        shapes.append((start, control, tip, roots))
    # [TIME] | [INFO] | Wolfos.ClawRefinement | Remove only the original rigid wedges; preserve every surrounding surface and its custom normals.
    old_vertices = {v for f in faces for v in f.verts}
    bmesh.ops.delete(bm, geom=faces, context='FACES_ONLY')
    for vertex in old_vertices:
        assert not vertex.link_faces
    bmesh.ops.delete(bm, geom=list(old_vertices), context='VERTS')
    for start, control, tip, roots in shapes:
        width_axis = (roots[0]-start).normalized()
        root_width = (roots[0]-start).length
        rings = []
        lengths = [0, .08, .17, .27, .38, .49, .60, .71, .81, .89, .95]
        widths = [root_width, 1.8, 2.15, 2.4, 2.45, 2.2, 1.8, 1.35, .9, .55, .27]
        depths = [.8, 1.1, 1.4, 1.7, 1.9, 1.75, 1.4, 1.02, .66, .4, .18]
        sides = 12
        created = []
        def add_vertex(local):
            value = bm.verts.new(matrix @ local); value[deform][group] = 1
            return value
        def face(vertices, coords):
            value = bm.faces.new(vertices); value[owner] = joint
            value.material_index = material; value.smooth = True
            for loop, coords in zip(value.loops, coords):
                loop[uv].uv = coords
            created.append(value)
        for t, width, depth in zip(lengths, widths, depths):
            center = (1-t)**2*start+2*t*(1-t)*control+t*t*tip
            tangent = (2*(1-t)*(control-start)+2*t*(tip-control)).normalized()
            depth_axis = tangent.cross(width_axis).normalized()
            ring = [add_vertex(center+width_axis*(math.cos(math.tau*i/sides)*width)
                               +depth_axis*(math.sin(math.tau*i/sides)*depth)) for i in range(sides)]
            rings.append(ring)
        for r in range(len(rings)-1):
            for i in range(sides):
                n = (i+1)%sides; u0 = i/sides; u1 = (i+1)/sides
                v0 = .95-.9*lengths[r]; v1 = .95-.9*lengths[r+1]
                face([rings[r][i], rings[r][n], rings[r+1][n], rings[r+1][i]],
                     [(u0,v0),(u1,v0),(u1,v1),(u0,v1)])
        end = add_vertex(tip); base = add_vertex(start)
        for i in range(sides):
            n = (i+1)%sides
            face([rings[-1][i],rings[-1][n],end],[(i/sides,.095),((i+1)/sides,.095),((i+.5)/sides,.05)])
            face([rings[0][n],rings[0][i],base],[((i+1)/sides,.95),(i/sides,.95),((i+.5)/sides,.98)])
        # [TIME] | [INFO] | Wolfos.ClawRefinement | Reorient only the newly closed claw shells; native mouth and other original winding remain untouched.
        bmesh.ops.recalc_face_normals(bm, faces=created)
        landmarks.append(dict(joint=joint, root=list(start), root_anchors=[list(r) for r in roots],
                              tip=list(tip), rings=len(rings), sides=sides,
                              root_anchor_error=max(min((v.co-matrix@p).length for v in rings[0]) for p in roots),
                              tip_error=(end.co-matrix@tip).length))
bm.normal_update(); bm.to_mesh(mesh); bm.free(); mesh.update()
assert Counter(face_key(f) for f in mesh.polygons if mesh.attributes['native_limb'].data[f.index].value not in claw_joints) == protected
custom = [(0,0,0)]*len(mesh.loops)
for polygon in mesh.polygons:
    saved = normals.get(face_key(polygon))
    if saved:
        for loop in polygon.loop_indices:
            custom[loop] = saved[tuple(mesh.vertices[mesh.loops[loop].vertex_index].co)]
mesh.normals_split_custom_set(custom); mesh.calc_loop_triangles()
assert max(v['root_anchor_error'] for v in landmarks) < .001
assert max(v['tip_error'] for v in landmarks) == 0
ob['claw_refinement'] = 'Four closed curved claws from native root, knuckle and tip landmarks'
bpy.ops.wm.save_as_mainfile(filepath=str(args.output), compress=True)
report = dict(triangles=len(mesh.loop_triangles), landmarks=landmarks,
              preserved='Native claw roots and tips; every other surface, rig and animation')
args.output.with_suffix('.claws.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
