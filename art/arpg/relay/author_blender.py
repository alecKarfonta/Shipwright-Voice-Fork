"""Original spirit lantern and winged soul, authored through local Blender MCP.
Native Y-up coordinates; object rotation is only the studio presentation.
"""
import bpy,bmesh,math
from mathutils import Vector
scene=bpy.data.scenes.get('ARPG Spirit Relay')
if scene:
    for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
else:scene=bpy.data.scenes.new('ARPG Spirit Relay')
bpy.context.window.scene=scene
palette={'stone':(42,53,68),'edge':(111,138,156),'bronze':(139,86,48),'gold':(222,178,82),'glass':(92,57,123),'spirit':(119,222,244),'core':(230,250,249)}
mats={}
for key,rgb in palette.items():
    m=bpy.data.materials.new('Relay '+key);m.use_nodes=True
    sh=next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    m.diffuse_color=tuple(v/255 for v in rgb)+(1,);sh.inputs['Base Color'].default_value=m.diffuse_color
    sh.inputs['Roughness'].default_value=.35;sh.inputs['Metallic'].default_value=.6 if key in ['bronze','gold'] else .1
    mats[key]=m
state={'part':0}
def mesh(label,v,f,mat,smooth=False):
    data=bpy.data.meshes.new('Relay '+label);data.from_pydata(v,[],f);data.update()
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    data.materials.append(mats[mat])
    for p in data.polygons:p.use_smooth=smooth
    ob=bpy.data.objects.new('Relay '+label,data);scene.collection.objects.link(ob);ob['arpg_part']=state['part'];ob.rotation_euler.x=math.pi/2
    return ob

def profile(label,rings,mat,sides=8,twist=math.pi/8):
    v=[]
    for y,r in rings:
        for i in range(sides):
            a=i*math.tau/sides+twist;v.append((math.cos(a)*r,y,math.sin(a)*r))
    f=[tuple(reversed(range(sides))),tuple(range((len(rings)-1)*sides,len(rings)*sides))]
    for j in range(len(rings)-1):
        for i in range(sides):k=j*sides+i;n=j*sides+(i+1)%sides;f.append((k,n,n+sides,k+sides))
    return mesh(label,v,f,mat)

def tube(label,pts,r,mat,sides=6):
    v=[]
    for i,pt in enumerate(pts):
        p=Vector(pt);t=(Vector(pts[min(i+1,len(pts)-1)])-Vector(pts[max(0,i-1)])).normalized();ref=Vector((0,1,0)) if abs(t.y)<.8 else Vector((1,0,0))
        a=t.cross(ref).normalized();b=t.cross(a)
        for j in range(sides):v.append(tuple(p+r*(a*math.cos(j*math.tau/sides)+b*math.sin(j*math.tau/sides))))
    f=[tuple(reversed(range(sides))),tuple(range((len(pts)-1)*sides,len(pts)*sides))]
    for k in range(len(pts)-1):
        for i in range(sides):n=(i+1)%sides;f.append((k*sides+i,k*sides+n,(k+1)*sides+n,(k+1)*sides+i))
    return mesh(label,v,f,mat,True)

profile('octagonal plinth',[(0,10),(.8,10),(2,9),(3,9),(4,7.4)],'stone')
profile('plinth bevel',[(3.5,7.7),(4.2,7.7),(4.8,6)],'edge')
profile('foot collar',[(4.5,5.8),(5.5,5.8),(6.2,4)],'bronze')
profile('fluted pillar',[(6,3.4),(8,3),(20,2.5),(24,4.5)],'stone')
profile('lower band',[(8,3.15),(9,3.15)],'gold')
profile('upper band',[(20,3),(21,3),(22,4)],'gold')
profile('lantern bowl',[(23,4),(25,8),(27,8),(28,6.8)],'bronze')
profile('bowl lip',[(27,8.1),(28,8.1),(28.5,7.5)],'gold')
profile('violet basin',[(28,6.5),(28.5,5.5)],'glass')
for i in range(4):
    a=math.pi/4+i*math.pi/2;c=math.cos(a);z=math.sin(a)
    tube('swept cage rib',[(c*7,27,z*7),(c*8.2,31,z*8.2),(c*7,39,z*7),(c*4.5,43,z*4.5)],.65,'gold')
    tube('pillar inlay',[(c*3.2,9,z*3.2),(c*2.8,15,z*2.8),(c*2.7,20,z*2.7)],.18,'gold')
profile('hipped lantern roof',[(41,9.8),(42,9.8),(44,6.5),(46,3.5),(47.5,2)],'stone',8)
profile('roof seam',[(41,9.9),(41.5,9.9)],'bronze')
profile('spire',[(47,2.5),(48,2.5),(49,1.2),(53,.18)],'gold')
# Four open windows and a suspended central crystal give the lantern a legible silhouette.
state['part']=1
profile('beacon crystal',[(-5,.2),(-2,2.5),(1,2.5),(5.5,.15)],'spirit',6,0)
profile('beacon collar',[(-.6,2.65),(.4,2.65)],'gold',6,0)
state['part']=2
profile('soul core',[(-4,.12),(-1,1.6),(1.5,1.8),(4,.12)],'core',7,0)
for sign in [-1,1]:
    v=[(sign*1,0,0),(sign*3.4,3,.2),(sign*7,4,0),(sign*5,1,-.4),(sign*3,-1,.3),(sign*1,-.8,0),(sign*3,1,1.2)]
    f=[(0,1,6),(1,2,6),(2,3,6),(3,4,6),(4,5,6),(5,0,6),(5,4,3,2,1,0)]
    mesh('spirit wing',v,f,'spirit')
    tube('wing vein',[(sign*1,0,1),(sign*3,1,1.2),(sign*6,3.5,.3)],.12,'gold')
# Join by semantic part, preserving every material and native-space pivot.
for part in range(3):
    bpy.ops.object.select_all(action='DESELECT')
    group=[ob for ob in scene.objects if ob.type=='MESH' and ob.get('arpg_part')==part]
    for ob in group:ob.select_set(True)
    bpy.context.view_layer.objects.active=group[0];bpy.ops.object.join()
    ob=bpy.context.object;ob.name='Relay part '+str(part);ob['arpg_part']=part
    if part==1:ob.location.z=34
    if part==2:ob.location=(23,-2,31)
cam=bpy.data.cameras.new('Relay Camera');ob=bpy.data.objects.new(cam.name,cam);scene.collection.objects.link(ob)
ob.location=(90,-140,89);ob.rotation_euler=(Vector((7,0,27))-ob.location).to_track_quat('-Z','Y').to_euler();cam.type='ORTHO';cam.ortho_scale=75;scene.camera=ob
for pos,power in [((20,-60,100),18000),((-40,20,65),12000)]:
    light=bpy.data.lights.new('Relay Softbox','AREA');light.energy=power;light.size=50;ob=bpy.data.objects.new(light.name,light);scene.collection.objects.link(ob);ob.location=pos;ob.rotation_euler=(Vector((0,0,26))-ob.location).to_track_quat('-Z','Y').to_euler()
world=bpy.data.worlds.new('Relay Night');world.use_nodes=True;bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs[0].default_value=(.025,.035,.06,1);bg.inputs[1].default_value=.4;scene.world=world
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1000;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
bpy.ops.object.select_all(action='DESELECT')
for ob in scene.objects:
    if ob.type=='MESH':ob.select_set(True)
bpy.ops.export_scene.gltf(filepath='/Users/alec/git/z64-romhack-tutorials/soh/art/arpg/relay/relay.glb',export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True)
print('Original relay asset:',[(ob.name,len(ob.data.polygons)) for ob in scene.objects if ob.type=='MESH'])
