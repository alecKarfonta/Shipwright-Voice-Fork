"""Native red/blue Tektite art. Run in Blender or through the local MCP.
Only the ARPG Tektite Workshop scene is replaced. Runtime keeps native AI/rigs.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector
ROOT=Path(__file__).resolve().parent
rig=json.loads((ROOT/'native_rig.json').read_text())
scene=bpy.data.scenes.get('ARPG Tektite Workshop') or bpy.data.scenes.new('ARPG Tektite Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['shell','edge','seam','belly','joint','claw','sclera','iris','pupil','glint']
PALETTES=[
 [(171,74,39),(223,135,67),(64,35,31),(118,64,49),(67,49,40),(198,166,109),(242,226,174),(216,89,27),(23,17,18),(255,249,222)],
 [(52,98,137),(116,164,190),(29,48,73),(60,101,121),(39,63,70),(184,204,183),(242,237,182),(166,115,34),(16,27,40),(255,253,229)]
]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'Tektite {sp} {name}') or bpy.data.materials.new(f'Tektite {sp} {name}')
  mat.use_nodes=True;mat.diffuse_color=(*[v/255 for v in rgb],1)
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
  node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Roughness'].default_value=.24 if name in ('iris','sclera','pupil') else .48 if name in ('shell','edge') else .78
  materials[sp,name]=mat
parts=[];variant=0;context={'limb':4}
def mesh(name,points,faces,material='shell',smooth=False,inks=None):
    limb=context['limb']
    data=bpy.data.meshes.new(name);data.from_pydata(points,[],faces);data.update()
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    for key in MATERIALS:data.materials.append(materials[variant,key])
    for i,p in enumerate(data.polygons):
        p.material_index=MATERIALS.index(inks[i] if inks else material);p.use_smooth=smooth
    ob=bpy.data.objects.new(f'Tektite.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='native crouch, lunge and landing poses';ob['native_units_per_authoring_unit']=100
    for f,pose in enumerate(rig['poses']):
        ob.matrix_world=Matrix.Translation(((variant-.5)*100,0,0)) @ axis @ Matrix(pose[str(limb)])
        ob.keyframe_insert(data_path='location',frame=f+1)
        ob.rotation_mode='QUATERNION';ob.keyframe_insert(data_path='rotation_quaternion',frame=f+1)
    parts.append(ob);return ob

def loft(name,rings,mat='shell',sections=12,striped=False):
    pts=[]
    for x,cy,ry,rz in rings:
        for i in range(sections):
            a=2*math.pi*i/sections;pts.append((x,cy+ry*math.cos(a),rz*math.sin(a)))
    faces=[tuple(reversed(range(sections)))];inks=[mat]
    for j in range(len(rings)-1):
        for i in range(sections):
            faces.append((j*sections+i,j*sections+(i+1)%sections,(j+1)*sections+(i+1)%sections,(j+1)*sections+i))
            inks.append('belly' if striped and 4<=i<=7 else mat)
    faces.append(tuple((len(rings)-1)*sections+i for i in range(sections)));inks.append(mat)
    return mesh(name,pts,faces,mat,True,inks)

def tube(name,path,radii,mat='seam',sides=7):
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

def shell_profile(x):
    keys=[(0,3,7.5),(5,6.7,11.8),(12,8.0,14.1),(20,6.5,11.1),(27,3.4,7.5),(35.5,-.2,.25)]
    for a,b in zip(keys,keys[1:]):
        if x<=b[0]:
            t=(x-a[0])/(b[0]-a[0]);return (a[1]*(1-t)+b[1]*t,a[2]*(1-t)+b[2]*t)
    return keys[-1][1:]

def shell_patch(name,xs,sign):
    angles=[4,14,32,52,72,91];pts=[]
    for x in xs:
        h,w=shell_profile(x)
        for deg in angles:
            a=math.radians(deg);pts.append((x,-2+(h+2)*math.cos(a),sign*w*math.sin(a)))
    n=len(pts);pts += [(x,y-.65,z*.98) for x,y,z in pts]
    faces=[];inks=[];width=len(angles)
    for i in range(len(xs)-1):
        for j in range(width-1):
            q=(i*width+j,(i+1)*width+j,(i+1)*width+j+1,i*width+j+1)
            faces.extend([q,tuple(n+k for k in reversed(q))]);inks.extend(['shell','seam'])
    border=list(range(width))+[i*width+width-1 for i in range(1,len(xs))]+list(range(n-2,n-width-1,-1))+[i*width for i in range(len(xs)-2,0,-1)]
    for i,j in zip(border,border[1:]+border[:1]):faces.append((i,j,j+n,i+n));inks.append('edge')
    return mesh(name,pts,faces,'shell',False,inks)

def anatomy():
    context['limb']=4
    loft('low ribbed abdomen',[(-2,-2,1.6,3.7),(1,-1.8,3.9,7.2),(6,-1.5,5.5,11),(12,-1.5,5.9,13),(20,-1.5,4.8,10),(28,-1.3,2.7,6),(34.7,-.4,.4,.5)],'belly',16)
    for sign in (-1,1):
        shell_patch('brow carapace',[0,2.5,6.5,10.3],sign)
        shell_patch('thoracic carapace',[10.9,14.3,19.2,23],sign)
        shell_patch('tapered rear carapace',[23.6,26.2,31,35.5],sign)
        # Small swept teeth on the shell skirt, with a visible dark underside.
        for x,z in [(9,12.9),(15,13.1),(21,10.2)]:
            plate('shell skirt tooth',[(x-2,-.8,sign*(z-.5)),(x+.9,-1,sign*(z+.3)),(x+4,-2.2,sign*(z+1)),(x+1,-3,sign*(z-1.5))],.6,'shell',(0,1,0))
        # Raised ribs are short structural ridges, not studs or metal plates.
        for x in (7,17,26):
            h,w=shell_profile(x)
            path=[(x+.2,-2+(h+2)*math.cos(a),sign*w*math.sin(a)) for a in [math.radians(v) for v in (12,34,60,85)]]
            tube('growth ridge',path,[.16,.19,.16,.06],'edge',6)
    for x,height in [(10,1.9),(19,1.4),(27,1.0)]:
        y,_=shell_profile(x)
        plate('dorsal keel',[(x-2,y-.5,-.4),(x+3,y-1,-.4),(x+1.6,y+height,-.2),(x-1,y+.4,-.3)],.8,'shell',(0,0,1))
    # One broad cyclopean eye is the Tektite's strongest identifying feature.
    rim=[(-.4,4.8*math.cos(a)-.3,7.2*math.sin(a)) for a in [j*2*math.pi/20 for j in range(21)]]
    tube('recessed orbital rim',rim,[.78]*len(rim),'seam',8)
    orb('single ivory eye',(-.8,-.3,0),(2.15,4.25,6.5),'sclera',24,12)
    orb('amber iris',(-2.58,-.3,0),(.66,3.1,3.3),'iris',20,10)
    orb('vertical slit pupil',(-3.1,-.3,0),(.27,2.5,.74),'pupil',12,10)
    orb('eye catchlight',(-3.25,.65,-.8),(.15,.53,.43),'glint',8,6)
    for s in (-1,1):
        tube('armored upper eyelid',[(.0,.65,s*7.0),(-.5,3.5,s*5.1),(-.2,4.5,s*2.5),(.35,4.65,0)],[.63,.74,.59,.38],'shell',8)
    for upper,lower,length in ((9,8,17.43),(14,13,9.64),(19,18,17.43),(24,23,9.64)):
        context['limb']=upper
        loft('femur tendon',[(-2,0,1.4,1.4),(0,0,1.9,1.9),(length*.5,0,1.55,1.65),(length,0,1.55,1.55)],'joint',10)
        loft('tapered femur shell',[(.8,0,1.7,1.8),(3,0,2.35,2.65),(length*.65,0,2.2,2.3),(length-1.0,0,1.5,1.8),(length+.4,0,.9,1.1)],'shell',8)
        tube('femur ridge',[(2,2.3,0),(length*.62,2.4,0),(length,1.3,0)],[.19,.22,.07],'edge',6)
        tube('swept knee spur',[(length-1,0,0),(length+2.4,1.5,.0),(length+4.7,1.1,0)],[1.4,.9,.045],'shell',7)
        context['limb']=lower
        orb('articulating knee',(0,0,0),(2.05,2.05,2.05),'joint',12,8)
        loft('slender tibia',[(.8,.0,1.9,1.9),(4,.1,2.1,2.0),(10,.1,1.25,1.2),(19,-.1,.87,.9),(24,-.4,1.45,1.5),(27,-1.3,.75,.8)],'shell',10)
        tube('tibial crest',[(1.5,2,0),(5,2.25,0),(17,1,0),(24,1,0)],[.21,.24,.12,.07],'edge',6)
        # Tips flank the native (2800,-200,0) dust/ripple attachment point.
        for sign in (-1,1):
            tube('splayed landing claw',[(24,-.4,sign*.65),(26,-1.6,sign*1.8),(28,-2,sign*2.2),(30.6,-1.1,sign*2.7)],[1.1,.77,.44,.04],'claw',8)
        plate('ankle scute',[(21.8,.2,-1.1),(24.1,3.4,0),(26.1,-.2,1.6),(25,-.9,-1.5)],.8,'shell',(0,1,0))
        if variant==1:
            # Blue water-walkers retain the same anchors, with a broader foot web.
            plate('water strider foot web',[(24,-1.8,-.6),(27.9,-2,2),(29.1,-1.6,0),(27.9,-2,-2)],.17,'edge',(0,1,0))

for variant in range(2):anatomy()
scene.frame_start=1;scene.frame_end=rig['frames'];scene.render.fps=20;scene.frame_set(1)
scene.world=bpy.data.worlds.get('Tektite Studio') or bpy.data.worlds.new('Tektite Studio');scene.world.use_nodes=True
bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs[0].default_value=(.07,.085,.11,1);bg.inputs[1].default_value=.7
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
formats=[i.identifier for i in bpy.ops.export_scene.gltf.get_rna_type().properties['export_format'].enum_items]
assert not formats or 'GLB' in formats
bpy.ops.export_scene.gltf(filepath=str(ROOT/'tektites.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
print('Exported',len(parts),'native-limb Tektite pieces. Live project remains:',bpy.data.filepath)
