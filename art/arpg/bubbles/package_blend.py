"""Run in a separate Blender --background --factory-startup process.

The live Blender user's unrelated project is never saved or replaced. Rebuilds
the original animation preview from the same native rig used during authoring.
"""
import bpy
import json
from pathlib import Path
from mathutils import Matrix, Vector
assert bpy.app.background and not bpy.data.filepath
root=Path(__file__).resolve().parent
for ob in list(bpy.context.scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
bpy.ops.import_scene.gltf(filepath=str(root/'bubbles.glb'))
scene=bpy.context.scene;scene.name='ARPG Bubble Workshop'
rig=json.loads((root/'native_rig.json').read_text())
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
for ob in list(scene.objects):
    limb=ob['arpg_limb'];variant=ob['arpg_variant'];ob.rotation_mode='QUATERNION'
    for frame,pose in enumerate(rig['poses']):
        ob.matrix_world=axis @ Matrix(pose[str(limb)])
        ob.keyframe_insert(data_path='location',frame=frame+1)
        ob.keyframe_insert(data_path='scale',frame=frame+1)
        ob.keyframe_insert(data_path='rotation_quaternion',frame=frame+1)
scene.frame_start=1;scene.frame_end=rig['frames'];scene.render.fps=20;scene.frame_set(19)
world=scene.world;world.use_nodes=True
bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs[0].default_value=(.04,.05,.07,1);bg.inputs[1].default_value=.7
assert 'AREA' in [i.identifier for i in bpy.types.Light.bl_rna.properties['type'].enum_items]
for name,pos,power,size in [('Key',(0,-40,45),9000,25),('Fill',(-50,-25,30),6000,25),('Rim',(30,40,35),10000,20)]:
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.size=size
    ob=bpy.data.objects.new(name,data);scene.collection.objects.link(ob);ob.location=pos
    ob.rotation_euler=(Vector((0,0,3))-ob.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Bubble Camera');ob=bpy.data.objects.new(data.name,data);scene.collection.objects.link(ob)
ob.location=(32,-85,34);ob.rotation_euler=(Vector((0,0,3))-ob.location).to_track_quat('-Z','Y').to_euler()
assert 'ORTHO' in [i.identifier for i in bpy.types.Camera.bl_rna.properties['type'].enum_items]
data.type='ORTHO';data.ortho_scale=65;scene.camera=ob
try:scene.render.engine='CYCLES'
except TypeError as error:raise RuntimeError(str(error))
assert 'CPU' in [i.identifier for i in scene.cycles.bl_rna.properties['device'].enum_items]
scene.cycles.samples=24;scene.cycles.device='CPU'
scene.render.resolution_x=1200;scene.render.resolution_y=1200;scene.render.resolution_percentage=100
bpy.ops.wm.save_as_mainfile(filepath=str(root/'bubbles.blend'),compress=True)
scene.render.filepath=str(root/'bubbles-studio.png');bpy.ops.render.render(write_still=True)
