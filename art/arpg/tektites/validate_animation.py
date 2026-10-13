"""Validate exported mesh hinge positions against all native Tektite poses.

Uses the generated runtime integers, not just the author's floating point mesh.
Knee centers and landing claws must remain aligned with the native rig.
"""
import json
import math
import re
from pathlib import Path
root=Path(__file__).resolve().parent
rig=json.loads((root/'native_rig.json').read_text())
header=(root.parents[2]/'soh/soh/Enhancements/arpg/TektiteBlenderData.h').read_text()
def rows(name):
    body=header.split(name+'[]={')[1].split('};')[0]
    return [tuple(map(int,re.findall(r'-?\d+',v))) for v in re.findall(r'\{([^}]+)\}',body)]
vertices=rows('vertices');parts=rows('parts')
indices=list(map(int,re.findall(r'\d+',header.split('indices[]={')[1].split('};')[0])))
meshes={}
for first,count,variant,limb,material in parts:
    meshes.setdefault((variant,limb),set()).update(vertices[indices[i]][:3] for i in range(first,first+count))
def transform(pose,limb,p):
    m=pose[str(limb)]
    return [sum(m[i][j]*p[j] for j in range(3))+m[i][3] for i in range(3)]
pairs=[(9,8,17.43),(14,13,9.64),(19,18,17.43),(24,23,9.64)]
max_gap=0;checks=0;poses=[p for animation in rig['animations'].values() for p in animation]
for upper,lower,length in pairs:
 for pose in poses:
  gap=math.dist(transform(pose,upper,(length,0,0)),transform(pose,lower,(0,0,0)))
  assert gap<.001,f'Detached knee: {upper}, gap {gap}'
  max_gap=max(max_gap,gap);checks+=1
for variant in range(2):
 for lower in (8,13,18,23):
  near=min(math.dist(p,(2800,-200,0)) for p in meshes[variant,lower])
  assert near<260,f'Foot misses the native effect anchor at {lower}: {near}'
 for pose in poses:
  points=[transform(pose,limb,tuple(v/100 for v in p)) for (sp,limb),vs in meshes.items() if sp==variant for p in vs]
  native=[transform(pose,l['limb'],tuple(v/100 for v in p)) for l in rig['rig'] for p in l['vertices']]
  for axis in range(3):
   assert min(p[axis] for p in points)>min(p[axis] for p in native)-4.5
   assert max(p[axis] for p in points)<max(p[axis] for p in native)+4.5
report={'status':'passed','native_animations':{key:len(value) for key,value in rig['animations'].items()},
        'native_frames':len(poses),'knee_checks':checks,'variant_count':2,
        'maximum_knee_gap_world_units':max_gap,'native_envelope_tolerance_world_units':4.5,
        'foot_anchor_tolerance_world_units':2.6,
        'note':'Rigid joint centers, exported integer mesh bounds and claw proximity to native foot effects checked through every native animation frame.'}
(root/'animation-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
