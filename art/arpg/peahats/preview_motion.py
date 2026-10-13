import bpy,json
from mathutils import Vector
from pathlib import Path
root=Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=str(root/'peahats.blend'))
s=bpy.context.scene;s.render.resolution_percentage=70;s.cycles.samples=12
rig=json.loads((root/'native_rig.json').read_text());frame=1
for name,poses in rig['animations'].items():
 if True:
  index=frame+len(poses)//2;s.frame_set(index)
  deps=bpy.context.evaluated_depsgraph_get();camera=s.camera;inv=camera.matrix_world.inverted()
  corners=[inv@(ob.matrix_world@Vector(c)) for ob in s.objects if ob.type=='MESH' and not ob.hide_render for c in ob.evaluated_get(deps).bound_box]
  center=Vector(((min(p.x for p in corners)+max(p.x for p in corners))/2,(min(p.y for p in corners)+max(p.y for p in corners))/2,0));camera.location+=camera.matrix_world.to_3x3()@center
  camera.data.ortho_scale=max(max(p.x for p in corners)-min(p.x for p in corners),(max(p.y for p in corners)-min(p.y for p in corners))*s.render.resolution_x/s.render.resolution_y)*1.15
  s.render.filepath=str(root/(name+'.png'));bpy.ops.render.render(write_still=True)
 frame+=len(poses)
