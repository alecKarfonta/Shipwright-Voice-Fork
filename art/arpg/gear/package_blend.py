"""Rebuild the isolated gear review scene from export_preview.cpp outputs."""
from pathlib import Path
import bpy
from mathutils import Vector
root=Path(__file__).resolve().parent
for ob in list(bpy.context.scene.objects):
    bpy.data.objects.remove(ob,do_unlink=True)
scene=bpy.context.scene
scene.name='ARPG Gear Identity Workshop'
names=['Forest','Knight','Great','Ember','Frost','Light','Wood','KnightShield','Mirror','Root','Star','Wind']
for i,name in enumerate(names):
    bpy.ops.wm.obj_import(filepath=str(root/(name+'.obj')),forward_axis='NEGATIVE_Z',up_axis='Y')
    for ob in bpy.context.selected_objects:
        ob.location=(i%6*30-75,0,26 if i<6 else -32)
world=bpy.data.worlds.new('Gear workshop world');scene.world=world;world.use_nodes=True
background=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND')
background.inputs['Color'].default_value=(.025,.035,.05,1);background.inputs['Strength'].default_value=.4
camera=bpy.data.cameras.new('Gear identity camera');ob=bpy.data.objects.new('Gear identity camera',camera);scene.collection.objects.link(ob)
ob.location=(0,-245,12);ob.rotation_euler=(Vector((0,0,12))-ob.location).to_track_quat('-Z','Y').to_euler();camera.type='ORTHO';camera.ortho_scale=212;scene.camera=ob
for name,position,energy,size in [('Key',(-75,-80,120),180000,110),('Fill',(85,-60,20),120000,100),('Rim',(0,70,60),180000,70)]:
    light=bpy.data.lights.new(name,'AREA');light.energy=energy;light.shape='DISK';light.size=size
    ob=bpy.data.objects.new(name,light);scene.collection.objects.link(ob);ob.location=position;ob.rotation_euler=(-ob.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1800;scene.render.resolution_y=1100;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.filepath=str(root/'gear-identity-preview.png')
bpy.ops.wm.save_as_mainfile(filepath=str(root/'gear-identity.blend'))
bpy.ops.render.render(write_still=True)
