"""Native Peahat art. Run in Blender or through the local MCP.
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
rest=rig['animations']['gPeehatFlyingAnim'][0]
connections=[]
scene=bpy.data.scenes.get('ARPG Peahat Workshop') or bpy.data.scenes.new('ARPG Peahat Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['bark','edge','leaf','vein','root','core','shadow']
PALETTES=[[(118,77,41),(180,139,79),(78,112,48),(151,170,83),(151,88,50),(223,152,73),(52,43,27)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'Peahat {sp} {name}') or bpy.data.materials.new(f'Peahat {sp} {name}')
  mat.use_nodes=True;mat.diffuse_color=(*[v/255 for v in rgb],1)
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
  node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Roughness'].default_value=.24 if name in ('eye','iris','pupil') else .48 if name in ('mantle','ridge') else .78
  materials[sp,name]=mat
parts=[];variant=0;context={'limb':7}
def mesh(name,points,faces,material='bark',smooth=False,inks=None):
    limb=context['limb']
    data=bpy.data.meshes.new(name);data.from_pydata(points,[],faces);data.update()
    for key in MATERIALS:data.materials.append(materials[variant,key])
    for i,p in enumerate(data.polygons):
        p.material_index=MATERIALS.index(inks[i] if inks else material);p.use_smooth=smooth
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    ob=bpy.data.objects.new(f'Peahat.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='original Peahat flex rig and native joint connections' ;ob['native_units_per_authoring_unit']=100
    for f,pose in enumerate(rig['poses']):
        ob.matrix_world=Matrix.Translation((variant*49,0,0)) @ axis @ Matrix(pose[str(limb)])
        ob.keyframe_insert(data_path='location',frame=f+1)
        ob.keyframe_insert(data_path='scale',frame=f+1)
        ob.rotation_mode='QUATERNION';ob.keyframe_insert(data_path='rotation_quaternion',frame=f+1)
    parts.append(ob);return ob

def loft(name,rings,mat='bark',sections=12,striped=False):
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

def tube(name,path,radii,mat='bark',sides=7):
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
    return mesh(name,pts,faces,mat,mat=='root')

def plate(name,points,depth,mat,normal=(0,-1,0)):
    points=[Vector(p) for p in points];n=Vector(normal);c=sum(points,Vector())/len(points);k=len(points)
    pts=[tuple(p-n*depth*.25) for p in points]+[tuple(p) for p in points]+[tuple(p.lerp(c,.12)+n*depth) for p in points]
    faces=[tuple(reversed(range(k)))]
    for j in range(2):
        for i in range(k):faces.append((j*k+i,j*k+(i+1)%k,(j+1)*k+(i+1)%k,(j+1)*k+i))
    faces.append(tuple(2*k+i for i in range(k)))
    return mesh(name,pts,faces,mat)

def orb(name,center,radius,mat,slices=16,rings=10):
    # [TIME] | [INFO] | Peahat.Asset | Closed, smoothly shaded eye/joint surface, not an overlapping body primitive.
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



# [TIME] | [INFO] | Peahat.Author | Fibrous bulb, overlapping crown sepals and six thin, veined blades follow native rigid joints.
def bulb_radius(x):
 rings=[(-7,.2),(-4,6),(0,11),(7,14),(15,15),(23,11),(29,5),(32,.3)]
 for i in range(len(rings)-1):
  a,b=rings[i],rings[i+1]
  if a[0]<=x<=b[0]:return a[1]+(b[1]-a[1])*(x-a[0])/(b[0]-a[0])
 return .3
context['limb']=3
loft('tapered fibrous bulb',[(x,0,bulb_radius(x),bulb_radius(x)*.96) for x in (-7,-4,0,4,7,11,15,19,23,26,29,32)],'bark',32)
for i in range(16):
 a=i*math.tau/16;path=[]
 for x in (-4,0,5,10,15,20,25,29):
  r=bulb_radius(x)+.14;angle=a+.05*math.sin(x*.15);path.append((x,r*math.cos(angle),r*.96*math.sin(angle)))
 tube('longitudinal bark rib',path,[.1,.25,.35,.42,.36,.3,.2,.08],'edge',5)
# Overlapping husk scales retain sharp tips and open shadow seams.
for row,x in enumerate((4,12,20)):
 for j in range(12):
  a=j*math.tau/12+row*.24;c=Vector((x,(bulb_radius(x)+.3)*math.cos(a),(bulb_radius(x)+.3)*.96*math.sin(a)))
  u=Vector((1,0,0));v=Vector((0,-math.sin(a),math.cos(a)));n=Vector((0,math.cos(a),math.sin(a)))
  pts=[c-u*4,c+v*2.5,c+u*4.5,c-v*2.5,c+n*.75]
  mesh('pointed husk scale',[tuple(p) for p in pts],[(0,1,4),(1,2,4),(2,3,4),(3,0,4),(3,2,1,0)],'bark')
# A stout native hub keeps blade pivots buried inside the moving bulb.
loft('root collar',[(-2,0,7,7),(0,0,7.5,7.5),(2,0,6.5,6.5)],'shadow',24)
# Crown leaves are modeled closed thin surfaces, with their own raised central vein.
context['limb']=23
loft('crown heart',[(24,0,7,7),(30,0,5,5),(35,0,2.5,2.5),(36.5,0,.2,.2)],'leaf',20)
for j in range(8):
 a=j*math.tau/8;radial=Vector((0,math.cos(a),math.sin(a)));side=Vector((0,-math.sin(a),math.cos(a)))
 pts=[];path=[];steps=10
 for side_layer in (-1,1):
  for k in range(steps+1):
   t=k/steps;x=31-20*t+3*math.sin(math.pi*t);r=3+26*t;w=(.2+6*math.sin(math.pi*t)**.8)*(1 if j%2==0 else .86)
   center=Vector((x+side_layer*.14,0,0))+radial*r
   for cross in (-1,0,1):pts.append(tuple(center+side*w*cross+Vector((.9*(1-abs(cross))*math.sin(math.pi*t),0,0))))
   if side_layer==1:path.append(tuple(center+Vector((1.05*math.sin(math.pi*t),0,0))))
 faces=[];m=(steps+1)*3
 for layer in (0,1):
  b=layer*m
  for k in range(steps):
   for c in range(2):faces.append((b+k*3+c,b+k*3+c+1,b+(k+1)*3+c+1,b+(k+1)*3+c))
 for edge in (0,2):
  for k in range(steps):faces.append((k*3+edge,(k+1)*3+edge,m+(k+1)*3+edge,m+k*3+edge))
 faces.extend([(0,1,2,m+2,m+1,m),(steps*3,steps*3+1,steps*3+2,m+steps*3+2,m+steps*3+1,m+steps*3)])
 mesh('curled crown sepal',pts,faces,'leaf',True)
 tube('crown leaf midrib',path,[.15+.17*math.sin(math.pi*k/steps) for k in range(steps+1)],'vein',5)
# Each leaf blade pivots and spins exactly where the original wing does.
for limb in (7,10,13,16,19,22):
 context['limb']=limb;pts=[];faces=[];n=20
 def curve(t):return -2+50*t,-.7*math.sin(math.pi*t),1.2*math.sin(math.pi*t)*math.sin(t*3)-2*t*t
 def width(t):return .3+9.4*math.sin(math.pi*t)**.8*(1-.24*t)
 for layer in (-1,1):
  for k in range(n+1):
   t=k/n;x,y,z=curve(t);w=width(t)*(1-.05*(k%2))
   for cross in (-1,-.5,0,.5,1):pts.append((x,y+layer*.12-.65*(1-abs(cross))*math.sin(math.pi*t),z+w*cross))
 m=(n+1)*5
 for layer in (0,1):
  b=layer*m
  for k in range(n):
   for c in range(4):faces.append((b+k*5+c,b+k*5+c+1,b+(k+1)*5+c+1,b+(k+1)*5+c))
 for edge in (0,4):
  for k in range(n):faces.append((k*5+edge,(k+1)*5+edge,m+(k+1)*5+edge,m+k*5+edge))
 faces.extend([tuple(range(5))+tuple(range(m+4,m-1,-1)),tuple(range(n*5,n*5+5))+tuple(range(m+n*5+4,m+n*5-1,-1))])
 mesh('swept leaf blade',pts,faces,'leaf',True)
 path=[]
 for k in range(n+1):
  t=k/n;x,y,z=curve(t);path.append((x,y-.13-.65*math.sin(math.pi*t),z))
 tube('blade central vein',path,[.11+.24*math.sin(math.pi*k/n) for k in range(n+1)],'vein',5)
 for k in range(3,18,3):
  t=k/n;x,y,z=curve(t)
  for sign in (-1,1):
   t2=min(.99,t+.085);x2,y2,z2=curve(t2)
   tube('blade branching vein',[(x,y-.8,z),(x+2,y-.48,z+sign*width(t)*.45),(x2,y2-.14,z2+sign*width(t2)*.9)],[.12,.1,.045],'vein',5)
# Independent weak root uses the native post-limb transform and remains absent on larvae.
context['limb']=24
loft('exposed tapered root',[(0,0,4,4),(4,0,10,10),(11,0,12,12),(20,0,7,7),(27,0,2.2,2.2),(30,0,.3,.3)],'root',28)
for j in range(12):
 a=j*math.tau/12;path=[(x,r*math.cos(a),r*math.sin(a)) for x,r in [(1,5),(5,10.2),(11,12.3),(19,8),(27,2.5)]]
 tube('root fiber',path,[.3,.6,.65,.4,.08],'core',6)
scene.frame_start=1;scene.frame_end=rig['frames'];scene.render.fps=20;scene.frame_set(1)
scene.world=bpy.data.worlds.get('Peahat Studio') or bpy.data.worlds.new('Peahat Studio');scene.world.use_nodes=True
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.export_scene.gltf(filepath=str(ROOT/'peahats.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
(ROOT/'connections.json').write_text('[]\n')
print('Exported',len(parts),'pieces on eight native limbs and the weak root.')
