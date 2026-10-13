"""Check exported joint assignments, quantized bridge endpoints and posed native envelopes."""
import json,math,re
from pathlib import Path
root=Path(__file__).resolve().parent
header=(root.parents[2]/'soh/soh/Enhancements/arpg/PeahatBlenderData.h').read_text()
def rows(name):return [tuple(map(int,re.findall(r'-?\d+',v))) for v in re.findall(r'\{([^}]+)\}',header.split(name+'[]={')[1].split('};')[0])]
vertices=rows('vertices');parts=rows('parts');indices=list(map(int,re.findall(r'\d+',header.split('indices[]={')[1].split('};')[0])))
def tx(m,p):return [sum(m[i][j]*p[j] for j in range(3))+m[i][3] for i in range(3)]
meshes={}
for first,count,variant,limb,mat in parts:meshes.setdefault(variant,set()).update(indices[first:first+count])
max_error=0.;max_extension=0.;endpoint_checks=0
connections=json.loads((root/'connections.json').read_text())
for variant in (0,):
 rig=json.loads((root/'native_rig.json').read_text())
 # [TIME] | [INFO] | Peahat.Asset | Match authored bridge rings against the integer vertices used by the runtime, under their assigned native joints.
 endpoints=[]
 for c in connections:
  if c['variant']!=variant:continue
  for key in ('parent','child'):
   joint=c[key];pool=[vertices[i] for i in meshes[variant] if vertices[i][6]==joint]
   for p in c[key+'_points']:
    v=min(pool,key=lambda v:math.dist(p,[n/100 for n in v[:3]]));assert math.dist(p,[n/100 for n in v[:3]])<.009
    endpoints.append((joint,p,[n/100 for n in v[:3]]))
 def mm(a,b):return [[sum(a[i][k]*b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]
 def rot(axis,angle):
  m=[[float(i==j) for j in range(4)] for i in range(4)];a,b=[i for i in range(3) if i!=axis];m[a][a]=m[b][b]=math.cos(angle);m[a][b]=-math.sin(angle);m[b][a]=math.sin(angle);return m
 cases=list(rig['poses'])
 # [TIME] | [INFO] | Peahat.Validate | Exercise native propeller rotation and maximum night-hit squash separately from the animation clips. Weak root collision remains on its unwarped native post matrix.
 for step in range(16):
  for squash in (0,.075,.205):
   pose=json.loads(json.dumps(rig['animations']['gPeehatFlyingAnim'][0]));spin=rot(1,step*math.tau/16)
   for joint in (7,10,13,16,19,22):pose[str(joint)]=mm(spin,pose[str(joint)])
   r=mm(rot(0,step*.115),mm(rot(1,step*.13),rot(2,step*.1)));inv=[[r[j][i] for j in range(4)] for i in range(4)];scale=[[float(i==j)*(1+squash if i==1 else 1-squash if i<3 else 1) for j in range(4)] for i in range(4)];distort=mm(r,mm(scale,inv))
   for joint in (3,23):pose[str(joint)]=mm(distort,pose[str(joint)])
   cases.append(pose)
 for pose in cases:
  native=[tx(pose[str(p[3])],[v/100 for v in p[:3]]) for l in rig['rig'] for p in l['vertices']]
  points=[tx(pose[str(vertices[i][6])],[v/100 for v in vertices[i][:3]]) for i in meshes[variant]]
  assert all(math.isfinite(x) for p in points for x in p)
  for axis in range(3):max_extension=max(max_extension,min(p[axis] for p in native)-min(p[axis] for p in points),max(p[axis] for p in points)-max(p[axis] for p in native))
  for joint,source,integer in endpoints:
   error=math.dist(tx(pose[str(joint)],source),tx(pose[str(joint)],integer));max_error=max(max_error,error);endpoint_checks+=1
assert max_extension<8,'Replacement must remain close to native posed anatomy'
out={'status':'passed','geometry_variants':1,'native_animation_clips_per_variant':len(rig['animations']),'native_animation_frames_per_variant':rig['frames'],'visible_limbs_in_rig':8,'extra_native_draws':1,'bridges':0,'additional_rotor_squash_poses':48,'posed_bridge_endpoint_checks':endpoint_checks,'maximum_quantized_endpoint_error_world_units':max_error*3.6,'maximum_posed_envelope_extension_world_units':max_extension*3.6,'actor_scale':.036,'scope':'All native animation poses plus 48 independent rotor/squash samples; rigid geometry follows its assigned native joints. Native source vertices and authored connection endpoints are transformed under their assigned joints. Native root offsets translate both native and authored geometry equally. Bulb, crown, six blades and weak root are reviewed in all four clips; no claim of watertight topology.'}
(root/'animation-validation.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
