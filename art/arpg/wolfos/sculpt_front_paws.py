"""Reshape the saved front-paw coat into tapered wrists and paired knuckles.

Claws, elbows, native mixed-joint faces, rig and animation remain untouched.
The control profiles are in native forearm space, mirrored for the left side.
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
targets = {14,20}
def face_key(face):
    points = [tuple(mesh.vertices[i].co) for i in face.vertices]
    return face.material_index, min(tuple(points[i:]+points[:i]) for i in range(len(points)))
saved_normals = {face_key(f): {tuple(mesh.vertices[mesh.loops[i].vertex_index].co): tuple(mesh.corner_normals[i].vector)
                             for i in f.loop_indices} for f in mesh.polygons
                 if mesh.attributes['native_limb'].data[f.index].value not in targets}
bm = bmesh.new(); bm.from_mesh(mesh)
deform = bm.verts.layers.deform.active; owner = bm.faces.layers.int['native_limb']
groups = {g.index:int(g.name.removeprefix('limb_')) for g in ob.vertex_groups}
def joint(v):
    values = [(groups[g],w) for g,w in v[deform].items() if w > 1e-6]
    assert len(values)==1 and abs(values[0][1]-1)<1e-5
    return values[0][0]
def region(f):
    j = f[owner]
    return j if j in targets and all(joint(v)==j for v in f.verts) else None
def protected_key(f):
    values = [(joint(v),*tuple(v.co)) for v in f.verts]
    return f.material_index, min(tuple(values[i:]+values[:i]) for i in range(len(values)))
protected = Counter(protected_key(f) for f in bm.faces if region(f) is None)
edges = [e for e in bm.edges if e.is_manifold and region(e.link_faces[0]) is not None
         and region(e.link_faces[0])==region(e.link_faces[1])]
bmesh.ops.subdivide_edges(bm,edges=edges,cuts=1,use_grid_fill=True,smooth=0)

# [TIME] | [INFO] | Wolfos.PawSculpt | Shape the wrist-to-palm transition directly instead of repeatedly subdividing the original triangular cuff.
profile = [(7,2.8,3.8,-.5),(11,3.0,4.0,-.5),(15,4.1,5.1,-.3),
           (19,5.5,6.6,-.1),(23,6.0,6.8,.1),(25,5.5,6.2,0),
           (27,4.3,4.9,-.5),(29.1,2.4,2.7,-1.0)]
def parameters(x):
    for a,b in zip(profile,profile[1:]):
        if x <= b[0]:
            t=max(0,min(1,(x-a[0])/(b[0]-a[0])));t=t*t*(3-2*t)
            return [a[i]*(1-t)+b[i]*t for i in range(1,4)]
    return list(profile[-1][1:])

changes = Counter(); maximum = 0; moved = {}; native_moves = []
for v in bm.verts:
    j=joint(v)
    if j not in targets:
        continue
    matrix=rig.data.bones[f'limb_{j:02}'].matrix_local
    local=matrix.inverted()@v.co
    if local.x <= 7:
        continue
    sy=1 if j==14 else -1
    ry,rz,center=parameters(local.x)
    z=local.z*sy
    angle=math.atan2(z,local.y+.7)
    y=center+math.cos(angle)*ry
    z=math.sin(angle)*rz
    if y>center:
        knuckles=(math.exp(-((z-3)/1.8)**2)+math.exp(-((z+3)/1.8)**2))
        y+=.9*knuckles*math.exp(-((local.x-23)/4.5)**2)
        y-=.65*math.exp(-(z/1.25)**2)*math.exp(-((local.x-25)/3.5)**2)
    blend=max(0,min(1,(local.x-7)/5));blend=blend*blend*(3-2*blend)
    point=Vector((local.x,local.y*(1-blend)+y*blend,local.z*(1-blend)+z*sy*blend))
    old=v.co.copy();new=matrix@point;maximum=max(maximum,(new-old).length)
    moved[(j,*tuple(old))]=(j,*tuple(new))
    native_moves.append(dict(joint=j,before=[round(x*100) for x in local],after=[round(x*100) for x in matrix.inverted()@new]))
    v.co=new;changes[j]+=1
expected=Counter()
for (material,points),count in protected.items():
    values=[moved.get(p,p) for p in points]
    expected[(material,min(tuple(values[i:]+values[:i]) for i in range(len(values))))]+=count
assert changes and Counter(protected_key(f) for f in bm.faces if region(f) is None)==expected
for edge in bm.edges:
    if all(f[owner] in targets for f in edge.link_faces):
        edge.smooth=True
bm.normal_update();bm.to_mesh(mesh);bm.free();mesh.update()
normals=[(0,0,0)]*len(mesh.loops)
for f in mesh.polygons:
    f.use_smooth=True
    previous=saved_normals.get(face_key(f))
    if previous:
        for loop in f.loop_indices:
            normals[loop]=previous[tuple(mesh.vertices[mesh.loops[loop].vertex_index].co)]
mesh.normals_split_custom_set(normals);mesh.calc_loop_triangles()
ob['paw_sculpt']='Tapered forearm transition, rounded palm, paired dorsal knuckles; claws and proximal elbow anchors fixed'
bpy.ops.wm.save_as_mainfile(filepath=str(args.output),compress=True)
report=dict(triangles=len(mesh.loop_triangles),sculpted_vertices=dict(changes),maximum_displacement=maximum,
            control_profile=profile,protected_faces=sum(protected.values()),
            native_vertex_moves=native_moves,
            preserved='Claws and proximal elbow anchors; joint-bridge topology, bone assignments, rig and animation. Paw-side bridge endpoints follow the shaped outline.')
args.output.with_suffix('.paws.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
