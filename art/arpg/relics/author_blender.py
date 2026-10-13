"""Original named equipment, authored in the local Blender MCP.
Mesh coordinates are native Y-up; object rotations/translations only stage the atelier.
Parts: 0 body/blade, 1 hilt, 2 empty scabbard, 3 fitted chest ornaments.
"""
import bpy, bmesh, math
from mathutils import Vector
from mathutils.geometry import tessellate_polygon

scene = bpy.data.scenes.get('ARPG Relic Atelier')
if scene:
    for ob in list(scene.objects): bpy.data.objects.remove(ob, do_unlink=True)
else: scene = bpy.data.scenes.new('ARPG Relic Atelier')
bpy.context.window.scene = scene
palette = {
 'iron':(61,68,79), 'edge':(190,210,216), 'bronze':(172,117,54), 'gold':(229,178,73),
 'ember':(247,102,29), 'red':(121,35,34), 'navy':(30,53,91), 'cyan':(96,199,214),
 'blue':(32,104,174), 'ivory':(224,220,175), 'green':(33,87,53), 'emerald':(52,178,100),
 'leather':(94,55,31), 'teal':(40,110,103), 'apricot':(247,160,105), 'sole':(35,32,34)
}
mats = {}
for key,rgb in palette.items():
    mat = bpy.data.materials.new('Relic '+key)
    mat.diffuse_color = tuple(v/255 for v in rgb)+(1,)
    mat.use_nodes=True
    shader=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    shader.inputs['Base Color'].default_value=mat.diffuse_color
    shader.inputs['Roughness'].default_value=.48
    shader.inputs['Metallic'].default_value=.65 if key in ['iron','edge','bronze','gold'] else .05
    mats[key]=mat

state={'id':0,'part':0,'serial':0}
objects=[]
def mesh(label, verts, faces, mat, smooth=False):
    data=bpy.data.meshes.new('Relic '+label)
    data.from_pydata(verts,[],faces);data.update()
    bm=bmesh.new();bm.from_mesh(data)
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    data.materials.append(mats[mat])
    for p in data.polygons:p.use_smooth=smooth
    ob=bpy.data.objects.new('Relic %02d %d %s %d'%(state['id'],state['part'],label,state['serial']),data)
    state['serial']+=1;scene.collection.objects.link(ob)
    ob['arpg_identity']=state['id'];ob['arpg_part']=state['part']
    ob.rotation_euler.x=math.pi/2
    # Distinct rows: four pairs of uniques followed by the three complete sets.
    if state['id']<8: col=state['id']%4;row=state['id']//4
    else: col=(state['id']-8)%4;row=2+(state['id']-8)//4
    ob.location=(col*40-60,0,95-row*47)
    if state['part']==2: ob.hide_render=True;ob.hide_set(True)
    if state['part']==3: ob.location.y-=3.8
    objects.append(ob)
    return ob

def plate(label, poly, z, depth, mat, bevel=.10):
    # Contoured bevels keep flat designs legible under native lighting.
    n=len(poly);cx=sum(p[0] for p in poly)/n;cy=sum(p[1] for p in poly)/n
    inner=[(cx+(x-cx)*(1-bevel),cy+(y-cy)*(1-bevel)) for x,y in poly]
    verts=[(x,y,z-depth) for x,y in poly]+[(x,y,z) for x,y in poly]+[(x,y,z+depth*.45) for x,y in inner]
    faces=[tuple(reversed(range(n)))]
    for i in range(n):
        j=(i+1)%n
        faces.extend([(i,j,n+j,n+i),(n+i,n+j,2*n+j,2*n+i)])
    faces.append(tuple(range(2*n,3*n)))
    return mesh(label,verts,faces,mat)

def tube(label, pts, radius, mat, sides=7):
    verts=[]
    for i,p in enumerate(pts):
        p=Vector(p);t=(Vector(pts[min(i+1,len(pts)-1)])-Vector(pts[max(0,i-1)])).normalized()
        ref=Vector((0,0,1)) if abs(t.z)<.8 else Vector((0,1,0))
        a=t.cross(ref).normalized();b=t.cross(a).normalized()
        r=radius[i] if isinstance(radius,list) else radius
        for j in range(sides):
            v=p+r*(a*math.cos(j*math.tau/sides)+b*math.sin(j*math.tau/sides));verts.append(tuple(v))
    faces=[tuple(reversed(range(sides)))]
    for i in range(len(pts)-1):
        for j in range(sides):faces.append((i*sides+j,i*sides+(j+1)%sides,(i+1)*sides+(j+1)%sides,(i+1)*sides+j))
    faces.append(tuple(range((len(pts)-1)*sides,len(pts)*sides)))
    return mesh(label,verts,faces,mat,True)

def curve(label,pts,radius,mat):
    # Subdivided Catmull-Rom keeps roots and wave ribs continuous, with pointed tips.
    out=[]
    for i in range(len(pts)-1):
        a=Vector(pts[max(i-1,0)]);b=Vector(pts[i]);c=Vector(pts[i+1]);d=Vector(pts[min(i+2,len(pts)-1)])
        for j in range(5):
            t=j/5;out.append(tuple(.5*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t)))
    out.append(pts[-1]);rs=[radius*(1-.9*(i/(len(out)-1))**2) for i in range(len(out))]
    return tube(label,out,rs,mat)

def gem(x,y,z,rx,ry,mat):
    return plate('cut gem',[(x,y+ry),(x+rx,y),(x,y-ry),(x-rx,y)],z,.48,mat,.45)

def ellipse(cx,cy,rx,ry,n=16):
    return [(cx+rx*math.sin(i*math.tau/n),cy+ry*math.cos(i*math.tau/n)) for i in range(n)]

def leaf(x,y,z,s,mat):
    return plate('leaf',[(x,y+2*s),(x+s,y+.5*s),(x+.8*s,y-.7*s),(x,y-2*s),(x-.8*s,y-.7*s),(x-s,y+.5*s)],z,.18*s,mat,.3)

def emblem(family,x,y,z,s=1,unique=False):
    if family==0:
        if unique:
            poly=[(x,y+2*s),(x-1.7*s,y+.3*s),(x-1.3*s,y-1.2*s),(x,y-2*s),(x+1.4*s,y-.7*s),(x+.8*s,y+1.2*s),(x+.2*s,y+.4*s)]
            plate('flame brooch rim',poly,z,.22*s,'bronze',.15)
            plate('flame brooch',[(x+(a-x)*.78,y+(b-y)*.78) for a,b in poly],z+.24*s,.15*s,'ember',.3)
            return
        for j in range(3):
            yy=y-j*2.25*s
            plate('flame chevron',[(x-2*s,yy+1.5*s),(x,yy+.4*s),(x+2*s,yy+1.5*s),(x+1.25*s,yy-.5*s),(x,yy-1.2*s),(x-1.25*s,yy-.5*s)],z,.18*s,'ember',.25)
    elif family==1:
        if unique:
            poly=[]
            for i in range(17):
                a=math.radians(55+i*250/16);poly.append((x+2*s*math.cos(a),y+2*s*math.sin(a)))
            for i in range(16,-1,-1):
                a=math.radians(55+i*250/16);poly.append((x+.65*s+1.8*s*math.cos(a),y+1.8*s*math.sin(a)))
            plate('crescent',poly,z,.15*s,'edge')
        else:
            for j in range(5):
                a=(-60+j*30)*math.pi/180
                curve('shell rib',[(x,y-1.4*s,z),(x+math.sin(a)*1.6*s,y+.3*s,z+.2*s),(x+math.sin(a)*2.2*s,y+math.cos(a)*2.4*s,z)],.22*s,'edge')
        gem(x,y-1.5*s,z+.3*s,.65*s,.9*s,'cyan')
    else:
        leaf(x,y,z,s,'emerald')
        curve('ivory vein',[(x,y-1.5*s,z+.3),(x,y,z+.5),(x,y+1.6*s,z+.3)],.10*s,'ivory')

def sword(i,family):
    state['part']=0
    if i==0: poly=[(-2.5,1),(-3,4),(-2.5,25),(0,32),(3,28),(3,5),(2.1,1)]
    elif i==1:
        poly=[(-1.7,1),(-1.4,9),(-.7,17),(1,24),(5,33),(4.5,25),(3.4,17),(2.2,9),(1.7,1)]
    elif i==8: poly=[(-1.7,1),(-2,7),(-3.7,19),(-3.5,24),(0,33),(3.5,24),(3.7,19),(2,7),(1.7,1)]
    elif i==12: poly=[(-2.4,1),(-3,8),(-2.1,12),(-3.8,17),(-2.3,20),(-3,25),(0,33),(3,25),(2.3,20),(3.8,17),(2.1,12),(3,8),(2.4,1)]
    else:
        left=[(-1.7+math.sin(y*.34)*.55,y) for y in range(1,28,3)]
        poly=left+[(0,33)]+[(-x,y) for x,y in reversed(left)]
    plate('blade bevel',poly,0,.7,'edge' if family!=0 else 'iron',.25)
    inner=[(x*.67,y*.90+.3) for x,y in poly]
    plate('blade face',inner,.34,.10,'green' if family==2 else 'iron' if family==0 else 'edge',.1)
    if i==0:
        for s in [-1,1]:plate('furnace channel',[(s*.6,5),(s*1.2,5.5),(s*1.2,26),(s*.65,28)],.55,.08,'ember',.15)
    elif i==1:plate('moonwell fuller',[(-.6,3),(-.4,13),(1,23),(3.8,29),(2.2,20),(.6,10),(.5,3)],.6,.07,'blue',.05)
    elif i==8:
        plate('ivory vein',[(-.28,3),(-.3,25),(0,30),(.3,25),(.28,3)],.63,.08,'ivory')
        for s in [-1,1]:
            for y in [11,16,21]:tube('leaf blade rib',[(0,y,.61),(s*2,y+3,.51)],.09,'ivory')
    elif i==12:
        plate('blazing fuller',[(-.6,3),(-.6,9),(.6,13),(-.6,17),(.6,22),(0,28),(1.5,23),(.5,17),(1.6,13),(.6,9),(.6,3)],.63,.09,'ember',.12)
    else:plate('tide fuller',[(-.4,3),(-.35,25),(0,30),(.35,25),(.4,3)],.6,.08,'cyan')
    state['part']=1
    metal='edge' if family==1 else 'bronze'
    grip='teal' if i==1 else ['red','navy','green'][family]
    tube('grip',[(0,-7.5,0),(0,-.3,0)],.87,grip,10)
    for y in [-6.8,-5.6,-4.4,-3.2,-2]:tube('grip winding',[(-.72,y,.55),(.72,y+.45,.55)],.13,'sole')
    for y in [-7.5,-.4]:tube('grip ferrule',[(0,y-.2,0),(0,y+.2,0)],1.02,metal,10)
    if family==0:
        for s in [-1,1]:plate('flame guard',[(0,-.6),(s*3,-1),(s*6,-.3),(s*7,2.5),(s*5.8,1.3),(s*4.9,1.2),(s*4.6,3),(s*3.1,1.5),(s*1.2,.8)],0,.8,metal,.16)
    elif i==1:
        for s in [-1,1]:
            poly=[(s*7*math.cos(a),1+3*math.sin(a)) for a in [math.pi+j*math.pi/14 for j in range(15)]]
            poly += [(s*5.5*math.cos(a),1.4+2*math.sin(a)) for a in reversed([math.pi+j*math.pi/14 for j in range(15)])]
            plate('moon guard',poly,0,.65,metal,.12)
    elif family==2:
        for s in [-1,1]:
            curve('antler guard',[(0,0,0),(s*2.5,.2,0),(s*5,1.4,0),(s*6.5,3.4,0)],.6,metal)
            curve('guard tine',[(s*3,.4,0),(s*4,2,0),(s*3.7,3.5,0)],.38,metal)
    else:
        plate('shell guard',[(-6,1.7),(-4,3),(-2,2.6),(0,3.6),(2,2.6),(4,3),(6,1.7),(3,-.5),(0,-.8),(-3,-.5)],0,.65,metal,.16)
        for s in [-1,0,1]:tube('shell guard ribs',[(0,-.4,.6),(s*3.4,2.3,.6)],.2,'cyan')
    if family==1:
        plate('moon pommel',ellipse(0,-8.5,1.55,1.55),0,.6,metal,.2)
        plate('moonstone',ellipse(0,-8.5,1.14,1.14),.5,.3,'cyan',.35)
    else:
        gem(0,-8.5,0,1.6,2,metal);gem(0,-8.5,.5,1.1,1.5,'emerald' if family==2 else 'ember')
    gem(0,.6,.7,1.1,1.5,'emerald' if family==2 else 'cyan' if family==1 else 'ember')
    state['part']=2
    # Scabbard envelope follows each blade, not a generic cylinder.
    if i==1: outline=[(-2,0),(-1.7,10),(-1,18),(.5,25),(5,34),(5.4,25),(4.1,17),(2.8,9),(2,0)]
    else:outline=[(-4,0),(-4.2,24),(0,34),(4.2,24),(4,0)]
    plate('scabbard',outline,0,1.1,['red','navy','green'][family],.12)
    for y,w in [(1,4),(9,4),(24,3.7)]:
        plate('scabbard fittings',[(-w,y-.5),(w,y-.5),(w,y+.5),(-w,y+.5)],1.1,.12,metal,.1)
    emblem(family,0,7,1.3,.72,True)

def shield(i,family):
    state['part']=0
    if i==2:poly=[(0,15),(-7,12),(-11,14),(-10,-3),(-7,-10),(0,-16),(7,-10),(10,-3),(11,14),(7,12)]
    elif i==3:poly=ellipse(0,0,12.5,15,8)
    elif i==9:poly=[(0,16),(-7,11),(-11,5),(-11,-3),(-7,-10),(0,-16),(7,-10),(11,-3),(11,5),(7,11)]
    elif i==13:poly=[(-12,14),(-9,-9),(0,-16),(9,-9),(12,14),(0,16)]
    else:
        poly=[(-11,7),(-10,12),(-6,14),(-3,13),(0,16),(3,13),(6,14),(10,12),(11,7),(8,-3),(0,-15),(-8,-3)]
    metal='gold' if i==3 else 'edge' if family==1 else 'bronze'
    plate('shield rim',poly,0,1.7,metal,.10)
    inside=[(x*.86,y*.89) for x,y in poly]
    plate('shield field',inside,1,.55,'navy' if family==1 else 'green' if family==2 else 'iron',.05)
    if i==3:
        for sx,sy in [(-1,1),(1,1),(-1,-1),(1,-1)]:
            plate('glass pane',[(sx*.7,sy*.7),(sx*10,sy*.7),(sx*9,sy*7),(sx*5,sy*11),(sx*.7,sy*12)],1.65,.38,'cyan' if sx*sy<0 else 'apricot',.14)
        for j in range(8):
            a=j*math.pi/4;c=math.cos(a);s=math.sin(a)
            plate('sun ray',[(0,0),(c*8-s*.2,s*8+c*.2),(c*2-s*1.5,s*2+c*1.5)],2.4,.16,'gold',.15)
        gem(0,0,2.8,2.5,3.7,'gold');gem(0,0,3.2,1.9,2.8,'ember')
    elif i==2:
        for s in [-1,1]:
            curve('ivory antler',[(0,-12,1.9),(s*3,-6,2),(s*3,2,2.1),(s*5,7,2),(s*4.5,12,1.9)],.8,'ivory')
            for y in [-5,0,5]:curve('antler branch',[(s*3,y,2),(s*6,y+2,2),(s*8,y+5,1.9)],.50,'ivory')
        heart=[(0,-3.5),(-2.5,-.5),(-2.5,1.4),(-1.2,2.2),(0,1.2),(1.2,2.2),(2.5,1.4),(2.5,-.5)]
        plate('heart frame',heart,2.7,.3,'bronze',.15)
        plate('emerald heart',[(x*.79,y*.79) for x,y in heart],3,.25,'emerald',.45)
    elif i==9:
        for y in [-7,-2,3,8]:
            for s in [-1,1]:curve('leaf vein',[(0,y-2,2),(s*4,y,2),(s*7,y+3,1.9)],.42,'ivory')
        leaf(0,0,2.5,3,'emerald')
    elif i==13:
        emblem(0,0,5,2.2,2.6);gem(0,10,2.5,1.4,2,'ember')
    else:
        for j in range(5):
            x=(j-2)*4.2
            curve('shell rib',[(0,-11,2),(x*.6,0,2.3),(x,10+2*(2-abs(j-2)),2)],.8,'cyan')
        plate('pearl seat',ellipse(0,-8,2.5,2.5),2.4,.4,'edge')
        plate('tide pearl',ellipse(0,-8,1.8,1.8),2.9,.5,'cyan',.5)
    for x,y in poly[::2]:gem(x*.94,y*.94,.5,.42,.5,metal)
    for x in [-3,3]:tube('back grip',[(x,-5,-1.5),(x,-3,-4),(x,3,-4),(x,5,-1.5)],.6,'leather')

def loft(label,rings,mat,n=16):
    verts=[]
    for y,rx,rz,cz in rings:
        for j in range(n):
            a=j*math.tau/n;verts.append((rx*math.cos(a),y,cz+rz*math.sin(a)))
    faces=[tuple(reversed(range(n)))]
    for i in range(len(rings)-1):
        for j in range(n):faces.append((i*n+j,i*n+(j+1)%n,(i+1)*n+(j+1)%n,(i+1)*n+j))
    faces.append(tuple(range((len(rings)-1)*n,len(rings)*n)))
    return mesh(label,verts,faces,mat,True)

def tunic(i,family):
    state['part']=0
    cloth='red' if i==4 else 'navy' if family==1 else 'green' if family==2 else 'iron'
    trim='ivory' if family==2 else 'cyan' if family==1 else 'red'
    metal='edge' if family==1 else 'bronze'
    loft('fitted cloth',[(-11,8.4,3.6,0),(-8,8,3.5,0),(-2.5,6,3,0),(3,6.6,3.3,0),(7.5,7.4,3.1,0),(10,4.2,2.5,0)],cloth,20)
    for s in [-1,1]:
        tube('short sleeve',[(s*6.5,7.5,0),(s*9,6.1,0),(s*11,4.7,0)],[3.1,3.4,3.6],cloth,12)
        tube('sleeve binding',[(s*10.4,5.1,0),(s*11.1,4.6,0)],3.65,trim,12)
        plate('shoulder armor',[(s*5,9),(s*7.5,10),(s*11,7.5),(s*10,5.8),(s*7,7)],2.3,.42,metal,.17)
    loft('belt',[(-2.6,6.2,3.23,0),(-1.3,6.24,3.25,0)],'leather' if family==2 else 'sole',20)
    gem(0,-2,3.3,1.1,.8,metal)
    for j in range(9):
        x=-7.3+j*1.825
        yy=-9.9+(.5 if j%2 else 0)
        plate('hem stitch',[(x-.65,yy+.5),(x,yy-.5),(x+.65,yy+.5),(x,yy-1.3)],3.5,.08,trim,.1)
    state['part']=3
    # These fitted pieces are shared by pickup and torso attachment, at separate origins.
    for s in [-1,1]:
        poly=[(s*.6,5.8),(s*3.4,5.8),(s*4.6,4.4),(s*3.3,2.1),(s*1.4,3.1)]
        plate('split collar',poly,0,.2,trim,.13)
        plate('fitted shoulder plate',[(s*3,4.5),(s*4.8,4.8),(s*5.4,3.2),(s*4.1,2.6)],.05,.22,metal,.15)
    if i==4:
        plate('charcoal scarf',[(-3,5.5),(-1,4),(0,1.3),(-1.2,-3.5),(-2.1,-2.4),(-1.5,1.5),(-3.8,3.6)],.3,.18,'iron')
    emblem(family,0,1,.7,.85,i in [4,5])
    for s in [-1,1]:tube('bodice trim',[(s*2.5,-1,.15),(s*3,-2,.1),(s*3.3,-4,.05)],.12,metal)

def boot(i,family):
    state['part']=0
    cloth='leather' if i==6 else 'sole' if i==7 else ['iron','navy','green'][family]
    metal='edge' if family==1 else 'bronze'
    cuff='ivory' if i==6 else 'red' if i==7 or family==0 else 'cyan' if family==1 else 'green'
    loft('boot upper',[(-6,2.6,2.5,.2),(-3,2.25,2.05,0),(2,2.4,2.1,0),(6.5,2.95,2.45,0),(7.4,3.0,2.5,0)],cloth,16)
    # Broad toe and heel with articulated welt, rather than intersecting spheres.
    loft('foot',[(-8.3,3,4.55,2.15),(-7,3.1,4.7,2.2),(-5.6,2.9,4.5,2.1),(-4.8,2.3,3.5,1.7)],cloth,16)
    loft('sole',[(-9,3.16,4.8,2.2),(-8.3,3.2,4.9,2.2),(-7.8,3.13,4.8,2.2)],'sole',16)
    loft('rolled cuff',[(6.1,3.15,2.65,0),(6.6,3.25,2.72,0),(7.9,3.25,2.72,0),(8.3,3.0,2.5,0)],cuff,16)
    if i==6:
        for s in [-1,1]:tube('crossed strap',[(s*-2.2,-2,2),(0,0,2.3),(s*2.7,3,2.3)],.35,'teal')
        for a in [0,math.pi/2]:
            c=math.cos(a);s=math.sin(a)
            plate('compass buckle',[(c*-1.8+s*0,3+s*-1.8),(s*.65,3+c*.65),(c*1.8,3+s*1.8),(-s*.65,3-c*.65)],2.9,.22,'bronze',.3)
        gem(0,3,3.1,.45,.55,'cyan')
    else:
        poly=[(0,6.6),(-2.4,5.2),(-2.1,-1),(0,-3.6),(2.1,-1),(2.4,5.2)]
        plate('shin guard',poly,2.2,.5,'red' if i==7 else metal,.16)
        if i==7:
            for y in [-.5,1,2.5,4]:tube('bronze diagonal laces',[(-1.5,y,2.8),(1.5,y+.7,2.8)],.16,'bronze')
        else:
            emblem(family,0,2.5,2.9,.65)
            if family==2:
                for side in [-1,1]:
                    for yy in [-.5,1,2.5]:curve('shin vine',[(0,yy,3),(side*1,yy+1,3),(side*1.7,yy+1.8,2.9)],.10,'ivory')
        plate('toe cap',[(-2.6,-6.9),(-2.7,-5.7),(0,-5),(2.7,-5.7),(2.6,-6.9)],6.3,.4,metal,.22)
    for s in [-1,1]:tube('boot seam',[(s*2.2,-5,1.6),(s*2,0,1.6),(s*2.7,6,1.6)],.1,'ivory' if i==6 else metal)

slots=[0,0,1,1,2,2,3,3,0,1,2,3,0,1,2,3,0,1,2,3]
families=[0,1,2,3,0,1,2,0,2,2,2,2,0,0,0,0,1,1,1,1]
for i in range(20):
    state['id']=i;state['part']=0
    if slots[i]==0:sword(i,families[i])
    elif slots[i]==1:shield(i,families[i])
    elif slots[i]==2:tunic(i,families[i])
    else:boot(i,families[i])
# Join each semantic part without applying the studio transform.
for i in range(20):
    for part in range(4):
        group=[ob for ob in scene.objects if ob.get('arpg_identity')==i and ob.get('arpg_part')==part]
        if not group:continue
        bpy.ops.object.select_all(action='DESELECT')
        for ob in group:ob.hide_set(False);ob.select_set(True)
        bpy.context.view_layer.objects.active=group[0]
        bpy.ops.object.join();ob=bpy.context.object
        ob.name='Relic %02d part %d'%(i,part)
        ob['arpg_identity']=i;ob['arpg_part']=part
        if part==2:ob.hide_render=True
# Export only own objects. Hidden scabbards must still be exported.
bpy.ops.object.select_all(action='DESELECT')
for ob in scene.objects:
    if ob.type=='MESH':ob.hide_set(False);ob.select_set(True)
bpy.ops.export_scene.gltf(filepath='/Users/alec/git/z64-romhack-tutorials/soh/art/arpg/relics/relics.glb',export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_cameras=False,export_lights=False)
for ob in scene.objects:
    if ob.get('arpg_part')==2:ob.hide_set(True)
cam=bpy.data.cameras.new('Relic Atelier Camera');camera=bpy.data.objects.new(cam.name,cam);scene.collection.objects.link(camera)
camera.location=(32,-280,25);camera.rotation_euler=(Vector((0,0,13))-camera.location).to_track_quat('-Z','Y').to_euler();cam.type='ORTHO';cam.ortho_scale=265;scene.camera=camera
scene.render.resolution_x=1400;scene.render.resolution_y=1600;scene.render.resolution_percentage=100
for pos,energy,size in [((-80,-140,170),140000,100),((80,-60,50),75000,80)]:
    light=bpy.data.lights.new('Relic Atelier Light','AREA');light.energy=energy;light.size=size
    ob=bpy.data.objects.new(light.name,light);scene.collection.objects.link(ob);ob.location=pos;ob.rotation_euler=(Vector((0,0,10))-ob.location).to_track_quat('-Z','Y').to_euler()
print('Created',len([o for o in scene.objects if o.type=='MESH']),'semantic meshes; triangles',sum(len(o.data.polygons) for o in scene.objects if o.type=='MESH'))
print('Exported all 20 identities. Live user blend file was not saved.')
