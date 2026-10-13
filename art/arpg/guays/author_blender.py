"""Native Guay art. Run in Blender or through the local MCP.
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
rest=rig['animations']['gGuayFlyAnim'][0]
connections=[]
scene=bpy.data.scenes.get('ARPG Guay Workshop') or bpy.data.scenes.new('ARPG Guay Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['plumage','coverts','primaries','quill','beak','edge','eye','pupil']
PALETTES=[[(35,28,47),(59,38,78),(44,30,61),(85,60,110),(136,94,45),(195,149,68),(239,98,21),(14,8,15)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'Guay {sp} {name}') or bpy.data.materials.new(f'Guay {sp} {name}')
  mat.use_nodes=True;mat.diffuse_color=(*[v/255 for v in rgb],1)
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
  node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Roughness'].default_value=.24 if name in ('eye','iris','pupil') else .48 if name in ('mantle','ridge') else .78
  materials[sp,name]=mat
parts=[];variant=0;context={'limb':7}
def mesh(name,points,faces,material='plumage',smooth=False,inks=None):
    limb=context['limb']
    # [TIME] | [INFO] | Guay.Author | Sculpt in the native resting world space, then bind each carved piece to its original rigid joint.
    inv=Matrix(rest[str(limb)]).inverted();points=[tuple(inv@Vector(p)) for p in points]
    data=bpy.data.meshes.new(name);data.from_pydata(points,[],faces);data.update()
    for key in MATERIALS:data.materials.append(materials[variant,key])
    for i,p in enumerate(data.polygons):
        p.material_index=MATERIALS.index(inks[i] if inks else material);p.use_smooth=smooth
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    ob=bpy.data.objects.new(f'Guay.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='original Guay flex rig and native joint connections' ;ob['native_units_per_authoring_unit']=100
    for f,pose in enumerate(rig['poses']):
        ob.matrix_world=Matrix.Translation((variant*49,0,0)) @ axis @ Matrix(pose[str(limb)])
        ob.keyframe_insert(data_path='location',frame=f+1)
        ob.keyframe_insert(data_path='scale',frame=f+1)
        ob.rotation_mode='QUATERNION';ob.keyframe_insert(data_path='rotation_quaternion',frame=f+1)
    parts.append(ob);return ob

def loft(name,rings,mat='plumage',sections=12,striped=False):
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

def tube(name,path,radii,mat='plumage',sides=7):
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
    # [TIME] | [INFO] | Guay.Asset | Closed, smoothly shaded eye/joint surface, not an overlapping body primitive.
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




# [TIME] | [INFO] | Guay.Author | A narrow avian body, feathered flight surfaces and hooked beak retain the native silhouette and seven original flex joints.
def body(name,rings,mat='plumage',n=20):
 pts=[]
 for z,cy,rx,ry in rings:
  for i in range(n):
   a=math.tau*i/n;pts.append((rx*math.cos(a),cy+ry*math.sin(a),z))
 faces=[tuple(reversed(range(n)))]
 for j in range(len(rings)-1):
  for i in range(n):faces.append((j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i))
 faces.append(tuple((len(rings)-1)*n+i for i in range(n)))
 return mesh(name,pts,faces,mat,True)
def feather(name,start,end,width,normal,mat='primaries',detail=True):
 a,b=Vector(start),Vector(end);n=Vector(normal).normalized()
 if context['limb']>=3:
  m=Matrix(rest[str(context['limb'])]);inv=m.inverted();n=m.to_3x3()@Vector((0,0,1))
  a,b=inv@a,inv@b;a.z=b.z=1.35 if context['limb'] in (3,5) else .28;a,b=m@a,m@b
 d=(b-a).normalized();u=d.cross(n).normalized();pts=[];rows=5
 for side in (1,-1):
  for j in range(rows):
   t=j/(rows-1);w=width*(.08+.92*math.sin(math.pi*t)**.65);p=a.lerp(b,t);curve=math.sin(math.pi*t)*.09
   for k in (-1,0,1):pts.append(tuple(p+u*(w*k)+n*(side*(.035+(.11 if k==0 else 0)*math.sin(math.pi*t))+curve)))
 faces=[];inks=[]
 for side in (0,1):
  for j in range(rows-1):
   for k in range(2):
    q=(side*rows*3+j*3+k,side*rows*3+j*3+k+1,side*rows*3+(j+1)*3+k+1,side*rows*3+(j+1)*3+k)
    faces.append(q if side==0 else q[::-1]);inks.append(mat)
 for j in range(rows-1):
  for k in (0,2):faces.append((j*3+k,(j+1)*3+k,rows*3+(j+1)*3+k,rows*3+j*3+k));inks.append(mat)
 for j in (0,rows-1):faces.append((j*3,j*3+1,j*3+2,rows*3+j*3+2,rows*3+j*3+1,rows*3+j*3));inks.append(mat)
 ob=mesh(name,pts,faces,mat,True,inks)
 if detail:tube('fine feather rachis',[tuple(a+n*.04),tuple(a.lerp(b,.45)+n*.25),tuple(a.lerp(b,.93)+n*.07)],[.045,.055,.015],'quill',5)
 return ob

def native_web(limb,parent):
 context['limb']=limb;l=next(l for l in rig['rig'] if l['limb']==limb);pts=[];faces=[];parents=[];children=[]
 # Both surfaces inherit the original mixed-joint assignments; thickness does not detach the leading edge.
 for direction in (-1,1):
  for tri in l['triangles']:
   face=[]
   for x,y,z,j in tri:
    p=Matrix(rest[str(j)])@Vector((x/100,y/100,z/100))
    if limb in (3,4,5,6) and p.z<0 and abs(p.x)<15:
     sign=-1 if limb in (3,4) else 1
     edge=Vector((sign*.37,-.21,1.67)) if abs(p.x)<4 else Vector((sign*7.19,5.9,2.55)) if abs(p.x)<8 else Vector((sign*11.4,9.3,1.26))
     p=p.lerp(edge,.52)
    p.y+=direction*.10;face.append(len(pts));pts.append(tuple(p));(parents if j==parent else children).append(tuple(p))
   faces.append(tuple(face if direction==1 else reversed(face)))
 ob=mesh('native feather web connection',pts,faces,'plumage',False)
 inv=Matrix(rest[str(limb)]).inverted();ob['arpg_parent_joint']=parent;ob['arpg_parent_ring_points']=[tuple(inv@Vector(p)) for p in parents];ob['arpg_child_ring_points']=[tuple(inv@Vector(p)) for p in children]
 connections.append({'limb':limb,'parent':parent,'parent_world_points':parents,'child_world_points':children})

def tail_attachment(limb,parent,start,end):
 # [TIME] | [INFO] | Guay.Tail | A shallow feathered root spans the original joint pair so the tail reads as attached in the full downstroke.
 context['limb']=limb;points=[]
 for z,cy,rx,ry in (start,end):
  for i in range(10):
   angle=math.tau*i/10;points.append((rx*math.cos(angle),cy+ry*math.sin(angle),z))
 faces=[tuple(reversed(range(10))),tuple(range(10,20))]
 for i in range(10):faces.append((i,(i+1)%10,(i+1)%10+10,i+10))
 ob=mesh('continuous tail attachment',points,faces,'plumage',True);inv=Matrix(rest[str(limb)]).inverted()
 ob['arpg_parent_joint']=parent;ob['arpg_parent_ring_points']=[tuple(inv@Vector(p)) for p in points[:10]];ob['arpg_child_ring_points']=[tuple(inv@Vector(p)) for p in points[10:]]
 connections.append({'limb':limb,'parent':parent,'parent_world_points':points[:10],'child_world_points':points[10:]})

context['limb']=2
body('tapered feathered torso',[(-11.5,.7,.12,.15),(-9.7,.45,1.25,.9),(-6,.1,2.7,1.55),(-1,-.7,3.25,2.2),(3,-1.2,3.5,2.7),(5.5,-.4,2.7,2.3),(8,-.8,2.85,2.5),(10,-1.9,2.15,1.6),(11,-2.5,1.15,.6)],n=24)
# Upper/lower bill taper into the native down-curved attack point.
body('hooked upper bill',[(9.2,-2.55,1.6,.8),(11.0,-2.8,1.6,.95),(14,-3.2,1.05,.8),(17.4,-4.3,.55,.55),(19,-7.3,.025,.03)],'beak',12)
body('lower bill',[(9.5,-3.7,1.25,.27),(12,-3.9,1.0,.25),(15.4,-4.4,.35,.15),(17.3,-4.9,.025,.025)],'edge',10)
for sign in (-1,1):
 tube('bill seam',[(sign*1.65,-3.4,10.6),(sign*1.1,-3.9,13.6),(sign*.45,-4.8,17.1)],[.045,.05,.015],'pupil',5)
 orb('nostril',(sign*1.51,-2.96,11.1),(.04,.16,.28),'pupil',10,6)
 # Orange eyes and the hooked forehead preserve the original hostile facial cue.
 orb('orange eye',(sign*2.73,-.55,8.6),(.24,.8,.84),'eye',16,10)
 orb('dark pupil',(sign*2.95,-.50,8.83),(.10,.43,.45),'pupil',14,8)
 tube('sloped brow',[(sign*2.4,.25,9.5),(sign*3.02,.46,8.6),(sign*2.66,.75,7.65)],[.18,.23,.10],'plumage',7)
 for k in range(4):
  z=3.8-k*2.6;x=sign*(2.7-.23*k)
  feather('layered flank covert',(x,-.25,z),(x*.74,-.6,z-4.3),.9,(sign*.9,.35,0),'coverts',False)
 for k in range(3):feather('swept cheek',(sign*2.1,-.2-k*.5,7.2),(sign*2.5,-.55-k*.5,4.3),.55,(sign,0,0),'coverts',False)
# Short overlapping mantle feathers break the broad upper body without changing its collision silhouette.
for row,z in enumerate([4,1,-2,-5]):
 for i in (-1,0,1):
  x=i*(1.7-.1*row);feather('mantle feather',(x,1.9-row*.12,z),(x*.88,1.8-row*.2,z-3.7),.95,(0,1,0),'coverts',False)
for sign in (-1,1):
 tube('tucked leg',[(sign*1.6,-2.15,-1),(sign*1.8,-3.45,-.2),(sign*1.7,-3.85,1.5)],[.28,.22,.20],'beak',7)
 for k in range(3):tube('folded talon',[(sign*(1.5+k*.23),-3.85,1.3),(sign*(1.5+k*.23),-4.3,2.2),(sign*(1.5+k*.23),-4.1,2.75)],[.12,.10,.015],'edge',6)
for sign,inner,outer in [(-1,3,4),(1,5,6)]:
 native_web(inner,2);native_web(outer,inner)
 context['limb']=inner
 root=Vector((sign*.5,.1,1.6));elbow=Vector((sign*7.2,5.9,2.55));n=Vector((-sign*.65,.75,0))
 tube('leading scapular edge',[tuple(root),tuple(root.lerp(elbow,.5)),tuple(elbow)],[.45,.48,.26],'plumage',9)
 for i in range(6):
  t=(i+.6)/7;p=root.lerp(elbow,t);end=p+Vector((sign*.1,-.1,-(5.8+1.8*t)));feather('secondary flight feather',p,end,.78,n,'primaries' if i%2==0 else 'coverts')
 context['limb']=outer
 tip=Vector((sign*18.25,17.5,-3.82));mid=Vector((sign*11.4,9.3,1.26))
 tube('outer leading wing bone',[tuple(elbow),tuple(mid),tuple(tip)],[.30,.25,.025],'plumage',8)
 for i in range(7):
  t=(i+.25)/7;p=elbow.lerp(tip,t);length=5.8*(1-t)+1.1;end=p+Vector((sign*(.7+1.1*t),.25,-length));feather('separated primary feather',p,end,.72-.37*t,n,'primaries' if i%2==0 else 'coverts')
 for i in range(4):
  t=(i+.2)/4;p=elbow.lerp(mid,t)+n*.12;feather('overlapping wing covert',p,p+Vector((sign*.3,0,-2.35)),.6,n,'coverts',False)
native_web(7,2);native_web(8,7)
tail_attachment(7,2,(-9.2,.45,1.5,.65),(-13.2,.72,1.9,.3))
tail_attachment(8,7,(-17.2,.52,2.2,.3),(-20.1,.15,2.1,.22))
context['limb']=7
for i in range(5):
 x=(i-2)*.72;feather('tail root covert',(x,.82,-11.8),(x*1.35,.65,-18.5),.65,(0,1,0),'coverts')
context['limb']=8
for i in range(5):
 x=(i-2)*1.5;feather('long tail feather',(x,.4,-18.5),(x*1.17,-1.7,-23.5+abs(i-2)*.35),.86,(0,1,-.1),'primaries')
scene.frame_start=1;scene.frame_end=rig['frames'];scene.render.fps=20;scene.frame_set(1)
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.export_scene.gltf(filepath=str(ROOT/'guays.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
(ROOT/'connections.json').write_text(json.dumps(connections,indent=2)+'\n')
print('Exported',len(parts),'pieces and',len(connections),'native joint connections.')
