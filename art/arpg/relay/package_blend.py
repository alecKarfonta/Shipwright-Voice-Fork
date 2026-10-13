"""Package only the authored GLB in a standalone editable Blender project."""
import bpy,math
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parent
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=str(root/'relay.glb'))
scene=bpy.context.scene;scene.name='ARPG Spirit Relay'
converted=set()
for ob in scene.objects:
    if ob.type=='MESH':
        for mat in ob.data.materials:
            if mat.name in converted:continue
            converted.add(mat.name)
            shader=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
            rgb=shader.inputs['Base Color'].default_value
            # Export palettes are authored as sRGB native material colors.
            shader.inputs['Base Color'].default_value=tuple(v**2.2 for v in rgb[:3])+(1,)
cam=bpy.data.cameras.new('Atelier Camera');ob=bpy.data.objects.new(cam.name,cam);scene.collection.objects.link(ob)
ob.location=(90,-140,89);ob.rotation_euler=(Vector((7,0,27))-ob.location).to_track_quat('-Z','Y').to_euler();cam.type='ORTHO';cam.ortho_scale=75;scene.camera=ob
for pos,energy,size in [((20,-60,100),18000,50),((-40,20,65),12000,50)]:
    data=bpy.data.lights.new('Atelier Softbox','AREA');data.energy=energy;data.size=size
    ob=bpy.data.objects.new(data.name,data);scene.collection.objects.link(ob);ob.location=pos;ob.rotation_euler=(Vector((0,0,10))-ob.location).to_track_quat('-Z','Y').to_euler()
world=bpy.data.worlds.new('Atelier Night');world.use_nodes=True
bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs[0].default_value=(.03,.042,.055,1);bg.inputs[1].default_value=1.1;scene.world=world
scene.view_settings.look='AgX - Medium High Contrast';scene.view_settings.exposure=1.2
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=32;scene.render.resolution_x=1000;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA'
bpy.ops.object.select_all(action='DESELECT')
bpy.ops.wm.save_as_mainfile(filepath=str(root/'relay.blend'),compress=True)
scene.render.image_settings.file_format='PNG';scene.render.filepath=str(root/'relay-preview.png')
bpy.ops.render.render(write_still=True)
