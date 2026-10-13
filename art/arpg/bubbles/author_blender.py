"""Native Bubble art. Run in Blender or through the local MCP.
Only the ARPG Bubble Workshop scene is replaced. Runtime keeps native AI/rigs.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector
ROOT=Path(__file__).resolve().parent
rig=json.loads((ROOT/'native_rig.json').read_text())
scene=bpy.data.scenes.get('ARPG Bubble Workshop') or bpy.data.scenes.new('ARPG Bubble Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['bone','edge','cavity','membrane','vein','eye','tooth','suture']
PALETTES=[[(219,215,191),(247,235,199),(27,31,39),(74,88,133),(153,170,203),(139,215,255),(235,224,183),(119,118,101)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'Bubble {sp} {name}') or bpy.data.materials.new(f'Bubble {sp} {name}')
  mat.use_nodes=True;mat.diffuse_color=(*[v/255 for v in rgb],1)
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
  node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Roughness'].default_value=.24 if name in ('eye','iris','pupil') else .48 if name in ('mantle','ridge') else .78
  materials[sp,name]=mat
parts=[];variant=0;context={'limb':4}
face_basis=Matrix(rig['poses'][18]['15']).to_3x3().transposed() @ Matrix(((0,0,1),(0,-1,0),(1,0,0)))
def mesh(name,points,faces,material='mantle',smooth=False,inks=None):
    limb=context['limb']
    if limb==15 and name!='wing root condyle':points=[tuple(face_basis @ Vector((p[0]*.84,p[1],p[2]))) for p in points]
    data=bpy.data.meshes.new(name);data.from_pydata(points,[],faces);data.update()
    for key in MATERIALS:data.materials.append(materials[variant,key])
    for i,p in enumerate(data.polygons):
        p.material_index=MATERIALS.index(inks[i] if inks else material);p.use_smooth=smooth
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    ob=bpy.data.objects.new(f'Bubble.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='native 15-limb Bubble rig, separate mandible and two two-part wings' ;ob['native_units_per_authoring_unit']=100
    for f,pose in enumerate(rig['poses']):
        ob.matrix_world=axis @ Matrix(pose[str(limb)])
        ob.keyframe_insert(data_path='location',frame=f+1)
        ob.keyframe_insert(data_path='scale',frame=f+1)
        ob.rotation_mode='QUATERNION';ob.keyframe_insert(data_path='rotation_quaternion',frame=f+1)
    parts.append(ob);return ob

def loft(name,rings,mat='skin',sections=12,striped=False):
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

def tube(name,path,radii,mat='skin',sides=7):
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

def plate(name,points,depth,mat,normal=(0,-1,0)):
    points=[Vector(p) for p in points];n=Vector(normal);c=sum(points,Vector())/len(points);k=len(points)
    pts=[tuple(p-n*depth*.25) for p in points]+[tuple(p) for p in points]+[tuple(p.lerp(c,.12)+n*depth) for p in points]
    faces=[tuple(reversed(range(k)))]
    for j in range(2):
        for i in range(k):faces.append((j*k+i,j*k+(i+1)%k,(j+1)*k+(i+1)%k,(j+1)*k+i))
    faces.append(tuple(2*k+i for i in range(k)))
    return mesh(name,pts,faces,mat)

def orb(name,center,radius,mat,slices=16,rings=10):
    # Closed, smoothly shaded eye/joint surface, not an overlapping body primitive.
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



context['limb']=15
# [TIME] | [INFO] | Bubble.Author | A cranial vault with an open front supports orbital rims, cheek arches and a long maxilla.
ps=[];n=28
rings=[(-10.7,-4.2,.4,.45),(-9,-4.2,4.1,4.3),(-6,-3.8,9.7,6.5),(-2.5,-3.8,8.0,7.0),(.8,-3.7,6.4,6.3),(3.4,-3.1,4.2,4.8),(5.5,-2.9,1.4,2.8)]
for y,cx,rx,rz in rings:
 for j in range(n):
  a=j*math.tau/n;ps.append((cx+rx*math.cos(a),y,rz*math.sin(a)))
fs=[]
for k in range(len(rings)-1):
 for j in range(n):
  if k>=3 and math.cos((j+.5)*math.tau/n)>.45:continue
  fs.append((k*n+j,k*n+(j+1)%n,(k+1)*n+(j+1)%n,(k+1)*n+j))
fs.append(tuple(reversed(range(n))));fs.append(tuple((len(rings)-1)*n+j for j in range(n)))
mesh('open cranial vault',ps,fs,'bone',True)
for sign in (-1,1):
    outline=[(-5.7,1.45),(-5.3,3.7),(-3.6,5.7),(-.7,5.4),(1.0,3.8),(.55,1.75),(-2.1,1.05)]
    center=Vector((-2.3,3.2));ps=[];n=len(outline)
    for scale,x in [(1.21,5.1),(1.03,7.8),(.78,8.15),(.67,6.85)]:
      for y,z in outline:ps.append((x,center.x+(y-center.x)*scale,sign*(center.y+(z-center.y)*scale)))
    fs=[];inks=[]
    for k in range(3):
      for j in range(n):fs.append((k*n+j,k*n+(j+1)%n,(k+1)*n+(j+1)%n,(k+1)*n+j));inks.append('cavity' if k==2 else 'edge' if k==1 else 'bone')
    fs.append(tuple(3*n+j for j in range(n)));inks.append('cavity')
    mesh('deep splayed orbital socket',ps,fs,'bone',True,inks)
    plate('sunken spectral pupil',[(7.08,-3.6,sign*3.2),(7.15,-2.55,sign*4.1),(7.1,-1.0,sign*3.2),(7.15,-2.55,sign*2.45)],.14,'eye',(1,0,0))
    tube('heavy curved brow',[(4.9,-5.95,sign*.8),(6.6,-6.25,sign*2),(7.5,-5.9,sign*4.4),(5.6,-4.3,sign*6.1)],[.58,.83,.86,.42],'bone',12)
    tube('zygomatic cheek arch',[(5.7,.85,sign*5.45),(5.2,2.7,sign*5.8),(1.5,3.25,sign*6),(-2.1,1.3,sign*6.15)],[.56,.74,.65,.36],'bone',12)
    plate('maxillary cheek',[(8,.35,sign*1.5),(8.8,1.9,sign*3.45),(12.8,3.7,sign*3.0),(13.5,4.9,sign*.8),(10.2,3.2,sign*.55)],.38,'bone',(1,0,0))
    tube('nasal bridge side',[(7,-3.8,sign*.7),(8.6,-1.0,sign*.65),(11.1,1.9,sign*1.0),(12.5,2.9,sign*1.2)],[.4,.38,.28,.12],'edge',9)
    tube('temporal ridge',[(-9.5,-3,sign*3.2),(-7,-6,sign*4.6),(-4,-6.8,sign*5.4)],[.25,.3,.15],'edge',8)
    tube('wing root condyle',[(-1.0,-1.0,sign*5.6),(-.96,-1.15,sign*6.0)],[.7,.6],'bone',12)
plate('nasal aperture',[(8.9,-.9,0),(12.1,2.8,-.92),(12.4,3.2,0),(12.1,2.8,.92)],.06,'cavity',(1,0,0))
arch=[(11.3,3.3,-3.8),(13.1,4.0,-2.5),(14.0,4.3,0),(13.1,4.0,2.5),(11.3,3.3,3.8)]
tube('upper dental arch',arch,[.62]*5,'bone',12)
for i in range(-4,5):
 z=i*.79;x=13.75-.13*z*z;length=2.35 if abs(i)==3 else 1.5
 tube('tapered upper fang',[(x,4.35,z),(x+.18,5.1,z),(x-.18,4.7+length,z*.94)],[.37,.37,.075],'tooth',9)
tube('cranial sagittal suture',[(-10,-5.7,0),(-7,-9.2,.13),(-4.2,-10.64,0),(-.9,-9.4,-.13),(2.4,-7.4,0)],[.035]*5,'suture',5)
context['limb']=4
# [TIME] | [INFO] | Bubble.Author | Separate mandibular rami and lower teeth retain the original snapping jaw transform.
for sign in (-1,1):
 tube('continuous lower jaw',[(-2.8,.05,sign*3.8),(1,-3.0,sign*4.5),(5.0,-7.7,sign*4.1),(8.5,-9.65,sign*2.8),(9.8,-9.75,0)],[.55,.8,.79,.65,.53],'bone',12)
 plate('ascending mandibular ramus',[(-2.8,.1,sign*3.8),(-1,-2.6,sign*4.5),(3,-5.5,sign*4.35),(4,-3.2,sign*4.1),(2,-.35,sign*4.6)],.22,'bone',(0,0,sign))
 for i in range(4):
  z=sign*(.45+i*.78);x=8.3-.16*z*z;y=-9.25+.1*z*z
  tube('lower biting tooth',[(x,y,z),(x-.75,y-.04,z),(x-1.35,y+.13,z*.96)],[.35,.32,.055],'tooth',8)
# [TIME] | [INFO] | Bubble.Author | Membrane grids share the native hinge cross-section; scallops and ribs follow the original two-part wings.
def wing(name,inner):
 steps=12;across=8;ps=[]
 profile=[(0,-4.7),(2.1,-2.65),(4.2,-4.4),(5.8,-1.95),(7.81,-4.37),(9.25,.1),(10.66,4.1)]
 for i in range(steps+1):
  t=i/steps;x=(-1.2+6.8*t) if inner else 10.66*t
  hi=5.6*t if inner else 5.6-1.5*t
  if inner:lo=-4.7*t
  else:
   for (ax,ay),(bx,by) in zip(profile,profile[1:]):
    if ax<=x<=bx:lo=ay+(by-ay)*(x-ax)/(bx-ax);break
  for j in range(across+1):
   v=j/across;y=lo+(hi-lo)*v;z=.36*math.sin(math.pi*t)*math.sin(math.pi*v)
   ps.append((x,y,z))
 fs=[]
 for i in range(steps):
  for j in range(across):
   a=i*(across+1)+j;fs.append((a,a+1,a+across+2,a+across+1))
 mesh(name,ps,fs,'membrane',True)
 if inner:
  tube('leading wing humerus',[(-1.2,0,0),(2.5,3.2,.08),(5.6,5.6,0)],[.38,.32,.24],'bone',10)
  tube('lower wing tendon',[(-1.2,0,0),(2.5,-2.65,.05),(5.6,-4.7,0)],[.23,.18,.13],'vein',8)
  tube('native hinge spar',[(5.6,-4.7,0),(5.6,0,0),(5.6,5.6,0)],[.14,.22,.24],'bone',8)
 else:
  tube('leading wing finger',[(0,5.6,0),(5.3,4.86,.06),(10.66,4.1,0)],[.24,.2,.035],'bone',10)
  for x,y in profile[1:-1]:
   if y< -3:
    tube('membrane finger rib',[(0,4.8,0),(x*.5,(y+4.8)/2,.2),(x,y,0)],[.17,.11,.025],'vein',7)
  tube('scalloped trailing edge',[(x,y,0) for x,y in profile],[.09]*len(profile),'vein',7)
for proximal,distal in [(9,8),(14,13)]:
 context['limb']=proximal;wing('pliant proximal membrane',True)
 context['limb']=distal;wing('scalloped distal membrane',False)
scene.frame_start=1;scene.frame_end=len(rig['poses']);scene.render.fps=20;scene.frame_set(19)
scene.world=bpy.data.worlds.get('Bubble Studio') or bpy.data.worlds.new('Bubble Studio');scene.world.use_nodes=True
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.export_scene.gltf(filepath=str(ROOT/'bubbles.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
print('Exported',len(parts),'native Bubble limb pieces.')
