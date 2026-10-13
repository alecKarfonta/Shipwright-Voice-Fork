"""Check exported joint assignments, quantized bridge endpoints and posed native envelopes."""
import json,math,re
from pathlib import Path
root=Path(__file__).resolve().parent
header=(root.parents[2]/'soh/soh/Enhancements/arpg/BeamosBlenderData.h').read_text()
def rows(name):return [tuple(map(int,re.findall(r'-?\d+',v))) for v in re.findall(r'\{([^}]+)\}',header.split(name+'[]={')[1].split('};')[0])]
vertices=rows('vertices');parts=rows('parts');indices=list(map(int,re.findall(r'\d+',header.split('indices[]={')[1].split('};')[0])))
def tx(m,p):return [sum(m[i][j]*p[j] for j in range(3))+m[i][3] for i in range(3)]
meshes={}
for first,count,variant,limb,mat in parts:meshes.setdefault(variant,set()).update(indices[first:first+count])
max_error=0.;max_extension=0.;endpoint_checks=0
connections=json.loads((root/'connections.json').read_text())
for variant in (0,):
 rig=json.loads((root/'native_rig.json').read_text())
 # [TIME] | [INFO] | Beamos.Asset | Match authored bridge rings against the integer vertices used by the runtime, under their assigned native joints.
 endpoints=[]
 for c in connections:
  if c['variant']!=variant:continue
  for key in ('parent','child'):
   joint=c[key];pool=[vertices[i] for i in meshes[variant] if vertices[i][6]==joint]
   for p in c[key+'_points']:
    v=min(pool,key=lambda v:math.dist(p,[n/100 for n in v[:3]]));assert math.dist(p,[n/100 for n in v[:3]])<.009
    endpoints.append((joint,p,[n/100 for n in v[:3]]))
 for pose in rig['poses']:
  native=[tx(pose[str(p[3])],[v/100 for v in p[:3]]) for l in rig['rig'] for p in l['vertices']]
  points=[tx(pose[str(vertices[i][6])],[v/100 for v in vertices[i][:3]]) for i in meshes[variant]]
  assert all(math.isfinite(x) for p in points for x in p)
  for axis in range(3):max_extension=max(max_extension,min(p[axis] for p in native)-min(p[axis] for p in points),max(p[axis] for p in points)-max(p[axis] for p in native))
  for joint,source,integer in endpoints:
   error=math.dist(tx(pose[str(joint)],source),tx(pose[str(joint)],integer));max_error=max(max_error,error);endpoint_checks+=1
assert max_extension<8,'Replacement must remain close to native posed anatomy'
out={'status':'passed','geometry_variants':1,'native_animation_clips_per_variant':len(rig['animations']),'native_animation_frames_per_variant':rig['frames'],'visible_limbs_in_rig':4,'bridges':0,'posed_bridge_endpoint_checks':endpoint_checks,'maximum_quantized_endpoint_error_world_units':max_error,'maximum_posed_envelope_extension_world_units':max_extension,'actor_scale':.01,'scope':'Full native animation poses; quantized endpoints follow their assigned native joints. Native source vertices and authored connection endpoints are transformed under their assigned joints. Native root offsets translate both native and authored geometry equally. Shutters, inset eye and pedestal are reviewed in the eight native clip frames; no claim of watertight topology.'}
(root/'animation-validation.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
# [TIME] | [INFO] | Beamos.Validation | The scanner's main motion is added by its limb-two callback, outside the eight-frame eyelid clip.
def mm(a,b):return [[sum(a[i][k]*b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]
def head_matrix(yaw,pitch):
 cy,sy,cx,sx=math.cos(yaw),math.sin(yaw),math.cos(pitch),math.sin(pitch)
 return [[cy,sy*sx,sy*cx,-60*sy*sx],[0,cx,-sx,60-60*cx],[-sy,cy*sx,cy*cx,-60*cy*sx],[0,0,0,1]]
extra=0.;checks=0
for pose in rig['poses']:
 for yaw in [i*math.tau/8 for i in range(8)]:
  for pitch in (0,.21,.43,0x1B91*math.pi/32768):
   h=head_matrix(yaw,pitch);sweep={k:mm(h,m) if int(k) in (5,8,9) else m for k,m in pose.items()}
   native=[tx(sweep[str(p[3])],[v/100 for v in p[:3]]) for l in rig['rig'] for p in l['vertices']]
   points=[tx(sweep[str(vertices[i][6])],[v/100 for v in vertices[i][:3]]) for i in meshes[0]];checks+=len(points)
   for axis in range(3):extra=max(extra,min(p[axis] for p in native)-min(p[axis] for p in points),max(p[axis] for p in points)-max(p[axis] for p in native))
assert extra<2,'Scanner sweep must track the native head envelope'
out.update({'native_callback_sweep_poses':256,'swept_vertex_checks':checks,'maximum_sweep_extension_small_world_units':extra,'maximum_sweep_extension_large_world_units':extra*1.4,'sweep_scope':'All eight shutter poses, eight head yaws and four pitches through the native attack clamp. Envelope validation does not prove per-triangle clearance or actual collision.'})
(root/'animation-validation.json').write_text(json.dumps(out,indent=2)+'\n');print('Callback sweeps:',checks,'checks; extension',extra)
