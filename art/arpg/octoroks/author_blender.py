"""Native Octorok art. Run in Blender or through the local MCP.
Only the ARPG Octorok Workshop scene is replaced. Runtime keeps native AI/rigs.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector
ROOT=Path(__file__).resolve().parent
rig=json.loads((ROOT/'native_rig.json').read_text())
scene=bpy.data.scenes.get('ARPG Octorok Workshop') or bpy.data.scenes.new('ARPG Octorok Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['mantle','ridge','mottle','skin','sucker','mouth','eye','iris','pupil','glint']
PALETTES=[[(177,190,170),(212,218,188),(87,115,99),(138,66,58),(213,162,129),(49,29,36),(238,209,126),(192,97,37),(27,24,25),(255,244,207)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'Octorok {sp} {name}') or bpy.data.materials.new(f'Octorok {sp} {name}')
  mat.use_nodes=True;mat.diffuse_color=(*[v/255 for v in rgb],1)
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
  node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Roughness'].default_value=.24 if name in ('eye','iris','pupil') else .48 if name in ('mantle','ridge') else .78
  materials[sp,name]=mat
parts=[];variant=0;context={'limb':4}
def mesh(name,points,faces,material='mantle',smooth=False,inks=None):
    limb=context['limb']
    data=bpy.data.meshes.new(name);data.from_pydata(points,[],faces);data.update()
    for key in MATERIALS:data.materials.append(materials[variant,key])
    for i,p in enumerate(data.polygons):
        p.material_index=MATERIALS.index(inks[i] if inks else material);p.use_smooth=smooth
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    ob=bpy.data.objects.new(f'Octorok.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='native 37-joint rig and original head/snout squash' ;ob['native_units_per_authoring_unit']=100
    for f,pose in enumerate(rig['preview']):
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

# [TIME] | [INFO] | Octorok.Author | Structured mantle, recessed eyes and a hollow elastic nozzle.
context['limb']=5
rings=[(-8,7,7,-1),(-4,15,12,-3),(7,20,17,-5),(19,18,14,-4),(29,12,10,-2),(36,4,4,0),(38,.25,.25,.5)]
pts=[];sectors=32
for y,rx,rz,cz in rings:
 for j in range(sectors):
  a=j*2*math.pi/sectors;flute=1+.025*math.cos(a*8)
  pts.append((rx*math.cos(a)*flute,y,cz+rz*math.sin(a)*flute))
faces=[tuple(reversed(range(sectors)))];inks=['skin']
for k in range(len(rings)-1):
 for j in range(sectors):
  faces.append((k*sectors+j,k*sectors+(j+1)%sectors,(k+1)*sectors+(j+1)%sectors,(k+1)*sectors+j))
  inks.append('skin' if k==0 else 'mantle')
faces.append(tuple((len(rings)-1)*sectors+j for j in range(sectors)));inks.append('ridge')
cap=mesh('tapered fluted mantle',pts,faces,'mantle',True,inks)
# Surface-following pigment patches; colored topology rather than raised beads.
from mathutils.bvhtree import BVHTree
bvh=BVHTree.FromPolygons([v.co for v in cap.data.vertices],[tuple(p.vertices) for p in cap.data.polygons])
for k in range(18):
 angle=k*2.39996;y=5+(k%5)*5.4
 def ring_distance(r):return abs(r[0]-y)
 near=min(rings,key=ring_distance);rx,rz,cz=near[1:]
 center=Vector((rx*math.cos(angle),y,cz+rz*math.sin(angle)))
 normal=Vector((math.cos(angle),.16,math.sin(angle))).normalized();u=normal.cross(Vector((0,1,0))).normalized();v=normal.cross(u)
 ps=[];radius=1.3+(k%3)*.6
 for r in (0,.55,1):
  for j in range(12):
   a=j*2*math.pi/12;query=center+radius*r*(math.cos(a)*u+.72*math.sin(a)*v)
   hit,n,_,_=bvh.find_nearest(query);ps.append(tuple(hit+n*.045))
 fs=[]
 for ring in range(2):
  for j in range(12):fs.append((ring*12+j,ring*12+(j+1)%12,(ring+1)*12+(j+1)%12,(ring+1)*12+j))
 mesh('irregular mantle pigment',ps,fs,'mottle',True)
for sign in (-1,1):
 orb('inset golden eye',(sign*10.6,9.5,9.3),(5.4,5.4,3.5),'eye',20,12)
 orb('burnished iris',(sign*10.6,9.5,12.2),(2.75,3.8,.65),'iris',16,10)
 orb('horizontal ink pupil',(sign*10.6,9.5,12.74),(2.4,.9,.25),'pupil',16,8)
 orb('small wet catchlight',(sign*10.1,10.7,12.89),(.5,.55,.12),'glint',8,6)
 tube('sculpted heavy brow',[(sign*5.7,10.7,10),(sign*7,13.8,11),(sign*11,15.2,9.6),(sign*15,12.7,7.7)],[.8,1.1,1.2,.65],'mottle',10)
 tube('lower orbital fold',[(sign*5.8,8.9,10.1),(sign*7.7,5.5,10.8),(sign*12.7,5.1,9.4),(sign*15.8,8.4,7.8)],[.45,.65,.65,.35],'skin',8)
context['limb']=8
# X is the nozzle direction in this native limb. Its moving aperture stays open.
loops=[(-2,6.1),(0,7.3),(4,6.1),(9,5.5),(13.5,6.8),(14.6,6.4),(14.6,4.9),(12.8,4.8),(8,3.5),(1,2.8)]
ps=[];n=24
for x,r in loops:
 for j in range(n):
  a=j*2*math.pi/n;ps.append((x,r*math.cos(a),r*math.sin(a)))
fs=[];inks=[]
for i in range(len(loops)-1):
 for j in range(n):fs.append((i*n+j,i*n+(j+1)%n,(i+1)*n+(j+1)%n,(i+1)*n+j));inks.append('mouth' if i>=7 else 'sucker' if 4<=i<=6 else 'skin')
fs.append(tuple((len(loops)-1)*n+j for j in reversed(range(n))));inks.append('mouth')
mesh('hollow breathing snout',ps,fs,'skin',True,inks)
for j in range(6):
 a=j*2*math.pi/6
 tube('nozzle longitudinal fold',[(x,r*math.cos(a),r*math.sin(a)) for x,r in [(0,7.2),(4,6.2),(9,5.6),(12,6.4)]],[.18,.19,.14,.08],'sucker',6)
context['limb']=37
orb('muscular tentacle crown',(0,-1.2,0),(9.5,9.5,13.8),'skin',24,12)
for root,middle,tip in [(15,14,13),(22,21,20),(29,28,27),(36,35,34)]:
 for limb,length,widths in [(root,13.12,[3.5,4.0,4.4,4.8]),(middle,13.12,[4.8,6.0,7.5,8.7]),(tip,23,[8.7,9.2,7,4.4,1.5,.15])]:
  context['limb']=limb
  ps=[];steps=len(widths);n=16
  for k,w in enumerate(widths):
   t=k/(steps-1);x=length*t
   # Thin flattened arms, not a chain of beads. Joint overhang closes native bends.
   if k==0:x=-.8
   elif k==steps-1 and limb!=tip:x+=.7
   y=-.35*math.sin(t*math.pi) if limb!=tip else -1.4*math.sin(t*math.pi)
   depth=1.35 if limb!=tip else 1.3*(1-t)+.25
   for j in range(n):
    a=j*2*math.pi/n;ps.append((x,y+depth*math.cos(a),w*math.sin(a)))
  fs=[tuple(reversed(range(n)))];inks=['skin']
  for k in range(steps-1):
   for j in range(n):
    fs.append((k*n+j,k*n+(j+1)%n,(k+1)*n+(j+1)%n,(k+1)*n+j));inks.append('sucker' if 4<=j<12 else 'skin')
  fs.append(tuple((steps-1)*n+j for j in range(n)));inks.append('skin')
  mesh('continuous tapered swimming arm',ps,fs,'skin',True,inks)
  # Low suction discs on the underside; they follow each original arm segment.
  for k in range(3 if limb!=tip else 4):
   x=2.2+k*(3.8 if limb!=tip else 4.3);r=.85 if limb!=tip else 1.25
   t=x/length;surface=(-1.4*math.sin(t*math.pi)+1.3*(1-t)+.25) if limb==tip else (-.35*math.sin(t*math.pi)+1.35)
   path=[(x+r*math.cos(a),surface-.06,r*math.sin(a)) for a in [j*2*math.pi/8 for j in range(9)]]
   tube('recessed suction rim',path,[.19]*len(path),'sucker',4)
scene.frame_start=1;scene.frame_end=len(rig['preview']);scene.render.fps=20;scene.frame_set(1)
scene.world=bpy.data.worlds.get('Octorok Studio') or bpy.data.worlds.new('Octorok Studio');scene.world.use_nodes=True
bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs[0].default_value=(.06,.08,.1,1);bg.inputs[1].default_value=.7
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
formats=[i.identifier for i in bpy.ops.export_scene.gltf.get_rna_type().properties['export_format'].enum_items]
assert not formats or 'GLB' in formats
bpy.ops.export_scene.gltf(filepath=str(ROOT/'octoroks.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
print('Exported',len(parts),'native-limb Octorok pieces. Live project remains:',bpy.data.filepath)
