"""Check the authored limbs against every native Skulltula animation frame."""
import json,math,re
from pathlib import Path
root=Path(__file__).resolve().parent
rig=json.loads((root/'native_rig.json').read_text());header=(root.parents[2]/'soh/soh/Enhancements/arpg/SkulltulaBlenderData.h').read_text()
def rows(name):
 return [tuple(map(int,re.findall(r'-?\d+',v))) for v in re.findall(r'\{([^}]+)\}',header.split(name+'[]={')[1].split('};')[0])]
vertices=rows('vertices');parts=rows('parts');indices=list(map(int,re.findall(r'\d+',header.split('indices[]={')[1].split('};')[0])))
meshes={}
for first,count,variant,limb,material in parts:meshes.setdefault(limb,set()).update(tuple(v/100 for v in vertices[indices[i]][:3]) for i in range(first,first+count))
def transform(m,p):return [sum(m[i][j]*p[j] for j in range(3))+m[i][3] for i in range(3)]
roots={8:(2.26,.81,1.8),20:(2.26,.81,-1.8),11:(-1.88,-.68,2),29:(-1.88,-.68,-2),14:(.47,.17,2.5),17:(.47,.17,-2.5),23:(4.14,1.49,1.6),26:(4.14,1.49,-1.6)}
checks=0;gap=0;excess=0
for pose in rig['poses']:
 native=[transform(pose[str(l['limb'])],tuple(v/100 for v in p)) for l in rig['rig'] if l['vertices'] for p in l['vertices']]
 points=[transform(pose[str(limb)],p) for limb in meshes for p in meshes[limb]]
 for axis in range(3):excess=max(excess,min(p[axis] for p in native)-min(p[axis] for p in points),max(p[axis] for p in points)-max(p[axis] for p in native))
 for limb,p in roots.items():
  a=transform(pose['5'],p);b=transform(pose[str(limb)],(0,0,0));gap=max(gap,math.dist(a,b));assert math.dist(a,b)<.001
  assert min(math.dist(v,p) for v in meshes[5])<.6
  assert min(math.dist(v,(0,0,0)) for v in meshes[limb])<.5
  checks+=1
assert excess<.85,('Geometry outside native animation envelope',excess)
report={'status':'passed','native_animation_frames':rig['frames'],'native_visible_limbs':len(meshes),'leg_anchor_checks':checks,'maximum_anchor_gap_world_units':gap*4,'maximum_envelope_extension_world_units':excess*4,'normal_actor_scale':.04,'big_actor_scale':.056,'rig':'Original leg-root rotations and independently animated mandible; no invented knee animation'}
(root/'animation-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
