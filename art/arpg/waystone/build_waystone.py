"""Original oath shrine and articulated cache. Run with local Blender MCP.

Only ARPG Waystone Workshop is rebuilt. Vertices use native X/Y-up/Z-forward;
object transforms arrange the studio view and are omitted by the compiler.
Parts 1 and 6 are local to their runtime crystal and lid pivots.
"""
import bpy
import bmesh
import math
from pathlib import Path
from mathutils import Matrix, Vector
ROOT=Path(__file__).resolve().parent
scene=bpy.data.scenes.get('ARPG Waystone Workshop') or bpy.data.scenes.new('ARPG Waystone Workshop')
bpy.context.window.scene=scene
for ob in list(scene.objects): bpy.data.objects.remove(ob,do_unlink=True)
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
names=['stone','cut','bronze','gold','wood','grain','iron','velvet','oath','gem','shadow']
rgb=[(57,76,81),(110,135,133),(133,96,53),(221,178,92),(69,43,34),(109,67,43),(51,61,70),(52,31,58),(136,213,207),(168,233,228),(28,36,41)]
materials={};objects=[];ctx={'part':0}
for name,color in zip(names,rgb):
    mat=bpy.data.materials.get('Waystone '+name) or bpy.data.materials.new('Waystone '+name)
    mat.diffuse_color=(*[v/255 for v in color],1);mat.use_nodes=True
    n=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    n.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in color],1)
    n.inputs['Metallic'].default_value=.7 if name in ('bronze','gold','iron') else 0
    n.inputs['Roughness'].default_value=.32 if name in ('bronze','gold','gem') else .78
    materials[name]=mat

def mesh(name,verts,faces,mat='stone',smooth=False):
    data=bpy.data.meshes.new(name);data.from_pydata(verts,[],faces);data.update()
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    data.materials.append(materials[mat])
    for p in data.polygons: p.use_smooth=smooth
    ob=bpy.data.objects.new(name,data);scene.collection.objects.link(ob);ob['arpg_part']=ctx['part']
    pivot=(0,36,0) if ctx['part']==1 else (0,14,15) if ctx['part']==6 else (0,0,0)
    ob.matrix_world=axis @ Matrix.Translation(pivot)
    # Show the lid at its earned-cache angle; runtime animates the same hinge.
    if ctx['part']==6: ob.matrix_world=ob.matrix_world @ Matrix.Rotation(-1.35,4,'X')
    # Only one emblem sits on the shrine; four other designs form a side study.
    if 9<=ctx['part']<=12: ob.matrix_world=axis @ Matrix.Translation((39+(ctx['part']-9)*15,-43,22))
    if ctx['part']>=13: ob.matrix_world=axis @ Matrix.Translation((43,0,-8))
    objects.append(ob);return ob

def plate(name,points,depth,mat='stone',normal=(0,0,1),bevel=.08):
    pts=[Vector(p) for p in points];n=Vector(normal);center=sum(pts,Vector())/len(pts);N=len(pts)
    v=[tuple(p-n*depth*.3) for p in pts]+[tuple(p+n*depth*.3) for p in pts]+[tuple(p.lerp(center,bevel)+n*depth*.7) for p in pts]
    f=[tuple(reversed(range(N)))]
    for j in range(2):
        for i in range(N): f.append((j*N+i,j*N+(i+1)%N,(j+1)*N+(i+1)%N,(j+1)*N+i))
    f.append(tuple(2*N+i for i in range(N)));return mesh(name,v,f,mat)

def box(name,center,size,mat='stone',bevel=.13):
    x,y,z=center;a,b,c=[v/2 for v in size]
    ob=mesh(name,[(x+i*a,y+j*b,z+k*c) for i,j,k in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]],[(0,3,2,1),(4,5,6,7),(0,1,5,4),(3,7,6,2),(0,4,7,3),(1,2,6,5)],mat)
    if bevel:
        mod=ob.modifiers.new('Carved edge','BEVEL');mod.width=bevel;mod.segments=2
    return ob

def sweep(name,points,radii,mat='bronze',sides=8):
    pts=[Vector(p) for p in points];v=[]
    for i,p in enumerate(pts):
        t=(pts[min(i+1,len(pts)-1)]-pts[max(i-1,0)]).normalized();u=t.cross(Vector((0,0,1)) if abs(t.z)<.9 else Vector((0,1,0))).normalized();w=t.cross(u)
        for j in range(sides):
            a=j*math.tau/sides;v.append(tuple(p+radii[i]*(math.cos(a)*u+math.sin(a)*w)))
    f=[tuple(reversed(range(sides)))]
    for i in range(len(pts)-1):
        for j in range(sides): f.append((i*sides+j,i*sides+(j+1)%sides,(i+1)*sides+(j+1)%sides,(i+1)*sides+j))
    f.append(tuple((len(pts)-1)*sides+j for j in range(sides)));return mesh(name,v,f,mat,True)

def ring(name,center,rx,ry,thick,mat='bronze',N=32):
    x,y,z=center;v=[]
    for dz,r in [(-thick*.5,1),(thick*.5,1),(thick*.5,1-thick/min(rx,ry)),(-thick*.5,1-thick/min(rx,ry))]:
        for j in range(N):
            a=math.tau*j/N;v.append((x+rx*math.cos(a)*r,y+ry*math.sin(a)*r,z+dz))
    f=[]
    for k in range(4):
        for j in range(N):f.append((k*N+j,k*N+(j+1)%N,((k+1)%4)*N+(j+1)%N,((k+1)%4)*N+j))
    return mesh(name,v,f,mat)

def crystal(name,center,r,h,mat='gem',N=6):
    x,y,z=center;v=[(x,y-h*.7,z)]
    for yy,rr in [(-h*.25,r*.85),(h*.2,r)]:
        v += [(x+rr*math.cos(i*math.tau/N),y+yy,z+rr*math.sin(i*math.tau/N)) for i in range(N)]
    v.append((x+r*.2,y+h*.75,z));f=[]
    for i in range(N):
        j=(i+1)%N;f.extend([(0,1+j,1+i),(1+i,1+j,1+N+j,1+N+i),(1+N+i,1+N+j,len(v)-1)])
    return mesh(name,v,f,mat)

# Cut octagonal masonry and a broad front apron ground the shrine in the scene.
for y,r,d in [(1,22,2),(3,20,2),(4.6,17.8,1.2)]:
    points=[(r*math.cos(a*math.tau/8+math.pi/8),y,r*math.sin(a*math.tau/8+math.pi/8)) for a in range(8)]
    plate('Octagonal dressed plinth',points,d,'stone' if y!=3 else 'cut',(0,1,0),.035)
plate('Coffer foundation',[(-15,1.1,12),(15,1.1,12),(16,1.1,31),(12,1.1,35),(-12,1.1,35),(-16,1.1,31)],2,'stone',(0,1,0),.055)
for x in (-15,15):
    # Broad chiseled pillar sections and angular inward-leaning crown.
    profile=[(x-3.6,5),(x+3.6,5),(x+3.2,29),(x*.77+3,44),(x*.47+2,53),(x*.47-2,54),(x*.77-3,44),(x-3.2,29)]
    plate('Split oath pylon',[(a,b,0) for a,b in profile],6,'stone',bevel=.07)
    line=[(x,6,4.3),(x,27,4.3),(x*.76,43,4.3),(x*.47,51.5,4.3)]
    sweep('Inlaid bronze spine',line,[.30]*4,'bronze',8)
    for j in range(5):
        y=9+j*5;xx=x if j<4 else x*.96
        plate('Chisel terrace',[(xx-2.9,y,3.3),(xx+2.9,y,3.3),(xx+2.9,y+.45,3.3),(xx-2.9,y+.45,3.3)],.5,'cut',bevel=.01)
    # Geometric carving grooves are inset dark bands with bright adjacent lips.
    for y in (13,23,34):
        xx=x if y<30 else x*.91
        sweep('Pylon carved rune',[(xx-1.6,y,4.05),(xx,y+1.3,4.15),(xx+1.4,y+.7,4.05)],[.15,.15,.15],'shadow',6)
    for z in (-3.9,3.9):
        box('Pylon foot fitting',(x,7,z),(7.2,2,.8),'bronze',.15)
        for dx in (-2.5,2.5): crystal('Foot rivet',(x+dx,7,z+.4),.25,.5,'gold',6)
ring('Suspension astrolabe',(0,36,0),10.3,12.5,.55,'bronze',48)
for i in range(12):
    a=math.tau*i/12;x=10.3*math.cos(a);y=36+12.5*math.sin(a)
    plate('Astrolabe tick',[(x-.25,y-.5,.4),(x+.25,y-.5,.4),(x+.25,y+.5,.4),(x-.25,y+.5,.4)],.3,'gold',bevel=.1)
plate('Oath crest backing',[(-6,50,1),(0,57,1),(6,50,1),(0,45,1)],1.6,'iron',bevel=.1)
plate('Wave light rail',[(12,8.5,4.3),(18,8.5,4.3),(18,31.5,4.3),(12,31.5,4.3)],.85,'cut',bevel=.035)
for i,y in enumerate((12,20,28)):
    x=15
    ring('Wave socket',(x,y,5.1),2.4,2.4,.52,'bronze',16)
    ctx['part']=2+i
    plate('Wave lens',[(x,y+2,5.2),(x+2,y,5.2),(x,y-2,5.2),(x-2,y,5.2)],.5,'oath',bevel=.17)
    ctx['part']=0
ctx['part']=1
crystal('Suspended oath heart',(0,0,0),3.2,12,'oath',7)
for i in range(3):
    a=math.tau*i/3;crystal('Orbit shard',(5.2*math.cos(a),-3.2+2*i,5.2*math.sin(a)),.65,2.7,'gem',5)

# Hollow wood coffer, individual planks, forged corner straps and a real lid hinge.
ctx['part']=5
box('Coffer floor',(0,5.5,23),(24,2,16),'wood')
box('Velvet lining',(0,6.6,23),(21,.5,13),'velvet',.2)
for z in (15,31):
    for i in range(8): box('Vertical walnut plank',(-10.4+i*2.97,10,z),(2.8,8,1.2),'wood',.16)
for x in (-12,12):
    for i in range(5): box('Side walnut plank',(x,10,16.4+i*3.25),(1.2,8,3.05),'wood',.16)
for y in (6.5,13.6):
    box('Front forged band',(0,y,31.7),(25,1.3,.7),'bronze')
    box('Rear forged band',(0,y,14.3),(25,1.3,.7),'bronze')
    for x in (-12.7,12.7):box('Side forged band',(x,y,23),(.7,1.3,18),'bronze')
for x in (-10,10):
    box('Iron corner strap',(x,10,31.8),(1.3,8.2,.8),'iron')
    for y in (7,10,13): crystal('Corner strap nail',(x,y,32.35),.26,.48,'gold',6)
    for z in (16,30):box('Coffer carved foot',(x,3.6,z),(3.4,3.5,3.3),'iron',.4)
for x in (-8,8):
    sweep('Hinge barrel',[(x-1.6,14,15),(x+1.6,14,15)],[.7,.7],'gold',12)
for i in range(7):
    x=-9+i*3
    sweep('Walnut grain',[(x,7.5,31.67),(x+.35,9,31.68),(x-.2,11,31.68),(x+.25,12.5,31.67)],[.045]*4,'grain',5)
ring('Coffer front lock',(0,10.5,32),2,2.5,.6,'gold',16)
plate('Lock keyhole',[(-.4,11.4,32.2),(.4,11.4,32.2),(.15,10.6,32.2),(.5,9.3,32.2),(-.5,9.3,32.2),(-.15,10.6,32.2)],.15,'shadow',bevel=.02)
ctx['part']=6
# Lid local origin is (0,14,15), positive local Z extends toward the front.
for i in range(8):
    x=-10.6+i*3.03
    profile=[(0,0),(4,2.5),(8,3.7),(12,3),(17,0),(17,-.85),(0,-.85)]
    plate('Arched lid plank',[(x,y,z) for z,y in profile],2.9,'wood',(1,0,0),.025)
for x in (-9.5,0,9.5):
    sweep('Lid iron rib',[(x,0,0),(x,2.6,4),(x,3.85,8),(x,3.15,12),(x,0,17)],[.32]*5,'iron',8)
    for z,y in [(3,2),(8,3.85),(13,2.5)]:crystal('Lid brass rivet',(x,y+.25,z),.24,.6,'gold',6)
for z in (0,17):box('Lid end rim',(0,-.1,z),(25,1.2,.9),'bronze')
plate('Lid inner leather panel',[(-10,-.9,2),(10,-.9,2),(10,-.9,15),(-10,-.9,15)],.35,'velvet',(0,-1,0),.055)
ctx['part']=7
# This interior exists only while the one earned cache is unclaimed.
for i in range(17):
    x=8*math.sin(i*2.4);z=23+4.5*math.cos(i*1.7);y=7.1+(i%3)*.42
    pts=[(x+1.45*math.cos(a*math.tau/8),y,z+1.45*math.sin(a*math.tau/8)) for a in range(8)]
    plate('Stacked cache coin',pts,.32,'gold',(0,1,0),.055)
for i in range(5):crystal('Cache gemstones',(-6+i*3,9+(i%2),22+(i%3)),1.15,3.4,'gem',6)
box('Rune slab',(5,8.6,27),(4,.75,3),'cut',.3)
for x in (4,5,6):sweep('Rune tablet marks',[(x,9.15,26),(x,9.15,28)],[.12,.12],'oath',6)

# Five original heraldic silhouettes: fortress, antler, flame, frost, lightning.
for k in range(5):
    ctx['part']=8+k
    if k==0:
        plate('Iron battlement',[(-3.7,48,2),(-3.7,53,2),(-2.2,53,2),(-2.2,51.5,2),(-.8,51.5,2),(-.8,54,2),(.8,54,2),(.8,51.5,2),(2.2,51.5,2),(2.2,53,2),(3.7,53,2),(3.7,48,2),(0,46.5,2)],.45,'oath',bevel=.05)
    elif k==1:
        for s in (-1,1):
            sweep('Wild antler',[(0,47,2),(s*1,49,2),(s*2,51,2),(s*3.5,54,2)],[.32,.32,.26,.12],'oath',6)
            for yy in (50,52):sweep('Antler tine',[(s*(yy-49)*.7,yy,2),(s*3.7,yy+.6,2),(s*4,yy+1.6,2)],[.22,.15,.04],'oath',6)
    elif k==2:
        plate('Cinder flame',[(0,46.5,2),(-3.5,49,2),(-3.3,52,2),(-1.9,50.5,2),(-.9,53,2),(0,55,2),(1.5,52,2),(1.2,50.5,2),(3,52,2),(3.6,49.5,2)],.45,'oath',bevel=.1)
    elif k==3:
        for i in range(6):
            a=i*math.tau/6;u=Vector((math.sin(a),math.cos(a),0));c=Vector((0,50.5,2));end=c+u*4.3
            sweep('Winter crystal ray',[tuple(c),tuple(end)],[.23,.13],'oath',6)
            v=Vector((u.y,-u.x,0))
            for s in (-1,1):sweep('Winter barb',[tuple(c+u*2.2),tuple(c+u*3.3+v*s*.9)],[.15,.08],'oath',6)
    else:
        plate('Storm bolt',[(1,55,2),(-3.5,50.5,2),(-.7,50.5,2),(-2,46.3,2),(3.6,52,2),(.4,52,2)],.55,'oath',bevel=.08)

# Separate ritual ward: cut octagonal foot, forked stone, inset channels and a suspended seal.
ctx['part']=13
for y,r,d in [(1,11,2),(3,9,2),(4.5,7.4,1)]:
    pts=[(r*math.cos(i*math.tau/8+math.pi/8),y,r*math.sin(i*math.tau/8+math.pi/8)) for i in range(8)]
    plate('Ward octagonal footing',pts,d,'cut' if y==3 else 'stone',(0,1,0),.055)
for side in (-1,1):
    profile=[(side*1.8,5),(side*6.5,5),(side*6.3,20),(side*4.2,29),(side*2.3,30),(side*3.4,18)]
    plate('Ward split monolith',[(x,y,0) for x,y in profile],4.8,'stone',bevel=.065)
    sweep('Ward bronze channel',[(side*4,6,3),(side*4.6,17,3),(side*2.9,27,3)],[.25]*3,'bronze',8)
    for y in (8,12,16):
        plate('Ward chiseled notch',[(side*3,y,2.5),(side*5.5,y+.4,2.5),(side*5.4,y+1,2.5),(side*3,y+.6,2.5)],.45,'cut',bevel=.02)
    box('Ward foot binding',(side*4.3,6,0),(5,1.4,6),'bronze',.16)
ring('Ward seal frame',(0,19,0),7.8,9.3,.55,'bronze',32)
for x in (-3,0,3):
    sweep('Ward threshold inscription',[(x,5.2,4),(x-.7,5.2,6),(x+.7,5.2,8)],[.12]*3,'oath',6)
ctx['part']=14
# The seal rises during channeling and settles, golden, after the ward is broken.
plate('Ritual seal diamond',[(0,26,0),(4.8,19,0),(0,12,0),(-4.8,19,0)],1.3,'oath',bevel=.12)
plate('Seal inset eye',[(0,22,.95),(2.2,19,.95),(0,16,.95),(-2.2,19,.95)],.35,'shadow',bevel=.04)
crystal('Seal center shard',(0,19,1.3),.75,3.6,'gem',6)
for side in (-1,1):
    sweep('Seal engraved strokes',[(side*1,23,1),(side*3,19,1),(side*1,15,1)],[.12]*3,'gold',6)

world=bpy.data.worlds.get('ARPG Waystone World') or bpy.data.worlds.new('ARPG Waystone World');scene.world=world;world.use_nodes=True
n=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND');n.inputs[0].default_value=(.045,.058,.07,1);n.inputs[1].default_value=.55
for name,pos,power,size in [('Key',(-65,-80,110),170000,55),('Fill',(90,-40,70),130000,60),('Rim',(-30,60,95),240000,45)]:
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.size=size;ob=bpy.data.objects.new(name,data);scene.collection.objects.link(ob);ob.location=pos;ob.rotation_euler=(Vector((15,0,25))-ob.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Waystone Camera');ob=bpy.data.objects.new(data.name,data);scene.collection.objects.link(ob)
ob.location=(105,-160,92);ob.rotation_euler=(Vector((28,-2,26))-ob.location).to_track_quat('-Z','Y').to_euler();data.type='ORTHO';data.ortho_scale=123;scene.camera=ob
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1600;scene.render.resolution_y=1050;scene.render.resolution_percentage=100
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':area.spaces.active.shading.type='MATERIAL';area.spaces.active.region_3d.view_perspective='CAMERA'
for ob in scene.objects:ob.select_set(ob in objects)
bpy.ops.export_scene.gltf(filepath=str(ROOT/'waystone.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_apply=True)
print('Exported',len(objects),'original shrine/cache meshes')
