"""Render native Dodongo animation poses from the standalone Blender source."""
import bpy
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parent
assert bpy.app.background and Path(bpy.data.filepath)==root/'dodongos.blend'
scene=bpy.context.scene;scene.cycles.samples=12;scene.render.resolution_x=600;scene.render.resolution_y=600
scene.camera.location=(85,-105,60);scene.camera.rotation_euler=(Vector((0,12,5))-scene.camera.location).to_track_quat('-Z','Y').to_euler();scene.camera.data.ortho_scale=85
for frame in (1,70,115,155,283,310,366,386):
 scene.frame_set(frame);scene.render.filepath=str(root/f'motion-{frame:03}.png');bpy.ops.render.render(write_still=True)
