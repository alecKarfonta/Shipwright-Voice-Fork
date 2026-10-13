"""Native Anubis art. Run in Blender or through the local MCP.
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
rest=rig['animations']['gAnubiceIdleAnim'][0]
connections=[]
scene=bpy.data.scenes.get('ARPG Anubis Workshop') or bpy.data.scenes.new('ARPG Anubis Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['linen','fold','recess','mask','edge','gold','lapis','jewel']
PALETTES=[[(177,153,109),(215,192,143),(68,57,40),(80,59,39),(132,103,62),(184,139,58),(46,72,106),(61,189,207)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'Anubis {sp} {name}') or bpy.data.materials.new(f'Anubis {sp} {name}')
  mat.use_nodes=True;mat.diffuse_color=(*[v/255 for v in rgb],1)
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
  node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Roughness'].default_value=.24 if name in ('eye','iris','pupil') else .48 if name in ('mantle','ridge') else .78
  materials[sp,name]=mat
parts=[];variant=0;context={'limb':7}
def mesh(name,points,faces,material='linen',smooth=False,inks=None):
    limb=context['limb']
    # [TIME] | [INFO] | Anubis.Author | Sculpt in the native resting world space, then bind each carved piece to its original rigid joint.
    inv=Matrix(rest[str(limb)]).inverted();points=[tuple(inv@Vector(p)) for p in points]
    data=bpy.data.meshes.new(name);data.from_pydata(points,[],faces);data.update()
    for key in MATERIALS:data.materials.append(materials[variant,key])
    for i,p in enumerate(data.polygons):
        p.material_index=MATERIALS.index(inks[i] if inks else material);p.use_smooth=smooth
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    ob=bpy.data.objects.new(f'Anubis.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='original Anubis flex rig and native joint connections' ;ob['native_units_per_authoring_unit']=100
    for f,pose in enumerate(rig['poses']):
        ob.matrix_world=Matrix.Translation((variant*49,0,0)) @ axis @ Matrix(pose[str(limb)])
        ob.keyframe_insert(data_path='location',frame=f+1)
        ob.keyframe_insert(data_path='scale',frame=f+1)
        ob.rotation_mode='QUATERNION';ob.keyframe_insert(data_path='rotation_quaternion',frame=f+1)
    parts.append(ob);return ob

def loft(name,rings,mat='linen',sections=12,striped=False):
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

def tube(name,path,radii,mat='linen',sides=7):
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
    # [TIME] | [INFO] | Anubis.Asset | Closed, smoothly shaded eye/joint surface, not an overlapping body primitive.
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



# [TIME] | [INFO] | Anubis.Author | Four original rigid joints: pedestal, inset eye, and two independently opening shutters.
def body(name,rings,mat='linen',n=16):
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

# [TIME] | [INFO] | Anubis.Author | Segmented mummy wrappings follow the eight original visible joints; there are no added locomotion bones.
def wrapped_segment(limb,name,rings,turns):
 context['limb']=limb;body(name,rings,'linen',18)
 lo,hi=rings[0][0],rings[-1][0]
 def radius(y):
  for low,high in zip(rings,rings[1:]):
   if low[0]<=y<=high[0]:
    f=(y-low[0])/(high[0]-low[0]);return tuple(low[k]+f*(high[k]-low[k]) for k in (1,2,3))
  return rings[-1][1:]
 # Flat spiraling fabric strips include folded edges, not a stack of rounded rings.
 for turn in range(turns):
  y0=lo+(hi-lo)*(turn+.15)/turns;pts=[];steps=28
  for i in range(steps+1):
   angle=math.tau*i/steps;cy=y0+(hi-lo)/turns*.5*i/steps
   for off in (-.58,-.43,.43,.58):
    y=min(hi-.12,max(lo+.12,cy+off));rx,rz,cz=radius(y);lift=.13 if abs(off)<.5 else .04
    pts.append(((rx+lift)*math.sin(angle),y,cz+(rz+lift)*math.cos(angle)))
  faces=[];inks=[]
  for i in range(steps):
   for k in range(3):faces.append((i*4+k,i*4+k+1,(i+1)*4+k+1,(i+1)*4+k));inks.append('fold' if k!=1 else 'linen')
  mesh('overlapping linen wrap',pts,faces,'linen',False,inks)
 for sign in (-1,1):
  y=(lo+hi)*.5;rx,rz,cz=radius(y)
  plate('torn linen corner',[(sign*rx*.5,y+.9,cz+rz+.13),(sign*rx*.9,y+.65,cz+rz*.7),(sign*rx*.72,y-1.1,cz+rz*.83)],.09,'fold')
wrapped_segment(3,'broad wrapped shoulders',[(30.4,5.9,5.8,-.6),(34,8.1,7.4,-.8),(40,11.6,9.1,-1),(44,11.8,8.7,-.5),(46.6,6.2,6,-.5)],6)
wrapped_segment(5,'upper articulated waist',[(24.3,5.4,4.7,-.3),(27,6,5.5,-.4),(32,6.0,5.4,-.4),(35.5,5.1,4.7,-.3)],4)
wrapped_segment(6,'lower articulated waist',[(16.4,4.6,4.1,-.4),(19,5.3,4.8,-.4),(25.5,5.4,4.8,-.4),(28.5,4.8,4.3,-.4)],4)
wrapped_segment(8,'tapered hovering tail',[(8.9,3.2,2.9,-.6),(12,4.4,3.9,-.6),(17.8,4.6,4.1,-.6),(20,3.8,3.6,-.5)],4)
wrapped_segment(9,'pointed tail tip',[(1.7,.15,.15,-.8),(4.5,1.6,1.4,-.7),(8,3.2,2.9,-.6),(11.7,3.3,3.0,-.6),(13.0,2.9,2.6,-.6)],4)
context['limb']=3
# A restrained lapis-and-brass collar gives the original broad chest readable structure.
for i in range(12):
 a=math.tau*(i+.5)/12
 tube('collar panel ridge',[(math.sin(a)*r,y,-.5+math.cos(a)*r*.88) for y,r in [(44.2,10),(42.9,11.4),(40.9,10.8)]],[.22,.30,.18],'gold',6)
for y,rx,rz in [(44.3,10.1,8.9),(40.9,10.9,9.6)]:ring('collar rim',(0,y,-.5),rx,rz,mat='gold',width=.24,n=32)
for sign in (-1,1):
 plate('blue collar inlay',[(sign*2.3,42.9,8.5),(sign*5.5,42.7,7.7),(sign*5.0,41.3,8.2),(sign*2.1,41.5,9.0)],.08,'lapis')
 tube('pendant cord',[(sign*6.5,40,7.6),(sign*4.5,35.7,7.0),(sign*1.3,32.7,8.1)],[.2,.2,.15],'gold',6)
context['limb']=11
panel('pendant brass setting',[(-2.45,33.35),(2.45,33.35),(2.6,30.9),(0,28.7),(-2.6,30.9)],8.07,.28,'gold')
panel('pendant dark inset',[(-1.95,32.95),(1.95,32.95),(2,31),(0,29.3),(-2,31)],8.42,.12,'recess')
mesh('cut turquoise amulet',[(-1.7,32.7,8.62),(1.7,32.7,8.62),(1.8,31,8.62),(0,29.65,8.62),(-1.8,31,8.62),(0,31.5,9.2)],[(0,1,5),(1,2,5),(2,3,5),(3,4,5),(4,0,5)],'jewel')
context['limb']=13
# Inset the original jackal planes slightly so the native translucent eye mesh remains exposed at its exact animated location.
limb=next(l for l in rig['rig'] if l['limb']==13);pts=[];faces=[]
for tri in limb['triangles']:
 face=[]
 for x,y,z,j in tri:
  p=Matrix(rest[str(j)])@Vector((x*.0098,y*.0098,z*.0098));face.append(len(pts));pts.append(tuple(p))
 faces.append(tuple(face))
head=mesh('beveled jackal mask',pts,faces,'mask')
bm=bmesh.new();bm.from_mesh(head.data);edges=[e for e in bm.edges if e.is_manifold and e.calc_face_angle(0)>.5]
bmesh.ops.bevel(bm,geom=edges,offset=.1,segments=3,affect='EDGES');bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(head.data);bm.free()
for sign in (-1,1):
 plate('inset long ear',[(sign*3.4,54.8,-6.5),(sign*4.9,61.9,-10.4),(sign*5.0,55.2,-6.3)],.05,'recess')
 tube('ear rim',[(sign*3.2,54.9,-6.55),(sign*4.95,62.3,-10.5),(sign*5.7,54.7,-5.9)],[.17,.09,.2],'edge',5)
 # Detail stays at the cheek's back edge, clear of the exact native eye planes.
 tube('carved cheek border',[(sign*6.8,49,-2.6),(sign*6.0,46.5,-7.8),(sign*1.8,43,-10.5)],[.2,.25,.15],'edge',6)
 tube('muzzle edge',[(sign*5.6,45.9,5.1),(sign*2.6,43.3,10.3),(0,40.9,14.7)],[.14,.13,.08],'edge',5)
for y,z,w in [(54.9,-2.4,2.8),(53.8,-.7,2.5)]:
 tube('brow engraving',[(-w,y,z),(0,y+.7,z+.2),(w,y,z)],[.1,.13,.1],'gold',5)
# Crown binding breaks the broad back plane into fitted strips without touching the muzzle.
for sign in (-1,1):
 for k in range(3):
  tube('back of mask binding',[(sign*1.2,55-k*2,-5.5-k),(sign*4.3,53-k*2,-6.7),(sign*6.2,49-k*1.1,-7.6)],[.22,.28,.17],'fold',5)
context['limb']=15
# A separately hinged lower jaw follows the original bite/opening animation.
limb=next(l for l in rig['rig'] if l['limb']==15);pts=[];faces=[]
for tri in limb['triangles']:
 face=[]
 for x,y,z,j in tri:
  face.append(len(pts));pts.append(tuple(Matrix(rest[str(j)])@Vector((x*.0098,y*.0098,z*.0098))))
 faces.append(tuple(face))
mesh('hinged lower jackal jaw',pts,faces,'mask')
for sign in (-1,1):
 tube('jaw carved edge',[(sign*5.3,45.8,5.2),(sign*3.0,43.5,9.6),(0,41.2,14.7)],[.24,.18,.1],'edge',6)
 tube('small lower canine',[(sign*3.0,44,9.1),(sign*3.0,45.0,9.0)],[.30,.03],'fold',6)
scene.frame_start=1;scene.frame_end=rig['frames'];scene.render.fps=20;scene.frame_set(82)
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.export_scene.gltf(filepath=str(ROOT/'anubis.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
(ROOT/'connections.json').write_text('[]\n')
print('Exported',len(parts),'native-joint pieces; eyes remain the original runtime display list.')
