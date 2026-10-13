"""Render authored anatomy at native running, spitting, stunned and burrow poses."""
import bpy,json
from pathlib import Path
assert bpy.app.background
root=Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=str(root/'mad-scrub.blend'))
scene=bpy.context.scene;scene.render.resolution_percentage=65;scene.cycles.samples=16
clips=json.loads((root/'native_rig.json').read_text())['animations'];offsets={};n=1
for name,frames in clips.items():offsets[name]=n;n+=len(frames)
for name,frame,label in [('gDekuNutsRunAnim',5,'run'),('gDekuNutsSpitAnim',6,'spit'),('gDekuNutsDamageAnim',7,'damage'),('gDekuNutsBurrowAnim',9,'burrow')]:
 for ob in scene.objects:
  if ob.get('arpg_limb')==0:ob.hide_render=label in ('run','damage')
 scene.frame_set(offsets[name]+frame);scene.render.filepath=str(root/(label+'-preview.png'));bpy.ops.render.render(write_still=True)
