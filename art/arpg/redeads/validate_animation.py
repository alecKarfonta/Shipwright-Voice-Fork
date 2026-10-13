"""Check exported joint assignments, quantized bridge endpoints and posed native envelopes."""
import json,math,re
from pathlib import Path
root=Path(__file__).resolve().parent
header=(root.parents[2]/'soh/soh/Enhancements/arpg/RedeadBlenderData.h').read_text()
def rows(name):return [tuple(map(int,re.findall(r'-?\d+',v))) for v in re.findall(r'\{([^}]+)\}',header.split(name+'[]={')[1].split('};')[0])]
vertices=rows('vertices');parts=rows('parts');indices=list(map(int,re.findall(r'\d+',header.split('indices[]={')[1].split('};')[0])))
def tx(m,p):return [sum(m[i][j]*p[j] for j in range(3))+m[i][3] for i in range(3)]
meshes={}
for first,count,variant,limb,mat in parts:meshes.setdefault(variant,set()).update(indices[first:first+count])
max_error=0.;max_extension=0.;endpoint_checks=0
connections=json.loads((root/'connections.json').read_text())
for variant in (0,1):
 rig=json.loads((root/('native_gibdo.json' if variant else 'native_redead.json')).read_text())
 # Match authored bridge rings against the integer vertices used by the runtime, under their assigned native joints.
 endpoints=[]
 for c in connections:
  if c['variant']!=variant:continue
  for key in ('parent','child'):
   joint=c[key];pool=[vertices[i] for i in meshes[variant] if vertices[i][6]==joint]
   for p in c[key+'_points']:
    v=min(pool,key=lambda v:math.dist(p,[n/100 for n in v[:3]]));assert math.dist(p,[n/100 for n in v[:3]])<.009
    endpoints.append((joint,p,[n/100 for n in v[:3]]))
 for pose in rig['poses']:
  native=[tx(pose[str(l['limb'])],[v/100 for v in p]) for l in rig['rig'] for p in l['vertices']]
  points=[tx(pose[str(vertices[i][6])],[v/100 for v in vertices[i][:3]]) for i in meshes[variant]]
  assert all(math.isfinite(x) for p in points for x in p)
  for axis in range(3):max_extension=max(max_extension,min(p[axis] for p in native)-min(p[axis] for p in points),max(p[axis] for p in points)-max(p[axis] for p in native))
  for joint,source,integer in endpoints:
   error=math.dist(tx(pose[str(joint)],source),tx(pose[str(joint)],integer));max_error=max(max_error,error);endpoint_checks+=1
assert max_extension<12,'Replacement must remain close to native posed anatomy'
out={'status':'passed','variants':2,'native_animation_clips_per_variant':len(rig['animations']),'native_animation_frames_per_variant':rig['frames'],'visible_limbs_per_variant':15,'bridges_per_variant':9,'posed_bridge_endpoint_checks':endpoint_checks,'maximum_quantized_endpoint_error_world_units':max_error,'maximum_posed_envelope_extension_world_units':max_extension,'scope':'Full native animation poses; quantized endpoints follow their assigned native joints. Overlapping hips, ankles and neck are visually reviewed; no claim of watertight topology or cloth simulation.'}
(root/'animation-validation.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
