"""Package the MCP-authored GLB in a standalone editable Blender document.

Run in a separate factory-startup Blender process so the user's live project is
never replaced. The quad/modifier authoring recipe remains build_reptiles.py.
"""
import bpy
from pathlib import Path
from mathutils import Vector

assert bpy.app.background and not bpy.data.filepath, 'Use a separate --background --factory-startup Blender process'
root=Path(__file__).resolve().parent
for obj in list(bpy.context.scene.objects):
    bpy.data.objects.remove(obj,do_unlink=True)
bpy.ops.import_scene.gltf(filepath=str(root/'reptiles.glb'))
scene=bpy.context.scene;scene.name='ARPG Reptile Workshop'
collections={}
for sp,title in [(0,'Lizalfos'),(1,'Dinolfos')]:
    group=bpy.data.collections.new(title);scene.collection.children.link(group)
    for limb in (8,9,10,15,16,17,22,23,24,25,29,30,33,38,39,40,45,46,47,48):
        child=bpy.data.collections.new(f'{title} limb {limb:02}');group.children.link(child);collections[sp,limb]=child
for ob in list(scene.objects):
    if ob.get('arpg_state')==2: ob.hide_render=True
    key=(ob.get('arpg_species'),ob.get('arpg_limb'))
    if key in collections:
        for collection in list(ob.users_collection): collection.objects.unlink(ob)
        collections[key].objects.link(ob)
world=scene.world;world.use_nodes=True
bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND')
bg.inputs[0].default_value=(.07,.085,.10,1);bg.inputs[1].default_value=.5
for name,position,power,size in [('Key',(-45,-50,85),180000,45),('Fill',(55,-25,50),100000,40),('Rim',(0,45,75),220000,35)]:
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.size=size
    ob=bpy.data.objects.new(name,data);scene.collection.objects.link(ob);ob.location=position
    ob.rotation_euler=(Vector((0,0,25))-ob.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Reptile Camera');ob=bpy.data.objects.new(data.name,data);scene.collection.objects.link(ob)
ob.location=(90,-140,75);ob.rotation_euler=(Vector((0,0,25))-ob.location).to_track_quat('-Z','Y').to_euler()
data.type='ORTHO';data.ortho_scale=112;scene.camera=ob
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1440;scene.render.resolution_y=960;scene.render.resolution_percentage=100
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':
        area.spaces.active.shading.type='MATERIAL';area.spaces.active.region_3d.view_perspective='CAMERA'
for ob in scene.objects: ob.select_set(False)
bpy.ops.wm.save_as_mainfile(filepath=str(root/'reptiles.blend'),compress=True)
print('Saved standalone ARPG Blender source with',len(scene.objects),'objects')
