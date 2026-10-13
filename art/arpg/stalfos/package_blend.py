"""Package the MCP-authored GLB in a standalone editable Blender document.

Run in a separate factory-startup Blender process so the user's live project is
never replaced. The quad/modifier authoring recipe remains build_stalfos.py.
"""
import bpy
from pathlib import Path
from mathutils import Vector

assert bpy.app.background and not bpy.data.filepath, 'Use a separate --background --factory-startup Blender process'
root=Path(__file__).resolve().parent
for obj in list(bpy.context.scene.objects):
    bpy.data.objects.remove(obj,do_unlink=True)
bpy.ops.import_scene.gltf(filepath=str(root/'stalfos.glb'))
scene=bpy.context.scene;scene.name='ARPG Stalfos Workshop'
collections={}
for sp,title in [(0,'Sentinel'),(1,'Champion'),(2,'Warden')]:
    group=bpy.data.collections.new(title);scene.collection.children.link(group)
    for limb in (10,11,12,13,14,15,18,21,26,27,28,29,34,35,36,37,38,48,49,50,57,58,59,60):
        child=bpy.data.collections.new(f'{title} limb {limb:02}');group.children.link(child);collections[sp,limb]=child
for ob in list(scene.objects):
    key=(ob.get('arpg_rank'),ob.get('arpg_limb'))
    if key in collections:
        for collection in list(ob.users_collection): collection.objects.unlink(ob)
        collections[key].objects.link(ob)
world=scene.world;world.use_nodes=True
bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND')
bg.inputs[0].default_value=(.06,.07,.08,1);bg.inputs[1].default_value=.5
for name,position,power,size in [('Key',(-85,-95,130),340000,75),('Fill',(100,-70,75),250000,65),('Rim',(0,90,135),460000,60)]:
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.size=size
    ob=bpy.data.objects.new(name,data);scene.collection.objects.link(ob);ob.location=position
    ob.rotation_euler=(Vector((0,0,28))-ob.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Stalfos Camera');ob=bpy.data.objects.new(data.name,data);scene.collection.objects.link(ob)
ob.location=(35,-240,88);ob.rotation_euler=(Vector((0,0,28))-ob.location).to_track_quat('-Z','Y').to_euler()
data.type='ORTHO';data.ortho_scale=170;scene.camera=ob
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1700;scene.render.resolution_y=900;scene.render.resolution_percentage=100
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':
        area.spaces.active.shading.type='MATERIAL';area.spaces.active.region_3d.view_perspective='CAMERA'
for ob in scene.objects: ob.select_set(False)
bpy.ops.wm.save_as_mainfile(filepath=str(root/'stalfos.blend'),compress=True)
print('Saved standalone ARPG Blender source with',len(scene.objects),'objects')

scene.render.image_settings.file_format='PNG';scene.render.filepath=str(root/'stalfos-preview.png')
bpy.ops.render.render(write_still=True)
