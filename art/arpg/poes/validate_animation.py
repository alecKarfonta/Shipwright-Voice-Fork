"""Compare final quantized anatomy with all six original animation clips."""
import json,math,re
from pathlib import Path

root=Path(__file__).resolve().parent
h=(root.parents[2]/'soh/soh/Enhancements/arpg/PoeBlenderData.h').read_text()
def rows(key):return [tuple(map(int,re.findall(r'-?\d+',s))) for s in re.findall(r'\{([^}]+)\}',h.split(key+'[]={')[1].split('};')[0])]
v=rows('vertices');rig=json.loads((root/'native_rig.json').read_text());clips=rig['animations']
def tx(m,p):return [(sum(m[i][j]*p[j]/100 for j in range(3))+m[i][3]) for i in range(3)]
max_extension=0;perclip={};checks=0
for name,clip in clips.items():
 ext=0
 for pose in clip:
  native=[tx(pose[str(p[3])],p) for l in rig['rig'] for p in l['vertices']]
  points=[tx(pose[str(p[6])],p) for p in v ];checks+=len(points)
  assert all(math.isfinite(c) for p in points for c in p)
  for k in range(3):ext=max(ext,min(p[k] for p in native)-min(p[k] for p in points),max(p[k] for p in points)-max(p[k] for p in native))
 perclip[name]=ext;max_extension=max(max_extension,ext)
assert max_extension<8,perclip
out={'status':'passed','clips':len(clips),'frames':sum(map(len,clips.values())),'posed_vertex_checks':checks,'native_visible_joints':9,'maximum_envelope_extension_world_units':max_extension,'per_clip_extension_world_units':perclip,'scope':'All 98 native clip frames at the original .01 actor scale. Held-lantern geometry is included. Original per-actor fade, collision and point-light callbacks remain authoritative. Bounds do not establish collision or triangle-clearance correctness.'}
(root/'animation-validation.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
