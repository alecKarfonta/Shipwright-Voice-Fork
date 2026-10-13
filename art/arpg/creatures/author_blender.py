"""Original Cairn Sentinel and Lantern Wraith, made through the local Blender MCP.
Meshes retain native Y-up coordinates; object transforms only arrange the studio.
"""
import bpy,bmesh,math,json
from mathutils import Vector
ROOT='/Users/alec/git/z64-romhack-tutorials/soh/art/arpg/creatures'
previous_engine=bpy.context.scene.render.engine
scene=bpy.data.scenes.get('ARPG Original Creatures')
if scene:
    for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
else:scene=bpy.data.scenes.new('ARPG Original Creatures')
bpy.context.window.scene=scene
palette={'basalt':(53,64,73),'stone':(108,126,136),'edge':(165,181,181),'bronze':(141,91,51),'gold':(215,178,102),'core':(96,233,214),'bone':(217,211,175),'dark':(24,29,43),'cloth':(39,81,101),'trim':(74,139,153),'spirit':(168,233,231)}
mats={}
for key,rgb in palette.items():
    m=bpy.data.materials.new('Creature '+key);m.use_nodes=True
    shader=next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    m.diffuse_color=tuple(v/255 for v in rgb)+(1,);shader.inputs['Base Color'].default_value=m.diffuse_color
    shader.inputs['Roughness'].default_value=.58;shader.inputs['Metallic'].default_value=.5 if key in ('bronze','gold') else .05
    if key in ('core','spirit'):
        shader.inputs['Emission Color'].default_value=m.diffuse_color;shader.inputs['Emission Strength'].default_value=.55
    mats[key]=m
part=0
objects=[]
def mesh(name,verts,faces,mat,smooth=False):
    data=bpy.data.meshes.new(name);data.from_pydata(verts,[],faces);data.update()
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    data.materials.append(mats[mat])
    for poly in data.polygons:poly.use_smooth=smooth
    ob=bpy.data.objects.new(name,data);scene.collection.objects.link(ob);ob['arpg_part']=part;ob['arpg_material']=mat;ob.rotation_euler.x=math.pi/2
    if part>=8:ob.location.x=100
    objects.append(ob);return ob

def rings(name,levels,mat,sides=8,twist=math.pi/8):
    # Each level is (x,y,z, x radius,z radius). Sharp cross sections make carved armor, not rounded blobs.
    verts=[]
    for x,y,z,rx,rz in levels:
        for i in range(sides):
            a=i*math.tau/sides+twist;verts.append((x+math.cos(a)*rx,y,z+math.sin(a)*rz))
    faces=[tuple(reversed(range(sides))),tuple(range((len(levels)-1)*sides,len(levels)*sides))]
    for j in range(len(levels)-1):
        for i in range(sides):a=j*sides+i;b=j*sides+(i+1)%sides;faces.append((a,b,b+sides,a+sides))
    return mesh(name,verts,faces,mat)

def tube(name,points,radii,mat,sides=6):
    if isinstance(radii,(float,int)):radii=[radii]*len(points)
    verts=[]
    for i,point in enumerate(points):
        p=Vector(point);axis=(Vector(points[min(i+1,len(points)-1)])-Vector(points[max(0,i-1)])).normalized()
        ref=Vector((0,1,0)) if abs(axis.y)<.8 else Vector((1,0,0));a=axis.cross(ref).normalized();b=axis.cross(a)
        for j in range(sides):verts.append(tuple(p+radii[i]*(a*math.cos(j*math.tau/sides)+b*math.sin(j*math.tau/sides))))
    faces=[tuple(reversed(range(sides))),tuple(range((len(points)-1)*sides,len(points)*sides))]
    for k in range(len(points)-1):
        for i in range(sides):n=(i+1)%sides;faces.append((k*sides+i,k*sides+n,(k+1)*sides+n,(k+1)*sides+i))
    return mesh(name,verts,faces,mat,True)

def plate(name,outline,depth,mat):
    verts=[(x,y,z-depth/2) for x,y,z in outline]+[(x,y,z+depth/2) for x,y,z in outline];n=len(outline)
    faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]
    faces.extend((i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n))
    return mesh(name,verts,faces,mat)

# 0: low armored trunk and luminous internal mechanism.
rings('Sentinel basalt chassis',[(0,15,0,13,19),(0,21,0,21,25),(0,27,0,19,22),(0,30,0,10,14)],'basalt',10)
rings('Sentinel chest rim',[(0,20,0,21.7,25.8),(0,22,0,21.7,25.8)],'bronze',10)
rings('Sentinel exposed crystal',[(0,27,0,7,9),(0,34,0,8,10),(0,43,0,.7,.9)],'core',6,0)
for z in (-14,0,14):tube('Sentinel spine hinge',[(0,27,z-3),(0,32,z),(0,27,z+3)],[2.6,3,2.6],'gold')
# The split carapace opens at runtime around its two side hinges.
for side in (-1,1):
    part=1 if side<0 else 2
    for j,z in enumerate((-16,0,16)):
        v=[(side*1,39-j%2,z-9),(side*12,40-j%2,z-8),(side*23,29,z-7),(side*24,24,z+7),(side*13,31,z+10),(side*1,37,z+9)]
        plate('Sentinel overlapping shale plate',v,2.5,'stone')
        tube('Sentinel beveled ridge',[v[0],v[1],v[2]],[.8,1.1,.6],'edge')
        tube('Sentinel turquoise fissure',[(side*5,39,z-6),(side*9,37,z-2),(side*8,36,z+3)],.45,'core',4)
    plate('Sentinel rear fin',[(side*5,36,-23),(side*8,48,-23),(side*15,31,-23)],2,'basalt')
# 3: carved ram-like head, deep sockets, separate eye slits, angular tusks.
part=3
rings('Sentinel head block',[(0,18,23,10,6),(0,27,25,14,9),(0,33,22,10,6)],'stone',6,0)
plate('Sentinel mouth cleft',[(-8,20,32),(8,20,32),(6,16,31),(-6,16,31)],1,'dark')
for side in (-1,1):
    plate('Sentinel brow',[ (side*2,29,33),(side*12,31,30),(side*11,27,33),(side*3,26,35)],2,'basalt')
    tube('Sentinel eye',[(side*3,26.7,35),(side*9,28,33)],.72,'core',5)
    tube('Sentinel stone tusk',[(side*10,20,29),(side*14,16,33),(side*15,23,38)],[2.7,2,.08],'edge')
# Four articulated legs: open space beneath shell and distinctly jointed feet.
for index,(sx,sz) in enumerate(((-1,-1),(1,-1),(-1,1),(1,1))):
    part=4+index;z=sz*15
    tube('Sentinel upper leg',[(sx*15,23,z),(sx*29,20,z+sz*6),(sx*33,11,z+sz*8)],[3.6,4.4,2.6],'basalt')
    rings('Sentinel knee armor',[(sx*28,17,z+sz*6,5,5),(sx*28,23,z+sz*6,6,5),(sx*28,26,z+sz*6,3,3)],'stone',6)
    tube('Sentinel shin',[(sx*33,13,z+sz*8),(sx*29,4,z+sz*14)],[3.5,2.8],'stone')
    for toe in (-1,0,1):tube('Sentinel claw',[(sx*29+toe*2.2,4,z+sz*13),(sx*31+toe*3.5,1,z+sz*21)],[1.8,.15],'edge',5)

# 8: a hollow, tattered mantle around an open hanging lantern; there is no round body.
part=8
rings('Wraith shoulder collar',[(0,47,0,6,5),(0,51,0,14,7),(0,56,0,12,6),(0,59,0,5,4)],'cloth',8)
tube('Wraith collar filigree',[(-13,53,3),(-8,48,7),(0,45,9),(8,48,7),(13,53,3)],.8,'gold')
for side in (-1,1):
    tube('Wraith lantern suspension',[(side*6,48,4),(side*7,36,4),(side*5,32,4)],.42,'gold',5)
rings('Wraith lantern base',[(0,22,3,4,4),(0,24,3,8,6),(0,26,3,7,5)],'bronze',6,0)
rings('Wraith lantern hood',[(0,38,3,8,6),(0,41,3,5,4),(0,44,3,1,1)],'bronze',6,0)
for i in range(6):
    a=i*math.tau/6
    tube('Wraith open cage rib',[(math.cos(a)*6,25,3+math.sin(a)*4),(math.cos(a)*8,31,3+math.sin(a)*6),(math.cos(a)*6,39,3+math.sin(a)*4)],.6,'gold',5)
# 9: hollow moth mask with pointed horns and inset eyes; not a skull or Poe reskin.
part=9
plate('Wraith mask shell',[(-8,61,5),(-11,72,3),(-6,79,1),(0,76,6),(6,79,1),(11,72,3),(8,61,5),(0,57,10)],3,'bone')
for side in (-1,1):
    plate('Wraith eye recess',[(side*2,68,8),(side*8,72,6),(side*7,65,8),(side*3,64,10)],.7,'dark')
    tube('Wraith narrow eye',[(side*3,67,10),(side*6.8,69,8)],.45,'core',5)
    tube('Wraith swept antenna',[(side*7,76,1),(side*13,84,-1),(side*14,93,1),(side*9,98,4)],[2,1.8,1.1,.08],'bone',6)
plate('Wraith central beak',[(-2,69,10),(2,69,10),(0,58,15)],1.6,'gold')
# 10/11: two long, ribbed arm wings with split hems and thin grasping fingers.
for side in (-1,1):
    part=10 if side<0 else 11
    v=[(side*11,54,0),(side*25,58,-1),(side*32,42,0),(side*27,21,2),(side*21,31,4),(side*17,22,2),(side*14,42,3)]
    plate('Wraith folded sleeve',v,1.1,'cloth')
    tube('Wraith sleeve seam',[v[0],v[1],v[2],v[3]],[.6,.9,.6,.12],'trim',5)
    tube('Wraith bony forearm',[(side*21,39,5),(side*22,28,8),(side*24,24,10)],[1.2,.9,1.1],'bone',6)
    for finger in range(3):tube('Wraith curved finger',[(side*(23+finger),25,10),(side*(24+finger*2),20,12),(side*(21+finger*2),18,13)],[.55,.4,.07],'bone',5)
# 12/13: floating tails, asymmetrical pointed hems, separate animation pivots.
for side in (-1,1):
    part=12 if side<0 else 13
    plate('Wraith trailing mantle',[(side*3,49,-4),(side*14,50,-3),(side*18,30,-7),(side*12,6,-4),(side*8,19,-2),(side*4,9,1),(side*2,32,-5)],.8,'cloth')
    tube('Wraith embroidered tail',[(side*12,48,-2),(side*15,31,-5),(side*11,10,-3)],[.6,.6,.08],'trim',5)
part=14
rings('Wraith suspended flame',[(0,27,3,.3,.3),(0,31,3,3,3),(0,35,3,2,2),(0,39,3,.12,.12)],'core',6,0)
part=15
# The native projectile uses a separate, pointed carved bone mesh.
tube('Bone dart',[ (0,0,-9),(0,0,-3),(0,0,5),(0,0,14)],[.5,2.4,1.4,.05],'bone',6)
for side in (-1,1):plate('Bone dart barb',[(0,0,-5),(side*5,0,-9),(side*2,0,0)],1,'bronze')
# A standalone scene copy preserves other Blender work and its active file path.
for ob in scene.objects:ob.select_set(ob.type=='MESH')
bpy.context.view_layer.objects.active=objects[0]
bpy.ops.export_scene.gltf(filepath=ROOT+'/creatures.glb',use_selection=True,use_active_scene=True,export_extras=True)
print(json.dumps({'parts':len(set(o['arpg_part'] for o in objects)),'objects':len(objects),'triangles':sum(len(o.data.polygons) for o in objects),'source':ROOT+'/creatures.glb'}))
