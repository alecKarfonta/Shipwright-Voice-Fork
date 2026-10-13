"""Native MadScrub art. Run in Blender or through the local MCP.
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
rest=rig['animations']['gDekuNutsStandAnim'][0]
rest['0']=[[int(i==j) for j in range(4)] for i in range(4)]
for pose in rig['poses']:pose['0']=rest['0']
connections=[]
scene=bpy.data.scenes.get('ARPG MadScrub Workshop') or bpy.data.scenes.new('ARPG MadScrub Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['bark','edge','recess','leaf','vein','rust','eye']
PALETTES=[[(100,112,57),(149,148,80),(39,48,23),(169,74,24),(229,149,43),(114,45,21),(255,143,21)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'MadScrub {sp} {name}') or bpy.data.materials.new(f'MadScrub {sp} {name}')
  mat.use_nodes=True;mat.diffuse_color=(*[v/255 for v in rgb],1)
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
  node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Roughness'].default_value=.24 if name in ('eye','iris','pupil') else .48 if name in ('mantle','ridge') else .78
  materials[sp,name]=mat
parts=[];variant=0;context={'limb':7}
def mesh(name,points,faces,material='bark',smooth=False,inks=None):
    limb=context['limb']
    # [TIME] | [INFO] | MadScrub.Author | Sculpt in the native resting world space, then bind each carved piece to its original rigid joint.
    inv=Matrix(rest[str(limb)]).inverted();points=[tuple(inv@Vector(p)) for p in points]
    if limb==13:points=[(x*.8,y,z) for x,y,z in points]
    data=bpy.data.meshes.new(name);data.from_pydata(points,[],faces);data.update()
    for key in MATERIALS:data.materials.append(materials[variant,key])
    for i,p in enumerate(data.polygons):
        p.material_index=MATERIALS.index(inks[i] if inks else material);p.use_smooth=smooth
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    ob=bpy.data.objects.new(f'MadScrub.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='original MadScrub flex rig and native joint connections' ;ob['native_units_per_authoring_unit']=100
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
    # [TIME] | [INFO] | MadScrub.Asset | Closed, smoothly shaded eye/joint surface, not an overlapping body primitive.
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



# [TIME] | [INFO] | MadScrub.Author | Four original rigid joints: pedestal, inset eye, and two independently opening shutters.
def body(name,rings,mat='bark',n=16):
 pts=[]
 for y,rx,rz,cz in rings:
  for i in range(n):
   a=math.tau*(i+.5)/n;pts.append((rx*math.sin(a),y,cz+rz*math.cos(a)))
 faces=[tuple(reversed(range(n)))]
 for j in range(len(rings)-1):
  for i in range(n):faces.append((j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i))
 faces.append(tuple((len(rings)-1)*n+i for i in range(n)))
 return mesh(name,pts,faces,mat)
def ring(name,center,rx,ry,axis='y',mat='bronze',width=.3,n=24):
 pts=[]
 for i in range(n+1):
  a=math.tau*i/n;x=rx*math.cos(a);y=ry*math.sin(a)
  pts.append((center[0]+x,center[1]+(0 if axis=='y' else y),center[2]+(y if axis=='y' else 0)))
 return tube(name,pts,[width]*len(pts),mat,6)
def panel(name,points,z,depth,mat):return plate(name,[(x,y,z) for x,y in points],depth,mat)

# [TIME] | [INFO] | MadScrub.Author | The native silhouette remains a mossy wooden seed under two separately animated leaf fans.
context['limb']=4
body('faceted moss bark seed',[(8.7,5.7,5.6,-.5),(11,9.2,8.7,.2),(17,11.3,10.5,.2),(24,10.1,9.9,-.5),(28.8,5.4,5.3,-2)],n=18)
for i in range(18):
 a=math.tau*i/18
 path=[(rx*math.sin(a),y,cz+rz*math.cos(a)) for y,rx,rz,cz in [(10,7.5,7.8,0),(14,10.5,10,.2),(19,11.35,10.6,0),(25,9,9,-.8),(28,5.8,5.8,-1.8)]]
 tube('long bark fissure',path,[.16,.26,.22,.23,.1],'recess',5)
 if i%2==0:
  path=[(p[0]+.22*math.sin(a),p[1],p[2]+.22*math.cos(a)) for p in path]
  tube('split bark ridge',path,[.2,.37,.33,.28,.08],'edge',5)
# Narrow angular eye sockets are recessed beneath the leaf brow.
for sign in (-1,1):
 points=[(sign*x,y,11.0) for x,y in [(2.5,22.6),(8,24),(8.2,21),(3,19.8)]]
 plate('carved eye socket',points,.35,'recess')
 points=[(sign*x,y,11.5) for x,y in [(3.5,22.0),(6.8,22.8),(6.8,21.2),(3.7,20.5)]]
 plate('small amber eye',points,.15,'eye')
 tube('bark brow',[(sign*2.2,23.5,10.7),(sign*5.5,24.8,10.5),(sign*8.8,24.9,8.8)],[.65,.8,.3],'edge',6)
# Snout is an open-ended fluted tube. Every ring stays on the native squash joint.
context['limb']=7
start=Vector((0,16,7.3));end=Vector((0,13.1,17.0));forward=(end-start).normalized();u=Vector((1,0,0));v=forward.cross(u)
pts=[];n=20
for center,rx,ry in [(start,4.6,4.25),(start.lerp(end,.65),4.1,3.8),(end,4.45,4.0),(end-forward*.12,3.05,2.8),(start+forward*.8,2.8,2.6)]:
 for i in range(n):
  t=math.tau*i/n;pts.append(tuple(center+u*math.cos(t)*rx+v*math.sin(t)*ry))
faces=[];inks=[]
for j in range(4):
 for i in range(n):
  faces.append((j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i));inks.append('recess' if j==3 else 'edge' if j==2 else 'bark')
faces.append(tuple(reversed([4*n+i for i in range(n)])));inks.append('recess')
mesh('hollow fluted wooden snout',pts,faces,'bark',False,inks)
for i in range(10):
 t=math.tau*i/10;rad=u*math.cos(t)+v*math.sin(t)
 tube('snout growth line',[tuple(start+rad*4.1),tuple(start.lerp(end,.6)+rad*3.9),tuple(end-forward*.2+rad*4.1)],[.12,.17,.12],'recess',5)
# Broad folded blades with serrated edges, a raised midrib, and integrated color bands.
def leaf(name,path,widths,mat='leaf'):
 path=[Vector(p) for p in path];pts=[];ns=len(path)
 for k,p in enumerate(path):
  tangent=(path[min(k+1,ns-1)]-path[max(0,k-1)]).normalized();side=tangent.cross(Vector((0,0,1)) if abs(tangent.z)<.8 else Vector((0,1,0))).normalized()
  ridge=side.cross(tangent).normalized()
  for f in (-1,-.5,0,.5,1):pts.append(tuple(p+side*(widths[k]*f)+ridge*(widths[k]*.24*(1-abs(f)))))
 faces=[];inks=[]
 for k in range(ns-1):
  for j in range(4):faces.append((k*5+j,k*5+j+1,(k+1)*5+j+1,(k+1)*5+j));inks.append('vein' if j==1 else mat if j!=3 else 'rust')
 # Duplicate a shallow underside with reversed winding so spinning leaves retain their surface.
 length=len(pts);pts.extend([(x,y-.2,z) for x,y,z in pts]);faces+= [tuple(i+length for i in reversed(f)) for f in faces];inks+=['rust']*len(inks)
 for side in (0,4):
  for k in range(ns-1):faces.append((k*5+side,(k+1)*5+side,(k+1)*5+side+length,k*5+side+length));inks.append('rust')
 return mesh(name,pts,faces,mat,False,inks)
context['limb']=10
# Front opening exposes the eyes and snout; side/back leaves drape from the original mantle pivot.
for i,a in enumerate([-.95,-1.5,-2.05,-2.6,math.pi,2.6,2.05,1.5,.95]):
 sx,sz=math.sin(a),math.cos(a)
 leaf('drooping mantle blade',[(sx*2,29,-4+sz*2),(sx*10,31,-4+sz*9),(sx*18,23,-4+sz*18),(sx*21,12,-4+sz*22),(sx*20,1,-4+sz*22)],[.25,5.2,6.1,3.4,.12])
for sign in (-1,1):
 leaf('short brow leaf',[(sign*1,29,-2),(sign*6,29,3),(sign*11,27,7),(sign*13,23,9)],[.15,3.5,4.0,.1])
context['limb']=13
for x,height,back in [(-11,47,-15),(-6,53,-20),(0,56,-27),(6,53,-20),(11,47,-15)]:
 leaf('crown spear leaf',[(x*.1,30,-5),(x*.45,37,-7),(x*.8,height-8,back*.7),(x,height,back)],[.2,4.3,4.8,.08])
context['limb']=14
body('root heart',[(1,3.5,3.5,0),(3,6.4,6,0),(8,7.5,7.4,0),(13,5.8,5.8,0),(16,2.8,2.8,0)],n=14)
for i in range(7):
 a=math.tau*i/7;tube('root heart grain',[(math.sin(a)*r,y,math.cos(a)*r) for y,r in [(2,4.5),(6,7.4),(10,7.0),(14,4.8)]],[.18,.22,.22,.12],'edge',5)
for sign,leg,foot in [(1,19,18),(-1,24,23)]:
 context['limb']=leg
 tube('jointed root shin',[(sign*1.7,6.4,0),(sign*2.2,2.1,.5),(sign*1.9,-3.6,0)],[2.0,1.5,2.2],'bark',10)
 tube('shin grain',[(sign*2.6,5.8,1.5),(sign*3.0,1.0,1.2),(sign*2.8,-3.5,1.8)],[.18,.16,.22],'edge',5)
 context['limb']=foot
 # Flared root toes keep the broad original sideways footprint.
 for toe,z in enumerate((-2.8,0,2.8)):
  tube('branching root toe',[(sign*1.8,-3.3,0),(sign*6.2,-3.5,z*.7),(sign*(12.7 if toe==1 else 11.1),-4.5,z)],[2.3,2.1,.45],'bark',8)
  tube('toe growth ridge',[(sign*3,-1.1,z*.4),(sign*7,-1.7,z*.8),(sign*11,-3.7,z)],[.24,.27,.08],'edge',5)
context['limb']=0
body('dark burrow hollow',[(.15,13,13,0),(.9,11.3,11.3,0)],'recess',24)
for i in range(9):
 a=math.tau*i/9;sx,sz=math.sin(a),math.cos(a)
 leaf('flower rosette',[(sx*7,.8,sz*7),(sx*16,6.5,sz*16),(sx*25,4,sz*25),(sx*32.5,.5,sz*32.5)],[.1,6.5,5.5,.08])
scene.frame_start=1;scene.frame_end=rig['frames'];scene.render.fps=20
scene.frame_set(1+sum(len(v) for k,v in rig['animations'].items() if list(rig['animations']).index(k)<list(rig['animations']).index('gDekuNutsStandAnim')))
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.export_scene.gltf(filepath=str(ROOT/'mad-scrub.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
(ROOT/'connections.json').write_text('[]\n')
print('Exported',len(parts),'native-joint pieces.')
