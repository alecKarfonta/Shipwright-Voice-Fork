"""Render the authored Poe using its native attack, fall, laugh and stand clips."""
import bpy,json
from pathlib import Path
assert bpy.app.background
root=Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=str(root/'poe.blend'))
scene=bpy.context.scene;scene.render.resolution_percentage=65;scene.cycles.samples=16
clips=json.loads((root/'native_rig.json').read_text())['animations'];offsets={};n=1
for name,frames in clips.items():offsets[name]=n;n+=len(frames)
for name,frame,label in [('gPoeAttackAnim',4,'attack'),('gPoeDamagedAnim',8,'damage'),('gPoeFleeAnim',4,'flee'),('gPoeAppearAnim',20,'appear')]:
 scene.frame_set(offsets[name]+frame);scene.render.filepath=str(root/(label+'-preview.png'));bpy.ops.render.render(write_still=True)
