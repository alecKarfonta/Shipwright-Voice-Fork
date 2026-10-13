"""Native Moblin art. Run in Blender or through the local MCP.
Only the ARPG Moblin Workshop scene is replaced. Runtime keeps native AI/rigs.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector
ROOT=Path(__file__).resolve().parent
rig=None
rest=None
connections=[]
scene=bpy.data.scenes.get('ARPG Moblin Workshop') or bpy.data.scenes.new('ARPG Moblin Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['skin','ridge','iron','edge','leather','cloth','cavity','ivory','eye','wood']
PALETTES=[[(145,114,66),(183,143,80),(75,88,82),(153,151,119),(78,47,30),(79,63,42),(29,24,20),(214,198,143),(209,153,46),(109,70,36)],[(156,98,65),(194,130,81),(66,73,72),(146,140,115),(66,37,24),(87,46,36),(31,22,18),(224,201,146),(227,162,48),(102,59,30)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'Moblin {sp} {name}') or bpy.data.materials.new(f'Moblin {sp} {name}')
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
    ob=bpy.data.objects.new(f'Moblin.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='original Moblin flex rig and native joint connections' ;ob['native_units_per_authoring_unit']=100
    ob.matrix_world=Matrix.Translation((variant*90,0,0)) @ axis @ Matrix(rest[str(limb)])
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
    choices=[]
    for flip in (False,True):
        order=list(range(n));order=order[::-1] if flip else order
        for shift in range(n):
            ids=order[shift:]+order[:shift];choices.append((sum((Vector(converted[ids[j]])-Vector(child_points[j])).length_squared for j in range(n)),ids))
    ids=min(choices)[1];parent_points=[parent_points[i] for i in ids];converted=[converted[i] for i in ids]
    ob=mesh(name,converted+child_points,[(j,(j+1)%n,n+(j+1)%n,n+j) for j in range(n)],'skin',True)
    ob['arpg_parent_joint']=parent;ob['arpg_parent_ring_points']=converted;ob['arpg_child_ring_points']=child_points
    connections.append({'variant':variant,'child':limb,'parent':parent,'parent_points':parent_points,'child_points':child_points})
    return ob

# [TIME] | [INFO] | Moblin.Author | Sculpted anatomy follows each original rigid limb; short skin bridges use the same native flex matrices as the original.
def band(name,x,cy,ry,rz,mat='leather',width=.8,n=16):
 return loft(name,[(x-width,cy,ry,rz),(x-width*.65,cy,ry+.16,rz+.16),(x+width*.65,cy,ry+.16,rz+.16),(x+width,cy,ry,rz)],mat,n)
def stud(name,p,r=.3):return orb(name,p,(r,r,r),'edge',8,5)
def transform_points(basis,points):return [tuple(basis@Vector(p)) for p in points]
def face():
 global mesh
 old=mesh
 scale=(1.04,1.12,1.03) if variant else (1,1,1)
 def mesh(name,points,faces,material='skin',smooth=False,inks=None):return old(name,[(p[0]*scale[0],p[1]*scale[1],p[2]*scale[2]) for p in points],faces,material,smooth,inks)
 loft('angular boar cranium',[(-8,-9,4,5),(-5.2,-10,7.7,8.4),(-.7,-10.1,9,9.7),(4,-9.3,8.2,9.1),(8.1,-8.2,6.1,7.6),(10.5,-5.7,3.8,6.4)],'skin',18)
 loft('projecting broad muzzle',[(6.8,-4.5,3.7,6.8),(10.5,-4.2,4.4,7.0),(13.1,-4.0,3.5,6.4)],'ridge',16)
 plate('flattened leathery nose',[(13.2,-6.7,-4.9),(13.2,-7.4,0),(13.2,-6.7,4.9),(13.2,-2.4,5.3),(13.2,-1.4,0),(13.2,-2.4,-5.3)],.25,'skin',(1,0,0))
 for sign in (-1,1):
  orb('deep nostril',(13.55,-4.8,sign*2.6),(.18,1.25,.96),'cavity',12,8)
  tube('nostril lip',[(13.48,-6.0,sign*2.2),(13.6,-5.4,sign*3.55),(13.5,-4.0,sign*3.35)],[.16,.2,.12],'ridge',6)
  eye=[(-10.7,3.8),(-13.3,7.2),(-11.1,7.7),(-9.8,4.7)];pts=[]
  for x,expand in [(9.9,1.22),(10.0,1),(9.4,.72)]:
   for y,z in eye:pts.append((x,-11.2+(y+11.2)*expand,sign*(5.8+(z-5.8)*expand)))
  fs=[];inks=[]
  for k in range(2):
   for j in range(4):fs.append((k*4+j,k*4+(j+1)%4,(k+1)*4+(j+1)%4,(k+1)*4+j));inks.append('ridge' if k==0 else 'cavity')
  fs.append((8,9,10,11));inks.append('cavity');mesh('slanted recessed eye',pts,fs,'skin',False,inks)
  orb('small amber iris',(9.62,-11.25,sign*5.75),(.16,.43,.56),'eye',12,8)
  orb('narrow pupil',(9.78,-11.25,sign*5.75),(.04,.32,.18),'cavity',8,6)
  tube('heavy scowling brow',[(8.9,-10.8,sign*2.9),(10.1,-12.7,sign*5.7),(8.9,-14.3,sign*8.0)],[.76,.95,.57],'ridge',10)
  plate('defined cheek plane',[(9.2,-10.4,sign*7.6),(10.6,-8.4,sign*6.8),(8.2,-5.5,sign*8.2),(4.9,-8.4,sign*9.2)],.45,'skin',(1,0,0))
  tube('upturned ivory tusk',[(9.5,-1.3,sign*5.2),(11.5,-3.3,sign*7.5),(12.5,-6.6,sign*8.2),(11.9,-9.3,sign*7.8)],[1.0,.82,.46,.035],'ivory',11)
  plate('pointed boar ear',[(-4.8,-15,sign*8.4),(-4.7,-19.2,sign*12.8),(1,-16.4,sign*10.1),(.8,-12.4,sign*8.7)],.65,'skin',(1,0,0))
  plate('ear inset',[(-3.9,-15.3,sign*9.0),(-3.8,-18,sign*11.75),(-.2,-15.9,sign*9.7)],.12,'leather',(1,0,0))
 tube('recessed mouth seam',[(11,-.55,-5.2),(12.9,-.3,0),(11,-.55,5.2)],[.22,.32,.22],'cavity',7)
 for j in range(7):
  z=(j-3)*1.5;tube('swept crown bristle',[(-5.6,-16.5,z),(-5.9,-20.2,z*.84),(-8.9,-20.7,z*.64)],[.85,.58,.03],'leather',6)
 for sign in (-1,1):
  for j in range(3):tube('jaw whisker tuft',[(0,-3.9-j,sign*8.1),(-3.1,-3.5-j,sign*9.1)],[.55,.03],'leather',6)
 if variant:
  tube('battle scar',[(7.9,-14.8,-4),(8.9,-13.7,-5.1),(9.7,-12.6,-5.4)],[.1,.13,.1],'leather',5)
 mesh=old

for variant,name,clip in [(0,'spear','gEnMbSpearStandStillAnim'),(1,'club','gEnMbClubStandStillClubDownAnim')]:
 rig=json.loads((ROOT/f'native_{name}.json').read_text());rest=rig['animations'][clip][0];chest=5 if variant else 6
 context['limb']=3;hip=1.06 if variant else 1
 pelvis=[(-.4,0,10.4*hip,10.8*hip),(3.3,0,11.0*hip,11.8*hip),(8,0,10.2*hip,11.2*hip),(13.2,0,8.6*hip,9.7*hip),(15.3,0,7.4*hip,8.4*hip)]
 loft('tapered hide-clad pelvis',pelvis,'cloth',18);band('broad belt',1.9,0,11.1*hip,11.95*hip,'leather',1.4)
 for side in (-1,1):
  for i in range(3):
   z=side*(2+i*3.2);plate('overlapping skirt strip',[(3,-11.1*hip,z-1.65),(3,-11.1*hip,z+1.65),(13.8-i,-8.3*hip,z+1.3),(15-i,-7.7*hip,z-.9)],.24,'leather',(0,-1,0));stud('belt rivet',(1.8,-11.3*hip,z),.27)
 plate('belt buckle',[(.7,-11.6*hip,-2.6),(.7,-11.6*hip,2.6),(3.2,-11.6*hip,2.6),(3.2,-11.6*hip,-2.6)],.35,'iron',(0,-1,0))
 # The club torso is drawn on joint 5 rather than the spear's joint 6.
 context['limb']=chest;old=mesh
 basis=Matrix(rest[str(chest)]).to_3x3().transposed() @ Matrix(((0,0,1),(1,0,0),(0,1,0)))
 def mesh(name,points,faces,material='skin',smooth=False,inks=None):return old(name,transform_points(basis,points),faces,material,smooth,inks)
 torso=[(.7,0,9.7*hip,10.8*hip),(5.5,.1,10.2*hip,13.9*hip),(11.9,.5,11.1*hip,15.1*hip),(16,.1,9.7*hip,14.6*hip),(18.8,0,5.0,6.4)]
 loft('tapered muscular chest',torso,'skin',20)
 for sign in (-1,1):
  plate('angular pectoral armor',[(5.4,9.45,sign*1.0),(6.3,9.25,sign*10.7),(13.7,9.6,sign*13.2),(16.5,9.2,sign*5.5),(14.9,11.4,sign*1.0)],.6,'iron',(0,1,0))
  tube('forged chest plate edge',[(6,9.95,sign*1.2),(6.9,9.75,sign*10.2),(13.3,10.0,sign*12.6),(15.7,9.75,sign*5.7)],[.25]*4,'edge',7)
  tube('shoulder harness',[(1,9.6,sign*5.5),(8,11.3,sign*6.5),(14,11.1,sign*8.6),(18,5.9,sign*9.6),(16,-8.5,sign*8.5)],[.65]*5,'leather',8)
  for x,z in [(8,7),(12.5,10.8),(14.2,5.8)]:stud('breastplate rivet',(x,11.1,sign*z),.32)
 for x in (4,7.3,10.6,13.9):tube('back mane ridge',[(x,-10.3,-5),(x+1.4,-12.2,0),(x,-10.3,5)],[.65,.9,.65],'leather',8)
 chest_bottom=transform_points(basis,ring(*torso[0]));chest_top=transform_points(basis,ring(*torso[-1]));mesh=old
 bridge('waist skin connection',3,ring(*pelvis[0]),chest_bottom)
 context['limb']=7;headbase=[(math.cos(a)*5.0,-1.8,math.sin(a)*6.2) for a in [j*math.tau/12 for j in range(12)]];bridge('neck skin connection',chest,chest_top,headbase);face()
 # Arms taper continuously through elbow and wrist, with separate plate and wrap detail.
 for shoulder,forearm,hand,sign in [(9,11,12,1),(14,16,17,-1)]:
  arm=1.28 if variant else 1;length=18.2 if variant else 12.6;context['limb']=shoulder
  upper=[(-3.0,0,4.2*arm,4.3*arm),(.0,0,6.0*arm,6.15*arm),(5.5,0,5.9*arm,5.8*arm),(length-1.8,0,3.7*arm,3.9*arm),(length,0,3.2*arm,3.5*arm)]
  loft('defined shoulder and upper arm',upper,'skin',16)
  shoulder_mesh=mesh
  if variant and shoulder==14:
   def mesh(name,points,faces,material='skin',smooth=False,inks=None):return shoulder_mesh(name,[(x,-y,-z) for x,y,z in points],faces,material,smooth,inks)
  plate('beveled shoulder guard',[(-4,-5.5*arm,-4.6*arm),(-5,-5.8*arm,4.6*arm),(1.6,-7.0*arm,5.5*arm),(6,-5.0*arm,3.7*arm),(5.8,-5.1*arm,-3.7*arm),(1.2,-6.9*arm,-5.2*arm)],.7,'iron',(0,-1,0))
  for z in (-3.3,0,3.3):tube('shoulder plate ridge',[(-3.3,-6.3*arm,z*arm),(1,-7.4*arm,z*arm),(5,-5.6*arm,z*.9*arm)],[.2,.3,.16],'edge',7)
  tube('swept shoulder spike',[(-2,-3.5*arm,sign*3),(-7,-4.8*arm,sign*4.4),(-10.5*arm,-4.9*arm,sign*5.2)],[1.65*arm,.94*arm,.04],'ivory',10)
  if variant:tube('secondary shoulder spike',[(-.5,-3.7,sign*5.5),(-4,-4.9,sign*9.8),(-6.5,-4.9,sign*12.0)],[1.4,.85,.03],'ivory',9)
  mesh=shoulder_mesh
  end=ring(*upper[-1]);context['limb']=forearm;flen=15.9 if variant else 12.4
  lower=[(.8,0,3.3*arm,3.5*arm),(3.4,0,4.5*arm,4.2*arm),(8,0,4.2*arm,3.6*arm),(flen-.1,0,2.8*arm,2.9*arm)]
  loft('tapered forearm',lower,'skin',16);bridge('elbow skin connection',shoulder,end,ring(*lower[0]))
  for x in (5.0,7.4,9.8):band('bound forearm leather',x,0,4.35*arm-(x-5)*.1,3.8*arm,'leather',.6)
  plate('forearm iron splint',[(3.2,-4.6*arm,-2.4),(3.2,-4.6*arm,2.4),(flen-1.5,-3.3*arm,2.0),(flen-.6,-3.0*arm,0),(flen-1.5,-3.3*arm,-2.0)],.4,'iron',(0,-1,0))
  end=ring(*lower[-1]);context['limb']=hand
  palm=[(.4,0,2.8*arm,2.9*arm),(4,0,3.5*arm,3.4*arm),(8.8,-.5,3.3*arm,3.3*arm),(10.1,-.9,2.7*arm,3.0*arm)]
  loft('broad gripping palm',palm,'skin',16);bridge('wrist skin connection',forearm,end,ring(*palm[0]));band('wrist cuff',1.8,0,3.0*arm,3.1*arm,'leather',.65)
  for finger in range(4):
   z=(finger-1.5)*1.55*arm;tube('curled segmented finger',[(7.2,-3.1*arm,z),(10.6,-3.6*arm,z),(13.1,-1.5*arm,z),(12,1.2*arm,z),(9.5,2*arm,z)],[.91*arm,1.0*arm,.91*arm,.74*arm,.65*arm],'skin',9)
   tube('knuckle crease',[(10.4,-4.52*arm,z-.45),(10.8,-4.6*arm,z+.45)],[.11,.11],'leather',5)
  tube('opposed grasping thumb',[(3.3,-1.4,sign*3.1*arm),(5.7,-3.7,sign*4.4*arm),(8.9,-4.7,sign*2.3*arm)],[1.35*arm,1.2*arm,.7*arm],'skin',10)
 # Bent knees and planted toes follow the original leg joints.
 for thigh,shin,foot,sign in [(20,21,22,1),(25,26,27,-1)]:
  leg=1.25 if variant else 1;context['limb']=thigh;length=17.54 if variant else 12.18
  upper=[(1,0,6.2*leg,5.5*leg),(4.6,0,6.3*leg,5.7*leg),(length-3.5,0,4.6*leg,4.35*leg),(length-.3,0,3.8*leg,3.8*leg)]
  loft('haunched thigh',upper,'skin',16)
  parent=[(10.5+math.cos(a)*3.1,-math.sin(a)*7.0,sign*7.0*hip) for a in [j*math.tau/12 for j in range(12)]];bridge('hip skin connection',3,parent,ring(*upper[0]))
  band('thigh leather strap',4.3,0,6.4*leg,5.8*leg,'leather',.65)
  end=ring(*upper[-1]);context['limb']=shin;length=16.32 if variant else 10.49
  lower=[(.4,0,3.85*leg,3.9*leg),(3.7,0,4.5*leg,4.2*leg),(length-1.5,0,2.8*leg,3.0*leg),(length,0,2.6*leg,2.8*leg)]
  loft('tapered calf and ankle',lower,'skin',16);bridge('knee skin connection',thigh,end,ring(*lower[0]));band('ankle leather binding',length-1.6,0,3.0*leg,3.1*leg,'leather',.65)
  plate('ridged shin guard',[(1.6,-4.3*leg,-2.9*leg),(1.6,-4.3*leg,2.9*leg),(length-2,-3.0*leg,2.2*leg),(length-.4,-2.7*leg,0),(length-2,-3.0*leg,-2.2*leg)],.5,'iron',(0,-1,0))
  tube('shin guard ridge',[(2,-4.9*leg,0),(length*.5,-4.3*leg,0),(length-1,-3.1*leg,0)],[.18,.32,.12],'edge',7)
  end=ring(*lower[-1]);context['limb']=foot;size=1.2 if variant else 1
  shape=[(-6.8*size,5.2,2.2,3.5*size),(-3*size,4.9,3.0,5.1*size),(4.5*size,5.1,2.7,5.8*size),(11.5*size,5.8,1.9,5.6*size),(14.9*size,6,1.0,4.8*size)]
  loft('broad weight-bearing foot',shape,'skin',18)
  child=[(math.cos(a)*2.9,2.1,math.sin(a)*3.0) for a in [j*math.tau/12 for j in range(12)]];bridge('ankle skin connection',shin,end,child)
  for toe in (-1,0,1):
   z=toe*3.8*size;tube('separated forward toe',[(9.2*size,5.8,z),(13.2*size,5.9,z),(15.5*size,6.0,z)],[1.6*size,1.55*size,1.0*size],'skin',10)
   tube('blunt ivory toenail',[(14.5*size,5.5,z),(16.0*size,5.7,z),(16.5*size,6.1,z)],[.95*size,.6*size,.035],'ivory',9)
 # Weapon geometry stays on the original gripping hand and within its native reach.
 if variant==0:
  context['limb']=17
  tube('tapered spear shaft',[(9.4,27,0),(9.4,-5,0),(9.4,-55.5,0)],[1.0,1.05,.85],'wood',12)
  for y in range(-15,18,3):
   tube('spear grip spiral',[(9.4+1.16*math.cos(a),y+a/math.tau*2.5,1.16*math.sin(a)) for a in [j*math.tau/12 for j in range(13)]],[.14]*13,'leather',5)
  for y in (-54,-51,25.8):tube('spear metal socket',[(9.4+1.25*math.cos(a),y,1.25*math.sin(a)) for a in [j*math.tau/16 for j in range(17)]],[.35]*17,'iron',6)
  plate('leaf-shaped spear point',[(9.4,-72.7,0),(12.8,-58.5,0),(11.6,-52.9,0),(7.1,-52.9,0),(6.05,-58.5,0)],.7,'iron',(0,0,1))
  tube('spear central ridge',[(9.4,-71.3,.55),(9.4,-59.1,.95),(9.4,-53.0,.7)],[.07,.28,.18],'edge',7)
 else:
  context['limb']=12
  tube('heavy club timber haft',[(14.5,13.5,1.6),(14.5,-30,1.6),(14.5,-65,1.6)],[1.2,1.6,2.0],'wood',12)
  for y in range(-20,13,3):tube('club grip wrap',[(14.5+1.65*math.cos(a),y+a/math.tau*2.6,1.6+1.65*math.sin(a)) for a in [j*math.tau/12 for j in range(13)]],[.23]*13,'leather',6)
  old=mesh
  def mesh(name,points,faces,material='skin',smooth=False,inks=None):return old(name,[(14.5+p[1],-p[0],1.6+p[2]) for p in points],faces,material,smooth,inks)
  head=[(52.8,0,5.5,5.4),(57.5,0,10.9,10.7),(72,0,12.1,12.0),(81.7,0,9.9,9.6),(85.1,0,6.3,6.0)]
  loft('faceted timber maul',head,'wood',12)
  for x,ry,rz in [(58.5,11.4,11.2),(73,12.2,12.1),(80.8,10.4,10.1)]:band('forged maul band',x,0,ry,rz,'iron',1.1)
  for j in range(8):
   a=j*math.tau/8;tube('split timber grain',[(59,10.8*math.cos(a),10.7*math.sin(a)),(67,12.0*math.cos(a+.03),12.0*math.sin(a+.03)),(79,10.4*math.cos(a),10.2*math.sin(a))],[.14,.21,.12],'leather',5)
   for x,rr in [(59,11.8),(73,12.5)]:stud('maul band rivet',(x,rr*math.cos(a),rr*math.sin(a)),.47)
  mesh=old
scene.frame_start=1;scene.frame_end=1;scene.render.fps=20
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.export_scene.gltf(filepath=str(ROOT/'moblins.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
(ROOT/'connections.json').write_text(json.dumps(connections,indent=2)+'\n')
print('Exported',len(parts),'pieces and',len(connections),'native connections.')
