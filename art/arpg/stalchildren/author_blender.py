"""Native Stalchild art. Run in Blender or through the local MCP.
Use an isolated background Blender process. Runtime keeps native AI/rigs.
"""
import bpy
import bmesh
assert bpy.app.background and not bpy.data.filepath
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector
ROOT=Path(__file__).resolve().parent
rig=json.loads((ROOT/'native_rig.json').read_text())
rest=rig['animations']['gStalchildWalkingAnim'][0]
connections=[]
scene=bpy.data.scenes.get('ARPG Stalchild Workshop') or bpy.data.scenes.new('ARPG Stalchild Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['bone','edge','socket','tooth','eye','crack']
PALETTES=[[(172,152,112),(209,191,153),(33,28,25),(223,205,163),(255,78,22),(79,66,48)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'Stalchild {sp} {name}') or bpy.data.materials.new(f'Stalchild {sp} {name}')
  mat.use_nodes=True;mat.diffuse_color=(*[v/255 for v in rgb],1)
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
  node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Roughness'].default_value=.32 if name=='eye' else .72
  if name=='eye':
   node.inputs['Emission Color'].default_value=(1,.07,.008,1);node.inputs['Emission Strength'].default_value=.8
  materials[sp,name]=mat
parts=[];variant=0;context={'limb':7}
def mesh(name,points,faces,material='bone',smooth=False,inks=None):
    limb=context['limb']
    # [TIME] | [INFO] | Stalchild.Author | Sculpt in the native resting world space, then bind each carved piece to its original rigid joint.
    points=[tuple(Vector(p)) for p in points]
    data=bpy.data.meshes.new(name);data.from_pydata(points,[],faces);data.update()
    for key in MATERIALS:data.materials.append(materials[variant,key])
    for i,p in enumerate(data.polygons):
        p.material_index=MATERIALS.index(inks[i] if inks else material);p.use_smooth=smooth
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    ob=bpy.data.objects.new(f'Stalchild.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='original Stalchild rigid rig and native joint connections' ;ob['native_units_per_authoring_unit']=100
    for f,pose in enumerate(rig['poses']):
        ob.matrix_world=Matrix.Translation((variant*70,0,0)) @ axis @ Matrix(pose[str(limb)])
        ob.keyframe_insert(data_path='location',frame=f+1)
        ob.keyframe_insert(data_path='scale',frame=f+1)
        ob.rotation_mode='QUATERNION';ob.keyframe_insert(data_path='rotation_quaternion',frame=f+1)
    parts.append(ob);return ob

def loft(name,rings,mat='bone',sections=12,striped=False):
    pts=[]
    for x,cy,ry,rz in rings:
        for i in range(sections):
            a=2*math.pi*i/sections;pts.append((x,cy+ry*math.cos(a),rz*math.sin(a)))
    faces=[tuple(reversed(range(sections)))];inks=[mat]
    for j in range(len(rings)-1):
        for i in range(sections):
            faces.append((j*sections+i,j*sections+(i+1)%sections,(j+1)*sections+(i+1)%sections,(j+1)*sections+i))
            inks.append(mat)
    faces.append(tuple((len(rings)-1)*sections+i for i in range(sections)));inks.append(mat)
    return mesh(name,pts,faces,mat,True,inks)

def tube(name,path,radii,mat='bone',sides=7):
    sides=min(sides,8)
    path=[Vector(p) for p in path];pts=[]
    for i,p in enumerate(path):
        tangent=(path[min(i+1,len(path)-1)]-path[max(0,i-1)]).normalized()
        u=tangent.cross(Vector((0,0,1)) if abs(tangent.z)<.9 else Vector((0,1,0))).normalized();v=tangent.cross(u)
        for j in range(sides):
            a=2*math.pi*j/sides;pts.append(tuple(p+radii[i]*(math.cos(a)*u+math.sin(a)*v)))
    faces=[tuple(reversed(range(sides)))]
    for i in range(len(path)-1):
        for j in range(sides):faces.append((i*sides+j,i*sides+(j+1)%sides,(i+1)*sides+(j+1)%sides,(i+1)*sides+j))
    faces.append(tuple((len(path)-1)*sides+j for j in range(sides)))
    return mesh(name,pts,faces,mat,mat!='accent')

def plate(name,points,depth,mat,normal=(0,0,1)):
    points=[Vector(p) for p in points];n=Vector(normal);c=sum(points,Vector())/len(points);k=len(points)
    pts=[tuple(p-n*depth*.25) for p in points]+[tuple(p) for p in points]+[tuple(p.lerp(c,.12)+n*depth) for p in points]
    faces=[tuple(reversed(range(k)))]
    for j in range(2):
        for i in range(k):faces.append((j*k+i,j*k+(i+1)%k,(j+1)*k+(i+1)%k,(j+1)*k+i))
    faces.append(tuple(2*k+i for i in range(k)))
    return mesh(name,pts,faces,mat)

def orb(name,center,radius,mat,slices=16,rings=10):
    # [TIME] | [INFO] | Stalchild.Asset | Closed, smoothly shaded eye/joint surface, not an overlapping body primitive.
    slices=min(slices,10 if mat=='eye' else 6);rings=min(rings,6 if mat=='eye' else 4)
    pts=[]
    for i in range(1,rings):
        theta=math.pi*i/rings
        for j in range(slices):
            phi=2*math.pi*j/slices
            pts.append((center[0]+radius[0]*math.cos(theta),center[1]+radius[1]*math.sin(theta)*math.cos(phi),center[2]+radius[2]*math.sin(theta)*math.sin(phi)))
    pts.extend([(center[0]+radius[0],center[1],center[2]),(center[0]-radius[0],center[1],center[2])]);a=len(pts)-2;b=a+1
    faces=[]
    for j in range(slices):
        faces.append((a,j,(j+1)%slices));faces.append((b,(rings-2)*slices+(j+1)%slices,(rings-2)*slices+j))
    for i in range(rings-2):
        for j in range(slices):faces.append((i*slices+j,(i+1)*slices+j,(i+1)*slices+(j+1)%slices,i*slices+(j+1)%slices))
    return mesh(name,pts,faces,mat,True)



# [TIME] | [INFO] | Stalchild.Author | Long bones have shafts, paired condyles and marrow channels; every piece remains on its original rigid joint.
def bone(name,length,r=.8,offset=(0,0,0)):
 x,y,z=offset
 path=[(x,y,z),(x+length*.13,y,z),(x+length*.45,y+.15,z),(x+length*.82,y-.1,z),(x+length,y,z)]
 tube(name,path,[r*1.4,r,r*.65,r,r*1.3],'bone',10)
 for end in (0,length):
  for sign in (-1,1):orb(name+' condyle',(x+end,y,z+sign*r*.55),(r*.75,r*.82,r*.78),'edge',10,6)
 tube(name+' subtle shaft ridge',[(x+length*.2,y+r*.66,z),(x+length*.5,y+r*.55,z),(x+length*.8,y+r*.65,z)],[r*.16]*3,'edge',5)
for limb,length,r in [(4,15.3,.9),(7,15.3,.9),(5,11,.7),(8,11,.7),(13,9.4,.94),(16,9.4,.94),(14,8.4,.64),(17,8.4,.64)]:
 context['limb']=limb
 if limb in (5,8,14,17):
  bone('radius or tibia',length,r,(0,.2,-.5));bone('ulna or fibula',length*.97,r*.58,(.1,-.6,.65))
 else:bone('femur' if limb in (4,7) else 'humerus',length,r)
# [TIME] | [INFO] | Stalchild.Asset | Separate metacarpals and three tapering phalanges retain the broad hooked native hands.
for limb,sign in [(15,1),(18,-1)]:
 context['limb']=limb
 orb('carpal cluster',(.3,2,sign*.45),(2,2,1),'bone',12,8)
 for finger,x in enumerate((-2.7,-.5,1.7,3.8)):
  tip=13.7-abs(finger-1.3)*.7
  path=[(x*.55,2,sign*.4),(x,5.2,sign*.7),(x*1.07,8.6,sign*1.2),(x*.94,11.3,sign*2.1),(x*.74,tip,sign*2.6)]
  tube('metacarpal and hooked phalanges',path,[.63,.76,.6,.47,.05],'bone',8)
  for p,r in [(path[1],.8),(path[2],.7),(path[3],.55)]:orb('finger knuckle',p,(r,r*.75,r*.7),'edge',8,6)
 tube('opposing thumb',[(2.1,2.2,sign*.3),(4.7,4,sign*.5),(5.4,6.2,sign*1.7),(4.5,7.8,sign*2.4)],[.85,.85,.63,.08],'bone',10)
# [TIME] | [INFO] | Stalchild.Asset | Feet: raised tarsal arch, separate metatarsals and curled toe tips.
for limb,sign in [(6,1),(9,-1)]:
 context['limb']=limb
 orb('heel',(-.5,-.2,0),(1.6,1.35,1.5),'bone',12,8)
 for k,z in enumerate((-2.8,-.85,1.1,2.6)):
  z*=sign;length=9.6-abs(k-1)*1.2
  path=[(-.1,0,z*.3),(3.3,-.5,z*.75),(6.8,-1.6,z),(length,-2.7,z),(length+.1,-3.3,z)]
  tube('arched metatarsal and toe',path,[.8,.8,.64,.5,.12],'bone',8)
  orb('toe knuckle',path[2],(.83,.65,.7),'edge',8,6)
# [TIME] | [INFO] | Stalchild.Asset | Pelvis is an open ring with flared iliac wings, not a solid block.
context['limb']=2
for sign in (-1,1):
 path=[(.2,-.5,sign*.7),(1.8,-3.2,sign*3.7),(5.3,-3.8,sign*6.8),(9.5,-2.7,sign*6.7),(12.2,-.2,sign*4.1),(10.2,2.1,sign*2.2),(7.3,2.5,sign*2.4),(6.3,.2,sign*3.2),(8,-1,sign*5.5)]
 tube('open pelvic rim',path,[1.1,1.15,1.2,1.1,1.05,.95,.95,.85,.7],'bone',10)
 plate('flared iliac crest',[(.3,-.5,sign*.5),(1.5,-2.8,sign*4.2),(4.2,-3.3,sign*7.1),(6.8,-2.7,sign*6.8),(5.7,-1.8,sign*3.4),(4.1,-.4,sign*1.6)],.65,'edge',normal=(0,-1,0))
tube('pubic bridge',[(10.4,1.6,-2.6),(12.3,1.1,0),(10.4,1.6,2.6)],[.9,.95,.9],'bone',10)
# [TIME] | [INFO] | Stalchild.Asset | The spine has individual vertebral bodies and transverse processes.
context['limb']=19
for j in range(5):
 y=.5+j*1.5
 orb('vertebral centrum',(0,y,0),(1.35,.57,1.18),'bone',10,6)
 tube('vertebral process',[(-1.3,y,.2),(0,y,-1.4),(1.3,y,.2)],[.35,.42,.35],'edge',6)
# [TIME] | [INFO] | Stalchild.Asset | Build ribs in a compact anatomical frame aligned to the chest's native hunch.
context['limb']=10
# [TIME] | [INFO] | Stalchild.Asset | Local chest x points up/forward; y points down/forward; z spans the shoulders.
def chest(side,up,front):return (2.8+.56*up+.83*front,-.6-.83*up+.56*front,side)
for j in range(5):
 up=4.3-j*2.1;width=6.3-(max(0,j-1)*.55);depth=4.8-j*.25
 for sign in (-1,1):
  path=[chest(sign*.65,up,-2),chest(sign*width*.72,up-.3,-1.3),chest(sign*width,up-.8,1),chest(sign*width*.76,up-1.4,depth),chest(sign*.75,up-1.6,depth+.4)]
  tube('separate curved rib',path,[.56,.68,.66,.53,.4],'bone',10)
for sign in (-1,1):
 tube('collarbone',[chest(sign*.7,5,2.3),chest(sign*3.1,5.2,1.7),chest(sign*6.3,5,.7)],[.75,.85,.9],'edge',10)
 plate('scapula',[chest(sign*1.4,4,-2.4),chest(sign*5.8,4,-1.5),chest(sign*5.5,.2,-2),chest(sign*2,-2,-2.2)],.35,'bone',normal=(-.83,-.56,0))
tube('sternum',[chest(0,4,5.1),chest(0,1,5),chest(0,-3,4.4),chest(0,-5,3.5)],[.8,.7,.65,.15],'edge',10)
# [TIME] | [INFO] | Stalchild.Asset | Skull: a domed vault over deep socket openings, angular zygomatic arches and a projecting nasal cavity.
context['limb']=11
for x in (-2.2,-.4,1.4):
 orb('cervical vertebra',(x,-.2,0),(.7,1.55,1.65),'bone',8,6)
loft('cranial vault',[(0,1,3,3),(5,1.8,4.8,6.2),(8.7,2.5,7.5,8),(12.9,3.2,10.8,8.3),(15.7,2.8,4.4,6.3),(17,2.4,1.2,2.1)],sections=20)
for sign in (-1,1):
 # [TIME] | [INFO] | Stalchild.Asset | The orbit is an actual layered rim around a recessed bowl, rather than a painted eyeball.
 outline=[(5.2,13.4,sign*2),(5,13.1,sign*6.7),(7.1,13.9,sign*8),(10.7,13.9,sign*7.6),(11.6,14.5,sign*3.6),(9.8,15,sign*1.9)]
 tube('sculpted orbital rim',outline+[outline[0]],[.78,.95,1.05,1.25,1.25,.85,.78],'edge',10)
 center=(8.2,11.45,sign*4.9)
 pts=outline+[center];faces=[(i,(i+1)%len(outline),len(outline)) for i in range(len(outline))]
 mesh('deep eye socket',pts,faces,'socket',False)
 orb('ember in eye socket',(8.1,13.3,sign*4.8),(.8,.48,.82),'eye',12,8)
 plate('temporal wall',[(5,5.5,sign*7.1),(11.5,6,sign*7.8),(11.1,13.6,sign*7.5),(7,13.4,sign*8),(4.5,9,sign*7.4)],.4,'bone',normal=(0,0,sign))
 front=[(9.8,15,sign*1.9),(11.6,14.5,sign*3.6),(10.7,13.9,sign*7.6)]
 back=[(13,13.73,sign*1.81),(13,12.41,sign*4.34),(13,8.57,sign*7.2)]
 mesh('continuous frontal bone',front+back,[(0,1,4,3),(1,2,5,4)],'bone',True)
 tube('zygomatic arch',[(6,5,sign*7.4),(4.2,8.5,sign*8),(3.6,12.6,sign*6.8),(4,16,sign*4.3)],[1.1,1.1,.9,1.1],'bone',10)
 plate('maxillary cheek',[(5.3,13,sign*2.3),(5.5,15.8,sign*4.7),(3.5,18,sign*5.6),(1.2,17.6,sign*4.7),(.7,16.4,sign*1.1)],.5,'bone',normal=(0,1,0))
mesh('nasal bridge and interorbital forehead',[(13,13.95,-1.8),(13,13.95,1.8),(9.8,15,1.9),(6.9,16.3,0),(9.8,15,-1.9)],[(0,1,2,4),(4,2,3)],'bone',True)
# [TIME] | [INFO] | Stalchild.Asset | Triangular nasal aperture, surrounded by protruding upper muzzle.
mesh('nasal opening',[(6.9,16.15,0),(3.4,17.8,-1.55),(3.4,17.8,1.55),(4.9,15.5,0)],[(0,1,3),(1,2,3),(2,0,3)],'socket')
tube('nasal rim',[(3.4,17.8,-1.7),(6.9,16.3,0),(3.4,17.8,1.7)],[.55,.58,.55],'edge',8)
loft('upper dental arch',[(.8,15,2.2,4.6),(2,15.4,2.5,5.3),(3.1,15.2,2.2,5.4)],sections=16)
for z in (-4.4,-2.9,-1.45,0,1.45,2.9,4.4):
 y=17.55-abs(z)*.13
 tube('upper tooth',[(1.6,y,z),(.1,y+.15,z),(-.8,y,z)],[.68,.56,.28],'tooth',8)
# [TIME] | [INFO] | Stalchild.Asset | Subtle sutures on the vault, kept thin enough to read as bone cracks.
tube('sagittal suture',[(15.8,5.8,0),(15.3,6.6,.4),(14.5,7.5,-.25),(13.8,8,.2)],[.12]*4,'crack',5)
# [TIME] | [INFO] | Stalchild.Asset | Author the jaw in head rest space, then rebind it into the distinct animated mandible joint.
head_to_jaw=Matrix(rest['12']).inverted()@Matrix(rest['11']);start=len(parts)
context['limb']=12
path=[(5,5.2,-5.7),(1.5,9,-5.8),(-2.8,14,-4.8),(-3.3,16.2,-2.6),(-3.5,16.8,0),(-3.3,16.2,2.6),(-2.8,14,4.8),(1.5,9,5.8),(5,5.2,5.7)]
tube('continuous articulated mandible',path,[1.05,1,.98,1.1,1.15,1.1,.98,1,1.05],'bone',12)
for z in (-4,-2.6,-1.3,0,1.3,2.6,4):
 y=16.6-abs(z)*.34
 tube('lower tooth',[(-3,y,z),(-2,y+.2,z),(-1.4,y+.15,z)],[.54,.48,.3],'tooth',8)
for ob in parts[start:]:
 for v in ob.data.vertices:v.co=head_to_jaw@v.co
scene.frame_start=1;scene.frame_end=rig['frames'];scene.render.fps=20;scene.frame_set(67)
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.export_scene.gltf(filepath=str(ROOT/'stalchildren.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
(ROOT/'connections.json').write_text('[]\n')
print('Exported',len(parts),'skeletal pieces on 17 native joints.')
