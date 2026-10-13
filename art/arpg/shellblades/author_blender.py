"""Native Shellblade art. Run in Blender or through the local MCP.
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
rest=rig['animations']['object_sb_Anim_000194'][5]
connections=[]
scene=bpy.data.scenes.get('ARPG Shellblade Workshop') or bpy.data.scenes.new('ARPG Shellblade Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['shell','edge','pearl','ridge','flesh','eye','pupil']
PALETTES=[[(121,119,104),(187,178,136),(207,181,151),(77,75,67),(112,45,70),(230,133,90),(33,17,29)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'Shellblade {sp} {name}') or bpy.data.materials.new(f'Shellblade {sp} {name}')
  mat.use_nodes=True;mat.diffuse_color=(*[v/255 for v in rgb],1)
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
  node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Roughness'].default_value=.24 if name in ('eye','iris','pupil') else .48 if name in ('mantle','ridge') else .78
  materials[sp,name]=mat
parts=[];variant=0;context={'limb':7}
def mesh(name,points,faces,material='shell',smooth=False,inks=None):
    limb=context['limb']
    data=bpy.data.meshes.new(name);data.from_pydata(points,[],faces);data.update()
    for key in MATERIALS:data.materials.append(materials[variant,key])
    for i,p in enumerate(data.polygons):
        p.material_index=MATERIALS.index(inks[i] if inks else material);p.use_smooth=smooth
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    ob=bpy.data.objects.new(f'Shellblade.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='original Shellblade flex rig and native joint connections' ;ob['native_units_per_authoring_unit']=100
    for f,pose in enumerate(rig['poses']):
        ob.matrix_world=Matrix.Translation((variant*49,0,0)) @ axis @ Matrix(pose[str(limb)])
        ob.keyframe_insert(data_path='location',frame=f+1)
        ob.keyframe_insert(data_path='scale',frame=f+1)
        ob.rotation_mode='QUATERNION';ob.keyframe_insert(data_path='rotation_quaternion',frame=f+1)
    parts.append(ob);return ob

def loft(name,rings,mat='shell',sections=12,striped=False):
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

def tube(name,path,radii,mat='shell',sides=7):
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
    # [TIME] | [INFO] | Shellblade.Asset | Closed, smoothly shaded eye/joint surface, not an overlapping body primitive.
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
    ob=mesh(name,converted+child_points,[(j,(j+1)%n,n+(j+1)%n,n+j) for j in range(n)],'shell',True)
    ob['arpg_parent_joint']=parent;ob['arpg_parent_ring_points']=converted;ob['arpg_child_ring_points']=child_points
    connections.append({'variant':variant,'child':limb,'parent':parent,'parent_points':parent_points,'child_points':child_points})
    return ob

# [TIME] | [INFO] | Shellblade.Author | Two concave, scalloped valves retain their original rigid hinge matrices and independent death fragments.
def outline(cx,rx,rz,r,a):
 scallop=1+.025*math.cos(a*14)*r**3
 return (cx+rx*r*math.cos(a)*scallop,rz*r*math.sin(a)*scallop)
def bowl(name,cx,rx,rz,depth):
 n=48;steps=8;pts=[];faces=[];inks=[]
 for side in (0,1):
  for j in range(steps+1):
   r=.025+(1-.025)*j/steps
   for i in range(n):
    a=i*math.tau/n;x,z=outline(cx,rx,rz,r,a)
    y=-(depth if side==0 else depth*.52)*max(0,1-r*r)**.7
    if side==0:y-=.8*math.cos(a*14)*r*math.sin(math.pi*r)
    pts.append((x,y,z))
  base=side*(steps+1)*n;faces.append(tuple(base+i for i in reversed(range(n))));inks.append('shell' if side==0 else 'pearl')
  for j in range(steps):
   for i in range(n):faces.append((base+j*n+i,base+j*n+(i+1)%n,base+(j+1)*n+(i+1)%n,base+(j+1)*n+i));inks.append('shell' if side==0 else 'pearl')
 for i in range(n):faces.append((steps*n+i,steps*n+(i+1)%n,(2*steps+1)*n+(i+1)%n,(2*steps+1)*n+i));inks.append('edge')
 mesh(name,pts,faces,'shell',True,inks)
 for j in range(14):
  a=j*math.tau/14;path=[]
  for k in range(13):
   r=.15+.83*k/12;x,z=outline(cx,rx,rz,r,a);y=-depth*max(0,1-r*r)**.7-.5;path.append((x,y,z))
  tube('raised radial shell rib',path,[.28+.48*math.sin(math.pi*k/12) for k in range(13)],'edge',6)
 for r in (.65,.96):
  path=[]
  for i in range(49):
   a=i*math.tau/48;x,z=outline(cx,rx,rz,r,a);path.append((x,-depth*max(0,1-r*r)**.7-.6,z))
  tube('layered growth ridge',path,[.25 if r<.9 else .65]*49,'ridge' if r<.9 else 'edge',5)
 for i in range(13):
  front=-1 if context['limb']==4 else 1;a=math.pi*(-.78+1.56*(i+.25*(front<0))/12)+(math.pi if front<0 else 0);x,z=outline(cx,rx,rz,.88,a)
  # [TIME] | [INFO] | Shellblade.Teeth | Upper/lower blade tips stagger rather than sharing identical closed-pose points.
  tooth=4 if i%2 else 5
  tube('curved ivory blade',[(x,-4.0,z),(x-front,-.2,z*.97),(x-2.1*front,tooth,z*.93)],[2.3,1.5,.08],'pearl',8)
for limb,cx,rx,rz,depth in [(4,46,49,40,19),(6,34,49,40,17)]:
 context['limb']=limb;bowl('ribbed lower valve' if limb==4 else 'ribbed upper valve',cx,rx,rz,depth)
 tube('hinge collar',[(82 if limb==4 else -2,-3,-20),(82 if limb==4 else -2,-3,0),(82 if limb==4 else -2,-3,20)],[3,3.4,3],'ridge',12)
context['limb']=8
orb('mantle pad',(34,-5,0),(18,5.5,18),'flesh',24,14)
orb('central amber eye',(27,-.4,0),(9.5,3.2,10.5),'eye',24,14)
orb('vertical iris',(24,2.1,0),(2.2,1.1,7.5),'pupil',20,12)
for sign in (-1,1):
 for j in range(3):tube('mantle folds',[(22+j*5,-2,sign*12),(31+j*3,-.5,sign*16),(40+j*2,-3,sign*12)],[.6,.9,.3],'flesh',7)
context['limb']=7
orb('upper mantle',(42,-5,0),(16,4,17),'flesh',24,12)
# [TIME] | [INFO] | Shellblade.Hinge | A recessed muscle follows the native mixed lower/upper matrices; only the two rigid shell limbs enter BodyBreak.
parent_points=[(70+7*math.cos(i*math.tau/18),-6,9*math.sin(i*math.tau/18)) for i in range(18)]
child_points=[(15+7*math.cos(i*math.tau/18),-6,9*math.sin(i*math.tau/18)) for i in range(18)]
def inner_surface(x,z,cx,depth):return -depth*.52*max(0,1-((x-cx)/49)**2-(z/40)**2)**.7-.7
parent_points=[(x,inner_surface(x,z,46,19),z) for x,y,z in parent_points]
child_points=[(x,inner_surface(x,z,34,17),z) for x,y,z in child_points]
bridge('hinged adductor muscle',4,parent_points,child_points)
parts[-1].data.polygons.foreach_set('material_index',[MATERIALS.index('flesh')]*len(parts[-1].data.polygons))
scene.frame_start=1;scene.frame_end=rig['frames'];scene.render.fps=20;scene.frame_set(22)
scene.world=bpy.data.worlds.get('Shellblade Studio') or bpy.data.worlds.new('Shellblade Studio');scene.world.use_nodes=True
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.export_scene.gltf(filepath=str(ROOT/'shellblades.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
(ROOT/'connections.json').write_text(json.dumps(connections,indent=2)+'\n')
print('Exported',len(parts),'pieces and',len(connections),'native connections.')
