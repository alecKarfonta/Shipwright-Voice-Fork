"""Package the MCP-authored GLB in a standalone editable Blender document.

Run in a separate factory-startup Blender process so the user's live project is
never replaced. The quad/modifier authoring recipe remains build_waystone.py.
"""
import bpy
from pathlib import Path
from mathutils import Vector

assert bpy.app.background and not bpy.data.filepath, 'Use a separate --background --factory-startup Blender process'
root=Path(__file__).resolve().parent
for obj in list(bpy.context.scene.objects):
    bpy.data.objects.remove(obj,do_unlink=True)
bpy.ops.import_scene.gltf(filepath=str(root/'waystone.glb'))
scene=bpy.context.scene;scene.name='ARPG Waystone Workshop'
collections={}
labels=['Shrine','Heart','First wave','Second wave','Third wave','Coffer','Hinged lid','Unclaimed treasure','Iron crest','Wild crest','Cinder crest','Winter crest','Storm crest','Ritual ward','Ritual seal']
for part,title in enumerate(labels):
    group=bpy.data.collections.new(title);scene.collection.children.link(group);collections[part]=group
for ob in list(scene.objects):
    part=ob.get('arpg_part')
    if part in collections:
        for collection in list(ob.users_collection):collection.objects.unlink(ob)
        collections[part].objects.link(ob)
world=scene.world;world.use_nodes=True
bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND')
bg.inputs[0].default_value=(.06,.07,.08,1);bg.inputs[1].default_value=.5
for name,position,power,size in [('Key',(-65,-80,110),170000,55),('Fill',(90,-40,70),130000,60),('Rim',(-30,60,95),240000,45)]:
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.size=size
    ob=bpy.data.objects.new(name,data);scene.collection.objects.link(ob);ob.location=position
    ob.rotation_euler=(Vector((28,-2,26))-ob.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Waystone Camera');ob=bpy.data.objects.new(data.name,data);scene.collection.objects.link(ob)
ob.location=(105,-160,92);ob.rotation_euler=(Vector((28,-2,26))-ob.location).to_track_quat('-Z','Y').to_euler()
data.type='ORTHO';data.ortho_scale=123;scene.camera=ob
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1600;scene.render.resolution_y=1050;scene.render.resolution_percentage=100
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':
        area.spaces.active.shading.type='MATERIAL';area.spaces.active.region_3d.view_perspective='CAMERA'
for ob in scene.objects: ob.select_set(False)
bpy.ops.wm.save_as_mainfile(filepath=str(root/'waystone.blend'),compress=True)
print('Saved standalone ARPG Blender source with',len(scene.objects),'objects')

scene.render.image_settings.file_format='PNG';scene.render.filepath=str(root/'waystone-preview.png')
bpy.ops.render.render(write_still=True)
