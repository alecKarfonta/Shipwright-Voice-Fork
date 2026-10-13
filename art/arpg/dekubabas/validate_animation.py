"""Check exported native integers against both original jaw animations.

The stem preview is illustrative; endpoint continuity is checked separately from
native runtime motion, which is covered by the opt-in game integration harness.
"""
import json,math,re
from pathlib import Path
root=Path(__file__).resolve().parent
rig=json.loads((root/'native_rig.json').read_text());header=(root.parents[2]/'soh/soh/Enhancements/arpg/DekuBabaBlenderData.h').read_text()
def rows(name):
 body=header.split(name+'[]={')[1].split('};')[0]
 return [tuple(map(int,re.findall(r'-?\d+',v))) for v in re.findall(r'\{([^}]+)\}',body)]
vertices=rows('vertices');parts=rows('parts');indices=list(map(int,re.findall(r'\d+',header.split('indices[]={')[1].split('};')[0])))
meshes={}
for first,count,variant,limb,material in parts:meshes.setdefault((variant,limb),set()).update(vertices[indices[i]][:3] for i in range(first,first+count))
def transform(m,p):return [sum(m[i][j]*p[j] for j in range(3))+m[i][3] for i in range(3)]
checks=0
for variant in range(2):
 for pose in rig['poses']:
  native=[transform(pose[str(l['limb'])],tuple(v/100 for v in p)) for l in rig['rig'] if l['vertices'] for p in l['vertices']]
  points=[transform(pose[str(limb)],tuple(v/100 for v in p)) for limb in (4,7) for p in meshes[variant,limb]]
  for axis in range(3):
   assert min(p[axis] for p in points)>min(p[axis] for p in native)-2.6
   assert max(p[axis] for p in points)<max(p[axis] for p in native)+2.6
  checks+=1
 for part in (8,9,10):
  vs=meshes[variant,part]
  assert min(p[2] for p in vs)<0 and max(p[2] for p in vs)>=2000
  assert min(math.dist(p,(0,0,0)) for p in vs)<130
  assert min(math.dist(p,(0,0,2000)) for p in vs)<130
endchecks=0
for pose in rig['preview']:
 for high,low in ((8,9),(9,10)):
  assert math.dist(transform(pose[str(high)],(0,0,0)),transform(pose[str(low)],(0,0,20)))<.001
  endchecks+=1
report={'status':'passed','native_jaw_frames':len(rig['poses']),'variant_frame_checks':checks,'studio_stem_endpoint_checks':endchecks,'native_head_envelope_tolerance_world_units':2.6,'note':'Quantized jaw meshes checked through all native animation frames; stem preview endpoint checks are supplemented by native live growth/lunge/pruning tests.'}
(root/'animation-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
