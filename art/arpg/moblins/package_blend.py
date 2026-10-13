"""Build editable weighted native rigs in an isolated Blender process."""
import bpy,json
from pathlib import Path
from mathutils import Matrix,Vector
assert bpy.app.background and not bpy.data.filepath
root=Path(__file__).resolve().parent
for ob in list(bpy.context.scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
bpy.ops.import_scene.gltf(filepath=str(root/'moblins.glb'))
scene=bpy.context.scene;scene.name='ARPG Moblin Workshop';parts=list(scene.objects)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
for variant,name,clip in [(0,'spear','gEnMbSpearStandStillAnim'),(1,'club','gEnMbClubStandStillClubDownAnim')]:
 rig=json.loads((root/f'native_{name}.json').read_text());rest=rig['animations'][clip][0];limbs=[l['limb'] for l in rig['rig'] if l['dl']]
 offset=Matrix.Translation((variant*90,0,0));arm=bpy.data.armatures.new('Native Moblin '+name);ob=bpy.data.objects.new(arm.name,arm);scene.collection.objects.link(ob)
 bpy.context.view_layer.objects.active=ob;ob.select_set(True);bpy.ops.object.mode_set(mode='EDIT')
 for limb in limbs:
  bone=arm.edit_bones.new(str(limb));bone.head=(0,0,0);bone.tail=(0,1,0);bone.matrix=offset@axis@Matrix(rest[str(limb)])
 bpy.ops.object.mode_set(mode='POSE')
 poses=[rest]+rig['poses']
 for limb in limbs:
  bone=ob.pose.bones[str(limb)];bone.rotation_mode='QUATERNION'
  for frame,pose in enumerate(poses):
   bone.matrix=offset@axis@Matrix(pose[str(limb)])
   for field in ('location','rotation_quaternion','scale'):bone.keyframe_insert(data_path=field,frame=frame+1)
 bpy.ops.object.mode_set(mode='OBJECT');ob.select_set(False)
 for part in parts:
  if part['arpg_variant']!=variant:continue
  limb=part['arpg_limb'];base=offset@axis@Matrix(rest[str(limb)]);parent=part.get('arpg_parent_joint');vg=part.vertex_groups.new(name=str(limb));pv=part.vertex_groups.new(name=str(parent)) if parent is not None else None
  for v in part.data.vertices:
   original=v.co.copy();is_parent=parent is not None and any((original-Vector(p)).length<.0001 for p in part['arpg_parent_ring_points'])
   (pv if is_parent else vg).add([v.index],1.0,'REPLACE');v.co=base@original
  part.animation_data_clear();part.matrix_world=Matrix.Identity(4);mod=part.modifiers.new('Native joint connections','ARMATURE');mod.object=ob
 frame=2
 for title,frames in rig['animations'].items():scene.timeline_markers.new(title,frame=frame);frame+=len(frames)
scene.frame_start=1;scene.frame_end=279;scene.render.fps=20;scene.frame_set(1)
world=scene.world;world.use_nodes=True;bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs[0].default_value=(.045,.052,.064,1);bg.inputs[1].default_value=.7
for name,pos,power,size in [('Key',(20,-90,130),75000,70),('Fill',(-75,-45,70),42000,65),('Rim',(140,70,120),80000,65)]:
 data=bpy.data.lights.new(name,'AREA');data.energy=power;data.size=size;ob=bpy.data.objects.new(name,data);scene.collection.objects.link(ob);ob.location=pos;ob.rotation_euler=(Vector((45,0,40))-ob.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Moblin Camera');ob=bpy.data.objects.new(data.name,data);scene.collection.objects.link(ob);ob.location=(145,-250,122);ob.rotation_euler=(Vector((45,-10,40))-ob.location).to_track_quat('-Z','Y').to_euler();data.type='ORTHO';data.ortho_scale=202;scene.camera=ob
try:scene.render.engine='CYCLES'
except TypeError as error:raise RuntimeError(str(error))
scene.cycles.samples=24;scene.cycles.device='CPU';scene.render.resolution_x=1800;scene.render.resolution_y=1100;scene.render.resolution_percentage=100
deps=bpy.context.evaluated_depsgraph_get();camera=scene.camera;inv=camera.matrix_world.inverted()
corners=[inv@(part.matrix_world@Vector(c)) for part in parts for c in part.evaluated_get(deps).bound_box]
center=Vector(((min(p.x for p in corners)+max(p.x for p in corners))/2,(min(p.y for p in corners)+max(p.y for p in corners))/2,0));camera.location+=camera.matrix_world.to_3x3()@center
camera.data.ortho_scale=max(max(p.x for p in corners)-min(p.x for p in corners),(max(p.y for p in corners)-min(p.y for p in corners))*scene.render.resolution_x/scene.render.resolution_y)*1.12
bpy.ops.wm.save_as_mainfile(filepath=str(root/'moblins.blend'),compress=True)
scene.render.filepath=str(root/'moblins-studio.png');bpy.ops.render.render(write_still=True)
