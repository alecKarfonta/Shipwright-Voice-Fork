"""Native Armos art. Run in Blender or through the local MCP.
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
rest=rig['animations']['gArmosRicochetAnim'][-1]
connections=[]
scene=bpy.data.scenes.get('ARPG Armos Workshop') or bpy.data.scenes.new('ARPG Armos Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['stone','edge','shadow','inlay','iron','eye','crack']
PALETTES=[[(113,116,102),(173,170,141),(38,43,39),(87,96,78),(95,93,88),(89,84,67),(52,59,51)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'Armos {sp} {name}') or bpy.data.materials.new(f'Armos {sp} {name}')
  mat.use_nodes=True;mat.diffuse_color=(*[v/255 for v in rgb],1)
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
  node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Roughness'].default_value=.24 if name in ('eye','iris','pupil') else .48 if name in ('mantle','ridge') else .78
  materials[sp,name]=mat
parts=[];variant=0;context={'limb':7}
def mesh(name,points,faces,material='stone',smooth=False,inks=None):
    limb=context['limb']
    # [TIME] | [INFO] | Armos.Author | Sculpt in the native resting world space, then bind each carved piece to its original rigid joint.
    inv=Matrix(rest[str(limb)]).inverted();points=[tuple(inv@Vector(p)) for p in points]
    data=bpy.data.meshes.new(name);data.from_pydata(points,[],faces);data.update()
    for key in MATERIALS:data.materials.append(materials[variant,key])
    for i,p in enumerate(data.polygons):
        p.material_index=MATERIALS.index(inks[i] if inks else material);p.use_smooth=smooth
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    ob=bpy.data.objects.new(f'Armos.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='original Armos flex rig and native joint connections' ;ob['native_units_per_authoring_unit']=100
    for f,pose in enumerate(rig['poses']):
        ob.matrix_world=Matrix.Translation((variant*49,0,0)) @ axis @ Matrix(pose[str(limb)])
        ob.keyframe_insert(data_path='location',frame=f+1)
        ob.keyframe_insert(data_path='scale',frame=f+1)
        ob.rotation_mode='QUATERNION';ob.keyframe_insert(data_path='rotation_quaternion',frame=f+1)
    parts.append(ob);return ob

def loft(name,rings,mat='stone',sections=12,striped=False):
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

def tube(name,path,radii,mat='stone',sides=7):
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
    # [TIME] | [INFO] | Armos.Asset | Closed, smoothly shaded eye/joint surface, not an overlapping body primitive.
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



# [TIME] | [INFO] | Armos.Author | Keep the horned idol silhouette, raised buckler and short stone sword within the original animated envelope.
def body_loft(name,rings,mat='stone',n=20):
 pts=[]
 for y,rx,rz,cz in rings:
  for i in range(n):
   a=i*math.tau/n;pts.append((rx*math.sin(a),y,cz+rz*math.cos(a)))
 faces=[tuple(reversed(range(n)))]
 for j in range(len(rings)-1):
  for i in range(n):faces.append((j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i))
 faces.append(tuple((len(rings)-1)*n+i for i in range(n)))
 return mesh(name,pts,faces,mat,False)
def panel(name,outline,z,depth,mat='stone'):
 ob=plate(name,[(x,y,z) for x,y in outline],depth,mat)
 bpy.context.view_layer.objects.active=ob
 bevel=ob.modifiers.new('Worn carved bevel','BEVEL');bevel.width=.22;bevel.segments=2;bevel.limit_method='ANGLE'
 bpy.ops.object.modifier_apply(modifier=bevel.name)
 bm=bmesh.new();bm.from_mesh(ob.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.02);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.02);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(ob.data);bm.free()
 return ob
def incision(name,path,width=.45,mat='crack'):
 return tube(name,path,[width]*len(path),mat,5)
def horn(name,sign):
 centers=[(sign*14,58,-1),(sign*20,62,-2),(sign*21,68,-2),(sign*20,74,-2)]
 return tube(name,centers,[5.5,4.3,2.8,.1],'stone',8)
context['limb']=4
body_loft('carved idol core',[(12.5,10,8,1),(16,13,11,1),(25,18,14,1),(39,20,15,1),(51,19,15,1),(59,15,12,0),(64,8,7,-1),(65,2,2,-1)])
for sign in (-1,1):
 horn('upturned carved horn',sign)
 # Broad cheek and temple planes articulate the silhouette instead of round overlays.
 panel('beveled temple',[(sign*10,57),(sign*15,57),(sign*19,50),(sign*18,41),(sign*13,44)],10,4,'edge')
 panel('eye recess',[(sign*3,53),(sign*12,54),(sign*12,50),(sign*5,49)],16.2,1,'shadow')
 panel('inset angular eye',[(sign*4.5,52),(sign*10.8,52.6),(sign*10,50.8),(sign*5.5,50.1)],17.15,.3,'eye')
 panel('heavy carved brow',[(sign*2,54),(sign*13,56),(sign*15,53.5),(sign*11,53.5),(sign*3,52.5)],17,2.5,'stone')
 panel('cheek ridge',[(sign*5,48),(sign*11,49),(sign*15,45),(sign*11,42),(sign*7,44)],16.4,2,'stone')
 incision('temple carving',[(sign*14,52,14.8),(sign*16,48,14.9),(sign*14.7,43,15.4)],.42,'inlay')
 # Hairline fissures live on the stone planes and share the body's native joint.
 incision('weathered brow fissure',[(sign*4,60,10.8),(sign*7,57.5,11.9),(sign*6.3,55.5,13.2)],.18)
 panel('lower torso panel',[(sign*2,37),(sign*12,37),(sign*14,27),(sign*10,20),(sign*3,21)],13,.8,'inlay')
 incision('torso chevron',[(sign*4,34,15),(sign*10,30,15.8),(sign*5,26,15)],.45,'edge')
 # Shoulder pegs fill the native independent-arm sockets.
 orb('stone shoulder socket',(sign*9,23,16),(4.8,5.3,4),'shadow',12,8)
# A projecting central nose and an inset, tooth-cut grimace.
mesh('angular nose',[(0,57,17),(-3,51,18),(3,51,18),(-3.3,46,19),(3.3,46,19),(0,48,24)],[(0,1,5),(0,5,2),(1,3,5),(5,4,2),(3,4,5),(0,2,4,3,1)],'edge')
panel('recessed grimace',[(-10,44),(-6,45),(0,43.8),(6,45),(10,44),(8,39),(-8,39)],16.1,1,'shadow')
for x in (-7.5,-4.5,-1.5,1.5,4.5,7.5):
 panel('carved upper tooth',[(x-1.1,44),(x+1.1,44),(x+.7,41.8),(x-.7,41.8)],17.3,.7,'edge')
 incision('mouth lower lip',[(-9,39,16.9),(-5,37.5,17.5),(5,37.5,17.5),(9,39,16.9)],.85,'stone')
# Raised crown crest, inset seams and side bands.
panel('forehead crest',[(-5,62),(0,65),(5,62),(3,56),(0,54),(-3,56)],11.5,2,'edge')
incision('crest channel',[(0,62.5,14),(0,57,16.2)],.5,'inlay')
for y,rx,rz in [(19,14.5,12),(35,20.4,15.5),(57,17,13.5)]:
 path=[(rx*math.sin(i*math.tau/24),y,1+rz*math.cos(i*math.tau/24)) for i in range(25)]
 tube('carved circumferential band',path,[.65]*25,'edge',6)
# Native base rocks independently when recoiling and hopping.
context['limb']=13
body_loft('faceted octagonal pedestal',[(.15,13.6,13.1,1),(1.6,15.4,14.8,1),(6.7,15.4,14.8,1),(9,13.8,13.3,1),(13,10.5,10.2,1),(16.2,9.3,8.6,1)],n=16)
for y,rx,rz in [(1.7,15.5,14.9),(7.4,14.9,14.3)]:
 path=[(rx*math.sin(i*math.tau/16),y,1+rz*math.cos(i*math.tau/16)) for i in range(17)]
 tube('pedestal beveled rim',path,[.45]*17,'edge',6)
for x in (-10,-5,0,5,10):incision('base chisel mark',[(x,2.7,14.4),(x+.3,6,14.4)],.25)
# Left buckler retains the full native shield envelope and a carved central boss.
context['limb']=7
orb('shield wrist',(9,24,18),(4.2,4.3,4.5),'stone',12,8)
shield=[(0.2,35.5),(1.7,42),(9,45.2),(16.5,41.5),(19,34),(16,24),(9,13.5),(2.8,24)]
panel('thick stone buckler',shield,24,2.2)
center=Vector((9,30));rim=[(center+(Vector(p)-center)*.88) for p in shield]
incision('shield bevel outline',[(p.x,p.y,27) for p in rim]+[(rim[0].x,rim[0].y,27)],.65,'edge')
panel('recessed shield field',[(9,40),(15.2,36),(14,27),(9,19),(4,27),(3,36)],26.2,.3,'inlay')
panel('diamond shield boss',[(9,37),(13.8,31),(9,24),(4.2,31)],27,1.1,'edge')
panel('boss inset',[(9,34.5),(11.1,31),(9,27.6),(6.9,31)],28.1,.25,'shadow')
for y in (39.5,21.5):orb('shield pin',(9,y,27.5),(1.1,1.1,.55),'iron',10,6)
incision('shield fracture',[(16,34,27.2),(14,31,28),(15,28,27.5)],.28)
# Right sword: squared grip, beveled guard and short tapered double edge.
context['limb']=10
orb('carved sword hand',(-9,21,20),(4.1,3.5,3.2),'stone',12,8)
panel('sword hilt',[(-10.7,19),(-7.3,19),(-7.4,25),(-10.6,25)],24,1,'iron')
panel('square crossguard',[(-13.4,24),(-4.7,24),(-4.7,26),(-13.4,26)],24,1.2,'edge')
mesh('chiseled sword blade',[(-11.7,26,24),(-6.3,26,24),(-6.8,34,24),(-9,38.3,24),(-11.2,34,24),(-9,27,27),(-9,34,26.3)],[(0,1,5),(1,2,6,5),(2,3,6),(3,4,6),(4,0,5,6),(4,3,2,1,0)],'edge')
incision('sword fuller',[(-9,27.5,27.15),(-9,33,26.8)],.3,'inlay')
scene.frame_start=1;scene.frame_end=rig['frames'];scene.render.fps=20;scene.frame_set(rig['frames'])
scene.world=bpy.data.worlds.get('Armos Studio') or bpy.data.worlds.new('Armos Studio');scene.world.use_nodes=True
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.export_scene.gltf(filepath=str(ROOT/'armos.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
(ROOT/'connections.json').write_text('[]\n')
print('Exported',len(parts),'rigid carved pieces.')
