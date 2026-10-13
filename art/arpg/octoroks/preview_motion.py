"""Render the saved native animation preview in an isolated Blender process."""
import bpy
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parent
assert bpy.app.background and Path(bpy.data.filepath)==root/'octoroks.blend'
scene=bpy.context.scene;scene.cycles.samples=12
scene.render.resolution_x=600;scene.render.resolution_y=600
scene.camera.location=(70,-190,65);scene.camera.rotation_euler=(Vector((0,0,4))-scene.camera.location).to_track_quat('-Z','Y').to_euler();scene.camera.data.ortho_scale=115
for frame in (1,9,17,21,23,29):
 scene.frame_set(frame);scene.render.filepath=str(root/f'motion-{frame:03}.png');bpy.ops.render.render(write_still=True)
