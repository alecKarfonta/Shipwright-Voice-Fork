"""Native Handmaster art. Run in Blender or through the local MCP.
Only the ARPG Handmaster Workshop scene is replaced. Runtime keeps native AI/rigs.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector
ROOT=Path(__file__).resolve().parent
rig=None
rest=None
connections=[]
scene=bpy.data.scenes.get('ARPG Handmaster Workshop') or bpy.data.scenes.new('ARPG Handmaster Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['skin','callus','tendon','crease','nail','nail_edge','bone','marrow']
PALETTES=[[(118,107,135),(129,117,144),(133,122,148),(61,49,72),(177,159,115),(109,91,72),(222,204,164),(96,61,79)],[(111,124,93),(123,134,102),(129,141,108),(52,63,43),(179,165,113),(102,94,65),(222,206,165),(99,73,60)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'Handmaster {sp} {name}') or bpy.data.materials.new(f'Handmaster {sp} {name}')
  mat.use_nodes=True;mat.diffuse_color=(*[v/255 for v in rgb],1)
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
  node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Roughness'].default_value=.24 if name in ('nail','bone') else .48 if name in ('callus','tendon') else .78
  materials[sp,name]=mat
parts=[];variant=0;context={'limb':7}
def mesh(name,points,faces,material='skin',smooth=False,inks=None):
    limb=context['limb']
    data=bpy.data.meshes.new(name);data.from_pydata(points,[],faces);data.update()
    for key in MATERIALS:data.materials.append(materials[variant,key])
    for i,p in enumerate(data.polygons):
        p.material_index=MATERIALS.index(inks[i] if inks else material);p.use_smooth=smooth
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    ob=bpy.data.objects.new(f'Handmaster.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='original Handmaster flex rig and native joint connections' ;ob['native_units_per_authoring_unit']=100
    ob.matrix_world=Matrix.Translation((variant*78,0,0)) @ axis @ Matrix(rest[str(limb)])
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
    return mesh(name,pts,faces,mat,True)

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



def ring(x,cy,ry,rz,n=16):return [(x,cy+ry*math.cos(j*math.tau/n),rz*math.sin(j*math.tau/n)) for j in range(n)]
def bridge(name,parent,parent_points,child_points):
    limb=context['limb'];change=Matrix(rest[str(limb)]).inverted() @ Matrix(rest[str(parent)])
    converted=[tuple(change@Vector(p)) for p in parent_points];n=len(converted)
    choices=[]
    for flip in (False,True):
        order=list(range(n));order=order[::-1] if flip else order
        for shift in range(n):
            ids=order[shift:]+order[:shift];choices.append((sum((Vector(converted[ids[j]])-Vector(child_points[j])).length_squared for j in range(n)),ids))
    ids=min(choices)[1];parent_points=[parent_points[i] for i in ids];converted=[converted[i] for i in ids]
    ob=mesh(name,converted+child_points,[(j,(j+1)%n,n+(j+1)%n,n+j) for j in range(n)],'skin',True)
    ob['arpg_parent_joint']=parent;ob['arpg_parent_ring_points']=converted;ob['arpg_child_ring_points']=child_points
    connections.append({'variant':variant,'child':limb,'parent':parent,'parent_points':parent_points,'child_points':child_points})
    return ob

# [TIME] | [INFO] | Handmaster.Author | Anatomical surfaces follow the existing native flex joints, including the separate fifth digit.
def shifted_ring(x,cy,ry,rz,z=0,n=16):return [(p[0],p[1],p[2]+z) for p in ring(x,cy,ry,rz,n)]
def resample(points,n=16):
 return [tuple(Vector(points[int(j*len(points)/n)]).lerp(Vector(points[(int(j*len(points)/n)+1)%len(points)]),j*len(points)/n%1)) for j in range(n)]
def ridges(length,radius,cy=0):
 for x in [length*.27,length*.34,length*.69,length*.76]:
  tube('transverse skin crease',[(x+.18*math.cos(a),cy-radius*math.cos(a),radius*.84*math.sin(a)) for a in [-1.1,-.7,0,.7,1.1]],[.07,.095,.105,.095,.07],'crease',5)
 # Finger silhouette and folds carry the form; avoid floating tendon cords.
def nail(length,width,cy=0):
 pts=[];sections=10
 for x,y,w in [(length*.31,cy-width*.80,width*.85),(length*.53,cy-width*1.13,width*.90),(length*.81,cy-width*.82,width*.60),(length*.98,cy+.15,width*.12)]:
  for j in range(sections):
   a=j*math.tau/sections;pts.append((x,y+.40*math.cos(a),w*math.sin(a)))
 faces=[tuple(reversed(range(sections)))];inks=['nail_edge']
 for i in range(3):
  for j in range(sections):faces.append((i*sections+j,i*sections+(j+1)%sections,(i+1)*sections+(j+1)%sections,(i+1)*sections+j));inks.append('nail' if 2<=j<=7 else 'nail_edge')
 faces.append(tuple(3*sections+j for j in range(sections)));inks.append('nail');mesh('curved worn nail',pts,faces,'nail',True,inks)
 for z in [-.42,0,.42]:tube('longitudinal nail ridge',[(length*.48,cy-width*1.05,z*width),(length*.68,cy-width*1.2,z*width*.82),(length*.91,cy-width*.45,z*width*.30)],[.04,.07,.02],'nail_edge',5)

def palm():
 context['limb']=2;sections=24;pts=[]
 profiles=[(-5.8,-6.3,-5.8,.3),(0,-10.8,.5,11.5),(5.8,-18.6,.9,14.9),(11.5,-18,.9,16.5),(20,-14.5,.2,17.3),(27.8,-12.8,-1.2,15.1)]
 for x,top,bottom,w in profiles:
  for j in range(sections):
   a=j*math.tau/sections;c=math.cos(a);ss=math.sin(a);y=(top+bottom)/2+(bottom-top)/2*math.copysign(abs(c)**.72,c);z=w*math.copysign(abs(ss)**.72,ss);pts.append((x,y,z))
 faces=[tuple(reversed(range(sections)))]
 for i in range(len(profiles)-1):
  for j in range(sections):faces.append((i*sections+j,i*sections+(j+1)%sections,(i+1)*sections+(j+1)%sections,(i+1)*sections+j))
 faces.append(tuple((len(profiles)-1)*sections+j for j in range(sections)));mesh('flattened anatomical palm',pts,faces,'skin',True)
 # Branching dorsal tendons converge at the wrist and fan into the original finger roots.
 for j,z in enumerate([-13.4,-3.1,8.5]):
  tube('fanned dorsal tendon',[(4,-15.7,z*.2),(11,-17.0,z*.57),(20,-13.65,z*.89),(27.5,-11.85,z)],[.60,.95,1.12,.70],'skin',12)
 for x in [12,16,23]:
  tube('palm flexion line',[(x,.85,-11),(x-1.5,1.05,-5),(x-1,1.05,3),(x+1,.45,10)],[.10,.14,.13,.08],'crease',6)
 tube('thenar crease',[(3,-.1,6),(7,.65,9),(13,.70,11.5),(19,.45,11)],[.11,.16,.15,.08],'crease',6)
 for x,z in [(17,-14),(23,5),(8,-11),(12,13)]:
  tube('old healed scar',[(x,-13.5,z),(x+1.7,-13.3,z+.5),(x+2.6,-13,z-.1)],[.08,.12,.04],'callus',6)

for variant in (0,1):
 rig=json.loads((ROOT/'native_rig.json').read_text());rest=rig['animations']['gWallmasterWaitAnim'][0]
 palm()
 for root,mid,tip,length,middle,terminal,width,center in [(4,6,7,15.73,13.08,15.04,5.0,8.54),(9,11,12,10.81,8.99,10.33,4.1,-13.42),(14,16,17,15.73,13.08,15.04,5.0,-3.08)]:
  context['limb']=root
  bridge('metacarpal web',2,shifted_ring(27.7 if root!=9 else 26.2,-7 if root!=9 else -7.1,width*1.05,width*1.05,center),ring(3.1,0,width*.94,width*.94))
  loft('proximal phalanx',[(3.1,0,width*.94,width*.94),(length*.42,-.1,width,width*.93),(length*.7,.1,width*.84,width*.86),(length-.6,0,width*.73,width*.77)],'skin',16)
  ridges(length,width*.94)
  context['limb']=mid;bridge('first finger joint',root,ring(length-.6,0,width*.73,width*.77),ring(2.8,0,width*.65,width*.66))
  loft('middle phalanx',[(2.8,0,width*.65,width*.66),(middle*.54,0,width*.66,width*.68),(middle-.55,0,width*.50,width*.53)],'skin',16);ridges(middle,width*.63)
  context['limb']=tip;bridge('distal finger joint',mid,ring(middle-.55,0,width*.50,width*.53),ring(1.9,0,width*.50,width*.52))
  loft('rounded finger pad',[(1.9,0,width*.50,width*.52),(terminal*.47,.2,width*.53,width*.53),(terminal*.73,.1,width*.38,width*.36),(terminal*.87,0,.35,.42)],'callus',16)
  ridges(terminal*.70,width*.50);nail(terminal,width*.52)
 # The thumb's long first connection is part of the native hand web, not a floating cylinder.
 context['limb']=21
 bridge('thumb saddle web',2,resample([(5.4,-13.4,15.3),(6.5,1.0,10.5),(18.9,-4.2,14.4),(18.9,-11.1,15.6)]),ring(15.2,-.3,5.1,5.0))
 loft('thumb basal knuckle',[(15.2,-.3,5.1,5),(16.8,-.3,5.2,5.1),(18.4,-.2,4.3,4.2)],'skin',16)
 context['limb']=23;bridge('thumb flexion joint',21,ring(18.4,-.2,4.3,4.2),ring(2.7,-.45,3.9,3.65))
 loft('thumb proximal phalanx',[(2.7,-.45,3.9,3.65),(7,-.35,3.65,3.6),(12.9,-.45,3.0,2.9)],'skin',16);ridges(13.5,3.55,-.35)
 context['limb']=24;bridge('thumb distal joint',23,ring(12.9,-.45,3.0,2.9),ring(1.9,-.4,2.6,2.8))
 loft('thumb pad',[(1.9,-.4,2.6,2.8),(5.5,-.35,2.8,2.7),(8,-.2,1.5,1.5),(9.5,.1,.4,.45)],'callus',16);nail(10.2,2.8,-.45)
 context['limb']=19
 anchors=[(-5.8,-6.1,0),(0,-10.0,-11.2),(5.0,-17.5,-7.3),(10.4,-19.1,0),(5.1,-17.5,7.6),(0,-10.0,11.2)]
 bridge('flexible wrist fold',2,resample(anchors,24),ring(5.7,3.6,6.25,11.1,24))
 # Open cuff with an inset cut surface and stylized paired forearm bones.
 n=24;pts=[]
 for x,cy,ry,rz in [(5.7,3.6,6.25,11.1),(12.5,3.8,6.1,11.3),(20.8,3.5,5.5,10.7),(20.7,3.5,4.1,9.1),(18.9,3.5,3.8,8.6)]:pts+=ring(x,cy,ry,rz,n)
 faces=[];inks=[]
 for k in range(4):
  for j in range(n):faces.append((k*n+j,k*n+(j+1)%n,(k+1)*n+(j+1)%n,(k+1)*n+j));inks.append('skin' if k<2 else 'marrow')
 faces.append(tuple(4*n+j for j in range(n)));inks.append('marrow');mesh('open wrist cuff',pts,faces,'skin',True,inks)
 for z in (-4.1,4.1):
  loft('paired forearm bone',[(18.6,3.5,2.6,2.8),(20.8,3.7,2.45,2.55),(21.7,3.8,2.1,2.1)],'bone',12)
  ob=parts[-1]
  for v in ob.data.vertices:v.co.z+=z
  loft('recessed bone core',[(21.72,3.8,1.25,1.22),(21.78,3.8,1.21,1.20)],'marrow',12)
  for v in parts[-1].data.vertices:v.co.z+=z
 for z in (-7,0,7):tube('wrist tendon',[(7,-1.7,z),(12,-1.5,z*.97),(18,-1.0,z*.95)],[.40,.48,.35],'skin',10)
 context['limb']=25
 tube('curled fifth digit',[(-2.2,-.1,0),(.7,.0,0),(3.9,1.6,0),(4.5,3.6,0)],[1.8,2.0,1.75,1.15],'skin',14)
 tube('fifth digit nail',[(3.9,2.2,0),(5.4,3.6,0),(6.7,5.4,0)],[1.1,.95,.06],'nail',12)
 for y in (.1,.8):tube('fifth digit crease',[(.8,y-1.6,-1.2),(1.4,y-1.75,0),(1.5,y-1.6,1.2)],[.045,.06,.045],'crease',5)
# [TIME] | [INFO] | Handmaster.AuthorNormals | Shared surface boundaries receive the same rest-space normal, including skin across native joints.
from collections import defaultdict
normal_groups=defaultdict(list)
for ob in parts:
 ob.data.update();rot=ob.matrix_world.to_3x3()
 for v in ob.data.vertices:
  key=(ob['arpg_variant'],*[round(x*10000) for x in ob.matrix_world@v.co]);normal_groups[key].append((ob,v.index,rot@v.normal))
normals={ob:[v.normal.copy() for v in ob.data.vertices] for ob in parts}
for group in normal_groups.values():
 if len(group)<2:continue
 n=sum((row[2].normalized() for row in group),Vector()).normalized()
 for ob,i,_ in group:normals[ob][i]=(ob.matrix_world.to_3x3().inverted()@n).normalized()
for ob in parts:ob.data.normals_split_custom_set_from_vertices(normals[ob])
scene.frame_start=1;scene.frame_end=1;scene.render.fps=20
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.export_scene.gltf(filepath=str(ROOT/'handmasters.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
(ROOT/'connections.json').write_text(json.dumps(connections,indent=2)+'\n')
print('Exported',len(parts),'pieces and',len(connections),'native connections.')
