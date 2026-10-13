"""Native Stinger art. Run in Blender or through the local MCP.
Only the ARPG Stinger Workshop scene is replaced. Runtime keeps native AI/rigs.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector
ROOT=Path(__file__).resolve().parent
rig=json.loads((ROOT/'native_rig.json').read_text())
rest=rig['animations']['gStingerHitAnim'][0]
connections=[]
scene=bpy.data.scenes.get('ARPG Stinger Workshop') or bpy.data.scenes.new('ARPG Stinger Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['skin','edge','belly','ridge','socket','eye','pupil']
PALETTES=[[(59,104,100),(159,169,128),(181,178,140),(106,56,46),(15,27,29),(213,165,69),(17,23,27)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'Stinger {sp} {name}') or bpy.data.materials.new(f'Stinger {sp} {name}')
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
    ob=bpy.data.objects.new(f'Stinger.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='original Stinger standard rig and native joint connections' ;ob['native_units_per_authoring_unit']=100
    for f,pose in enumerate(rig['poses']):
        ob.matrix_world=Matrix.Translation((variant*65,0,0)) @ axis @ Matrix(pose[str(limb)])
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
    ob=mesh(name,converted+child_points,[(j,(j+1)%n,n+(j+1)%n,n+j) for j in range(n)],'skin',True)
    ob['arpg_parent_joint']=parent;ob['arpg_parent_ring_points']=converted;ob['arpg_child_ring_points']=child_points
    connections.append({'variant':variant,'child':limb,'parent':parent,'parent_points':parent_points,'child_points':child_points})
    return ob

# [TIME] | [INFO] | Stinger.Author | Broad, thin fin surfaces and articulated tail joints follow the original ray silhouette.
def section(x,cy,ry,rz,cz=0,n=24):return [(x,cy+ry*math.cos(j*math.tau/n),cz+rz*math.sin(j*math.tau/n)) for j in range(n)]
def body(name,rings,head=False):
 smooth=[]
 for j in range(len(rings)-1):
  a,b,c,d=[rings[k] for k in (max(0,j-1),j,j+1,min(len(rings)-1,j+2))]
  for q in range(3):
   t=q/3;value=[.5*(2*b[k]+(-a[k]+c[k])*t+(2*a[k]-5*b[k]+4*c[k]-d[k])*t*t+(-a[k]+3*b[k]-3*c[k]+d[k])*t*t*t) for k in range(len(b))]
   value[2]=max(.06,value[2]);value[3]=max(.08,value[3]);smooth.append(value)
 rings=smooth+[rings[-1]]
 pts=[p for r in rings for p in section(*r)];n=24;faces=[];inks=[]
 for j in range(len(rings)-1):
  for i in range(n):
   faces.append((j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i))
   top=math.cos((i+.5)*math.tau/n)*( -1 if head else 1)
   inks.append('skin' if top>.2 else 'belly' if top<-.35 else 'edge')
 faces.extend([tuple(reversed(range(n))),tuple((len(rings)-1)*n+i for i in range(n))]);inks.extend(['skin','skin'])
 mesh(name,pts,faces,'skin',True,inks)
 return section(*rings[0]),section(*rings[-1])
def connect(name,parent,pa,cb):
 transform=Matrix(rest[str(context['limb'])]).inverted()@Matrix(rest[str(parent)]);converted=[transform@Vector(p) for p in pa];n=len(cb)
 variants=[]
 for reverse in (False,True):
  ids=list(range(n));ids=ids[::-1] if reverse else ids
  for shift in range(n):
   order=ids[shift:]+ids[:shift];variants.append((sum((converted[order[j]]-Vector(cb[j])).length_squared for j in range(n)),order))
 order=min(variants)[1];bridge(name,parent,[pa[i] for i in order],cb)
context['limb']=3
bodyStart,bodyEnd=body('tapered dorsal body',[(0,1.0,6.4,18.6),(4,1.3,7.0,19.0),(12,1,6.0,18.0),(21,.4,4.8,15.0),(30.8,.1,3.4,8.3)])
for sign in (-1,1):
 for j in range(4):
  x=7+j*4.5;tube('dorsal gill seam',[(x,5.3-j*.3,sign*9),(x+1.2,3.8-j*.35,sign*13),(x+2.1,1.9,sign*(16-j*.4))],[.18,.28,.1],'socket',6)
 tube('lateral ridge',[(1,1.1,sign*18.6),(9,1.3,sign*18.5),(19,1,sign*15.8),(29,.8,sign*9.5)],[.3,.45,.34,.2],'edge',7)
context['limb']=4
start,end=body('muscular tail root',[(-.7,.1,3.4,8.2),(4,0,3.3,7.2),(11,0,2.4,4.8),(17.6,-.2,1.0,1.5)])
connect('body tail connection',3,bodyEnd,start)
parent=4
for limb,length,rad in [(5,13.8,1.5),(6,28.5,.85)]:
 context['limb']=limb;first,last=body('flexible tapered tail',[(0,-.2,rad*.8,rad),(length*.3,-.2,rad*.65,rad*.8),(length*.7,0,rad*.4,rad*.45),(length,0,.14,.2)])
 connect('tail articulation',parent,end,first);end=last;parent=limb
 if limb==6:
  tube('barbed stinger',[(19,.15,0),(24,1,0),(28.5,2.0,0)],[.7,.55,.025],'ridge',10)
  for side in (-1,1):tube('rear facing barb',[(23,.5,side*.25),(21,1.2,side*1.8),(19,1,side*2.3)],[.38,.28,.02],'edge',7)
# Independent dorsal fins remain visible during native underground travel.
for limb,length,height in [(9,24,10.0),(10,25,5.4)]:
 context['limb']=limb
 profile=[(0,0,0),(length*.25,height*.15,0),(length*.58,height*.7,0),(length*.85,height,0),(length,height*.15,0),(length*.8,-2,0),(length*.3,-3,0)]
 pts=[(x,y,z+side*.6) for side in (-1,1) for x,y,z in profile];n=len(profile)
 fs=[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
 mesh('swept dorsal fin',pts,fs,'skin',False)
 tube('fin leading edge',profile[:5],[.28,.32,.4,.28,.06],'edge',7)
 for i in range(1,4):
  x=length*(.28+.17*i);tube('fin supporting ray',[(x*.8,-1,0),(x,height*(.18+.20*i),0)],[.24,.12],'ridge',6)
context['limb']=12
headStart,_=body('flattened ray head',[(-.3,-.3,6.0,18.6),(5,-.7,5.6,17),(11,-.8,4.0,12),(17,-.3,2.3,7.7),(21.6,0,.5,1.5)],True)
connect('head mantle connection',3,bodyStart,headStart)
# Face is modeled into the broad snout rather than painted onto a sphere.
for sign in (-1,1):
 orb('recessed eye socket',(9,-4.8,sign*8.5),(3.1,1.2,2.5),'socket',20,12)
 orb('amber eye',(9.3,-5.5,sign*8.5),(2.0,.76,1.7),'eye',20,12)
 orb('vertical pupil',(9.8,-6.1,sign*8.5),(.34,.16,1.25),'pupil',16,10)
 tube('brow rim',[(6.6,-5.25,sign*8),(8.2,-5.8,sign*10.7),(10.8,-4.95,sign*9.5)],[.48,.55,.2],'ridge',9)
 for j in range(3):
  tube('underside gill slit',[(4+j*2.5,4.3-j*.3,sign*5),(5+j*2.5,3.8-j*.3,sign*9.3)],[.22,.16],'socket',7)
tube('curved mouth opening',[(15.2,2.1,-5.3),(17.2,2.6,-2.3),(17.8,2.65,0),(17.2,2.6,2.3),(15.2,2.1,5.3)],[.16,.28,.3,.28,.16],'socket',8)
for base,tip,sign in [(14,15,1),(17,18,-1)]:
 context['limb']=base
 rings=[(1.8,.1,1.6,24,sign*7),(5,1.3,2.5,27,sign*8),(11,1.3,2.3,26,sign*2),(18.3,.2,1.8,17.8,sign*1.4)]
 first,end=body('cambered pectoral fin root',rings)
 localToHead=Matrix(rest['12']).inverted()@Matrix(rest[str(base)])
 pa=[tuple(localToHead@Vector(p)) for p in section(-1,.1,1.6,24,sign*7)]
 connect('flexible pectoral root',12,pa,first)
 for j in range(5):
  z=(-14+j*10)*sign;tube('pectoral fin ray',[(2,1.6,z),(9,3.1,z*.8),(17.8,1.8,z*.52)],[.15,.22,.1],'edge',6)
 context['limb']=tip
 first,last=body('swept outer pectoral fin',[(.0,.2,1.8,17.8,sign*1.4),(5,-.4,1.5,24,sign*-7),(12,-1,1.05,23,sign*-18),(18,-1.5,.65,12,sign*-25),(21,-1.8,.14,1,sign*-23)])
 connect('continuous wing elbow',base,end,first)
 # Long trailing lobe matches the original swept outline while retaining a thin membrane.
 pts=[];path=[(10,-1.3,-33*sign),(9,-1.6,-41*sign),(2,-1.8,-49*sign),(-6,-1.9,-56*sign),(-11.8,-1.9,-60*sign)]
 for i,(x,y,z) in enumerate(path):
  width=[6,5,4,2,.08][i]
  for dx,dy in [(-width,-.18),(width,-.18),(width,.18),(-width,.18)]:pts.append((x+dx,y+dy,z))
 faces=[(3,2,1,0),(16,17,18,19)]+[(i*4+j,i*4+(j+1)%4,(i+1)*4+(j+1)%4,(i+1)*4+j) for i in range(4) for j in range(4)]
 mesh('tapered trailing fin lobe',pts,faces,'skin',True)
 for j in range(4):
  z=(12-j*9)*sign;tube('outer fin rib',[(.3,1.9,z),(7,1.2,(z/sign-12)*sign),(16,-.45,(-23-j*2)*sign)],[.16,.21,.09],'edge',6)
 tube('trailing fin rim',[(7,-1,-35*sign),(5,-1.3,-44*sign),(-2,-1.7,-51*sign),(-11.8,-1.85,-60*sign)],[.32,.3,.2,.025],'ridge',7)
scene.frame_start=1;scene.frame_end=rig['frames'];scene.render.fps=20;scene.frame_set(43)
scene.world=bpy.data.worlds.get('Stinger Studio') or bpy.data.worlds.new('Stinger Studio');scene.world.use_nodes=True
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.export_scene.gltf(filepath=str(ROOT/'stingers.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
(ROOT/'connections.json').write_text(json.dumps(connections,indent=2)+'\n')
print('Exported',len(parts),'pieces and',len(connections),'native connections.')
