"""Bevel selected rigid body edges on a saved Wolfos master.

The face, eye surfaces, teeth, claws and mixed-joint connections stay fixed.
This is an edit to existing topology, not a replacement anatomy generator.
"""
import argparse
import json
import math
from pathlib import Path
import sys

import bpy
import bmesh

parser = argparse.ArgumentParser()
parser.add_argument('--master', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
assert bpy.app.background and not bpy.data.filepath
assert not args.output.exists(), 'Choose a new revision'
bpy.ops.wm.open_mainfile(filepath=str(args.master))
ob = bpy.data.objects['EDIT THIS Wolfos mesh']
mesh = ob.data
body = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 19, 20}
before_vertices, before_faces = len(mesh.vertices), len(mesh.polygons)

def face_key(face):
    return (face.material_index, tuple(sorted(tuple(mesh.vertices[i].co) for i in face.vertices)))

preserve_normals = {}
for face in mesh.polygons:
    if mesh.attributes['native_limb'].data[face.index].value not in body:
        preserve_normals[face_key(face)] = {tuple(mesh.vertices[mesh.loops[i].vertex_index].co): tuple(mesh.corner_normals[i].vector)
                                          for i in face.loop_indices}

bm = bmesh.new(); bm.from_mesh(mesh)
deform = bm.verts.layers.deform.active
owner = bm.faces.layers.int['native_limb']
groups = {g.index: int(g.name.removeprefix('limb_')) for g in ob.vertex_groups if g.name.startswith('limb_')}

def joint(v):
    weights = [(groups[g], w) for g, w in v[deform].items() if g in groups and w > 1e-6]
    assert len(weights) == 1 and abs(weights[0][1]-1) < 1e-5, 'Never discard blended weights'
    return weights[0][0]

edges = {j: [] for j in body}
for edge in bm.edges:
    if not edge.is_manifold or edge.calc_face_angle() < math.radians(28):
        continue
    j = joint(edge.verts[0])
    if j not in body or joint(edge.verts[1]) != j:
        continue
    nearby = {f for v in edge.verts for f in v.link_faces}
    if any(f[owner] != j or not mesh.materials[f.material_index]['native_texture'].endswith('FurTex') for f in nearby):
        continue
    if any(joint(v) != j for f in nearby for v in f.verts):
        continue
    edges[j].append(edge)

edited = {}
for j, selection in sorted(edges.items()):
    if not selection:
        continue
    # [TIME] | [INFO] | Wolfos.BodyRefinement | Conservative edge widths soften native muscle facets while protecting the original joint boundary vertices.
    width = 1.4 if j == 12 else (.55 if j in (4, 6, 10) else .85)
    material = selection[0].link_faces[0].material_index
    result = bmesh.ops.bevel(bm, geom=selection, offset=width, segments=3, profile=.5,
                            clamp_overlap=True, material=material)
    for face in result['faces']:
        assert all(joint(v) == j for v in face.verts)
        face[owner] = j
        face.smooth = True
    edited[j] = dict(edges=len(selection), width=width, new_faces=len(result['faces']))
assert edited, 'No eligible body edges'
for vertex in bm.verts:
    joint(vertex)
# [TIME] | [INFO] | Wolfos.BodyRefinement | Preserve original winding, including the deliberately oriented mouth surfaces; update normals without reorienting faces.
bm.normal_update()
bm.to_mesh(mesh); bm.free(); mesh.update()
normals = [(0, 0, 0)]*len(mesh.loops)
for face in mesh.polygons:
    face.use_smooth = True
    original = preserve_normals.get(face_key(face))
    if original:
        for loop in face.loop_indices:
            normals[loop] = original[tuple(mesh.vertices[mesh.loops[loop].vertex_index].co)]
mesh.normals_split_custom_set(normals)
mesh.calc_loop_triangles()
report = dict(before_vertices=before_vertices, before_faces=before_faces,
              vertices=len(mesh.vertices), polygons=len(mesh.polygons), triangles=len(mesh.loop_triangles),
              edits=edited, protected='Head, eyes, teeth, claws and mixed-joint boundary neighborhoods')
ob['body_refinement'] = 'Conservative bevel on rigid coat interiors; native attachments retained'
bpy.ops.wm.save_as_mainfile(filepath=str(args.output), compress=True)
args.output.with_suffix('.shape.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
