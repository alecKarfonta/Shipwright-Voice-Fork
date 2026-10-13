"""Native Redead art. Run in Blender or through the local MCP.
Only the ARPG Redead Workshop scene is replaced. Runtime keeps native AI/rigs.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector
ROOT=Path(__file__).resolve().parent
rig=json.loads((ROOT/'native_redead.json').read_text())
rest=rig['animations']['gGibdoRedeadIdleAnim'][0]
connections=[]
scene=bpy.data.scenes.get('ARPG Redead Workshop') or bpy.data.scenes.new('ARPG Redead Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['skin','ridge','mask','cavity','wrap','edge','eye','nail','cloth']
PALETTES=[[(109,78,60),(155,111,74),(139,91,50),(27,25,22),(161,133,93),(198,168,118),(216,139,65),(86,67,45),(72,57,43)],[(115,109,82),(145,134,98),(156,146,110),(28,31,26),(178,166,121),(211,196,149),(231,189,71),(102,91,59),(107,101,72)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'Redead {sp} {name}') or bpy.data.materials.new(f'Redead {sp} {name}')
  mat.use_nodes=True;mat.diffuse_color=(*[v/255 for v in rgb],1)
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
  node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Roughness'].default_value=.24 if name in ('eye','iris','pupil') else .48 if name in ('mantle','ridge') else .78
  materials[sp,name]=mat
parts=[];variant=0;context={'limb':7}
def mesh(name,points,faces,material='skin',smooth=False,inks=None):
    limb=context['limb']
    data=bpy.data.meshes.new(name);data.from_pydata(points,[],faces);data.update()
    for key in MATERIALS:data.materials.append(materials[variant,key])
    for i,p in enumerate(data.polygons):
        p.material_index=MATERIALS.index(inks[i] if inks else material);p.use_smooth=smooth
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    ob=bpy.data.objects.new(f'Redead.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='original ReDead/Gibdo flex rig and native mixed-matrix connections' ;ob['native_units_per_authoring_unit']=100
    for f,pose in enumerate(rig['poses']):
        ob.matrix_world=Matrix.Translation((variant*42,0,0)) @ axis @ Matrix(pose[str(limb)])
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



def ring(x,cy,ry,rz,n=12):return [(x,cy+ry*math.cos(j*math.tau/n),rz*math.sin(j*math.tau/n)) for j in range(n)]
def bridge(name,parent,parent_points,child_points):
    limb=context['limb'];change=Matrix(rest[str(limb)]).inverted() @ Matrix(rest[str(parent)])
    converted=[tuple(change@Vector(p)) for p in parent_points];n=len(converted)
    ob=mesh(name,converted+child_points,[(j,(j+1)%n,n+(j+1)%n,n+j) for j in range(n)],'wrap' if variant else 'skin',True)
    ob['arpg_parent_joint']=parent;ob['arpg_parent_ring_points']=converted;ob['arpg_child_ring_points']=child_points
    connections.append({'variant':variant,'child':limb,'parent':parent,'parent_points':parent_points,'child_points':child_points})
    return ob

def wraps(rings,turns=6,mat='wrap',n=14):
    # [TIME] | [INFO] | Redead.Author | Continuous helical cloth strips follow the tapered limb; edges are separate lifted seams.
    def sample(x):
      for a,b in zip(rings,rings[1:]):
       if a[0]<=x<=b[0]:
        t=(x-a[0])/(b[0]-a[0]);return [a[k]*(1-t)+b[k]*t for k in range(1,4)]
      return rings[-1][1:]
    x0=rings[0][0]+.3;x1=rings[-1][0]-.3;pitch=(x1-x0)/turns;ps=[];path=[]
    for j in range(turns*n+1):
      x=x0+(x1-x0)*j/(turns*n);a=j*math.tau/n;cy,ry,rz=sample(x)
      for dx in (-pitch*.37,pitch*.37):ps.append((max(x0,min(x1,x+dx)),cy+(ry+.06)*math.cos(a),(rz+.06)*math.sin(a)))
      path.append((x,cy+(ry+.13)*math.cos(a),(rz+.13)*math.sin(a)))
    mesh('overlapping spiral linen',ps,[(j*2,j*2+1,j*2+3,j*2+2) for j in range(turns*n)],mat)
    tube('lifted cloth seam',path,[.045]*len(path),'edge',4)

def head():
 global mesh
 old=mesh;basis=Matrix(rest['24']).to_3x3().transposed() @ Matrix(((0,0,1),(0,-1,0),(1,0,0)))
 def mesh(name,points,faces,material='skin',smooth=False,inks=None):return old(name,[tuple(basis@Vector((p[0]+4,p[1],p[2]))) for p in points],faces,material,smooth,inks)
 # Canonical face coordinates are forward/down/side, with the original hunched head orientation.
 orb('tapered cranium',(.2,-2.3,0),(4.5,6.2,4.7),'skin',18,12)
 if variant==0:
  # A sculpted mask has a deliberately open eye and mouth lattice rather than a painted flat icon.
  outline=[(-7.7,0),(-6.9,3.6),(-3.8,5.4),(.1,5.8),(4.6,3.6),(6.0,0),(4.6,-3.6),(.1,-5.8),(-3.8,-5.4),(-6.9,-3.6)]
  tube('carved mask perimeter',[(5.7,y,z) for y,z in outline+[outline[0]]],[.52]*11,'mask',10)
  for sign in (-1,1):
   eye=[(-5.8,.65),(-5.3,2.5),(-3.7,4.1),(-2.0,2.9),(-2.25,.9)]
   ps=[]
   for x,scale in [(5.0,1.2),(6.35,1),(5.15,.64)]:
    for y,z in eye:ps.append((x,-3.8+(y+3.8)*scale,sign*(2.1+(z-2.1)*scale)))
   fs=[];inks=[];n=len(eye)
   for k in range(2):
    for j in range(n):fs.append((k*n+j,k*n+(j+1)%n,(k+1)*n+(j+1)%n,(k+1)*n+j));inks.append('mask' if k==0 else 'cavity')
   fs.append(tuple(2*n+j for j in range(n)));inks.append('cavity');mesh('recessed carved orbital hollow',ps,fs,'mask',False,inks)
   plate('faint eye ember',[(5.25,-4.45,sign*2.0),(5.35,-3.8,sign*2.45),(5.25,-3.1,sign*2.0),(5.35,-3.8,sign*1.6)],.05,'eye',(1,0,0))
   plate('long cheek plane',[(5.7,-1.9,sign*4.8),(6.15,-1.1,sign*2.9),(5.9,4.8,sign*2.0),(5.0,4.5,sign*3.2)],.25,'mask',(1,0,0))
   tube('brow ridge',[(5.4,-6.8,0),(6.4,-6.2,sign*2.3),(5.9,-4.5,sign*4.8)],[.48,.75,.43],'ridge',9)
  plate('forehead crest',[(5.4,-7.5,0),(6.2,-5.9,-1.1),(6.8,-2.2,0),(6.2,-5.9,1.1)],.23,'mask',(1,0,0))
  mouth=[(-1.3,0),(-.7,1.5),(2.5,1.7),(5.1,.9),(5.65,0),(5.1,-.9),(2.5,-1.7),(-.7,-1.5)]
  tube('elongated scream rim',[(6.3,y,z) for y,z in mouth+[mouth[0]]],[.37]*9,'mask',9)
  mesh('deep open scream',[(5.95,y,z) for y,z in mouth],[tuple(range(8))],'cavity')
  for z in (-.9,0,.9):tube('irregular mask tooth',[(6.3,-.8,z),(6.35,.3,z*.95)],[.25,.08],'edge',7)
  for sign in (-1,1):
   tube('mask binding cord',[(5.0,-5.7,sign*4.5),(1,-5.3,sign*4.8),(-3,-4.0,sign*3.4)],[.17,.17,.15],'cloth',7)
  for j in range(6):tube('rear sinew ridge',[(-3,-6+j*.8,-2.9),(-4.1,-4+j*.9,0),(-3,-6+j*.8,2.9)],[.1,.17,.1],'ridge',6)
 else:
  # Broad bandage strips form the face around a recessed opening; they do not cover the eyes.
  for y,width in [(-7.8,2.7),(-6.8,3.8),(-5.7,4.5),(-1.2,4.8),(.1,4.5),(1.4,4.0),(2.7,3.4),(4.0,2.3)]:
   pts=[(3.8,y-.45,-width),(5.05,y-.15,0),(3.8,y+.1,width),(3.8,y+1.1,width),(5.05,y+.85,0),(3.8,y+.55,-width)]
   mesh('layered facial wrapping',pts,[(0,1,4,5),(1,2,3,4)],'wrap');tube('ragged facial seam',pts[:3],[.09]*3,'edge',6)
  for sign in (-1,1):
   orb('deep eye slit',(4.45,-3.65,sign*2.15),(1.2,1.3,1.5),'cavity',12,8)
   plate('gibdo amber slit',[(5.55,-4.3,sign*2.15),(5.65,-3.65,sign*2.6),(5.55,-3.0,sign*2.15),(5.65,-3.65,sign*1.7)],.04,'eye',(1,0,0))
   tube('temple linen fold',[(2.2,-6.9,sign*4.1),(.7,-2.4,sign*4.8),(1.2,2,sign*3.8)],[.65,.7,.55],'wrap',9)
  for j in range(5):tube('rear linen strip',[(-2.5,-7+j*2,-2.5),(-4.3,-5.2+j*2,0),(-2.5,-7+j*2,2.5)],[.52]*3,'wrap',8)
 mesh=old

for variant in (0,1):
 # Pelvis ring is oriented around native world-up rather than the limb-X convention.
 context['limb']=25
 old=mesh
 def mesh(name,points,faces,material='skin',smooth=False,inks=None):return old(name,[(p[2],p[0],p[1]) for p in points],faces,material,smooth,inks)
 pelvis=[(-3.1,0,3.7,5.7),(-.5,.4,4.4,6.5),(4,0,3.4,5.4)]
 loft('continuous pelvis',pelvis,'skin',12)
 if variant:wraps(pelvis,3)
 else:
  for sign in (-1,1):plate('hip crest',[(-2,1,sign*5),(-.2,3.9,sign*6),(2.1,3,sign*5.8),(3,1,sign*4.7)],.4,'ridge',(0,0,sign))
 mesh=old
 context['limb']=14
 torso=[(-5,0,3.3,5.4),(0,.2,4.1,6.4),(4.5,.4,4.7,8.0),(9.4,.6,4.9,9.0),(12.1,.3,4.1,8.5),(14.0,0,2.2,3.1)]
 if variant:torso=[(x,cy,ry*1.12,rz*.96) for x,cy,ry,rz in torso]
 loft('tapered hunched torso',torso,'skin',12)
 parent=[(p[2],p[0],p[1]) for p in ring(4,0,3.4,5.4)]
 bridge('flexible waist connection',25,parent,ring(*torso[0]))
 if variant:
  wraps(torso,9)
  for sign in (-1,1):tube('crossed burial sash',[(-3,3.3,sign*3.8),(4,5.6,0),(11,3.8,-sign*6.8)],[.48,.55,.55],'edge',8)
 else:
  for x,w,y in [(2,6.5,4.4),(4.7,7.5,5.0),(7.3,8.2,5.45),(9.8,8.1,4.9)]:
   for sign in (-1,1):tube('subcutaneous rib ridge',[(x,y*.9,0),(x-.3,y+.2,sign*w*.45),(x-1.5,y*.8,sign*w*.87)],[.32,.43,.16],'ridge',8)
  tube('raised sternum',[(.5,4.6,0),(5,5.6,0),(10,5.25,0)],[.32,.43,.4],'ridge',9)
  for x in [0,2.5,5,7.5,10]:plate('vertebral ridge',[(x-1,-4.5,0),(x,-5.25,-1),(x+1,-4.6,0),(x,-5.25,1)],.2,'ridge',(0,-1,0))
 loft('crooked neck',[(12.8,0,1.9,2.3),(15.0,-.4,1.7,1.9),(17,-.7,1.65,1.75)],'skin',12)
 for thigh,shin,foot in [(3,4,6),(8,9,11)]:
  context['limb']=thigh
  rings=[(.7,0,3.1,3.75),(5,.4,3.5,3.5),(11,.3,2.8,2.8),(17.8,0,2.05,2.15)]
  if variant:rings=[(x,cy,ry*1.2,rz*1.2) for x,cy,ry,rz in rings]
  loft('lean thigh',rings,'skin',12)
  if variant:wraps(rings,7)
  else:tube('thigh tendon',[(1,-2.5,0),(8,-3.0,.3),(16,-1.9,0)],[.22,.35,.16],'ridge',7)
  end=ring(*rings[-1]);context['limb']=shin
  rings=[(.7,0,2.05*(1.2 if variant else 1),2.15*(1.2 if variant else 1)),(5,.2,2.4,2.2),(10,.8,1.9,1.6),(15.3,1.8,1.2,1.45)]
  loft('tapered shin',rings,'skin',12);bridge('deforming knee',thigh,end,ring(*rings[0]))
  if variant:wraps(rings,6)
  else:tube('tibial edge',[(1,-1.9,0),(8,-1.4,0),(15,.6,0)],[.23,.32,.15],'ridge',7)
  context['limb']=foot
  rings=[(-2.3,1.1,1.1,1.5),(0,1.8,1.8,2.3),(6,2.9,1.1,2.4),(10.5,3.25,.72,2.0)]
  loft('long planted foot',rings,'skin',14)
  if variant:wraps(rings,5)
  for j in range(4):
   z=-1.8+j*1.2;tube('distinct toe',[(8.5,3,z),(11.4,3.45,z),(13.2-abs(z)*.2,3.5,z)],[.55,.52,.32],'wrap' if variant else 'skin',8)
   if not variant:plate('toe nail',[(11.5,2.95,z-.3),(13,3.15,z-.25),(13.15,3.25,z+.25),(11.5,2.95,z+.3)],.08,'nail',(0,-1,0))
 for upper,fore,hand,sign in [(16,17,18,1),(20,21,22,-1)]:
  context['limb']=upper
  rings=[(-2.2,0,.7,.65),(-1.1,0,2.15,2.0),(.7,0,2.65,2.45),(3.5,.2,2.8,2.6),(7.5,0,2.2,2),(11.9,0,1.5,1.65)]
  if variant:rings=[(x,cy,ry*1.17,rz*1.17) for x,cy,ry,rz in rings]
  loft('upper arm',rings,'skin',12)
  parent=[(10.4+math.cos(j*math.tau/12)*1.7,.3+math.sin(j*math.tau/12)*2.2,sign*7.1) for j in range(12)]
  # Reverse one shoulder ring so its seam follows the arm's native handedness.
  if sign<0:parent=list(reversed(parent))
  bridge('attached shoulder',14,parent,ring(*rings[2]))
  if variant:wraps(rings,5)
  end=ring(*rings[-1]);context['limb']=fore
  rings=[(.6,0,1.5*(1.17 if variant else 1),1.65*(1.17 if variant else 1)),(4,.2,1.9,1.8),(8,.15,1.45,1.35),(12.5,0,.9,1.1)]
  loft('forearm',rings,'skin',12);bridge('deforming elbow',upper,end,ring(*rings[0]))
  if variant:wraps(rings,5)
  else:
   for z in (-.6,.6):tube('forearm tendons',[(2,-1.4,z),(7,-1.3,z),(12,-.8,z*.7)],[.12,.15,.07],'ridge',6)
  end=ring(*rings[-1]);context['limb']=hand
  rings=[(.4,0,.9,1.1),(3.2,0,1.0,2.5),(6.1,-.1,.85,2.65)]
  loft('broad grasping palm',rings,'skin',12);bridge('deforming wrist',fore,end,ring(*rings[0]))
  if variant:wraps(rings,3)
  for j in range(4):
   z=-1.9+j*1.25;length=4.4 if j in (1,2) else 3.4
   tube('separate crooked finger',[(5.5,-.1,z),(8,-.15,z*1.12),(6.8+length,.65,z*1.08),(6.6+length,1.65,z)],[.53,.52,.39,.25],'wrap' if variant else 'skin',8)
   if not variant:tube('hooked nail',[(6.6+length,1.15,z),(6.6+length,2,z),(6.1+length,2.6,z)],[.27,.18,.025],'nail',6)
  tube('opposable thumb',[(2,0,-2),(4.5,0,-3.6),(6.6,.9,-3.9),(6.6,1.7,-3.7)],[.7,.65,.42,.27],'wrap' if variant else 'skin',8)
 context['limb']=24;head()
 if not variant:
  context['limb']=25
  for sign in (-1,1):mesh('torn burial loincloth',[(-4,-1,sign*4),(4,-1,sign*4),(3.5,-8,sign*4.7),(1,-6.9,sign*5),(-1,-10,sign*4.8),(-4,-7.6,sign*4.2)],[(0,1,2,3,4,5)],'cloth')
scene.frame_start=1;scene.frame_end=rig['frames'];scene.render.fps=20;scene.frame_set(100)
scene.world=bpy.data.worlds.get('Redead Studio') or bpy.data.worlds.new('Redead Studio');scene.world.use_nodes=True
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.export_scene.gltf(filepath=str(ROOT/'redeads.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
(ROOT/'connections.json').write_text(json.dumps(connections,indent=2)+'\n')
print('Exported',len(parts),'pieces and',len(connections),'native skin connections.')
