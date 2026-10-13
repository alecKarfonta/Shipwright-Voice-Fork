"""Render the authored mummy using its native attack, fall, laugh and stand clips."""
import bpy,json
from pathlib import Path
assert bpy.app.background
root=Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=str(root/'anubis.blend'))
scene=bpy.context.scene;scene.render.resolution_percentage=65;scene.cycles.samples=16
clips=json.loads((root/'native_rig.json').read_text())['animations'];offsets={};n=1
for name,frames in clips.items():offsets[name]=n;n+=len(frames)
for name,frame,label in [('gAnubiceAttackingAnim',12,'attack'),('gAnubiceFallDownAnim',16,'fall'),('gAnubiceLaughingAnim',6,'laugh'),('gAnubiceStandUpAnim',5,'stand')]:
 scene.frame_set(offsets[name]+frame);scene.render.filepath=str(root/(label+'-preview.png'));bpy.ops.render.render(write_still=True)
