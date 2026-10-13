"""Render the native reference and authored open/closed shutters in background Blender."""
import bpy,json
from pathlib import Path
from mathutils import Matrix,Vector
root=Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=str(root/'beamos.blend'));scene=bpy.context.scene
scene.render.resolution_x=1100;scene.render.resolution_y=900;scene.camera.data.ortho_scale=113
for frame,name in [(1,'shutters-closed'),(8,'shutters-open')]:
 scene.frame_set(frame);scene.render.filepath=str(root/(name+'.png'));bpy.ops.render.render(write_still=True)
for ob in list(scene.objects):
 if ob.type in ('MESH','ARMATURE'):bpy.data.objects.remove(ob,do_unlink=True)
rig=json.loads((root/'native_rig.json').read_text());pose=rig['animations']['gBeamosAnim'][7]
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
for limb in rig['rig']:
 if not limb['dl']:continue
 pts=[];faces=[]
 for tri in limb['triangles']:
  face=[]
  for x,y,z,joint in tri:
   face.append(len(pts));pts.append(tuple(axis@Matrix(pose[str(joint)])@Vector((x/100,y/100,z/100))))
  faces.append(tuple(face))
 mesh=bpy.data.meshes.new('Original native mesh');mesh.from_pydata(pts,[],faces);mesh.update();ob=bpy.data.objects.new(str(limb['limb']),mesh);scene.collection.objects.link(ob)
 mat=bpy.data.materials.new('Reference '+str(limb['limb']));mat.diffuse_color=(.45,.46,.4,1) if limb['limb']==10 else (.35,.38,.4,1);mesh.materials.append(mat)
scene.render.filepath=str(root/'native-reference.png');bpy.ops.render.render(write_still=True)
