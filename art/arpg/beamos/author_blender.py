"""Native Beamos art. Run in Blender or through the local MCP.
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
rest=rig['animations']['gBeamosAnim'][0]
connections=[]
scene=bpy.data.scenes.get('ARPG Beamos Workshop') or bpy.data.scenes.new('ARPG Beamos Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['stone','edge','shadow','bronze','iron','eye','crack']
PALETTES=[[(117,119,107),(166,165,139),(31,35,32),(145,120,72),(83,94,95),(245,72,32),(59,66,58)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'Beamos {sp} {name}') or bpy.data.materials.new(f'Beamos {sp} {name}')
  mat.use_nodes=True;mat.diffuse_color=(*[v/255 for v in rgb],1)
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
  node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Roughness'].default_value=.24 if name in ('eye','iris','pupil') else .48 if name in ('mantle','ridge') else .78
  materials[sp,name]=mat
parts=[];variant=0;context={'limb':7}
def mesh(name,points,faces,material='stone',smooth=False,inks=None):
    limb=context['limb']
    # [TIME] | [INFO] | Beamos.Author | Sculpt in the native resting world space, then bind each carved piece to its original rigid joint.
    inv=Matrix(rest[str(limb)]).inverted();points=[tuple(inv@Vector(p)) for p in points]
    data=bpy.data.meshes.new(name);data.from_pydata(points,[],faces);data.update()
    for key in MATERIALS:data.materials.append(materials[variant,key])
    for i,p in enumerate(data.polygons):
        p.material_index=MATERIALS.index(inks[i] if inks else material);p.use_smooth=smooth
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    ob=bpy.data.objects.new(f'Beamos.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='original Beamos flex rig and native joint connections' ;ob['native_units_per_authoring_unit']=100
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
    # [TIME] | [INFO] | Beamos.Asset | Closed, smoothly shaded eye/joint surface, not an overlapping body primitive.
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



# [TIME] | [INFO] | Beamos.Author | Four original rigid joints: pedestal, inset eye, and two independently opening shutters.
def body(name,rings,mat='stone',n=16):
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
context['limb']=10
body('worn octagonal foot',[(.15,16.7,13.3,2.5),(1.4,19,15.5,2.5),(4.4,19,15.5,2.5),(6.5,16.7,13.8,2.5)],n=16)
body('lower bronze footing',[(6.5,16.8,13.9,2.5),(7.4,16.8,13.9,2.5),(8,15.7,12.8,2.5)],'bronze')
body('tapered sentinel pillar',[(7,15.5,12.5,2),(13,14.7,11.5,1.7),(23,12,10,1.2),(34,9.5,8.8,.5),(42,8,8,0),(46,8,8,0)])
body('upper stone shoulder',[(41,8.1,8.1,0),(43,10,9.5,0),(46,10,9.5,0),(48,8.8,8.5,0)],'edge')
body('rotating neck bearing',[(46,7.3,7.3,0),(49.8,7.3,7.3,0)],'iron',24)
for y in (46.5,48.8):ring('bearing ring',(0,y,0),7.4,7.4,mat='bronze',width=.32)
# Carved vertical recesses follow the taper; raised stone ribs give the pillar a directional silhouette.
for i in range(8):
 a=math.tau*i/8;nx,nz=math.sin(a),math.cos(a)
 path=[(nx*rx,y,cz+nz*rz) for y,rx,rz,cz in [(9,15.2,12.4,2),(16,14,11.2,1.5),(28,11,9.7,.8),(39,8.9,8.6,0)]]
 tube('deep tapered chisel groove',path,[.55,.65,.6,.35],'crack',5)
 # A long angular rib, not a stack of round bulges.
 path2=[(p[0]+nx*.45,p[1],p[2]+nz*.45) for p in path]
 tube('carved pillar arris',path2,[.32,.42,.38,.23],'edge',5)
def front(x,y):
 rings=[(7,15.5,12.5,2),(13,14.7,11.5,1.7),(23,12,10,1.2),(34,9.5,8.8,.5),(42,8,8,0)]
 for low,high in zip(rings,rings[1:]):
  if low[0]<=y<=high[0]:
   f=(y-low[0])/(high[0]-low[0]);rx,rz,cz=[low[k]+f*(high[k]-low[k]) for k in (1,2,3)]
   return cz+rz*math.sqrt(max(0,1-(x/rx)**2))+.15
 raise ValueError(y)
for sign in (-1,1):
 outline=[(sign*x,y) for x,y in [(4.5,12),(10,12),(9,23),(6.7,29),(4.8,23)]]
 plate('recessed front carving',[(x,y,front(x,y)) for x,y in outline],.12,'crack')
 path=[(sign*x,y,front(sign*x,y)+.27) for x,y in [(6.2,14),(7.6,18),(6.4,22),(6.7,25.5)]]
 tube('inlaid zigzag',path,[.18]*4,'bronze',5)
panel('central carved sigil',[(-3.5,31),(0,35),(3.5,31),(0,26)],9.8,.55,'edge')
panel('sigil recess',[(-1.6,31),(0,33),(1.6,31),(0,28.5)],10.35,.18,'shadow')
for x in (-12,12):
 orb('footing rivet',(x,4.2,16.5),(.8,.8,.5),'bronze',8,5)
# Shell hemispheres are authored closed, around the native horizontal hinges. Their back is enclosed.
for sign,limb in [(1,5),(-1,8)]:
 context['limb']=limb
 n=32;rows=7;pts=[]
 for j in range(rows+1):
  t=.035+(math.pi/2-.08)*j/rows
  for i in range(n):
   a=math.tau*i/n
   pts.append((13*math.cos(t)*math.sin(a),60+sign*12.3*math.sin(t),12.7*math.cos(t)*math.cos(a)))
 faces=[];inks=[]
 for j in range(rows):
  for i in range(n):
   faces.append((j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i));inks.append('iron' if i%8 not in (0,7) else 'bronze')
 faces.append(tuple(rows*n+i for i in range(n)));inks.append('bronze')
 mesh('upper shutter shell' if sign>0 else 'lower shutter shell',pts,faces,'iron',True,inks)
 ring('shutter bevel',(0,60+sign*.45,0),13,12.7,mat='bronze',width=.36,n=32)
 # Etched broad quarter-seams and small flat fasteners move with the shutter itself.
 for a in (0,math.pi/2,math.pi,3*math.pi/2):
  path=[(13.1*math.cos(t)*math.sin(a),60+sign*12.4*math.sin(t),12.8*math.cos(t)*math.cos(a)) for t in (.12,.4,.7,1.05,1.37)]
  tube('shutter radial seam',path,[.17]*len(path),'shadow',5)
 for a in (-.48,.48,math.pi-.48,math.pi+.48):
  t=.25;p=(13.25*math.cos(t)*math.sin(a),60+sign*12.6*math.sin(t),12.95*math.cos(t)*math.cos(a))
  orb('shutter rivet',p,(.48,.48,.48),'edge',8,5)
# The native eye joint rotates with the scanner. Limit the lens to the original eye bounds; the beam charge originates further forward.
context['limb']=9
orb('recessed eye housing',(0,60,6.0),(10.2,4.75,2.9),'shadow',24,12)
ring('eye bezel',(0,60,8.7),8.9,4.1,axis='z',mat='bronze',width=.46,n=32)
orb('burnished iris',(0,60,9.0),(5.6,3.25,.5),'bronze',24,10)
ring('iris aperture',(0,60,9.42),2.65,2.9,axis='z',mat='iron',width=.35,n=24)
orb('ember lens',(0,60,9.35),(2.35,2.6,.45),'eye',24,12)
# Mechanical hinge caps ride with the eye, never with the stationary pedestal.
for sign in (-1,1):
 orb('axle end cap',(sign*11.8,60,-.4),(1.25,2.4,2.4),'bronze',12,8)
 orb('inset axle bolt',(sign*12.8,60,-.4),(.28,1.1,1.1),'iron',10,6)
scene.frame_start=1;scene.frame_end=rig['frames'];scene.render.fps=20;scene.frame_set(8)
scene.world=bpy.data.worlds.get('Beamos Studio') or bpy.data.worlds.new('Beamos Studio');scene.world.use_nodes=True
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.export_scene.gltf(filepath=str(ROOT/'beamos.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
(ROOT/'connections.json').write_text('[]\n')
print('Exported',len(parts),'rigid mechanical pieces.')
