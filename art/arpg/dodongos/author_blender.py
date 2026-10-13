"""Native Dodongo art. Run in Blender or through the local MCP.
Only the ARPG Dodongo Workshop scene is replaced. Runtime keeps native AI/rigs.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector
ROOT=Path(__file__).resolve().parent
rig=json.loads((ROOT/'native_rig.json').read_text())
scene=bpy.data.scenes.get('ARPG Dodongo Workshop') or bpy.data.scenes.new('ARPG Dodongo Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['skin','scute','belly','groove','horn','eye','pupil','mouth']
PALETTES=[[(107,125,79),(144,151,93),(178,137,71),(62,73,48),(226,207,154),(228,156,45),(25,26,18),(67,37,36)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'Dodongo {sp} {name}') or bpy.data.materials.new(f'Dodongo {sp} {name}')
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
    ob=bpy.data.objects.new(f'Dodongo.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='original 30-limb Dodongo rig and native deformation' ;ob['native_units_per_authoring_unit']=100
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



# [TIME] | [INFO] | Dodongo.Author | Broad angular snout and recessed eyes follow the original upper-head joint; the jaw is independently animated.
context['limb']=7
loft('continuous armored head',[(-5.8,-1.5,3.0,5.8),(-3,-2.5,7.6,11.1),(2,-3.1,8.1,12.35),(8,-2.7,7.1,10.8),(15,-2.1,5.1,8.8),(20.5,-1.4,3.4,7.6),(21.25,-1.15,2.7,6.7)],'skin',20)
# Dark palate is visible only when the original jaw opens.
loft('recessed upper palate',[(1,3.0,.32,8.4),(7,3.6,.3,8.6),(15,2.85,.22,7.3),(20.8,1.6,.18,6.1)],'mouth',14)
for sign in (-1,1):
 plate('angular cheek shield',[(-4,-1,sign*11),(0,-7,sign*10.8),(6,-6.7,sign*10.7),(7,-1,sign*11.1),(1,2.4,sign*11.6)],.5,'scute',(0,0,sign))
 orb('sunken eye socket',(5,-6.9,sign*10.2),(3.45,2.1,.52),'groove',14,8)
 orb('amber reptile eye',(5.8,-7.0,sign*10.64),(2.0,1.34,.33),'eye',14,8)
 plate('vertical slit pupil',[(5.85,-8.22,sign*10.99),(6.15,-7,sign*11.03),(5.85,-5.78,sign*10.99),(5.48,-7,sign*11.03)],.04,'pupil',(0,0,sign))
 tube('swept protective brow',[(1,-7.7,sign*9.9),(4,-8.3,sign*10.3),(7,-7.9,sign*10.0),(8.2,-6.5,sign*9.9)],[.6,.65,.6,.25],'scute',10)
 orb('nostril cavity',(18.3,-4.4,sign*6.1),(1.6,.25,.84),'groove',12,7)
 tube('raised nostril rim',[(16.8,-4.2,sign*6.1),(17.6,-4.6,sign*6.8),(19,-4.2,sign*6.5)],[.3,.38,.22],'scute',8)
 for i in range(7):
  x=3.8+i*2.45;z=sign*(9.3-.14*x);y=3.65-.072*x
  tube('upper curved tooth',[(x,y,z),(x+.3,y+1,z*.97),(x+.1,y+2.5,z*.92)],[.38,.31,.025],'horn',8)
 tube('swept cheek horn',[(-3,-5.5,sign*10),(-4.8,-8.7,sign*12.1),(-6,-11.4,sign*12.7)],[1.5,.9,.03],'horn',10)
for x,y,w in [(1,-11.0,6),(6.1,-9.8,5.5),(11.2,-8,4.7),(16,-6,3.8)]:
 plate('crown scute',[(x-2,y+.35,0),(x,y+.1,-w),(x+3,y+.75,0),(x,y+.1,w)],.5,'scute',(0,-1,0))
context['limb']=6
loft('cupped lower jaw',[(-3.6,.6,1.7,4),(-.5,1.4,2.5,6.1),(6,2.45,2.8,7.6),(14,2.6,2.35,7.25),(20.4,2.3,1.45,5.9),(21,2.1,.85,5.2)],'belly',18)
loft('lower mouth interior',[(.5,-.95,.22,5),(6,-.6,.22,6.8),(14,.35,.21,6.6),(20,.95,.18,4.9)],'mouth',16)
for sign in (-1,1):
 tube('lower jaw lip',[(-2,.2,sign*4.3),(5,.1,sign*7.6),(14,1.0,sign*7.1),(20.5,1.1,sign*5.5)],[.4,.5,.4,.25],'skin',9)
 for i in range(6):
  x=4+i*2.7;z=sign*(7.6-.09*x);y=.3+.038*x
  tube('lower interlocking tooth',[(x,y,z),(x+.2,y-.9,z*.95),(x+.15,y-1.9,z*.9)],[.3,.25,.025],'horn',8)
# [TIME] | [INFO] | Dodongo.Author | One continuous torso carries separate dorsal scutes and a warm exposed belly.
context['limb']=16
loft('continuous low torso',[(-1.7,0,5.4,9.1),(0,.7,7.5,11.8),(5,1.3,8.3,11.9),(10,.9,7.4,10.5),(15,.3,6.3,7.6),(19.5,0,4.25,5.2),(20,0,3.8,4.6)],'skin',20)
for x,y,z in [(0,8.2,9.2),(4.2,9.7,9.4),(8.5,8.85,8.1),(12.6,7.9,6.8),(16.5,6.25,5.0)]:
 plate('overlapping dorsal shield',[(x-2,y-1.2,0),(x-1,y-2.2,-z),(x+2.6,y-2.5,-z*.72),(x+3.5,y-1.5,0),(x+2.6,y-2.5,z*.72),(x-1,y-2.2,z)],.85,'scute',(0,1,0))
 tube('dorsal spine',[(x+.5,y-.2,0),(x+1.6,y+2,0),(x+3.2,y+3.0,0)],[.9,.65,.025],'horn',9)
for sign in (-1,1):
 for x in [2.0,6.3,10.7,14.6]:
  z=sign*(11.7-.016*x*x)
  plate('lateral diamond scale',[(x-1.8,.5,z),(x,.3,z+sign*.3),(x+2.1,2.2,z),(x,4.1,z-sign*.7)],.3,'scute',(0,0,sign))
for x in [1,4.5,8,11.5,15]:
 y=-6.7+max(0,x-6)*.16;w=9.8-max(0,x-5)*.4
 plate('ventral segmented pad',[(x-1.6,y,-w*.7),(x+1.5,y,-w*.7),(x+1.8,y-.2,0),(x+1.5,y,w*.7),(x-1.6,y,w*.7),(x-1.9,y-.2,0)],.25,'belly',(0,-1,0))
# Segment ends overlap about the original joint origins, keeping the original tail sweep envelope.
for limb,length,radius in [(15,12.91,5.65),(14,11.89,4.05),(13,16.5,2.5)]:
 context['limb']=limb
 end=.18 if limb==13 else radius*.64
 loft('tapered flexible tail',[(-2.2,0,radius*.86,radius*.9),(0,0,radius,radius),(length*.45,.3,radius*.9,radius*.8),(length*.85,.12,end*1.2,end),(length+1.0,0,end,end*.8)],'skin',16)
 for x in [1.0,length*.43,length*.73]:
  r=radius*(1-.45*x/length)
  plate('tail dorsal scute',[(x-1.5,r-.2,0),(x,r*.6,-r*.72),(x+2.0,r-.45,0),(x,r*.6,r*.72)],.35,'scute',(0,1,0))
  tube('tail ridge thorn',[(x,r,0),(x+1.0,r+1.7,0),(x+2.5,r+2.3,0)],[.6,.45,.02],'horn',8)
 for sign in (-1,1):
  tube('tail ventral seam',[(-1,-radius*.62,sign*radius*.5),(length*.5,-radius*.5,sign*radius*.42),(length,-end*.6,sign*end*.5)],[.18,.13,.06],'belly',7)
# [TIME] | [INFO] | Dodongo.Author | Two broad weight-bearing legs retain the original crouched gait, ankle orientation and foot effects.
for upper,shin,foot,sign in [(23,22,21,1),(30,29,28,-1)]:
 context['limb']=upper
 loft('muscular thigh',[(-.4,0,2.8,2.5),(2.8,.35,3.5,3.3),(7.5,.2,4.15,3.6),(12.5,0,3.0,2.6),(15.8,0,2.5,2.3)],'skin',14)
 for x in [3,7.5,11.5]:plate('thigh plate',[(x-1.8,-.8,3.1),(x,-2.5,3),(x+2,-.3,3),(x,2,3.3)],.35,'scute',(0,0,1))
 context['limb']=shin
 loft('tapered shank',[(-.4,0,2.75,2.5),(3,0,3.25,2.8),(8,0,2.45,2.4),(13,0,2.1,1.9),(16.8,0,1.9,1.85)],'skin',14)
 tube('raised shin ridge',[(0,-1.6,2.3),(5,-1.8,2.6),(12,-1.1,2),(16,-.8,1.6)],[.42,.48,.33,.17],'scute',9)
 context['limb']=foot
 # Model in a canonical grounded frame and inverse-transform into the original ankle joint.
 pose=Matrix(rig['animations']['gDodongoWaitAnim'][0][str(foot)])
 basis=pose.to_3x3().transposed() @ Matrix(((0,0,sign),(0,1,0),(1,0,0)))
 old_mesh=mesh
 def mesh(name,points,faces,material='skin',smooth=False,inks=None):
  return old_mesh(name,[tuple(basis @ Vector(p)) for p in points],faces,material,smooth,inks)
 loft('broad grounded foot',[(-3.0,-.7,1.0,1.8),(0,-.9,1.6,3.4),(4,-1.1,1.9,3.6),(6.2,-1.35,1.3,3.0)],'skin',14)
 # [TIME] | [INFO] | Dodongo.Author | Canonical X points forward, Y points up, and Z spans the splayed toes.
 for lateral in [-2.4,0,2.4]:
  tube('splayed toe',[(1,-1.1,lateral),(4.8,-2.0,lateral*1.2),(7.8,-2.1,lateral*1.3)],[1.35,1.0,.7],'skin',10)
  tube('curved toe claw',[(6.5,-1.7,lateral*1.3),(8.3,-1.6,lateral*1.34),(9.8,-2.5,lateral*1.33)],[.7,.55,.02],'horn',9)
 mesh=old_mesh
scene.frame_start=1;scene.frame_end=len(rig['poses']);scene.render.fps=20
scene.frame_set(366)
scene.world=bpy.data.worlds.get('Dodongo Studio') or bpy.data.worlds.new('Dodongo Studio');scene.world.use_nodes=True
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.export_scene.gltf(filepath=str(ROOT/'dodongos.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
print('Exported',len(parts),'native Dodongo limb pieces.')
