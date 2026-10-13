import bpy
from pathlib import Path
root=Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=str(root/'redeads.blend'))
s=bpy.context.scene;s.render.resolution_percentage=70;s.cycles.samples=12
for f in [45,80,155,190,225]:
 s.frame_set(f);s.render.filepath=str(root/f'pose-{f}.png');bpy.ops.render.render(write_still=True)
