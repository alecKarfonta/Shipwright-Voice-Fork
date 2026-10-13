"""Validate integer runtime geometry through every original Moblin animation pose."""
import json,math,re
from pathlib import Path
root=Path(__file__).resolve().parent
header=(root.parents[2]/'soh/soh/Enhancements/arpg/MoblinBlenderData.h').read_text()
def rows(name):return [tuple(map(int,re.findall(r'-?\d+',v))) for v in re.findall(r'\{([^}]+)\}',header.split(name+'[]={')[1].split('};')[0])]
vertices=rows('vertices');parts=rows('parts');indices=list(map(int,re.findall(r'\d+',header.split('indices[]={')[1].split('};')[0])))
def tx(m,p):return [sum(m[i][j]*p[j] for j in range(3))+m[i][3] for i in range(3)]
connections=json.loads((root/'connections.json').read_text());results=[]
for variant,name,scale in [(0,'spear',1),(1,'club',2)]:
 rig=json.loads((root/f'native_{name}.json').read_text());ids=set(i for first,count,sp,limb,mat in parts if sp==variant for i in indices[first:first+count])
 points={j:set(tuple(v/100 for v in vertices[i][:3]) for i in ids if vertices[i][6]==j) for j in {vertices[i][6] for i in ids}}
 endpoints=[];count=0;error=0;extension=0
 for c in connections:
  if c['variant']!=variant:continue
  count+=1
  for key in ('parent','child'):
   for p in c[key+'_points']:
    integer=min(points[c[key]],key=lambda q:math.dist(p,q));assert math.dist(p,integer)<.009
    endpoints.append((c[key],p,integer))
 assert count==12 and len(points)==15
 for pose in rig['poses']:
  # Extracted native vertices record the matrix that actually transforms each vertex.
  native=[tx(pose[str(l['limb'])],[v/100 for v in p]) for l in rig['rig'] for p in l['vertices']]
  posed=[tx(pose[str(j)],p) for j,group in points.items() for p in group]
  assert all(math.isfinite(x) for p in posed for x in p)
  for axis in range(3):extension=max(extension,min(p[axis] for p in native)-min(p[axis] for p in posed),max(p[axis] for p in posed)-max(p[axis] for p in native))
  for j,p,q in endpoints:error=max(error,math.dist(tx(pose[str(j)],p),tx(pose[str(j)],q)))
 assert extension<25,'Replacement strays beyond the native posed silhouette budget'
 results.append({'variant':name,'native_clips':len(rig['animations']),'native_frames':len(rig['poses']),'visible_limbs':15,'joint_connections':count,'posed_endpoint_checks':len(endpoints)*len(rig['poses']),'maximum_quantized_endpoint_error_world_units':error*scale,'maximum_posed_envelope_extension_world_units':extension*scale})
results[0]['patrol_world_scale_multiplier']=1.4
out={'status':'passed','variants':results,'scope':'Every original animation frame; bridge endpoints use native matrices. Envelope comparison is per axis, not collision proof; motion renders and native combat separately reviewed.'}
(root/'animation-validation.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
