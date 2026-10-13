"""Validate all native Bubble frames, bounds and both moving wing seams."""
import json,math,re
from pathlib import Path
root=Path(__file__).resolve().parent
rig=json.loads((root/'native_rig.json').read_text());header=(root.parents[2]/'soh/soh/Enhancements/arpg/BubbleBlenderData.h').read_text()
def rows(name):return [tuple(map(int,re.findall(r'-?\d+',v))) for v in re.findall(r'\{([^}]+)\}',header.split(name+'[]={')[1].split('};')[0])]
vertices=rows('vertices');parts=rows('parts');indices=list(map(int,re.findall(r'\d+',header.split('indices[]={')[1].split('};')[0])))
meshes={}
for first,count,variant,limb,material in parts:meshes.setdefault(limb,set()).update(tuple(v/100 for v in vertices[indices[i]][:3]) for i in range(first,first+count))
def transform(m,p):return [sum(m[i][j]*p[j] for j in range(3))+m[i][3] for i in range(3)]
checks=0;gap=0;excess=0
for pose in rig['poses']:
 native=[transform(pose[str(l['limb'])],tuple(v/100 for v in p)) for l in rig['rig'] if l['vertices'] for p in l['vertices']]
 points=[transform(pose[str(limb)],p) for limb in meshes for p in meshes[limb]]
 for axis in range(3):excess=max(excess,min(p[axis] for p in native)-min(p[axis] for p in points),max(p[axis] for p in points)-max(p[axis] for p in native))
 for a,b in ((9,8),(14,13)):
  for j in range(9):
   y=-4.7+10.3*j/8;pa=(5.6,y,0);pb=(0,y,0)
   d=math.dist(transform(pose[str(a)],pa),transform(pose[str(b)],pb));gap=max(gap,d);assert d<.001
   assert min(math.dist(v,pa) for v in meshes[a])<.09
   assert min(math.dist(v,pb) for v in meshes[b])<.09
   checks+=1
assert excess<3,('Geometry outside native animation envelope',excess)
report={'status':'passed','native_animation_frames':rig['frames'],'native_visible_limbs':len(meshes),'wing_seam_checks':checks,'maximum_seam_gap_world_units':gap,'maximum_envelope_extension_world_units':excess,'normal_actor_scale':.01,'great_green_actor_scale':.03,'rig':'Original skull, snapping mandible and two articulated wing pairs'}
(root/'animation-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
