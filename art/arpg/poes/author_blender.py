"""Native Poe art. Run in Blender or through the local MCP.
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
rest=rig['animations']['gPoeFloatAnim'][0]
connections=[]
scene=bpy.data.scenes.get('ARPG Poe Workshop') or bpy.data.scenes.new('ARPG Poe Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['cloth','fold','recess','seam','skin','knuckle','bronze','edge','eye']
PALETTES=[[(151,162,129),(196,203,168),(31,32,42),(92,110,81),(126,143,97),(176,186,129),(103,71,32),(183,141,65),(255,235,161)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'Poe {sp} {name}') or bpy.data.materials.new(f'Poe {sp} {name}')
  mat.use_nodes=True;mat.diffuse_color=(*[v/255 for v in rgb],1)
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
  node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Roughness'].default_value=.24 if name in ('eye','iris','pupil') else .48 if name in ('mantle','ridge') else .78
  materials[sp,name]=mat
parts=[];variant=0;context={'limb':4}
def mesh(name,points,faces,material='cloth',smooth=False,inks=None):
    limb=context['limb']
    # [TIME] | [INFO] | Poe.Author | Sculpt in the native resting world space, then bind each carved piece to its original rigid joint.
    inv=Matrix(rest[str(limb)]).inverted();points=[tuple(inv@Vector(p)) for p in points]
    data=bpy.data.meshes.new(name);data.from_pydata(points,[],faces);data.update()
    for key in MATERIALS:data.materials.append(materials[variant,key])
    for i,p in enumerate(data.polygons):
        p.material_index=MATERIALS.index(inks[i] if inks else material);p.use_smooth=smooth
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    ob=bpy.data.objects.new(f'Poe.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='original Poe flex rig and native joint connections' ;ob['native_units_per_authoring_unit']=100
    for f,pose in enumerate(rig['poses']):
        ob.matrix_world=Matrix.Translation((variant*49,0,0)) @ axis @ Matrix(pose[str(limb)])
        ob.keyframe_insert(data_path='location',frame=f+1)
        ob.keyframe_insert(data_path='scale',frame=f+1)
        ob.rotation_mode='QUATERNION';ob.keyframe_insert(data_path='rotation_quaternion',frame=f+1)
    parts.append(ob);return ob

def loft(name,rings,mat='cloth',sections=12,striped=False):
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

def tube(name,path,radii,mat='cloth',sides=7):
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
    # [TIME] | [INFO] | Poe.Asset | Closed, smoothly shaded eye/joint surface, not an overlapping body primitive.
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




# [TIME] | [INFO] | Poe.Author | Retain the original broad hood, floating torn hem, long arms and small hand-held lamp.
def native_piece(limb,name,mat='cloth',inset=1,bevel=0):
 context['limb']=limb;data=next(l for l in rig['rig'] if l['limb']==limb);pts=[];faces=[]
 for tri in data['triangles']:
  face=[]
  for x,y,z,j in tri:face.append(len(pts));pts.append(tuple(Matrix(rest[str(j)])@Vector((x/100*inset,y/100*inset,z/100*inset))))
  faces.append(tuple(face))
 ob=mesh(name,pts,faces,mat)
 if bevel:
  bm=bmesh.new();bm.from_mesh(ob.data);edges=[e for e in bm.edges if e.is_manifold and e.calc_face_angle(0)>.35]
  bmesh.ops.bevel(bm,geom=edges,offset=bevel,segments=3,affect='EDGES');bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(ob.data);bm.free()
 return ob
# The lower shroud is a single open, folded cloth surface with an uneven hanging edge.
context['limb']=4;n=48;pts=[]
for row in range(9):
 t=row/8
 for i in range(n):
  a=math.tau*i/n;fold=math.cos(a*8+.3)*(.18+.65*t)
  rx=12.3+5*t;rz=13.6+.6*t
  edge=12.2+3.7*math.sin(a*3+.2)-3.3*math.cos(a*4+.4)
  y=39.7*(1-t)+edge*t
  pts.append(((rx+fold)*math.sin(a),y,(rz+fold)*math.cos(a)+1.5))
faces=[];inks=[]
for row in range(8):
 for i in range(n):
  faces.append((row*n+i,row*n+(i+1)%n,(row+1)*n+(i+1)%n,(row+1)*n+i));inks.append('fold' if i%6 in (0,1) else 'cloth')
mesh('hanging folded shroud',pts,faces,'cloth',True,inks)
for i in range(0,n,6):
 a=math.tau*(i+.5)/n;edge=12.2+3.7*math.sin(a*3+.2)-3.3*math.cos(a*4+.4)
 tube('hem thread',[(17.4*math.sin(a),edge+2,14.1*math.cos(a)+1.5),(17.6*math.sin(a+.015),edge-.2,14.3*math.cos(a+.015)+1.5)], [.10,.025],'fold',5)
# The recognizable pointed hood keeps the native facial opening and broad shoulder drape.
context['limb']=5;hood=native_piece(5,'folded draped hood','cloth',1,0)
bm=bmesh.new();bm.from_mesh(hood.data);bmesh.ops.subdivide_edges(bm,edges=list(bm.edges),cuts=3,use_grid_fill=True)
for _ in range(3):bmesh.ops.smooth_vert(bm,verts=[v for v in bm.verts if not v.is_boundary],factor=.35,use_axis_x=True,use_axis_y=True,use_axis_z=True)
bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(hood.data);bm.free()
for f in hood.data.polygons:f.use_smooth=True
for sign in (-1,1):
 tube('folded hood opening',[(sign*9.4,38.4,13.9),(sign*12.6,50.8,12.9),(sign*5.9,63.5,14.6),(sign*.08,64.3,1.0)],[.45,.7,.6,.2],'fold',8)
 tube('long shoulder crease',[(sign*5.5,60,8.5),(sign*13.0,49,7),(sign*19.2,34,2),(sign*22.1,27.9,.6)],[.2,.38,.45,.08],'seam',6)
 tube('back falling fold',[(sign*.5,63,.3),(sign*8.5,53,-10),(sign*13,40,-14),(sign*10,19,-18)],[.2,.35,.4,.1],'fold',6)
 tube('front mantle hem',[(sign*22.6,27.8,.9),(sign*13.2,27.6,17),(sign*6.6,31,19),(0,33.7,20.5)],[.25,.4,.3,.15],'fold',6)
 for k in range(6):
  y=40.5+k*2.4;x=sign*(10.0+(.25 if k<4 else -.5)*(k if k<4 else k-4));z=14
  tube('hood hand stitch',[(x-sign*.27,y,z+.1),(x+sign*.28,y+.3,z+.12)],[.12,.09],'seam',5)
# Thin sleeves follow three different original joints; they do not bridge the elbow with rigid geometry.
def sleeve(limb,name,start,end,r0,r1):
 context['limb']=limb;start,end=Vector(start),Vector(end);axis=(end-start).normalized();u=axis.cross(Vector((0,0,1))).normalized();v=axis.cross(u);pts=[];n=18
 for row in range(7):
  t=row/6;center=start.lerp(end,t);rad=r0*(1-t)+r1*t
  for i in range(n):
   a=math.tau*i/n;r=rad+.25*math.cos(a*6);p=center+r*(math.cos(a)*u+math.sin(a)*v)
   if row==6:p+=axis*(.7*math.cos(a*3))
   pts.append(tuple(p))
 faces=[];inks=[]
 for row in range(6):
  for i in range(n):faces.append((row*n+i,row*n+(i+1)%n,(row+1)*n+(i+1)%n,(row+1)*n+i));inks.append('fold' if i%6==0 else 'cloth')
 mesh(name,pts,faces,'cloth',True,inks)
 for side in (-1,1):
  path=[tuple(start.lerp(end,t)+(r0*(1-t)+r1*t)*u*side) for t in (0,.3,.65,1)]
  tube('sleeve seam',path,[.12,.18,.18,.1],'seam',5)
sleeve(10,'outstretched right sleeve',(11.1,37.9,5.5),(30.0,25.8,-9.2),4.5,2.5)
sleeve(20,'left upper sleeve',(-12.2,38.5,1.9),(-22.0,29,15.6),4.4,3.7)
sleeve(19,'raised left cuff',(-21.9,29.8,13.4),(-13.8,37.2,26.3),3.65,3.0)
# Hands use elongated phalanges and small knuckles, following each original wrist.
context['limb']=9
orb('open palm',(29.1,27.5,-8.3),(3.6,1.7,3.9),'skin',12,8)
for i in range(4):
 x=26.3+i*1.7;z=-10.4+i*.4
 tube('long separated finger',[(x,27,z),(x+4.3,24.9,z-2.2),(x+6.2,23,z-1.1),(x+5.6,22.7,z+.6)],[.85,.78,.55,.16],'skin',7)
 orb('finger knuckle',(x+4.2,24.9,z-2.15),(.8,.65,.8),'knuckle',10,6)
tube('curved thumb',[(28,28.4,-5.3),(31.3,27.3,-3.4),(32.1,25.7,-2.6)],[1.05,.8,.12],'skin',8)
context['limb']=16
orb('lantern gripping palm',(-14.3,36.8,26.6),(3.3,1.8,4.0),'skin',12,8)
for i in range(4):
 x=-17.2+i*1.8
 tube('gripping finger',[(x,36.8,29),(x,35.5,32.2),(x,33.8,31.7),(x,34.0,29.6)],[.78,.7,.55,.2],'skin',7)
 orb('grip knuckle',(x,35.6,31.5),(.8,.7,.85),'knuckle',10,6)
tube('gripping thumb',[(-11.2,37.2,24.2),(-9.4,36.2,27),(-10.7,34.7,28.8)],[1,.8,.2],'skin',7)
# Preserve the black pointed face with two small glowing eyes, rather than lighting the whole hood.
context['limb']=11;native_piece(11,'inset pointed face','recess',.98,0)
for sign in (-1,1):
 orb('small watchful eye',(sign*3.9,50.0,21.85),(1.5,2.2,.42),'eye',16,10)
# Only the metal frame is authored here. The native glowing lantern remains inside it at runtime.
context['limb']=18
m=Matrix(rest['18'])
def lp(x,y,z):return tuple(m@Vector((x,y,z)))
def perimeter(a,r):
 c,ss=math.cos(a),math.sin(a);scale=max(abs(c),abs(ss));return c*r/scale,ss*r/scale
def lring(name,y,r,width):
 pts=[]
 for i in range(33):
  x,z=perimeter(math.tau*i/32,r);pts.append(lp(x,y,z))
 tube(name,pts,[width]*33,'edge',6)
for y,r,w in [(1.8,5.8,.28),(5.8,4.98,.26),(17.4,7.85,.32),(18.5,7.6,.3)]:lring('forged square lantern rim',y,r,w)
for i in range(8):
 a=math.tau*i/8
 path=[]
 for y,r in [(1.8,5.8),(5.8,4.98),(11.5,6.38),(18.0,7.95)]:
  x,z=perimeter(a,r);path.append(lp(x,y,z))
 tube('tapered lantern rib',path,[.22,.22,.24,.25],'bronze',7)
 x,z=perimeter(a,5.8);tube('top crown spoke',[lp(x,1.8,z),lp(x*.45,-.7,z*.45),lp(0,-2.5,0)],[.22,.18,.12],'edge',6)
 x,z=perimeter(a,7.95);tube('base pointed spoke',[lp(x,18,z),lp(x*.55,21.9,z*.55),lp(0,26.45,0)],[.27,.2,.13],'bronze',6)
loop=[lp(2.1*math.cos(math.tau*i/28),-2.7+1.6*math.sin(math.tau*i/28),0) for i in range(29)]
tube('wrought carrying ring',loop,[.24]*29,'bronze',7)
scene.frame_start=1;scene.frame_end=rig['frames'];scene.render.fps=20;scene.frame_set(83)
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.export_scene.gltf(filepath=str(ROOT/'poe.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
(ROOT/'connections.json').write_text('[]\n')
print('Exported',len(parts),'original-rig pieces; native glowing lantern remains inside the frame at runtime.')
