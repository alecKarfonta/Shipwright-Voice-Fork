"""Editable Blender-authored replacement meshes, in the native EN_ZF limb spaces.

Run from Blender's Text Editor or the local Blender MCP. Only the named workshop
scene is rebuilt. Export uses evaluated mesh corner normals, native coordinates,
semantic materials, and per-piece rank/weapon-state metadata.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parent
SCENE_NAME = 'ARPG Reptile Workshop'
scene = bpy.data.scenes.get(SCENE_NAME) or bpy.data.scenes.new(SCENE_NAME)
bpy.context.window.scene = scene
for obj in list(scene.objects):
    bpy.data.objects.remove(obj, do_unlink=True)
pose = {int(k): Matrix(v) for k, v in json.loads((ROOT / 'native_pose.json').read_text()).items()}
# OoT Y-up to Blender Z-up. One authoring unit is 100 native model units.
axis = Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS = ['skin','belly','scale','armor','trim','leather','claw','eye','steel','edge','mouth','pupil','scute']
context = {}
species = 0
limb = 9
context['rank'] = 0
context['state'] = 0  # 0 always, 1 drawn weapon, 2 sheathed weapon
parts = []
materials = {}
palettes = [
    [(79,119,75),(180,163,103),(44,76,58),(42,65,65),(187,149,68),(59,43,33),(217,211,160),(231,170,52),(79,109,114),(195,214,207),(77,34,31),(15,20,14),(69,105,69)],
    [(131,76,46),(184,132,73),(71,45,37),(47,52,61),(186,119,53),(53,33,30),(220,193,142),(247,101,35),(78,96,102),(201,207,192),(76,29,27),(16,12,11),(113,65,42)],
]
for sp in range(2):
    for i, (name, rgb) in enumerate(zip(MATERIALS,palettes[sp])):
        mat = bpy.data.materials.get(f'ARPG {sp} {name}') or bpy.data.materials.new(f'ARPG {sp} {name}')
        mat.diffuse_color = (*[v/255 for v in rgb],1)
        mat.use_nodes = True
        node = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
        node.inputs['Base Color'].default_value = (*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
        node.inputs['Roughness'].default_value = .38 if name in ('armor','trim','steel','edge') else .72
        node.inputs['Metallic'].default_value = .65 if name in ('armor','trim','steel','edge') else 0
        materials[sp,i] = mat

def mesh(name, vertices, faces, mat='skin', smooth=False, face_mats=None):
    data = bpy.data.meshes.new(name)
    data.from_pydata(vertices, [], faces)
    data.update()
    bm = bmesh.new(); bm.from_mesh(data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(data); bm.free()
    for i in range(len(MATERIALS)):
        data.materials.append(materials[species,i])
    for i,p in enumerate(data.polygons):
        p.material_index = MATERIALS.index(face_mats[i] if face_mats else mat)
        p.use_smooth = smooth
    obj = bpy.data.objects.new(f'{"Dino" if species else "Liz"}.{limb:02}.{name}',data)
    scene.collection.objects.link(obj)
    obj['arpg_species'] = species; obj['arpg_limb'] = limb
    obj['arpg_rank'] = context['rank']; obj['arpg_state'] = context['state']
    obj.matrix_world = Matrix.Translation(((-22 if species==0 else 22),0,0)) @ axis @ pose[limb]
    parts.append(obj)
    return obj

def loft(name, rings, mat='skin', smooth=True, sections=12, stripe=False):
    # Deliberately authored cross-sections: taper at the joint, muscle belly,
    # tendon ridge, and flattened back/front instead of overlapping ellipsoids.
    vertices=[]
    for x,cy,ry,rz in rings:
        for i in range(sections):
            a=2*math.pi*i/sections
            vertices.append((x,cy+ry*math.cos(a),rz*math.sin(a)))
    faces=[tuple(reversed(range(sections)))]; inks=[mat]
    for j in range(len(rings)-1):
        for i in range(sections):
            faces.append((j*sections+i,j*sections+(i+1)%sections,(j+1)*sections+(i+1)%sections,(j+1)*sections+i))
            inks.append('scale' if stripe and i in (5,6) else mat)
    faces.append(tuple((len(rings)-1)*sections+i for i in range(sections))); inks.append(mat)
    return mesh(name,vertices,faces,mat,smooth,inks)

def plate(name, points, normal, depth, mat='armor', bevel=.14):
    # Closed, beveled polygon. Broad planar face plus a narrow catching edge.
    points=[Vector(p) for p in points]; n=Vector(normal).normalized()
    center=sum(points,Vector())/len(points);count=len(points)
    vertices=[tuple(p-n*depth*.25) for p in points]
    vertices += [tuple(p+n*depth*.35) for p in points]
    vertices += [tuple(p.lerp(center,bevel)+n*depth) for p in points]
    faces=[tuple(reversed(range(count)))];inks=[mat]
    for ring in range(2):
        for i in range(count):
            faces.append((ring*count+i,ring*count+(i+1)%count,(ring+1)*count+(i+1)%count,(ring+1)*count+i));inks.append(mat)
    faces.append(tuple(2*count+i for i in range(count)));inks.append(mat)
    return mesh(name,vertices,faces,mat,False,inks)

def horn(name, path, radii, mat='claw', sides=8):
    pts=[Vector(p) for p in path]; vertices=[]
    for i,p in enumerate(pts):
        tangent=(pts[min(i+1,len(pts)-1)]-pts[max(i-1,0)]).normalized()
        u=tangent.cross(Vector((0,0,1)) if abs(tangent.z)<.9 else Vector((0,1,0))).normalized();v=tangent.cross(u)
        for j in range(sides):
            a=2*math.pi*j/sides
            vertices.append(tuple(p+radii[i]*(math.cos(a)*u+math.sin(a)*v)))
    faces=[tuple(reversed(range(sides)))]
    for i in range(len(pts)-1):
        for j in range(sides):
            faces.append((i*sides+j,i*sides+(j+1)%sides,(i+1)*sides+(j+1)%sides,(i+1)*sides+j))
    faces.append(tuple((len(pts)-1)*sides+j for j in range(sides)))
    return mesh(name,vertices,faces,mat,True)

def band(name,x,ry,rz,width,mat='trim',cy=0):
    return loft(name,[(x-width,cy,ry,rz),(x-width*.7,cy,ry+.12,rz+.12),(x+width*.7,cy,ry+.12,rz+.12),(x+width,cy,ry,rz)],mat,False)

def fin(name,x,y,height,width=1.6,mat='scale'):
    return plate(name,[(x-width,y,-.35),(x+width*.7,y,-.35),(x-.5,y+height,-.08),(x-width*1.3,y+height*.68,-.15)],(0,0,1),.65,mat,.12)

def scale_strip(name,x0,x1,y,z,width,mat='scale',count=4):
    for j in range(count):
        x=x0+(x1-x0)*j/max(1,count-1)
        plate(name+str(j),[(x-1.1,y,z),(x-.1,y,z-width),(x+1.35,y,z),(x-.1,y,z+width)],(0,1,0),.16,mat,.2)

def head(dino):
    # Twelve-corner wedge: broad temporal skull narrows into a long, flat muzzle.
    rings=[(-3,-2.1,1.5,2.3),(-1.8,-3.8,2,3.45),(1,-4.2,2.3,4.05),(4.5,-3.45,2.5,4.0),(7,-2.1,2.45,2.7),(12,-1.3,2.5,2.15),(15.5,-1.05,2.3,2.75),(17,-.6,1.8,2.4)]
    verts=[]
    for x,top,bottom,w in rings:
        mid=(top+bottom)/2
        verts.extend([(x,top,0),(x,top+.15,w*.65),(x,top+.55,w*.91),(x,mid,w),(x,bottom-.35,w*.93),(x,bottom,w*.64),(x,bottom,0),(x,bottom,-w*.64),(x,bottom-.35,-w*.93),(x,mid,-w),(x,top+.55,-w*.91),(x,top+.15,-w*.65)])
    faces=[tuple(reversed(range(12)))];inks=['skin']
    for r in range(len(rings)-1):
        for i in range(12):
            faces.append((r*12+i,r*12+(i+1)%12,(r+1)*12+(i+1)%12,(r+1)*12+i));inks.append('scale' if i in (0,1,10,11) and r<3 else 'skin')
    faces.append(tuple((len(rings)-1)*12+i for i in range(12)));inks.append('skin')
    mesh('sculpted cranial planes',verts,faces,'skin',True,inks)
    for s in (-1,1):
        plate('recessed eye socket',[(1,-3.35,s*3.85),(5.4,-2.55,s*4.02),(6,-.1,s*3.55),(1.3,-.3,s*4.12)],(0,0,s),.22,'scale',.08)
        plate('slanted amber eye',[(1.9,-2.65,s*4.14),(4.9,-2.13,s*4.15),(4.65,-1.15,s*4.16),(2,-1.1,s*4.35)],(0,0,s),.11,'eye',.04)
        plate('vertical pupil',[(3.1,-2.4,s*4.36),(3.45,-2.36,s*4.37),(3.58,-1.25,s*4.45),(3.3,-1.19,s*4.44)],(0,0,s),.045,'pupil',.06)
        plate('knife brow',[(.5,-3.3,s*3.8),(3,-3.9,s*3.85),(6.4,-2.4,s*3.5),(5.2,-1.95,s*4.45),(1.6,-2.5,s*4.45)],(0,-.5,s),.5,'skin',.16)
        plate('cheek ridge',[(0,.1,s*3.5),(4.7,.5,s*3.9),(6.5,1.4,s*2.65),(1.2,2.15,s*3.55),(-1.8,1.1,s*3.15)],(0,0,s),.35,'scale',.17)
        plate('nostril',[(14.4,-.75,s*2.43),(15.7,-.6,s*2.66),(16,-.05,s*2.7),(14.5,-.12,s*2.55)],(0,-.3,s),.055,'pupil',.1)
        for i in range(7):
            x=4.8+i*1.6;z=s*(2.85 if i<2 else 2.05 if i<5 else 2.4)
            horn('upper tooth',[(x,2.18,z),(x+.16,2.7,z),(x-.16,3.5 if i%3==0 else 3.08,z*.98)],[.27,.17,.025],sides=6)
        if dino:
            horn('swept temple horn',[(-1,-2.5,s*3.2),(-2.5,-4.4,s*4.3),(-5.8,-6.5,s*4.9),(-7,-8.2,s*4.6)],[1.25,.93,.44,.035],sides=10)
            horn('jaw spur',[(-1.5,1.2,s*3),(-3.7,2.2,s*4.2),(-5.2,1.8,s*4.6)],[.78,.47,.03])
        else:
            for j in range(3):
                plate('blade cheek frill',[(.1,j*1.2-2,s*3.2),(-2.1,j*.95-3,s*3.1),(-5.1,j*1.7-3.7,s*(5.1-j*.2)),(-3,j*1.4-1.3,s*3.8)],(0,0,s),.26,'scale',.1)
    for x,y,w in [(-1,-4.1,2.2),(2,-4.25,1.8),(5,-3.6,1.2)]:
        plate('cranial shield',[(x-1.9,y,0),(x,y-.3,-w),(x+2,y+.2,0),(x,y-.3,w)],(0,-1,0),.22,'scale',.14)
    context['rank']=1
    plate('brow circlet',[(-1,-4.4,-2.7),(1,-4.65,-2.1),(1,-4.65,2.1),(-1,-4.4,2.7)],(0,-1,0),.24,'trim',.12)
    context['rank']=2
    for s in (-1,1): horn('veteran crest',[(-1,-4.3,s*1.8),(-2,-5.8,s*2),(-3.5,-6.6,s*2.5)],[.45,.31,.025],'trim')
    context['rank']=0

def jaw(dino):
    loft('angular lower mandible',[(-1,.35,.5,2.25),(1,.65,.65,3),(5,.62,.7,2.9),(10,.65,.56,2.4),(14.7,.53,.4,2.6)],'skin',True,12)
    plate('mouth interior',[(1,-.1,-2.5),(11,-.1,-2),(14.4,.04,-2.3),(14.4,.04,2.3),(11,-.1,2),(1,-.1,2.5)],(0,-1,0),.09,'mouth',.03)
    for s in (-1,1):
        for i in range(6):
            x=3.2+i*1.85
            horn('lower tooth',[(x,0,s*2.4),(x+.15,-.8,s*2.35),(x+.22,-1.3,s*2.3)],[.25,.13,.025],sides=6)
        if dino: horn('mandible tusk',[(1.2,.25,s*2.7),(2.1,-1.6,s*3.4),(3.6,-3.4,s*3.55),(4.2,-4.1,s*3.1)],[.82,.64,.27,.025])
    scale_strip('chin scute',2,12,1.24,0,2,'belly',5)

def arm(length,lower,dino,leg=False):
    ry,rz=(2.6,2.65) if leg else (1.55,1.6)
    loft('tendon and muscle surface',[(-.7,.1,ry*.68,rz*.62),(length*.06,.15,ry*.85,rz*.82),(length*.27,.35,ry,rz),(length*.5,.22,ry*.9,rz*.95),(length*.72,0,ry*.61,rz*.68),(length,.1,ry*.47,rz*.5),(length+.55,.1,ry*.56,rz*.6)],stripe=True)
    if lower:
        plate('forged greave' if leg else 'vambrace',[(length*.12,ry*.81,-rz*.8),(length*.32,ry*1.02,-rz*.96),(length*.8,ry*.7,-rz*.65),(length*.97,ry*.51,0),(length*.8,ry*.7,rz*.65),(length*.32,ry*1.02,rz*.96),(length*.12,ry*.81,rz*.8)],(0,1,0),.46,'armor',.11)
        for x,f in [(length*.25,.97),(length*.74,.67)]: band('leather fastening',x,ry*f+.12,rz*f+.12,.28,'leather',.16)
        plate('raised armor keel',[(length*.22,ry*1.2,0),(length*.36,ry*1.24,-.35),(length*.87,ry*.9,0),(length*.36,ry*1.24,.35)],(0,1,0),.18,'trim',.1)
    else:
        for s in (-1,1):
            plate('muscle scale ridge',[(length*.12,0,s*rz*.89),(length*.4,-.3,s*rz*1.01),(length*.71,0,s*rz*.64),(length*.35,.85,s*rz*.96)],(0,0,s),.15,'scale',.2)
    context['rank']=1
    band('champion joint cuff',length*.1,ry*.95,rz*.92,.2,'trim',.13)
    context['rank']=2
    fin('elite armor blade',length*.3,ry*1.03,2.6 if dino else 1.4,.85,'trim')
    context['rank']=0
    if dino and lower: fin('iron spur',length*.65,ry*.95,2.3,1,'armor')

def hand(grip=False):
    if grip:
        loft('sword hand',[(-1.15,-1.2,1.4,1.15),(.1,-1.25,1.6,1.42),(1.45,-1.1,1.3,1.15)],'skin',True,10)
        for i in range(3):
            y=-2.6+i*1.05
            horn('folded finger',[(-.1,y,-1.15),(1.15,y,-1),(1.55,y,.35),(.4,y,1.32)],[.4,.45,.4,.3],'skin',8)
    else:
        loft('metacarpal',[(-.8,0,.8,.95),(1.4,-.1,1.05,1.35),(3.8,-.4,.65,1.15)],'skin',True,10)
        for i in range(3):
            y=-1.3+i*1.12
            horn('articulated finger',[(3,y,.25),(5.3,y-.3,.65),(6.55,y-.6,1.15),(7.35,y-1.05,1.2)],[.5,.44,.32,.2],'skin')
            horn('sickle claw',[(7.2,y-.94,1.2),(8.7,y-1.5,1.4),(9.15,y-2.05,.75)],[.34,.22,.025])
        horn('opposable thumb',[(1,-.65,.8),(1.5,-2.35,1.4),(3.1,-3.1,1.1),(4,-3.4,.5)],[.67,.5,.34,.04],'skin')

def weapon(dino):
    hand(True);context['state']=1
    reach=36 if dino else 19
    horn('wrapped grip',[(0,1.05,0),(0,-3.2,0)],[.44,.44],'leather',10)
    for j in range(5):
        y=.5-j*.68
        horn('diagonal grip binding',[(-.42,y,-.25),(.43,y-.3,-.25)],[.1,.1],'trim',6)
    plate('swept crossguard',[(-3.2,-2.5,0),(-2.2,-3.7,0),(1.7,-3.9,0),(3.35,-2.75,0),(2.3,-3.02,0),(-1.8,-3.12,0)],(0,0,1),.48,'trim',.16)
    # A single forged blade with a central ridge, tapered bevel and angular tip.
    widths=[.9,1.7,1.78,1.6,1.3,.025] if not dino else [1.7,3.4,3.65,3.7,2.9,.025]
    verts=[]
    for j,w in enumerate(widths):
        t=j/5; x=t*t*(5.9 if dino else 3.45);y=-4-t*(reach-4)
        verts.extend([(x-w,y,0),(x-w*.7,y,.28),(x,y,.48),(x+w*.73,y,.25),(x+w,y,0),(x+w*.73,y,-.25),(x,y,-.48),(x-w*.7,y,-.28)])
    faces=[tuple(reversed(range(8)))];inks=['steel']
    for j in range(5):
        for k in range(8):
            faces.append((j*8+k,j*8+(k+1)%8,(j+1)*8+(k+1)%8,(j+1)*8+k));inks.append('edge' if k in (0,3,4,7) else 'steel')
    faces.append(tuple(40+i for i in range(8)));inks.append('edge')
    mesh('ridge forged cleaver' if dino else 'ridge forged falchion',verts,faces,'steel',False,inks)
    if dino:
        for j in range(4):
            y=-8-j*5.2
            def edge(yy):
                t=(-yy-4)/32;k=min(4,int(t*5));f=t*5-k
                return 5.9*t*t-(widths[k]*(1-f)+widths[k+1]*f)
            plate('cleaver back tooth',[(edge(y)+.35,y,0),(edge(y-.5)-1.65,y-.5,0),(edge(y-2.25)-1.0,y-2.25,0),(edge(y-2.6)+.35,y-2.6,0)],(0,0,1),.32,'steel',.14)
    context['state']=0

def foot():
    loft('heel and metatarsal',[(-1.4,0,.72,1.55),(0,-.2,1,1.9),(2.8,-.3,.85,2.3),(4.6,-.25,.5,2)],stripe=True)
    for j in range(3):
        z=(j-1)*2.25
        horn('long toe',[(2.9,-.1,z*.7),(5,-.25,z),(6.6,-.1,z*1.07),(7.4,.1,z*1.09)],[.65,.7,.53,.35],'skin')
        horn('hooked foot talon',[(7,.1,z*1.09),(8.4,.05,z*1.15),(9.3,.78,z*1.17)],[.52,.4,.025])
        for x,y in [(4,-.95),(5.2,-1),(6.4,-.75)]:
            plate('toe scale',[(x-.65,y,z),(x,y,z-.5),(x+.8,y,z),(x,y,z+.5)],(0,-1,0),.13,'belly',.15)

def back_armor():
    for sign in (-1,1):
        plate('scapular backplate',[(-2.45,-3.55,sign*.3),(-1.25,-4.3,sign*4.3),(1.8,-4.5,sign*5.1),(5.5,-3.55,sign*3.65),(7.1,-2.6,sign*.35),(2,-4.65,sign*.25)],(0,-1,0),.4,'armor',.1)
        plate('scapular engraving',[(-1.7,-4.1,sign*1.1),(-.5,-4.8,sign*3.45),(2.2,-4.88,sign*4.1),(2.3,-4.9,sign*3.77),(-.35,-4.83,sign*3.12)],(0,-1,0),.09,'trim',.12)
    for x,y,w in [(-1,-4.1,1.4),(1.3,-4.8,1.65),(3.7,-4.15,1.45),(5.6,-3.4,1.1)]:
        plate('spinal armor scute',[(x-1,y,0),(x,y,-w),(x+1.15,y+.2,0),(x,y,w)],(0,-1,0),.28,'scale',.15)

def torso(chest,dino):
    if chest:
        back_armor()
        loft('shoulder girdle',[(-2.8,0,3.6,4.7),(-.5,.2,4.9,5.8),(2.8,.25,4.7,5.9),(5.8,.1,3.6,4.65),(8.7,0,2.5,3.1)],stripe=True,sections=16)
        for s in (-1,1):
            plate('pectoral cuirass',[(-2.3,3.8,s*.25),(-1.7,4.4,s*4.4),(1.3,4.7,s*5.3),(5.5,3.45,s*3.8),(6.3,3.7,s*.3),(2.3,5,s*.18)],(0,1,0),.4,'armor',.09)
            plate('cuirass chased border',[(-1.9,4.8,s*.45),(-1.3,4.85,s*3.9),(1.2,5.12,s*4.55),(1.3,5.12,s*4.25),(-1,4.97,s*3.7)],(0,1,0),.08,'trim',.08)
            if dino:
                plate('razor shoulder guard',[(.1,-1.3,s*5.4),(4,-1.8,s*5.5),(6,0,s*4.9),(3,3.1,s*6.8),(-1,2.6,s*6.65)],(0,0,s),.55,'armor',.12)
                horn('shoulder spike',[(1.1,.3,s*6),(1.7,-.3,s*7.8),(2.8,-1,s*9)],[.85,.52,.025],'claw')
            context['rank']=1
        plate('champion throat clasp',[(4.2,4.2,0),(2.5,5.2,-1),(1,5.6,0),(2.5,5.2,1)],(0,1,0),.24,'trim',.13)
        context['rank']=2
        plate('elite chest stone',[(3.5,5.45,0),(2.5,5.8,-.55),(1.7,6,0),(2.5,5.8,.55)],(0,1,0),.4,'eye',.2)
        context['rank']=0
    else:
        loft('lean continuous abdomen',[(-5.1,.2,3.4,3.8),(-3,.6,4.4,5.05),(0,.7,4.1,4.7),(3,.4,3.35,4.05),(6,.1,3.15,3.7),(9.3,-.05,3.45,4.1),(12,0,3.7,4.5)],stripe=True,sections=16)
        for i in range(7):
            x=-1+i*1.75; y=4.28 if i<2 else 3.65 if i<5 else 3.9; w=3.65 if i<2 else 3.05
            plate('overlapping abdominal scute',[(x-.95,y-.3,-w),(x-.45,y+.05,0),(x-.95,y-.3,w),(x+.55,y-.25,w*.94),(x+1,y+.18,0),(x+.55,y-.25,-w*.94)],(0,1,0),.16,'belly',.08)
        band('waist harness',-.6,4.55,5.05,.46,'leather',.6)
        plate('waist buckle',[(-1.4,5.4,-.85),(.1,5.4,-.85),(.1,5.4,.85),(-1.4,5.4,.85)],(0,1,0),.3,'trim',.18)
        for x in (0,3,6,9): fin('dorsal crest',x,-3.4,-(3.4 if dino else 2.15),1.4)

def neck(dino):
    loft('sinewed neck',[(-1,.1,2.45,2.7),(1,.15,2.7,2.9),(4,.2,2.5,2.6),(7,.25,2.4,2.65),(10.5,.2,2.7,2.9),(11.8,.1,2.5,2.65)],stripe=True)
    for i in range(5):
        x=i*2+1
        plate('throat shield',[(x-1,2.6,-1.9),(x-.2,3.03,0),(x-1,2.6,1.9),(x+.6,2.6,1.7),(x+1.2,3.07,0),(x+.6,2.6,-1.7)],(0,1,0),.1,'belly',.1)
        fin('neck sail',x,-2.5,-(4 if dino else 2.75),1.05)

def tail(tip,dino):
    if tip:
        length=8.5 if dino else 15.5
        loft('whip tail tip',[(-1,0,1.8,2.1),(2,.15,1.4,1.6),(length*.5,.7,.85,1),(length*.8,.95,.42,.52),(length,1.1,.06,.065)],stripe=True)
        for x in (1,3.5,6) if dino else (1,4,7,10): fin('tail sawtooth',x,-1,-(1.8 if dino else 1.35),1)
    else:
        loft('tail root',[(-1,0,2.5,3.25),(1,.2,2.7,3.05),(4,.3,2.35,2.75),(7,.4,1.9,2.2),(9,.4,1.6,1.9)],stripe=True)
        for x in (1,3.5,6): fin('tail root spine',x,-2,-2.1,1)

def scabbard(dino):
    length=34 if dino else 17
    loft('fitted leather scabbard',[(-.7,0,1.3,.62),(0,0,1.45,.72),(length*.6,0,1.25,.65),(length*.9,-.2,.8,.49),(length,-.35,.2,.28)],'leather',False,8)
    for x in (.1,length*.38,length*.86): band('scabbard binding',x,1.43 if x<length*.6 else 1.02,.76 if x<length*.6 else .6,.2,'trim')
    context['state']=2
    horn('stored grip',[(-5.2,0,0),(-.4,0,0)],[.45,.45],'leather',10)
    plate('stored guard',[(0,-3.1,0),(-.7,-2.6,0),(-.7,2.6,0),(0,3.1,0),(.1,1.8,0),(-.12,-1.8,0)],(0,0,1),.35,'trim',.1)
    horn('stored pommel',[(-5.6,0,0),(-4.9,0,0)],[.65,.65],'trim',8)
    context['state']=0

for species in range(2):
    dino=bool(species)
    for limb in (8,9,10,15,16,17,22,23,24,25,29,30,33,38,39,40,45,46,47,48):
        context['rank']=context['state']=0
        if limb==8: jaw(dino)
        elif limb==9: head(dino)
        elif limb==10: neck(dino)
        elif limb==15: weapon(dino)
        elif limb in (16,23): arm(8.5,True,dino)
        elif limb in (17,24): arm(13,False,dino)
        elif limb==22: hand()
        elif limb==25: torso(True,dino)
        elif limb in (29,30): tail(limb==29,dino)
        elif limb==33: scabbard(dino)
        elif limb in (38,45): foot()
        elif limb in (39,46): arm(8,True,dino,True)
        elif limb in (40,47): arm(15.07,False,dino,True)
        elif limb==48: torso(False,dino)

# Subdivision follows the authored muscle/cranial topology; hard armor and
# brows retain their cut edges. Fitted scale patches use the evaluated surface.
for base in list(parts):
    label=base.name.lower()
    if any(n in label for n in ('tendon and muscle','lean continuous','shoulder girdle','sinewed neck','sculpted cranial','tail root','whip tail','angular lower','metacarpal','heel and metatarsal')):
        mod=base.modifiers.new('Anatomical surface refinement','SUBSURF')
        mod.subdivision_type='CATMULL_CLARK';mod.levels=1;mod.render_levels=1
bpy.context.view_layer.update()
for base in list(parts):
    label=base.name.lower()
    if not any(n in label for n in ('sculpted cranial','tendon and muscle','sinewed neck','tail root','lean continuous')):
        continue
    species=base['arpg_species'];limb=base['arpg_limb'];context['rank']=context['state']=0
    evaluated=base.evaluated_get(bpy.context.evaluated_depsgraph_get())
    data=evaluated.to_mesh()
    for poly in data.polygons:
        p=poly.center;n=poly.normal
        # Irregular dorsal/outer patches leave the throat and joint creases clear.
        if poly.index%3!=0 or abs(n.x)>.48 or n.y>.3: continue
        if limb==9 and (p.x>6.8 or p.y>-.8): continue
        u=Vector((1,0,0));u=(u-n*u.dot(n)).normalized();v=n.cross(u)
        w=.46 if limb==9 else .38
        q=p+n*.025
        points=[q-u*.72,q-v*w,q+u*.66,q+v*w]
        plate('fitted imbricated scale',[tuple(a) for a in points],tuple(n),.075,'scute',.12)
    evaluated.to_mesh_clear()

# Studio setup stays in the source scene; only mesh objects carrying arpg_limb export.
world=bpy.data.worlds.get('ARPG Reptile World') or bpy.data.worlds.new('ARPG Reptile World')
scene.world=world; world.use_nodes=True
bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs[0].default_value=(.07,.085,.10,1);bg.inputs[1].default_value=.5
for name,position,power,size in [('Key',(-45,-50,85),180000,45),('Fill',(55,-25,50),100000,40),('Rim',(0,45,75),220000,35)]:
    data=bpy.data.lights.new('ARPG '+name,'AREA'); data.energy=power;data.shape='DISK';data.size=size
    ob=bpy.data.objects.new(data.name,data);scene.collection.objects.link(ob);ob.location=position
    ob.rotation_euler=(Vector((0,0,25))-ob.location).to_track_quat('-Z','Y').to_euler()
camera=bpy.data.cameras.new('ARPG Reptile Camera');ob=bpy.data.objects.new(camera.name,camera);scene.collection.objects.link(ob)
ob.location=(90,-140,75);ob.rotation_euler=(Vector((0,0,25))-ob.location).to_track_quat('-Z','Y').to_euler();camera.type='ORTHO';camera.ortho_scale=112;scene.camera=ob
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1500;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':
        area.spaces.active.shading.type='MATERIAL'
        area.spaces.active.region_3d.view_perspective='CAMERA'
bpy.context.view_layer.update()
for obj in scene.objects:
    obj.select_set(obj.type=='MESH')
bpy.ops.export_scene.gltf(filepath=str(ROOT/'reptiles.glb'),export_format='GLB',use_selection=True,export_extras=True,export_apply=True,use_active_scene=True)

print('Authored',len(parts),'mesh pieces; exported editable',ROOT/'reptiles.glb')
