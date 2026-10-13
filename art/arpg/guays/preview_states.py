"""Render original Guay wing poses from the editable native-rig scene."""
import bpy
from pathlib import Path
assert bpy.app.background
root=Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=str(root/'guays.blend'))
scene=bpy.context.scene;scene.render.resolution_percentage=65;scene.cycles.samples=16
for frame,label in [(3,'downstroke'),(5,'fold'),(7,'upstroke')]:
 scene.frame_set(frame);scene.render.filepath=str(root/(label+'-preview.png'));bpy.ops.render.render(write_still=True)
