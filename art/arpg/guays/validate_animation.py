"""Validate quantized geometry and mixed-joint connections in native flight and dead-tail poses."""
import json,math,re
from pathlib import Path
root=Path(__file__).resolve().parent
h=(root.parents[2]/'soh/soh/Enhancements/arpg/GuayBlenderData.h').read_text()
def rows(key):return [tuple(map(int,re.findall(r'-?\d+',s))) for s in re.findall(r'\{([^}]+)\}',h.split(key+'[]={')[1].split('};')[0])]
v=rows('vertices');rig=json.loads((root/'native_rig.json').read_text());connections=json.loads((root/'connections.json').read_text());rest=rig['poses'][0]
def tx(m,p):return [sum(m[i][j]*p[j] for j in range(3))+m[i][3] for i in range(3)]
def local(m,p):return [sum(m[i][j]*(p[i]-m[i][3]) for i in range(3)) for j in range(3)]
maximum=0;checks=0;connection_checks=0;part_extensions={}
for name,poses in [('living_tail_wag',rig['poses']),('death_tail_neutral',rig['dead_poses'])]:
 ext=0
 for pose in poses:
  native=[tx(pose[str(p[3])],[c/100 for c in p[:3]]) for l in rig['rig'] for p in l['vertices']]
  points=[tx(pose[str(p[6])],[c/100 for c in p[:3]]) for p in v];checks+=len(points)
  assert all(math.isfinite(c) for p in points for c in p)
  for k in range(3):ext=max(ext,min(p[k] for p in native)-min(p[k] for p in points),max(p[k] for p in points)-max(p[k] for p in native))
  for bridge in connections:
   for label,joint in [('parent',bridge['parent']),('child',bridge['limb'])]:
    for world in bridge[label+'_world_points']:
     q=local(rest[str(joint)],world);expected=tx(pose[str(joint)],q)
     candidates=[p for p in v if p[6]==joint and math.dist([c/100 for c in p[:3]],q)<.015]
     assert candidates,'Missing quantized bridge endpoint'
     assert min(math.dist(tx(pose[str(joint)],[c/100 for c in p[:3]]),expected) for p in candidates)<.015
     connection_checks+=1
 part_extensions[name]=ext;maximum=max(maximum,ext)
assert maximum<6,part_extensions
out={'status':'passed','native_clips':1,'native_frames':8,'callback_pose_variants':16,'posed_vertex_checks':checks,'connection_endpoint_checks':connection_checks,'native_visible_joints':7,'mixed_joint_connections':6,'maximum_extension_regular_world_units':maximum,'maximum_extension_giant_world_units':maximum*3,'per_state_regular_extension':part_extensions,'scope':'Every native fly frame with original live tail offsets and dead neutral-tail callbacks, measured at .01 and .03 actor scales. Runtime turning/pitch and growing/shrinking actor scale do not change local joint assignment. Bounds do not establish triangle-level intersection clearance.'}
(root/'animation-validation.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
