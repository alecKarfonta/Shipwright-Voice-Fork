"""Render animation extremes and the vulnerable rear from the saved native rig."""
import bpy
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parent
assert bpy.app.background and Path(bpy.data.filepath)==root/'skulltulas.blend'
scene=bpy.context.scene;scene.cycles.samples=12
scene.render.resolution_x=600;scene.render.resolution_y=600
for frame in (1,17,33,47,62,86,111):
 scene.camera.location=(8,-38,10);scene.camera.rotation_euler=(Vector((0,0,3))-scene.camera.location).to_track_quat('-Z','Y').to_euler();scene.camera.data.ortho_scale=34
 scene.frame_set(frame);scene.render.filepath=str(root/f'motion-{frame:03}.png');bpy.ops.render.render(write_still=True)
scene.frame_set(1);scene.camera.location=(8,38,10);scene.camera.rotation_euler=(Vector((0,0,3))-scene.camera.location).to_track_quat('-Z','Y').to_euler()
scene.render.filepath=str(root/'rear.png');bpy.ops.render.render(write_still=True)
