"""Verify posed bounds and original articulated joint anchors, including swallowed-bomb swelling."""
import json,math,re
from pathlib import Path
root=Path(__file__).resolve().parent
rig=json.loads((root/'native_rig.json').read_text());header=(root.parents[2]/'soh/soh/Enhancements/arpg/DodongoBlenderData.h').read_text()
def rows(name):return [tuple(map(int,re.findall(r'-?\d+',v))) for v in re.findall(r'\{([^}]+)\}',header.split(name+'[]={')[1].split('};')[0])]
vertices=rows('vertices');parts=rows('parts');indices=list(map(int,re.findall(r'\d+',header.split('indices[]={')[1].split('};')[0])))
meshes={}
for first,count,variant,limb,mat in parts:meshes.setdefault(limb,set()).update(vertices[indices[i]][:3] for i in range(first,first+count))
def tx(m,p):return [sum(m[i][j]*p[j] for j in range(3))+m[i][3] for i in range(3)]
max_extension=0.;max_gap=0.;anchors=0
links=[(16,15,15.14),(15,14,12.91),(14,13,11.89),(23,22,15),(22,21,16.72),(30,29,15),(29,28,16.72)]
for pose in rig['poses']:
 native=[tx(pose[str(l['limb'])],[v/100 for v in p]) for l in rig['rig'] for p in l['vertices']]
 points=[tx(pose[str(l)],[v/100 for v in p]) for l,vs in meshes.items() for p in vs]
 for axis in range(3):
  max_extension=max(max_extension,min(p[axis] for p in native)-min(p[axis] for p in points),max(p[axis] for p in points)-max(p[axis] for p in native))
 for a,b,length in links:
  gap=math.dist(tx(pose[str(a)],(length,0,0)),tx(pose[str(b)],(0,0,0)));max_gap=max(max_gap,gap);assert gap<.001,(a,b,gap)
  for limb,end in [(a,length*100),(b,0)]:assert min(math.dist(p,(end,0,0)) for p in meshes[limb])<700,(limb,end)
  anchors+=1
# Native OverrideLimbDraw scales these two meshes independently. Verify the full procedural scale range without changing collision/attachment transforms.
swelling=0
for tick in range(9):
 sx=math.sin(tick*math.pi/8)+1;sy=math.sin(tick*math.pi/8)*.5+1
 for limb in (15,16):
  for p in meshes[limb]:assert all(math.isfinite(v) and abs(v)<32000 for v in (p[0]*sx,p[1]*sy,p[2]*sy))
  swelling+=1
print('Measured envelope extension',max_extension);assert max_extension<5,'Authored shape should remain near the native collision envelope'
out={'status':'passed','native_animation_frames':rig['frames'],'native_visible_limbs':len(meshes),'joint_anchor_checks':anchors,'maximum_anchor_gap_authoring_units':max_gap,'maximum_envelope_extension_world_units':max_extension*1.875,'normal_actor_scale':.01875,'procedural_swelling_checks':swelling,'rig':'Original upper head, lower jaw, torso, three-part tail and two three-part legs; native draw-time torso/tail swelling retained'}
(root/'animation-validation.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
