"""Native Skulltula art. Run in Blender or through the local MCP.
Only the ARPG Skulltula Workshop scene is replaced. Runtime keeps native AI/rigs.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector
ROOT=Path(__file__).resolve().parent
rig=json.loads((ROOT/'native_rig.json').read_text())
scene=bpy.data.scenes.get('ARPG Skulltula Workshop') or bpy.data.scenes.new('ARPG Skulltula Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['shell','edge','socket','belly','fold','leg','band','fang']
PALETTES=[[(210,199,165),(240,224,188),(37,29,24),(159,78,43),(98,45,31),(111,75,43),(197,137,69),(244,223,183)]]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'Skulltula {sp} {name}') or bpy.data.materials.new(f'Skulltula {sp} {name}')
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
    ob=bpy.data.objects.new(f'Skulltula.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='native 29-limb Skulltula rig, separate jaw and eight rotating legs' ;ob['native_units_per_authoring_unit']=100
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


# [TIME] | [INFO] | Skulltula.Author | Skull plates face local -Y; warm unarmored belly faces +Y.
context['limb']=5
# [TIME] | [INFO] | Skulltula.Author | A tapered shell carries the facial plates without spherical body overlaps.
loft('continuous armored cephalothorax',[(-3.25,-.1,.12,.12),(-2.7,-.45,1.05,1.8),(-1.4,-.65,1.75,3.0),(.4,-.9,2.0,3.5),(2.0,-.9,1.65,3.0),(3.7,-.4,1.2,2.15),(5.4,-.15,.55,1.15),(6.2,-.05,.12,.6)],'belly',24)
plate('broad cranial shield',[(-3.3,-.6,0),(-2.7,-1.7,-1.8),(-1.5,-2.75,-2.9),(-.5,-3.02,-3.05),(-.9,-3.18,-1.7),(-.48,-3.19,0),(-.9,-3.18,1.7),(-.5,-3.02,3.05),(-1.5,-2.75,2.9),(-2.7,-1.7,1.8)],.19,'shell')
for sign in (-1,1):
    # [TIME] | [INFO] | Skulltula.Author | True recessed orbital walls, with irregular bevels and dark socket floors.
    outline=[(-.8,.9),(-.75,1.75),(-.45,2.65),(.35,3.02),(1.55,2.65),(1.85,1.7),(1.15,.87)]
    center=Vector((.45,0,1.9));ps=[];n=len(outline)
    for scale,y in [(1.23,-2.68),(1.04,-3.2),(.78,-3.22),(.64,-3.04)]:
        for x,z in outline:ps.append((center.x+(x-center.x)*scale,y,sign*(center.z+(z-center.z)*scale)))
    fs=[];inks=[]
    for k in range(3):
        for i in range(n):fs.append((k*n+i,k*n+(i+1)%n,(k+1)*n+(i+1)%n,(k+1)*n+i));inks.append('socket' if k==2 else 'shell' if k==0 else 'edge')
    fs.append(tuple(3*n+i for i in range(n)));inks.append('socket')
    mesh('deep angular eye socket',ps,fs,'shell',False,inks)
    plate('cheekbone buttress',[(1.2,-2.95,sign*3.07),(2.1,-2.7,sign*2.95),(3.9,-1.9,sign*1.88),(4.4,-1.7,sign*1.08),(2.25,-2.62,sign*1.1),(1.78,-3.0,sign*1.8)],.15,'shell')
    tube('temple suture',[(-2.3,-1.83,sign*1.6),(-1.55,-2.42,sign*1.85),(-1.25,-2.64,sign*2.5)],[.025,.035,.016],'fold',5)
plate('nasal bridge',[(-.85,-3.03,0),(.1,-3.2,-.47),(1.42,-3.17,-.54),(1.1,-3.3,0),(1.42,-3.17,.54),(.1,-3.2,.47)],.13,'shell')
plate('triangular nasal hollow',[(1.02,-2.96,0),(2.55,-2.83,-.56),(2.39,-2.86,0),(2.55,-2.83,.56)],.01,'socket')
for sign in (-1,1):
    tube('nasal rim',[(1.05,-3.12,0),(2.63,-2.97,sign*.68),(2.98,-2.8,sign*.72)],[.09,.12,.1],'edge',7)
    for k in range(4):
        z=sign*(.42+k*.49);x=3.05+abs(z)*.18
        tube('fixed upper tooth',[(x,-2.47,z),(x+.32,-2.56,z),(x+.76,-2.42,z*.94)],[.23,.24,.10],'shell',7)
# [TIME] | [INFO] | Skulltula.Author | Transverse folds expose the native vulnerable rear instead of another armored mask.
for x in (-1.6,-.65,.4,1.45,2.5):
    width=2.1 if x<2 else 1.75
    path=[(x+.22*abs(z)/width,.83-.68*(abs(z)/width)**2,z) for z in [-width,-width*.7,0,width*.7,width]]
    tube('abdominal fold',path,[.08,.1,.12,.1,.08],'fold',6)
for x,y,z in [(2.26,.81,1.8),(-1.88,-.68,2),(.47,.17,2.5),(4.14,1.49,1.6)]:
    for sign in (-1,1):
        tube('integrated leg root socket',[(x,y-.85,sign*z*.82),(x,y-.3,sign*z*.96),(x,y,sign*z)],[.48,.43,.35],'leg',9)
context['limb']=4
# [TIME] | [INFO] | Skulltula.Author | This whole mandible remains on the original independently animated jaw joint.
for sign in (-1,1):
    tube('curving lower mandible',[(-.5,.25,sign*1.1),(.6,-.2,sign*1.5),(1.8,-.34,sign*1.35),(2.75,-.26,sign*.75)],[.25,.36,.36,.17],'shell',9)
    tube('long venom fang',[(1.3,-.24,sign*1.38),(2.2,-.25,sign*1.0),(3.5,.05,sign*.53)],[.32,.23,.035],'fang',10)
    for k in range(3):
        tube('jaw denticle',[(.1+k*.52,-.26,sign*1.4),(-.2+k*.52,-.45,sign*.78)],[.2,.04],'fang',7)
# [TIME] | [INFO] | Skulltula.Author | Eight bent tapered legs follow the original rigid meshes and leg-root rotations.
paths={8:[(0,0,-.2),(3.1,-.85,.6),(5,-3.5,.58),(4.8,-10.0,-.67),(4.19,-12.04,-1.47)],11:[(0,0,-.2),(1.9,1.8,.18),(2.3,4.5,.1),(1.7,8.4,-1.12),(.36,10.69,-1.74)],14:[(0,0,-.2),(2.5,1.3,.36),(5.1,6.2,.32),(4.9,11.6,-1.0),(4.87,13.07,-1.1)],23:[(0,0,-.1),(1.9,-2.35,.35),(2.5,-5.75,.25),(1.8,-10.15,-.92),(.69,-11.85,-1.69)]}
for left,right in [(8,20),(11,29),(14,17),(23,26)]:
    for limb,sign in [(left,1),(right,-1)]:
        context['limb']=limb;p=[Vector((x,y*sign,z)) for x,y,z in paths[left]]
        tube('tapered segmented leg',p,[.32,.38,.41,.23,.018],'leg',8)
        for k in (1,2):
            a=p[k].lerp(p[k-1],.05);b=p[k].lerp(p[k+1],.08)
            tube('joint armor collar',[a,p[k],b],[.37,.44,.32],'band',8)
        for k in (1,2,3):
            a=p[k].lerp(p[k-1],.15);b=p[k].lerp(p[k-1],.27)
            tube('swept leg barb',[a,a+Vector((.4,0,.38)),b+Vector((.43,0,.43))],[.14,.13,.012],'band',5)
scene.frame_start=1;scene.frame_end=len(rig['poses']);scene.render.fps=20;scene.frame_set(1)
scene.world=bpy.data.worlds.get('Skulltula Studio') or bpy.data.worlds.new('Skulltula Studio');scene.world.use_nodes=True
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.export_scene.gltf(filepath=str(ROOT/'skulltulas.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
print('Exported',len(parts),'native-limb Skulltula pieces.')
