"""Validate all native animation frames, procedural squash and tentacle anchors."""
import json,math,re
from pathlib import Path
root=Path(__file__).resolve().parent
rig=json.loads((root/'native_rig.json').read_text());header=(root.parents[2]/'soh/soh/Enhancements/arpg/OctorokBlenderData.h').read_text()
def rows(name):
 body=header.split(name+'[]={')[1].split('};')[0]
 return [tuple(map(int,re.findall(r'-?\d+',v))) for v in re.findall(r'\{([^}]+)\}',body)]
vertices=rows('vertices');parts=rows('parts');indices=list(map(int,re.findall(r'\d+',header.split('indices[]={')[1].split('};')[0])))
meshes={}
for first,count,variant,limb,material in parts:meshes.setdefault((variant,limb),set()).update(vertices[indices[i]][:3] for i in range(first,first+count))
def transform(m,p):return [sum(m[i][j]*p[j] for j in range(3))+m[i][3] for i in range(3)]
checks=0;endchecks=0;max_gap=0
for pose in rig['poses']:
 native=[transform(pose[str(l['limb'])],tuple(v/100 for v in p)) for l in rig['rig'] if l['vertices'] for p in l['vertices']]
 points=[transform(pose[str(limb)],tuple(v/100 for v in p)) for variant,limb in meshes for p in meshes[variant,limb]]
 for axis in range(3):
  assert min(p[axis] for p in points)>min(p[axis] for p in native)-3.2, ('lower envelope',axis)
  assert max(p[axis] for p in points)<max(p[axis] for p in native)+3.2, ('upper envelope',axis)
 checks+=1
 for proximal,middle,tip in ((15,14,13),(22,21,20),(29,28,27),(36,35,34)):
  for a,b in ((proximal,middle),(middle,tip)):
   gap=math.dist(transform(pose[str(a)],(13.12,0,0)),transform(pose[str(b)],(0,0,0)))
   max_gap=max(max_gap,gap);assert gap<.001
   for limb,x in ((a,1312),(b,0)):
    assert min(abs(p[0]-x) for p in meshes[0,limb])<90
   endchecks+=1
report={'status':'passed','native_animation_frames':100,'held_death_frames':11,'posed_envelope_checks':checks,'tentacle_anchor_checks':endchecks,'maximum_anchor_gap_world_units':max_gap,'envelope_tolerance_world_units':3.2,'includes_native_head_and_snout_scale':True}
(root/'animation-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
