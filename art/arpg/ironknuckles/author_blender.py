"""Native IronKnuckle art. Run in Blender or through the local MCP.
Only the ARPG IronKnuckle Workshop scene is replaced. Runtime keeps native AI/rigs.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector
ROOT=Path(__file__).resolve().parent
rig=json.loads((ROOT/'native_rig.json').read_text())
rest=rig['animations']['gIronKnuckleWalkAnim'][0]
connections=[]
scene=bpy.data.scenes.get('ARPG IronKnuckle Workshop') or bpy.data.scenes.new('ARPG IronKnuckle Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['plate','edge','gold','mail','leather','cloth','socket','gem']
PALETTES=[[(183,179,143),(236,233,208),(160,115,43),(64,64,57),(64,39,26),(110,36,30),(20,23,22),(104,152,143)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'IronKnuckle {sp} {name}') or bpy.data.materials.new(f'IronKnuckle {sp} {name}')
  mat.use_nodes=True;mat.diffuse_color=(*[v/255 for v in rgb],1)
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
  node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Roughness'].default_value=.24 if name in ('eye','iris','pupil') else .48 if name in ('mantle','ridge') else .78
  materials[sp,name]=mat
parts=[];variant=0;context={'limb':7}
def mesh(name,points,faces,material='plate',smooth=False,inks=None):
    limb=context['limb']
    data=bpy.data.meshes.new(name);data.from_pydata(points,[],faces);data.update()
    for key in MATERIALS:data.materials.append(materials[variant,key])
    for i,p in enumerate(data.polygons):
        p.material_index=MATERIALS.index(inks[i] if inks else material);p.use_smooth=smooth
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    ob=bpy.data.objects.new(f'IronKnuckle.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='original Iron Knuckle flex rig and native joint connections' ;ob['native_units_per_authoring_unit']=100
    for f,pose in enumerate(rig['poses']):
        ob.matrix_world=Matrix.Translation((variant*65,0,0)) @ axis @ Matrix(pose[str(limb)])
        ob.keyframe_insert(data_path='location',frame=f+1)
        ob.keyframe_insert(data_path='scale',frame=f+1)
        ob.rotation_mode='QUATERNION';ob.keyframe_insert(data_path='rotation_quaternion',frame=f+1)
    parts.append(ob);return ob

def loft(name,rings,mat='plate',sections=12,striped=False):
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

def tube(name,path,radii,mat='plate',sides=7):
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
    ob=mesh(name,converted+child_points,[(j,(j+1)%n,n+(j+1)%n,n+j) for j in range(n)],'mail',True)
    ob['arpg_parent_joint']=parent;ob['arpg_parent_ring_points']=converted;ob['arpg_child_ring_points']=child_points
    connections.append({'variant':variant,'child':limb,'parent':parent,'parent_points':parent_points,'child_points':child_points})
    return ob

# [TIME] | [INFO] | IronKnuckle.Author | Rigid detachable chest plates stay in their original limb spaces; flexible underlayers bridge native bending joints.
def shell(name,rings,front=True,mat='plate',n=18):
 pts=[];sgn=1 if front else -1
 for x,cy,ry,rz in rings:
  for i in range(n+1):
   a=-math.pi/2+math.pi*i/n;pts.append((x,cy+sgn*ry*math.cos(a),rz*math.sin(a)))
 fs=[]
 for j in range(len(rings)-1):
  for i in range(n):fs.append((j*(n+1)+i,j*(n+1)+i+1,(j+1)*(n+1)+i+1,(j+1)*(n+1)+i))
 ob=mesh(name,pts,fs,mat,True)
 # Thickness is authored explicitly, avoiding runtime modifiers.
 inner=[(x,y-sgn*.24,z*.986) for x,y,z in pts];k=len(pts);faces=list(fs)+[tuple(k+i for i in reversed(f)) for f in fs]
 edges=[(j,j+1) for j in range(n)]+[((len(rings)-1)*(n+1)+j,(len(rings)-1)*(n+1)+j+1) for j in range(n)]
 edges += [(j*(n+1),(j+1)*(n+1)) for j in range(len(rings)-1)]+[(j*(n+1)+n,(j+1)*(n+1)+n) for j in range(len(rings)-1)]
 faces.extend((a,b,k+b,k+a) for a,b in edges)
 parts.remove(ob);bpy.data.objects.remove(ob,do_unlink=True)
 return mesh(name,pts+inner,faces,mat,True)

def rim(name,x,cy,ry,rz,mat='gold',front=True):
 sign=1 if front else -1
 return tube(name,[(x,cy+sign*ry*math.cos(a),rz*math.sin(a)) for a in [-math.pi/2+j*math.pi/12 for j in range(13)]],[.16]*13,mat,5)

def rivet(name,p,mat='gold',size=.32):return orb(name,p,(size,size,size),mat,8,5)
# Pelvis and articulated leg sockets.
context['limb']=2
pelvis=[(1.3,0,3.2,6.5),(5,0,4.1,7.7),(9.0,0,4.3,7.8),(11,0,3.8,6.8)]
loft('articulated mail hips',pelvis,'mail',16)
for x in (2.6,5.2,7.7,10):rim('hip plate lip',x,-.1,4.4,7.9,'edge',False)
for thigh,shin,foot,sign in [(3,4,5,-1),(6,7,8,1)]:
 context['limb']=thigh
 rings=[(-.6,0,3.9,4.0),(3,0,4.25,4.1),(8,0,3.7,3.7),(15.5,0,2.75,2.8),(18.6,0,2.35,2.5)]
 loft('mail cuisse underlayer',rings,'mail',14)
 parent=[(8.8+math.cos(j*math.tau/12)*2.8,math.sin(j*math.tau/12)*3.8,sign*4.2) for j in range(12)]
 # Use nearest ring correspondence to avoid a twisted hip connection.
 converted=[Matrix(rest[str(thigh)]).inverted()@Matrix(rest['2'])@Vector(p) for p in parent];child=ring(*rings[0])
 choices=[(sum((converted[(j+k)%12]-Vector(child[j])).length_squared for j in range(12)),k) for k in range(12)];shift=min(choices)[1];parent=parent[shift:]+parent[:shift]
 bridge('mail hip connection',2,parent,child)
 shell('fluted thigh plate',[(1.4,-.2,4.0,3.9),(5.5,-.2,4.45,3.6),(12.9,-.2,3.4,2.8),(15.7,-.2,2.8,2.4)],False)
 for z in (-1.5,0,1.5):tube('cuissart embossed rib',[(2,-4.1,z),(7,-4.5,z*.85),(14,-3.15,z*.7)],[.12,.19,.1],'edge',6)
 end=ring(*rings[-1]);context['limb']=shin
 rings=[(-.3,0,2.4,2.5),(2.6,.2,3.45,3.6),(7,0,2.6,2.55),(12.7,0,1.7,2.0)]
 loft('closed articulated greave',rings,'plate',16);bridge('mail knee connection',thigh,end,ring(*rings[0]))
 plate('raised knee shield',[(-1.1,-2.6,0),(1,-3.9,-2.5),(4,-3.65,-2),(5.3,-3.0,0),(4,-3.65,2),(1,-3.9,2.5)],.4,'gold',(0,-1,0))
 tube('greave central ridge',[(4,-3.1,0),(8,-2.6,0),(12.5,-1.75,0)],[.22,.17,.11],'edge',7)
 for x in (5.3,10.8):rim('greave retaining band',x,0,2.85 if x<6 else 2.0,2.8 if x<6 else 2.15,'gold',False)
 context['limb']=foot
 boot=[(-2.1,0,1.5,2.0),(.4,1,2.6,2.8),(5,2.0,1.9,3.1),(10.2,2.55,1.15,2.85),(12.1,2.65,.8,2.0)]
 loft('segmented broad sabaton',boot,'plate',16)
 for x,cy,ry,rz in [(1,1,2.5,2.85),(4,1.8,2.2,3.15),(7,2.4,1.6,3.1),(10,2.6,1.25,2.85)]:rim('sabatons overlapping edge',x,cy,ry,rz,'edge',False)
# Arm underlayers and overlapping vambrace shells.
for upper,fore,hand in [(14,15,18),(19,20,21)]:
 context['limb']=upper
 rings=[(-1.8,0,2.2,2.4),(.5,0,3.1,3.0),(4.0,0,3.0,2.9),(7.6,0,2.55,2.5)]
 loft('padded upper arm',rings,'mail',14);end=ring(*rings[-1]);context['limb']=fore
 rings=[(-.2,0,2.65,2.6),(2.7,0,3.35,3.4),(7.6,0,2.5,2.6),(10.5,0,2.1,2.4)]
 loft('layered closed vambrace',rings,'plate',16);bridge('flexible elbow',upper,end,ring(*rings[0]))
 for x,rad in [(1.7,3.25),(4.1,3.2),(7.4,2.8),(10.3,2.3)]:rim('vambrace brass rim',x,0,rad,rad,'gold',False)
 for sign in (-1,1):rivet('vambrace rivet',(4,-2.0,sign*2.6),size=.27)
 end=ring(*rings[-1]);context['limb']=hand
 rings=[(-.3,0,2.0,2.35),(2.7,0,2.4,3.0),(6.0,-.1,2.0,2.85),(7.3,-.1,1.2,2.1)]
 loft('forged gauntlet palm',rings,'plate',14);bridge('mail wrist connection',fore,end,ring(*rings[0]))
 for j in range(4):
  z=-2.1+j*1.4;tube('curled individual finger',[(4.3,-1.6,z),(6.7,-1.6,z),(8,0,z),(6.9,1.25,z)],[.65,.75,.69,.48],'edge',8)
 tube('opposing gauntlet thumb',[(1.7,-.6,-2.6),(3,-1.8,-3.6),(5,-1.6,-3.6)],[.85,.83,.6],'gold',8)
# Native independently articulated pauldrons and their lower lames.
for limb,sign in [(22,1),(24,-1)]:
 context['limb']=limb
 # Local X points outward, Y upward; this is a tapered shoulder dome.
 loft('broad domed pauldron',[(-3.8,0,3.0,5.7),(-1.4,1.3,5.6,7.8),(2.5,1.1,6.0,8.0),(7.2,-1,4.6,7.1),(10.3,-2.3,2.6,5.4)],'plate',20)
 for x,cy,ry,rz in [(-2,1.2,5.4,7.6),(6,-.4,5.0,7.6),(9.3,-2,3.3,6)]:rim('pauldron rolled edge',x,cy,ry,rz,'gold',True)
 for z in (-4.6,0,4.6):rivet('pauldron gold stud',(4,5.5-abs(z)*.27,z),size=.4)
 plate('pauldron crest',[(.4,6.4,0),(2.5,6.85,-1.5),(5.5,5.5,0),(2.5,6.85,1.5)],.28,'gold',(0,1,0))
for limb in (23,25):
 context['limb']=limb
 for x,cy,ry,rz in [(0,-2.0,1.2,4.7),(3,-2.7,1.3,4.4),(5.7,-3.0,1.15,3.7)]:loft('overlapping shoulder lame',[(x,cy,ry,rz),(x+2.4,cy-.4,ry*.9,rz*.88)],'edge',12)
# Chest armor is two separate native debris pieces.
front=[(-4,0,5.7,7.8),(1.5,0,8.8,11.7),(9.5,0,11.0,13),(16.7,0,9.4,12),(22.2,0,4.1,7.8)]
back=[(-2,0,5.7,7.8),(2.5,0,7.8,11.7),(12.2,0,10.9,13),(20.7,0,8.2,11),(26.2,0,3.0,6.5)]
for limb,rings,isfront in [(26,front,True),(27,back,False)]:
 context['limb']=limb;shell('detachable breastplate' if isfront else 'detachable backplate',rings,isfront,n=22)
 for r in [rings[0],rings[1],rings[-1]]:rim('rolled armor border',*r,'gold',isfront)
 sign=1 if isfront else -1
 for side in (-1,1):
  tube('engraved chest sweep',[(1.6,sign*7.7,side*4),(8,sign*10,side*5.6),(15,sign*8.4,side*5.7),(20.5,sign*4.8,side*4.5)],[.15,.17,.15,.12],'edge',7)
  for x,y,z in [(2,5.2,10.5),(9,5.7,11.3),(15,4.9,10.8)]:rivet('chest clasp',(x,sign*y,side*z),size=.32)
 if isfront:
  plate('raised chest shield',[(5,10.5,0),(8.5,11.15,-2.6),(13.5,10.35,-2.6),(16.2,9.4,0),(13.5,10.35,2.6),(8.5,11.15,2.6)],.6,'gold',(0,1,0))
  plate('turquoise inset',[(8.4,11.5,0),(11,11.5,-1.3),(14.2,10.6,0),(11,11.5,1.3)],.2,'gem',(0,1,0))
 else:
  tube('raised back spine',[(0,-6.5,0),(9,-11.2,0),(17,-9.5,0),(24,-4.7,0)],[.32,.42,.37,.2],'gold',8)
# Armor loss exposes a smaller, visibly quilted torso instead of a hollow shell.
context['limb']=28
loft('fitted gambeson',[(-3.8,0,3.5,5.4),(1.0,0,5.6,7.4),(10,0,6.0,7.6),(18,0,4.0,6.1),(21,0,2.0,3.5)],'cloth',16)
for x in (1,4.5,8,11.5,15):
 for side in (-1,1):tube('quilted diagonal seam',[(x-1,4.9,side*4.5),(x+1,6.1,side*1.5),(x+3,5.5,-side*3.3)],[.075]*3,'leather',5)
for x in (0,14.5):rim('gambeson strap',x,0,5.9 if x==0 else 5.0,7.6 if x==0 else 6.9,'leather',True)
context['limb']=29
loft('mail waist under belt',[(-3.4,2.0,3.8,5.3),(0,2.5,4.1,5.8),(4.5,2.3,3.3,5.1)],'mail',14)
# Helmet local X forward, Y down; sculpt the iron hood and angled cheek planes.
context['limb']=12
loft('forged helmet shell',[(-6.4,-6.1,5.2,4.8),(-4.4,-6.2,6.8,6.3),(0,-6.0,7.9,6.8),(5,-6,6.9,6.4),(8.1,-5.7,5.1,4.9)],'plate',20)
plate('recessed visor opening',[(8.18,-9.6,-4.8),(9.8,-9.0,0),(8.18,-9.6,4.8),(8.3,-5.5,4.2),(10.0,-5.6,0),(8.3,-5.5,-4.2)],.04,'socket',(1,0,0))
for side in (-1,1):
 plate('beveled brow',[(8,-11,side*5.2),(10.4,-10.2,0),(10.25,-8.95,0),(8.65,-8.5,side*4.9)],.35,'gold',(1,0,0))
 plate('angular cheek guard',[(8.4,-6.0,side*5.1),(10.2,-5.4,side*1.5),(9.2,-.9,side*2.1),(6.8,-.1,side*4.1)],.4,'plate',(1,0,0))
 tube('lower cheek trim',[(7,-.3,side*4.2),(9.5,-.8,side*2.1),(10.35,-5.1,side*1.5)],[.16,.2,.17],'gold',7)
 for y in (-4.7,-3.4,-2.1):tube('cheek ventilation slit',[(8.9,y,side*3.9),(9.75,y+.3,side*2.4)],[.14,.14],'socket',6)
plate('visor central nose guard',[(10.1,-11,0),(10.6,-9.2,-.6),(11.2,-4.6,-.7),(10.6,-3.7,0),(11.2,-4.6,.7),(10.6,-9.2,.6)],.22,'edge',(1,0,0))
tube('helmet crest ridge',[(-5,-12.6,0),(-2,-14.4,0),(2,-14.0,0),(5,-13.2,0),(8.1,-11.2,0)],[.25,.35,.35,.3,.2],'gold',9)
context['limb']=13
loft('dark helmet lining',[(-2,-5.2,3.7,3.8),(2,-5.2,4.2,4.2),(6,-5.2,3.3,3.5)],'socket',14)
# Axe is a beveled crescent, metal collars and a wrapped haft, in the original weapon limb.
context['limb']=17
tube('long octagonal axe haft',[(4,-.5,-51),(4,-.5,-40),(4,-.5,20)],[.85,.9,1.0],'leather',12)
for z in range(-15,19,3):
 tube('leather grip spiral',[(4+1.06*math.cos(a),-.5+1.06*math.sin(a),z+a/math.tau*2.2) for a in [j*math.tau/12 for j in range(13)]],[.14]*13,'gold',5)
for z in (-49,-30,-23,20):
 tube('forged haft collar',[(4+1.4*math.cos(a),-.5+1.4*math.sin(a),z) for a in [j*math.tau/16 for j in range(17)]],[.32]*17,'edge',6)
# Bevelled blade perimeter stays within the native axe reach.
for side in (-1,1):
 pts=[(4,-.5,-48),(side*7+4,-.5,-50.8),(side*16+4,-.5,-49),(side*18+4,-.5,-42),(side*15+4,-.5,-32),(side*9+4,-.5,-27),(side*8+4,-.5,-35),(4,-.5,-34)]
 if side<0:pts=[(max(-13.8,x),y,z) for x,y,z in pts]
 arc=[]
 for j in range(1,5):
  a,b,c,d=[Vector(pts[k]) for k in (j-1,j,j+1,j+2)]
  for q in range(3):
   t=q/3;arc.append(tuple(.5*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t)))
 arc.append(pts[5]);pts=[pts[0]]+arc+pts[6:]
 plate('crescent axe blade',pts,1.8,'plate',(0,1,0));tube('keen crescent cutting edge',arc,[.18]*len(arc),'edge',7)
 plate('blade gold inlay',[(4+side*5,1.4,-46),(4+side*12,1.4,-43),(4+side*10,1.4,-35),(4+side*5,1.4,-37)],.15,'gold',(0,1,0))
rivet('axe head central rivet',(4,2,-40),'gold',.8)
scene.frame_start=1;scene.frame_end=rig['frames'];scene.render.fps=20;scene.frame_set(258)
scene.world=bpy.data.worlds.get('IronKnuckle Studio') or bpy.data.worlds.new('IronKnuckle Studio');scene.world.use_nodes=True
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.export_scene.gltf(filepath=str(ROOT/'ironknuckles.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
(ROOT/'connections.json').write_text(json.dumps(connections,indent=2)+'\n')
print('Exported',len(parts),'pieces and',len(connections),'native connections.')
