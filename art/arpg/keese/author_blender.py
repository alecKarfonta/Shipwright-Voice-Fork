"""Build upgraded native Keese in an isolated scene using the live Blender MCP.

Mesh data stays in OoT joint-local space; object animation is an authoring preview
of the eight original flight poses. Runtime continues to use EnFirefly's rig.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parent
rig = json.loads((ROOT / 'native_rig.json').read_text())
scene = bpy.data.scenes.get('ARPG Keese Workshop') or bpy.data.scenes.new('ARPG Keese Workshop')
bpy.context.window.scene = scene
for ob in list(scene.objects):
    bpy.data.objects.remove(ob, do_unlink=True)
axis = Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS = ['fur','ruff','membrane','web','rib','ear','claw','eye','pupil','mouth','accent']
PALETTES = [
    [(84,74,119),(123,109,154),(89,70,111),(119,87,131),(73,59,91),(157,111,143),(222,218,187),(255,221,111),(25,17,27),(45,24,42),(180,142,91)],
    [(72,53,59),(116,68,56),(132,51,34),(190,77,36),(49,35,39),(214,92,42),(238,199,140),(255,233,117),(43,20,21),(42,17,22),(244,151,42)],
    [(107,150,171),(164,196,203),(73,126,168),(112,177,201),(42,77,116),(151,210,218),(229,242,222),(207,255,238),(17,44,69),(26,50,79),(172,224,237)],
]
materials = {}
for sp,palette in enumerate(PALETTES):
    for name,rgb in zip(MATERIALS,palette):
        mat=bpy.data.materials.get(f'Keese {sp} {name}') or bpy.data.materials.new(f'Keese {sp} {name}')
        mat.use_nodes=True;mat.diffuse_color=(*[v/255 for v in rgb],1)
        node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
        node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
        node.inputs['Roughness'].default_value=.42 if name in ('claw','eye','accent') else .78
        materials[sp,name]=mat
parts=[]
variant=0
limb=10

def mesh(name,points,faces,material='fur',smooth=False,inks=None):
    data=bpy.data.meshes.new(name);data.from_pydata(points,[],faces);data.update()
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    for key in MATERIALS:data.materials.append(materials[variant,key])
    for i,p in enumerate(data.polygons):
        p.material_index=MATERIALS.index(inks[i] if inks else material);p.use_smooth=smooth
    ob=bpy.data.objects.new(f'Keese.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='gKeeseFlyAnim';ob['native_units_per_authoring_unit']=100
    for f,pose in enumerate(rig['poses']):
        ob.matrix_world=Matrix.Translation(((variant-1)*100,0,0)) @ axis @ Matrix(pose[str(limb)])
        ob.keyframe_insert(data_path='location',frame=f+1)
        ob.rotation_mode='QUATERNION';ob.keyframe_insert(data_path='rotation_quaternion',frame=f+1)
    parts.append(ob);return ob

def loft(name,rings,mat='fur',sections=12,striped=False):
    pts=[]
    for x,cy,ry,rz in rings:
        for i in range(sections):
            a=2*math.pi*i/sections;pts.append((x,cy+ry*math.cos(a),rz*math.sin(a)))
    faces=[tuple(reversed(range(sections)))];inks=[mat]
    for j in range(len(rings)-1):
        for i in range(sections):
            faces.append((j*sections+i,j*sections+(i+1)%sections,(j+1)*sections+(i+1)%sections,(j+1)*sections+i))
            inks.append('ruff' if striped and 4<=i<=7 else mat)
    faces.append(tuple((len(rings)-1)*sections+i for i in range(sections)));inks.append(mat)
    return mesh(name,pts,faces,mat,True,inks)

def tube(name,path,radii,mat='rib',sides=7):
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

def wing(section):
    # The straight internal hinge boundaries exactly follow the original rig.
    # Only silhouette edges are scalloped; neighboring panels must never separate.
    if section==0:
        outline=[(0,0,0),(11.4,14.23,0),(11.4,-14.24,0),(8.3,-8.7,0),(4.3,-5.5,0)]
        bones=[[(0,0,0),(5.2,6.9,.12),(11.4,14.23,0)]]
        veins=[[(0,0,.17),(5.6,-3.2,.21),(11.4,-14.24,.1)]]
    elif section==1:
        outline=[(0,14.23,0),(10.61,-3.31,0),(7.1,-5.5,0),(4.3,-8.1,0),(0,-14.24,0)]
        bones=[];veins=[[(0,14.23,.18),(5.2,3.8,.27),(10.61,-3.31,.12)]]
    else:
        outline=[(0,16.64,0),(26.58,-7.41,0),(20.0,-4.9,0),(15.7,-4.8,0),(13.4,-6.45,0),(8.8,-4.4,0),(4.6,-3.4,0),(0,-3.87,0)]
        bones=[[(0,16.64,0),(7.8,10.1,.15),(17.9,.3,.08),(26.58,-7.41,0)]]
        veins=[[(0,16.64,.12),(6.5,7,.35),(13.4,-6.45,.12)],[(0,16.64,.12),(2.4,5.5,.3),(4.6,-3.4,.12)]]
    # Two raised radial rings give the membrane a restrained tensioned surface.
    center=sum((Vector(p) for p in outline),Vector())/len(outline);n=len(outline)
    pts=[tuple(Vector(p)+Vector((0,0,.08))) for p in outline]
    pts += [tuple(Vector(p).lerp(center,.40)+Vector((0,0,.30))) for p in outline]
    pts += [tuple(center+Vector((0,0,.34)))]
    faces=[];inks=[]
    for i in range(n):
        faces.append((i,(i+1)%n,n+(i+1)%n,n+i));inks.append('membrane')
        faces.append((n+i,n+(i+1)%n,2*n));inks.append('web' if i%3 else 'membrane')
    # The underside is independent and reversed: no mirrored normals or paper edge.
    front=len(pts);pts += [(x,y,-z) for x,y,z in pts]
    for face,ink in list(zip(faces,inks)):faces.append(tuple(front+i for i in reversed(face)));inks.append(ink)
    for i in range(n):faces.append((i,front+i,front+(i+1)%n,(i+1)%n));inks.append('rib')
    mesh('tensioned wing membrane',pts,faces,'membrane',False,inks)
    for p in bones:tube('slender leading finger',p,[.40-.09*j for j in range(len(p))])
    for p in veins:tube('membrane finger',p,[.26,.17,.055],sides=6)
    if section==2:
        tube('curved thumb claw',[(0,16.5,0),(-1.0,18.5,.2),(-1.8,19,.0),(-1.4,17.8,-.15)],[.48,.33,.17,.03],'claw')
        if variant==2:
            for t in (.27,.52):
                p=Vector((0,16.64,0)).lerp(Vector((26.58,-7.41,0)),t)
                tube('frost ridge',[tuple(p),tuple(p+Vector((-1.1,1.5,.8))),tuple(p+Vector((-.8,2.6,.5)))],[.58,.36,.035],'accent',5)
        elif variant==1:
            for t in (.25,.50,.74):
                p=Vector((0,16.64,0)).lerp(Vector((26.58,-7.41,0)),t)
                tube('ember seam',[tuple(p+Vector((-.7,.55,.37))),tuple(p+Vector((.8,-.8,.30)))],[.12,.09],'accent',5)

for variant in range(3):
    limb=10
    loft('tapered chest and abdomen',[(-1,0,1.8,2.5),(2,.6,3.3,4.2),(6,1.0,3.3,4.5),(11,.1,2.4,3.6),(16,-.2,1.55,2.3),(20,.3,.45,.45)],striped=True)
    # A few flat, swept fur points define the shoulder without inflating the torso.
    for s in (-1,1):
        for i in range(3):
            x=2+i*2.3
            plate('swept shoulder fur',[(x,-1.5,s*3.4),(x+2,-1.6,s*4.1),(x+4,-1.0,s*(5.1-i*.4)),(x+3,.1,s*3.5)],.22,'ruff',normal=(0,0,s))
    for limb in (6,9):
        tube('tarsus',[(0,0,0),(3.4,.2,0),(5,-.1,0)],[.58,.45,.33],'fur')
        for z in (-1.7,0,1.7):
            tube('hooked toe',[(3.7,0,0),(5.8,-.2,z),(7.6,-.1,z*1.15),(8,-1,z*1.08)],[.36,.28,.19,.025],'claw',6)
    limb=27
    loft('angular bat skull',[(-5,-.1,1.3,2.1),(-3,-.25,2.8,3.7),(0,-.5,3.25,4.2),(3,-.7,2.7,3.4),(5.5,-.5,1.9,1.9),(6.5,-.3,.8,.7)],striped=True)
    for s in (-1,1):
        ear=[(-3,-1.1,s*3),(-5,-2.0,s*5.4),(-12,-2,s*9.2),(-9,1.2,s*7),(-4,1.7,s*3.5)]
        plate('tall swept ear',ear,.8,'fur')
        plate('recessed ear', [(-4.6,-2.0,s*3.9),(-6,-2.2,s*5.4),(-10.8,-2.25,s*8.25),(-8.3,-1.5,s*6.4)],.12,'ear')
        eye=[(-1.35,-3.5,s*.9),(-2.5,-2.8,s*3.4),(-.55,-3.0,s*3.9),(1.2,-3.7,s*2.2),(.7,-3.95,s*1.2)]
        plate('deep eye socket',eye,.28,'pupil')
        plate('golden bat eye',[(-1.05,-3.84,s*1.2),(-2.1,-3.12,s*3.2),(-.45,-3.4,s*3.52),(.85,-4.02,s*2.2),(.35,-4.23,s*1.4)],.10,'eye')
        plate('narrow pupil',[(-1.72,-3.88,s*2.2),(-.55,-4.03,s*2.53),(.58,-4.16,s*2.18),(-.55,-4.18,s*2.02)],.055,'pupil')
        tube('sharp brow',[(-1.6,-3.85,s*.9),(-2.3,-3.5,s*2.3),(-2.65,-2.55,s*3.7)],[.28,.41,.21],'ruff')
        tube('ivory fang',[(3.5,-2.9,s*1.5),(4.8,-3.6,s*1.45),(6.8,-3.1,s*1.1)],[.49,.33,.025],'claw',8)
    plate('open mouth',[(2.8,-3.32,-1.7),(2.8,-3.32,1.7),(5.4,-2.65,1.2),(5.4,-2.65,-1.2)],.14,'mouth')
    plate('nose leaf',[(.4,-3.72,0),(2.6,-4.4,1.15),(3.25,-3.7,0),(2.6,-4.4,-1.15)],.35,'fur')
    for s in (-1,1):
        plate('nostril',[(2.1,-4.53,s*.4),(2.5,-4.65,s*.77),(2.9,-4.22,s*.45)],.06,'pupil')
    for limb,section in ((17,0),(16,1),(15,2),(24,0),(23,1),(22,2)):wing(section)

scene.frame_start=1;scene.frame_end=8;scene.render.fps=20;scene.frame_set(5)
scene.world=bpy.data.worlds.get('Keese Studio') or bpy.data.worlds.new('Keese Studio');scene.world.use_nodes=True
bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs[0].default_value=(.07,.085,.11,1);bg.inputs[1].default_value=.7
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
formats=[i.identifier for i in bpy.ops.export_scene.gltf.get_rna_type().properties['export_format'].enum_items]
assert not formats or 'GLB' in formats  # Blender 5 uses a dynamic enum.
bpy.ops.export_scene.gltf(filepath=str(ROOT/'keese.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
print('Exported',len(parts),'native-limb Keese pieces. Live project remains:',bpy.data.filepath)
