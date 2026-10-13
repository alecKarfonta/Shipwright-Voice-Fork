"""Rebuild editable native animation as weighted armatures in an isolated Blender process."""
import bpy,json
from pathlib import Path
from mathutils import Matrix,Vector
assert bpy.app.background and not bpy.data.filepath
root=Path(__file__).resolve().parent
for ob in list(bpy.context.scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
bpy.ops.import_scene.gltf(filepath=str(root/'shellblades.glb'))
scene=bpy.context.scene;scene.name='ARPG Shellblade Workshop'
rig=json.loads((root/'native_rig.json').read_text());rest=rig['animations']['object_sb_Anim_000194'][5]
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
parts=list(scene.objects)
limbs=[l['limb'] for l in rig['rig'] if l['dl']]
for variant in (0,):
 offset=Matrix.Translation((variant*155,0,0));arm=bpy.data.armatures.new('Native Shellblade skeleton '+str(variant));ob=bpy.data.objects.new(arm.name,arm);scene.collection.objects.link(ob)
 bpy.context.view_layer.objects.active=ob;ob.select_set(True);bpy.ops.object.mode_set(mode='EDIT')
 for limb in limbs:
  bone=arm.edit_bones.new(str(limb));bone.head=(0,0,0);bone.tail=(0,1,0);bone.matrix=offset@axis@Matrix(rest[str(limb)])
 bpy.ops.object.mode_set(mode='POSE')
 for limb in limbs:
  bone=ob.pose.bones[str(limb)];bone.rotation_mode='QUATERNION'
  for frame,pose in enumerate(rig['poses']):
   bone.matrix=offset@axis@Matrix(pose[str(limb)])
   for field in ('location','rotation_quaternion','scale'):bone.keyframe_insert(data_path=field,frame=frame+1)
 bpy.ops.object.mode_set(mode='OBJECT');ob.select_set(False)
 for part in parts:
  if part['arpg_variant']!=variant:continue
  limb=part['arpg_limb'];base=offset@axis@Matrix(rest[str(limb)]);parent=part.get('arpg_parent_joint')
  vg=part.vertex_groups.new(name=str(limb));pv=part.vertex_groups.new(name=str(parent)) if parent is not None else None
  for v in part.data.vertices:
   original=v.co.copy();is_parent=parent is not None and any((original-Vector(p)).length<.0001 for p in part['arpg_parent_ring_points'])
   (pv if is_parent else vg).add([v.index],1.0,'REPLACE');v.co=base@original
  part.animation_data_clear();part.matrix_world=Matrix.Identity(4);mod=part.modifiers.new('Native joint connections','ARMATURE');mod.object=ob
scene.frame_start=1;scene.frame_end=rig['frames'];scene.render.fps=20;scene.frame_set(22)
world=scene.world;world.use_nodes=True;bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs[0].default_value=(.045,.052,.064,1);bg.inputs[1].default_value=.7
assert 'AREA' in [i.identifier for i in bpy.types.Light.bl_rna.properties['type'].enum_items]
for name,pos,power,size in [('Key',(0,-60,100),22000,45),('Fill',(-60,-30,55),12000,45),('Rim',(70,40,90),22000,35)]:
 data=bpy.data.lights.new(name,'AREA');data.energy=power*1.8;data.size=size*1.6;ob=bpy.data.objects.new(name,data);scene.collection.objects.link(ob);ob.location=pos;ob.rotation_euler=(Vector((0,0,25))-ob.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Shellblade Camera');ob=bpy.data.objects.new(data.name,data);scene.collection.objects.link(ob);ob.location=(110,-155,110);ob.rotation_euler=(Vector((0,0,25))-ob.location).to_track_quat('-Z','Y').to_euler()
assert 'ORTHO' in [i.identifier for i in bpy.types.Camera.bl_rna.properties['type'].enum_items];data.type='ORTHO';data.ortho_scale=160;scene.camera=ob
try:scene.render.engine='CYCLES'
except TypeError as error:raise RuntimeError(str(error))
assert 'CPU' in [i.identifier for i in scene.cycles.bl_rna.properties['device'].enum_items];scene.cycles.samples=24;scene.cycles.device='CPU';scene.render.resolution_x=1600;scene.render.resolution_y=1050;scene.render.resolution_percentage=100
bpy.ops.wm.save_as_mainfile(filepath=str(root/'shellblades.blend'),compress=True)
scene.render.filepath=str(root/'shellblades-studio.png');bpy.ops.render.render(write_still=True)
