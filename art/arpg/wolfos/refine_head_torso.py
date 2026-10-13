"""Subdivide and relax the saved skull, chest and abdomen surfaces.

Work only inside rigid coat regions. Keep the eye seating, ears, mouth boundary
and original mixed-joint triangles pinned. The native rig is never rebuilt.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import sys

import bpy
import bmesh
from mathutils import Vector
from mathutils.bvhtree import BVHTree

parser = argparse.ArgumentParser()
parser.add_argument('--master', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--limbs', type=int, nargs='+', default=[7, 12, 17], help='Rigid coat regions to refine; default skull, chest and abdomen')
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
assert bpy.app.background and not bpy.data.filepath
assert not args.output.exists(), 'Choose a new revision'
bpy.ops.wm.open_mainfile(filepath=str(args.master))
ob = bpy.data.objects['EDIT THIS Wolfos mesh']; mesh = ob.data
rig = next(m.object for m in ob.modifiers if m.type == 'ARMATURE')
head_inverse = rig.data.bones['limb_17'].matrix_local.inverted()
targets = set(args.limbs)
assert targets <= {5, 7, 11, 12, 14, 17, 20}, 'Choose an explicitly reviewed coat region'

def face_key(face):
    return face.material_index, tuple(sorted(tuple(mesh.vertices[i].co) for i in face.vertices))

original_normals = {face_key(f): {tuple(mesh.vertices[mesh.loops[i].vertex_index].co): tuple(mesh.corner_normals[i].vector)
                               for i in f.loop_indices} for f in mesh.polygons}
eye_faces = [list(f.vertices) for f in mesh.polygons if mesh.attributes['native_limb'].data[f.index].value == 18]
eye_bvh = BVHTree.FromPolygons([v.co for v in mesh.vertices], eye_faces, all_triangles=True)

def head_surface():
    faces = [list(f.vertices) for f in mesh.polygons if mesh.attributes['native_limb'].data[f.index].value == 17
             and mesh.materials[f.material_index]['native_texture'].endswith('FurTex')]
    return BVHTree.FromPolygons([v.co for v in mesh.vertices], faces)

eye_samples = []
for face in eye_faces:
    a, b, c = [mesh.vertices[i].co.copy() for i in face]
    for i in range(9):
        for j in range(9-i):
            eye_samples.append(a*(1-(i+j)/8)+b*i/8+c*j/8)
before_distances = [head_surface().find_nearest(p)[3] for p in eye_samples]

bm = bmesh.new(); bm.from_mesh(mesh)
deform = bm.verts.layers.deform.active
owner = bm.faces.layers.int['native_limb']
group_joints = {g.index: int(g.name.removeprefix('limb_')) for g in ob.vertex_groups if g.name.startswith('limb_')}

def joint(v):
    weights = [(group_joints[g], w) for g, w in v[deform].items() if g in group_joints and w > 1e-6]
    assert len(weights) == 1 and abs(weights[0][1]-1) < 1e-5, 'One native bone per vertex is required'
    return weights[0][0]

def region(face):
    j = face[owner]
    if j in targets and mesh.materials[face.material_index]['native_texture'].endswith('FurTex') and all(joint(v) == j for v in face.verts):
        return j
    return None

def protected_key(face):
    values = [(joint(v), *tuple(v.co)) for v in face.verts]
    return min(tuple(values[i:]+values[:i]) for i in range(len(values)))

protected = Counter(protected_key(f) for f in bm.faces if region(f) is None)
before_vertices = len(bm.verts)
subdivisions = []
for level in range(2):
    edges = [e for e in bm.edges if e.is_manifold and region(e.link_faces[0]) is not None
             and region(e.link_faces[0]) == region(e.link_faces[1])]
    assert edges
    counts = Counter(region(e.link_faces[0]) for e in edges)
    bmesh.ops.subdivide_edges(bm, edges=edges, cuts=1, use_grid_fill=True, smooth=0)
    for v in bm.verts:
        joint(v)
    subdivisions.append(dict(level=level+1, edges=len(edges), regions=dict(counts)))

free = []
for v in bm.verts:
    j = joint(v)
    if j not in targets or any(region(f) != j for f in v.link_faces):
        continue
    if j == 17:
        local = head_inverse @ v.co
        if local.y < -13 or eye_bvh.find_nearest(v.co)[3] < 3:
            continue
    free.append(v)
assert free
start = {v: v.co.copy() for v in free}
for iteration in range(5):
    moves = {}
    for v in free:
        neighbors = [e.other_vert(v).co for e in v.link_edges]
        average = sum(neighbors, Vector()) / len(neighbors)
        delta = (average-v.co)*.38
        limit = .35 if joint(v) == 17 else .55
        if delta.length > limit:
            delta *= limit/delta.length
        moves[v] = delta
    for v, delta in moves.items():
        v.co += delta

assert Counter(protected_key(f) for f in bm.faces if region(f) is None) == protected, 'A pinned surface or joint boundary moved'
movement = {j: max(((v.co-start[v]).length for v in free if joint(v) == j), default=0) for j in targets}
free_counts = dict(Counter(joint(v) for v in free))
bm.normal_update(); bm.to_mesh(mesh); bm.free(); mesh.update()
normals = [(0, 0, 0)]*len(mesh.loops)
for face in mesh.polygons:
    face.use_smooth = True
    is_target_coat = mesh.attributes['native_limb'].data[face.index].value in targets and mesh.materials[face.material_index]['native_texture'].endswith('FurTex')
    original = original_normals.get(face_key(face)) if not is_target_coat else None
    if original:
        for loop in face.loop_indices:
            normals[loop] = original[tuple(mesh.vertices[mesh.loops[loop].vertex_index].co)]
mesh.normals_split_custom_set(normals); mesh.calc_loop_triangles()
after_surface = head_surface()
after_distances = [after_surface.find_nearest(p)[3] for p in eye_samples]
attachment_change = max(abs(a-b) for a, b in zip(before_distances, after_distances))
assert attachment_change < .05, f'Eye seating changed: {attachment_change}'
report = dict(before_vertices=before_vertices, vertices=len(mesh.vertices), triangles=len(mesh.loop_triangles),
              subdivisions=subdivisions, relaxed_vertices=free_counts, maximum_motion=movement,
              protected_surfaces=len(protected), eye_surface_samples=len(eye_samples),
              maximum_eye_seating_change=attachment_change,
              method='Two rigid-region subdivision passes and five bounded surface relaxation passes; protected landmarks and mixed-joint geometry pinned')
ob['surface_refinement'] = 'Subdivided rigid coat regions '+str(sorted(targets))+'; pinned eyes and joint boundaries'
bpy.ops.wm.save_as_mainfile(filepath=str(args.output), compress=True)
args.output.with_suffix('.sculpt.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
