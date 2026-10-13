"""Validate exported mesh hinge positions against all eight native flight poses.

Uses the generated runtime integers, not just the author's floating point mesh.
The two sides of each thin membrane meet around a shared hinge centerline.
"""
import json
import math
import re
from pathlib import Path
root=Path(__file__).resolve().parent
rig=json.loads((root/'native_rig.json').read_text())
header=(root.parents[2]/'soh/soh/Enhancements/arpg/KeeseBlenderData.h').read_text()
def rows(name):
    body=header.split(name+'[]={')[1].split('};')[0]
    return [tuple(map(int,re.findall(r'-?\d+',v))) for v in re.findall(r'\{([^}]+)\}',body)]
vertices=rows('vertices');parts=rows('parts')
indices=list(map(int,re.findall(r'\d+',header.split('indices[]={')[1].split('};')[0])))
meshes={}
for first,count,variant,limb,material in parts:
    meshes.setdefault((variant,limb),set()).update(vertices[indices[i]][:3] for i in range(first,first+count))
def hinge(variant,limb,xy):
    points=meshes[variant,limb]
    front=(*xy,8);back=(*xy,-8)
    assert front in points and back in points, f'Missing exported hinge at {variant}:{limb}:{xy}'
    return tuple((a+b)/200 for a,b in zip(front,back))
def transform(pose,limb,p):
    m=pose[str(limb)]
    return [sum(m[i][j]*p[j] for j in range(3))+m[i][3] for i in range(3)]
pairs=[(17,(1140,1423),16,(0,1423)),(17,(1140,-1424),16,(0,-1424)),
       (16,(0,1423),15,(0,1664)),(16,(1061,-331),15,(0,-387))]
max_gap=0;checks=0
for variant in range(3):
    for a,p,b,q in pairs:
        for mirror in (0,7):
            va=hinge(variant,a+mirror,p);vb=hinge(variant,b+mirror,q)
            for pose in rig['poses']:
                gap=math.dist(transform(pose,a+mirror,va),transform(pose,b+mirror,vb))
                assert gap<.05, f'Wing seam separated by {gap} at limb {a+mirror}'
                max_gap=max(max_gap,gap);checks+=1
    for pose in rig['poses']:
        points=[transform(pose,limb,tuple(v/100 for v in p)) for (sp,limb),vs in meshes.items() if sp==variant for p in vs]
        native=[transform(pose,l['limb'],tuple(v/100 for v in p)) for l in rig['rig'] for p in l['vertices']]
        for axis in range(3):
            assert min(p[axis] for p in points)>min(p[axis] for p in native)-3
            assert max(p[axis] for p in points)<max(p[axis] for p in native)+3
report={'status':'passed','native_frames':rig['frames'],'variant_count':3,'hinge_checks':checks,
        'maximum_hinge_center_gap_world_units':max_gap*.5,
        'native_envelope_tolerance_world_units':1.5,
        'note':'Original wing hinges and actor-sized bounds verified using exported, quantized geometry; visual game checks remain required.'}
(root/'animation-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
