"""Native Deku Baba art. Run in Blender or through the local MCP.
Only the ARPG DekuBaba Workshop scene is replaced. Runtime keeps native AI/rigs.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parent
rig=json.loads((ROOT/'native_rig.json').read_text())
scene=bpy.data.scenes.get('ARPG DekuBaba Workshop') or bpy.data.scenes.new('ARPG DekuBaba Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['skin','ridge','spot','lip','interior','gum','tooth','stem','leaf','vein','bark']
PALETTES=[
 [(54,87,148),(85,131,175),(220,216,170),(191,168,93),(92,31,55),(172,72,91),(242,226,166),(68,108,51),(54,130,67),(147,178,69),(115,77,44)],
 [(58,58,112),(112,105,156),(230,188,119),(186,140,69),(77,25,42),(155,57,75),(242,217,144),(54,85,44),(44,102,55),(149,152,66),(99,62,38)]
]
materials={}
for sp,palette in enumerate(PALETTES):
 for name,rgb in zip(MATERIALS,palette):
  mat=bpy.data.materials.get(f'DekuBaba {sp} {name}') or bpy.data.materials.new(f'DekuBaba {sp} {name}')
  mat.use_nodes=True;mat.diffuse_color=(*[v/255 for v in rgb],1)
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
  node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Roughness'].default_value=.24 if name in ('interior','gum') else .48 if name in ('skin','ridge') else .78
  materials[sp,name]=mat
parts=[];variant=0;context={'limb':4}
def mesh(name,points,faces,material='skin',smooth=False,inks=None):
    limb=context['limb']
    data=bpy.data.meshes.new(name);data.from_pydata(points,[],faces);data.update()
    for key in MATERIALS:data.materials.append(materials[variant,key])
    for i,p in enumerate(data.polygons):
        p.material_index=MATERIALS.index(inks[i] if inks else material);p.use_smooth=smooth
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    ob=bpy.data.objects.new(f'DekuBaba.{variant}.{limb:02}.{name}',data);scene.collection.objects.link(ob)
    ob['arpg_variant']=variant;ob['arpg_limb']=limb
    ob['native_animation']='native jaw frames; illustrative rooted stem pose';ob['native_units_per_authoring_unit']=100
    for f,pose in enumerate(rig['preview']):
        ob.matrix_world=Matrix.Translation(((variant-.5)*100,0,0)) @ axis @ Matrix(pose[str(limb)])
        ob.keyframe_insert(data_path='location',frame=f+1)
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

def tube(name,path,radii,mat='stem',sides=7):
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


def leaf(name,angle,length,width,rise,mat='leaf',origin=(0,0,0)):
    pts=[];rows=9;cols=5
    for layer in range(2):
        for i in range(rows):
            t=i/(rows-1);w=width*(math.sin(math.pi*t)**.8)*(.8+.2*t)+.025
            if i%2:w*=.87
            for j in range(cols):
                u=2*j/(cols-1)-1;along=length*t
                y=rise*math.sin(math.pi*t*.88)+1.1*(1-u*u)*math.sin(math.pi*t)-layer*.16
                pts.append((origin[0]+along*math.sin(angle)+u*w*math.cos(angle),origin[1]+y,origin[2]+along*math.cos(angle)-u*w*math.sin(angle)))
    n=rows*cols;faces=[];inks=[]
    for i in range(rows-1):
        for j in range(cols-1):
            q=(i*cols+j,(i+1)*cols+j,(i+1)*cols+j+1,i*cols+j+1)
            faces.extend([q,tuple(n+k for k in reversed(q))]);inks.extend([mat,'stem'])
    border=list(range(cols))+[i*cols+cols-1 for i in range(1,rows)]+list(range(n-2,n-cols-1,-1))+[i*cols for i in range(rows-2,0,-1)]
    for i,j in zip(border,border[1:]+border[:1]):faces.append((i,j,j+n,i+n));inks.append(mat)
    mesh(name,pts,faces,mat,True,inks)
    path=[(origin[0]+length*t*math.sin(angle),origin[1]+rise*math.sin(math.pi*t*.88)+1.1*math.sin(math.pi*t)+.1,origin[2]+length*t*math.cos(angle)) for t in (0,.2,.4,.6,.8,1)]
    tube(name+' central vein',path,[.18,.24,.23,.18,.12,.025],'vein',6)

xs=[-5,-2,3,8,13,18,22,24]
upperXs=[-8,-5,0,5,10,16,21,24]
upper=[(.4,.5),(14.4,14),(12.5,9),(10,7),(9.5,5.7),(9.4,5.4),(6.0,2.8),(.3,.4)]
lower=[(.6,.5),(4.5,2.5),(8,5.5),(9.5,8),(9.4,8),(7.6,6.5),(4.7,4),(0.3,.5)]
def profile(x,upperJaw):
    rings=upper if upperJaw else lower
    coords=upperXs if upperJaw else xs
    for i in range(len(coords)-1):
        if x<=coords[i+1]:
            t=(x-coords[i])/(coords[i+1]-coords[i]);return tuple(rings[i][k]*(1-t)+rings[i+1][k]*t for k in (0,1))
    return rings[-1]
def surface(x,a,upperJaw,extra=0):
    w,h=profile(x,upperJaw);s=-1 if upperJaw else 1
    flute=1+.025*math.cos(6*a)
    return (x,s*(h*math.sin(a)*flute+extra)+.8,w*math.cos(a)*flute)
def jaw(upperJaw):
    context['limb']=4 if upperJaw else 7;steps=13;pts=[]
    coords=upperXs if upperJaw else xs
    for inside in (False,True):
        for x in coords:
            for j in range(steps):
                a=math.pi*j/(steps-1);p=surface(x,a,upperJaw)
                if inside:p=(p[0],.8+(-1 if upperJaw else 1)*2.0*math.sin(a)*math.sin(math.pi*(x-coords[0])/(coords[-1]-coords[0])),p[2])
                pts.append(p)
    n=len(pts)//2;faces=[];inks=[]
    for i in range(len(coords)-1):
        for j in range(steps-1):
            q=(i*steps+j,(i+1)*steps+j,(i+1)*steps+j+1,i*steps+j+1)
            faces.extend([q,tuple(n+k for k in reversed(q))]);inks.extend(['skin' if j%4 else 'ridge','interior'])
    border=list(range(steps))+[i*steps+steps-1 for i in range(1,len(coords))]+list(range(n-2,n-steps-1,-1))+[i*steps for i in range(len(coords)-2,0,-1)]
    for i,j in zip(border,border[1:]+border[:1]):faces.append((i,j,j+n,i+n));inks.append('lip')
    cap=mesh('fluted upper flower cap' if upperJaw else 'cupped lower jaw',pts,faces,'skin',True,inks)
    bm=bmesh.new();bm.from_mesh(cap.data);bvh=BVHTree.FromBMesh(bm);bm.free()
    lip=[surface(x,0,upperJaw) for x in coords]+[surface(x,math.pi,upperJaw) for x in reversed(coords)];lip.append(lip[0])
    tube('rolled fleshy mouth rim',lip,[.55]*len(lip),'lip',8)
    s=1 if upperJaw else -1
    for side in (-1,1):
        for index,x in enumerate((3,8,13,18,22) if upperJaw else (5.5,10.5,15.5,20.5)):
            w,h=profile(x,upperJaw);z=side*(w-.45);length=(3.6 if upperJaw else 3.0)*(1 if index<4 else .75)
            tube('interlocking hooked tooth',[(x,.6,z),(x+.3,.6+s*length*.42,z*.94),(x+.65,.6+s*length,z*.76)], [.78,.53,.025],'tooth',7)
        for x in (5,13,20):
            a=.35 if side==1 else math.pi-.35
            path=[surface(x+t,a,upperJaw,.15) for t in (-1.2,0,1.5)]
            tube('floral edge vein',path,[.12,.17,.035],'ridge',6)
    # Raised ivory botanical markings follow the curved shell, not floating disks.
    if upperJaw:
        for x,a,rx,ra in [(1,1.57,2.3,.19),(9,.8,2.2,.2),(9,2.34,2.2,.2),(17,1.57,2,.24)]+([(17,.6,1.2,.13),(17,2.54,1.2,.13)] if variant else []):
            ps=[surface(x,a,True)]+[surface(x+rx*r*math.cos(t),a+ra*r*math.sin(t),True) for r in (1/3,2/3,1) for t in [j*math.pi/6 for j in range(12)]]
            for k,p in enumerate(ps):
                hit,normal,_,_=bvh.ray_cast(Vector((p[0],-40,p[2])),Vector((0,1,0)))
                assert hit is not None,'Marking escaped the authored cap'
                ps[k]=(hit.x,hit.y-.13,hit.z)
            fs=[(0,j+1,(j+1)%12+1) for j in range(12)]
            for ring in range(2):
                for j in range(12):fs.append((1+ring*12+j,1+(ring+1)*12+j,1+(ring+1)*12+(j+1)%12,1+ring*12+(j+1)%12))
            mesh('ivory petal marking',ps,fs,'spot',True)
    # Recessed palate folds add depth when the original bite animation opens.
    for x in (7,12,17):
        w,h=profile(x,upperJaw)
        path=[(x,.8-s*(.6+1.4*(1-u*u)),w*u*.8) for u in (-.75,-.4,0,.4,.75)]
        tube('palate fold',path,[.12,.24,.3,.24,.12],'gum',6)

def anatomy():
    jaw(True);jaw(False)
    for part in (8,9,10):
        context['limb']=part
        path=[(0,0,z) for z in (0,1,5,10,15,19,20.4)]
        tube('fibrous stem',path,[1.05,1.15,.94,.92,.97,1.12,1.05],'stem',12)
        for j in range(5):
            a=j*2*math.pi/5
            path=[(math.cos(a+z*.035)*1.04,math.sin(a+z*.035)*1.04,z) for z in (0,5,10,15,20)]
            tube('raised stem fiber',path,[.12,.16,.15,.17,.1],'vein',5)
        # Small overlapping growth nodes hide the original rigid stem hinges.
        orb('flexible growth node',(0,0,.2),(1.18,1.18,1.18),'stem',12,6)
        if part==9:
            for s in (-1,1):tube('swept stem thorn',[(s*.8,0,7+s*2),(s*2.2,-.6,8+s*2),(s*2.8,-.9,10+s*2)],[.55,.36,.025],'lip',7)
        if part==8:
            for j in range(3):
                a=j*2*math.pi/3
                tube('neck sepal',[(math.sin(a)*.9,math.cos(a)*.9,16),(math.sin(a)*2.1,math.cos(a)*2.1,19),(math.sin(a)*1.5,math.cos(a)*1.5,21)],[.65,.9,.025],'leaf',7)
    context['limb']=11
    orb('root crown',(0,1.5,0),(3.1,1.8,3.1),'bark',12,7)
    for j in range(3):leaf('broad rooted leaf',j*2*math.pi/3,24,8,5)
    for j in range(3):leaf('inner sepal',j*2*math.pi/3+math.pi/3,11,3,8)
    context['limb']=12
    tube('cut woody stalk',[(0,-10.2,0),(.12,-6,.1),(-.1,-1,-.15),(.05,5,.0),(.0,10.5,.08),(.0,12.3,.1)],[.6,.67,.61,.6,.48,.26],'bark',10)
    for j in range(3):
        a=j*2*math.pi/3
        tube('stick bark seam',[(math.sin(a)*.6,-9,math.cos(a)*.6),(math.sin(a+.2)*.63,0,math.cos(a+.2)*.63),(math.sin(a)*.48,10,math.cos(a)*.48)],[.05,.07,.03],'lip',5)
    tube('broken side bud',[(.0,4,.0),(.45,5,.0),(.64,5.7,0)],[.4,.3,.18],'bark',7)

for variant in range(2):anatomy()
scene.frame_start=1;scene.frame_end=rig['frames'];scene.render.fps=20;scene.frame_set(1)
scene.world=bpy.data.worlds.get('Deku Baba Studio') or bpy.data.worlds.new('Deku Baba Studio');scene.world.use_nodes=True
bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs[0].default_value=(.07,.085,.11,1);bg.inputs[1].default_value=.7
for ob in scene.objects:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
formats=[i.identifier for i in bpy.ops.export_scene.gltf.get_rna_type().properties['export_format'].enum_items]
assert not formats or 'GLB' in formats
bpy.ops.export_scene.gltf(filepath=str(ROOT/'dekubabas.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=False)
print('Exported',len(parts),'native Deku Baba pieces. Live project preserved:',bpy.data.filepath)
