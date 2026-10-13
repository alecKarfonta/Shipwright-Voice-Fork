"""Blender-authored Stalfos, aligned to the native MiddleGuard animation.

Rebuilds only ARPG Stalfos Workshop. All 24 visible limbs stay rigid in their
native spaces so attacks, shield collision and BodyBreak use the original rig.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Vector, Matrix
ROOT=Path(__file__).resolve().parent
scene=bpy.data.scenes.get('ARPG Stalfos Workshop') or bpy.data.scenes.new('ARPG Stalfos Workshop');bpy.context.window.scene=scene
for ob in list(scene.objects): bpy.data.objects.remove(ob,do_unlink=True)
pose={int(k):Matrix(v) for k,v in json.loads((ROOT/'native_pose.json').read_text()).items()}
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['bone','shadow','armor','trim','cloth','eye','steel','edge','cavity','strap','inlay']
palettes=[[(214,207,174),(102,104,89),(57,84,74),(147,125,82),(53,64,54),(225,100,68)],
          [(216,211,182),(106,104,91),(84,53,63),(188,153,107),(65,37,44),(249,164,79)],
          [(225,218,179),(105,108,97),(44,74,73),(204,158,76),(57,47,78),(110,234,207)]]
materials={};parts=[];species=0;limb=11
for sp in range(3):
 for i,(name,rgb) in enumerate(zip(MATERIALS,palettes[sp]+[(99,122,122),(206,219,206),(29,35,31),(63,45,31),(124,151,137)])):
  mat=bpy.data.materials.get(f'Stalfos {sp} {name}') or bpy.data.materials.new(f'Stalfos {sp} {name}');mat.diffuse_color=(*[v/255 for v in rgb],1);mat.use_nodes=True
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED');node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Roughness'].default_value=.43 if name in ('armor','trim','steel','edge') else .8
  node.inputs['Metallic'].default_value=.65 if name in ('armor','trim','steel','edge') else 0
  materials[sp,i]=mat

def mesh(name, vertices, faces, mat='bone', smooth=False, face_mats=None):
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
    obj = bpy.data.objects.new(f'{["Sentinel","Champion","Warden"][species]}.{limb:02}.{name}',data)
    scene.collection.objects.link(obj)
    obj['arpg_limb'] = limb
    obj['arpg_rank'] = species
    obj.matrix_world = Matrix.Translation(((species-1)*50,0,0)) @ axis @ pose[limb]
    parts.append(obj)
    return obj

def loft(name, rings, mat='bone', smooth=True, sections=12, stripe=False):
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
            inks.append('shadow' if stripe and i in (5,6) else mat)
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

def horn(name, path, radii, mat='bone', sides=8):
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


def pathbone(name,points,radius,depth=None,mat='bone',sides=10,steps=4):
    # A single continuous sweep; no individual tube segments or bead joints.
    controls=[Vector(p) for p in points];samples=[]
    for i in range(len(controls)-1):
        a,b,c,d=controls[max(0,i-1)],controls[i],controls[i+1],controls[min(i+2,len(controls)-1)]
        for j in range(steps):
            t=j/steps;samples.append((2*b+(c-a)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t)*.5)
    samples.append(controls[-1]);verts=[];depth=radius if depth is None else depth
    for i,p in enumerate(samples):
        tangent=(samples[min(i+1,len(samples)-1)]-samples[max(0,i-1)]).normalized()
        u=tangent.cross(Vector((0,0,1)) if abs(tangent.z)<.85 else Vector((0,1,0))).normalized();v=tangent.cross(u)
        profile=.87+.13*math.sin(math.pi*i/(len(samples)-1))
        for j in range(sides):
            a=j*math.tau/sides;verts.append(tuple(p+u*math.cos(a)*radius*profile+v*math.sin(a)*depth*profile))
    faces=[tuple(reversed(range(sides)))]
    for i in range(len(samples)-1):
        for j in range(sides): faces.append((i*sides+j,i*sides+(j+1)%sides,(i+1)*sides+(j+1)%sides,(i+1)*sides+j))
    faces.append(tuple((len(samples)-1)*sides+j for j in range(sides)))
    return mesh(name,verts,faces,mat,True)

def band(name,x,ry,rz,width,cy=0,mat='strap'):
    return loft(name,[(x-width,cy,ry,rz),(x-width*.75,cy,ry+.1,rz+.1),(x+width*.75,cy,ry+.1,rz+.1),(x+width,cy,ry,rz)],mat,False,16)

def skull():
    # Domed parietal/occipital vault; lower front left open for real orbital holes.
    rings=[(4.6,.7,2.2,2.5),(6,1,3.45,3.8),(8.5,.9,4.25,4.55),(10.2,.8,4.35,4.65),(12.3,.8,3.65,3.9),(14,.8,2.45,2.9),(14.8,.8,.5,.8)]
    v=[];N=20
    for x,cy,ry,rz in rings:
        for i in range(N):
            a=i*math.tau/N;v.append((x,cy+math.cos(a)*ry,math.sin(a)*rz))
    f=[]
    for r in range(len(rings)-1):
        for i in range(N):
            if r<3 and math.cos((i+.5)*math.tau/N)>.25: continue
            f.append((r*N+i,r*N+(i+1)%N,(r+1)*N+(i+1)%N,(r+1)*N+i))
    f.append(tuple((len(rings)-1)*N+i for i in range(N)));mesh('cranial vault with open face',v,f,'bone',True)
    for s in (-1,1):
        outline=[(9.6,1.05),(9.8,2.8),(8.9,4.1),(6.9,4.12),(5.95,3.0),(6.3,1.3),(7.7,.8)]
        center=Vector((7.85,2.5));inner=[Vector(p) for p in outline];outer=[center+(p-center)*1.35 for p in inner];n=len(inner)
        verts=[(p.x,6.2,s*p.y) for p in inner]+[(p.x,5.8,s*p.y) for p in outer]+[(p.x,4.45,s*p.y) for p in inner]
        faces=[]
        for i in range(n):
            j=(i+1)%n;faces.extend([(i,j,n+j,n+i),(i,2*n+i,2*n+j,j)])
        mesh('deep orbital rim',verts,faces,'bone',True)
        plate('orbital recess',[(p.x,4.35,s*p.y) for p in inner],(0,1,0),.08,'cavity',.03)
        plate('sunken ember eye',[(8.5,5.05,s*2.5),(7.9,5.08,s*3.12),(7.1,5.1,s*2.5),(7.8,5.08,s*1.95)],(0,1,0),.14,'eye',.12)
        plate('supraorbital brow',[(9.35,6.2,s*.65),(10.5,5.25,s*1.2),(10.45,4.9,s*3.55),(9.25,5.8,s*4.7),(8.98,6.35,s*4.05),(9.4,6.65,s*2)],(0,1,0),.25,'bone',.1)
        pathbone('zygomatic arch',[(6.2,5.8,s*4.15),(5.2,4.9,s*4.9),(5.25,2.4,s*4.2),(6.45,1.3,s*3.6)],.53,.66)
        plate('maxilla cheek',[(5.8,6.7,s*.8),(6.1,6.3,s*1.7),(5.15,6.8,s*3.3),(4.2,7.4,s*3.2),(2.9,8,s*1.5),(3.1,8.3,s*.85)],(0,1,0),.6,'bone',.18)
        plate('nasal sidewall',[(7.2,6.2,s*.15),(6.7,6.6,s*.85),(3.9,8.7,s*1.03),(4.2,8.55,s*.6)],(0,1,0),.2,'bone',.12)
    plate('nasal aperture',[(7,6.7,0),(4,8.5,-.77),(3.7,8.6,0),(4,8.5,.77)],(0,1,0),.06,'cavity',.03)
    # Continuous alveolar arch with individually beveled incisors and canines.
    pathbone('upper dental arch',[(2.7,6.9,-3.45),(2.5,8.1,-2.4),(2.55,8.45,0),(2.5,8.1,2.4),(2.7,6.9,3.45)],.72,.52)
    for i in range(-4,5):
        z=i*.73;y=8.55-.10*z*z;length=1.3 if abs(i)==3 else .92
        plate('upper tooth',[(2.35,y-.12,z-.3),(2.4,y-.1,z+.3),(2.35-length,y+.02,z+.25),(2.35-length,y+.05,z-.25)],(0,1,0),.37,'bone',.19)
    # Fine sutures are shallow, fitted lines on the dome.
    pathbone('cranial suture',[(14.65,1.05,-.3),(13.7,3.0,-.35),(12.5,4.2,-.22),(11.35,4.95,-.36)],.038,.042,'shadow',6,2)
    if species>=1:
        pathbone('coronet circlet',[(10.9,2.1,-4.35),(12.8,4.0,-2.9),(13.4,4.2,0),(12.8,4,2.9),(10.9,2.1,4.35)],.24,.32,'trim',8,3)
    if species==2:
        for z in (-2.5,0,2.5):
            x=13.1-abs(z)*.13
            plate('crown point',[(x,4.3,z-.55),(x+2.9,3.75,z),(x,4.3,z+.55),(x-.6,4.6,z)],(0,1,0),.22,'trim',.08)
        plate('crown stone',[(14.5,4.08,0),(13.75,4.4,-.4),(13,4.7,0),(13.75,4.4,.4)],(0,1,0),.2,'eye',.2)

def jaw():
    for s in (-1,1):
        # Flattened ascending ramus into the U-shaped mandibular body.
        pathbone('mandibular body',[(.3,0,s*4.5),(1,-2.4,s*4.8),(2.6,-6,s*4.15),(3.45,-7.4,s*2.4),(3.6,-7.7,0)],.58,.78)
        plate('mandible ramus',[(.1,.3,s*4.25),(-.5,-2.5,s*4.8),(1.75,-5.1,s*4.45),(2.5,-4.4,s*4.3),(1.2,-1.5,s*4.45)],(0,0,s),.46,'bone',.1)
    for i in range(-4,5):
        z=i*.73;y=-7.4+.11*z*z
        plate('lower tooth',[(3.8,y,z-.29),(3.8,y,z+.29),(4.9,y+.07,z+.25),(4.9,y+.07,z-.25)],(0,-1,0),.3,'bone',.15)

def vertebrae(length):
    loft('vertebral core',[(0,0,.72,.72),(length,0,.72,.72)],'shadow',True,10)
    for x in [i*1.4+.6 for i in range(int(length/1.4))]:
        loft('vertebral body',[(x-.48,0,1.3,1.45),(x-.26,0,1.6,1.75),(x+.25,0,1.58,1.72),(x+.48,0,1.28,1.4)],'bone',True,10)
        for s in (-1,1):
            plate('transverse process',[(x-.3,-.15,s*1.2),(x-.2,-.5,s*2.4),(x+.3,-.7,s*2.15),(x+.45,-.1,s*1.2)],(0,-1,0),.27,'bone',.1)
        plate('spinous process',[(x-.4,-1.15,-.3),(x-.2,-2.7,0),(x+.4,-1.6,.3),(x+.4,-1.1,-.3)],(0,-1,0),.15,'bone',.1)

def chest():
    # Lower thoracic vertebrae belong to limb 14; avoid overlapping that mesh.
    before=len(parts);vertebrae(6.2)
    for ob in parts[before:]:
        for v in ob.data.vertices: v.co.x+=6.5
    for r in range(6):
        x=3.1+r*1.72;width=5.5+math.sin((r+1)*.45)*2.2;front=6+r*.32
        for s in (-1,1):
            pathbone('continuous rib',[(x,-.75,s*1.25),(x-.9,.8,s*width),(x-1.2,4.1,s*(width+.25)),(x-.25,front,s*3.5),(x+.8,front+.35,s*.45)],.28+r*.02,.43+r*.015,'bone',8,4)
    plate('sternum',[(3.5,6.15,0),(5.7,7.15,-.7),(11.2,8.05,-.85),(13.6,8.2,-.6),(13.6,8.2,.6),(11.2,8.05,.85),(5.7,7.15,.7)],(0,1,0),.5,'bone',.13)
    for s in (-1,1):
        pathbone('clavicle',[(13.55,4,s*.8),(13.9,3.6,s*3.5),(13.4,1.4,s*7.75)],.48,.62,'bone',10,5)
        plate('scapular wing',[(12,-1.5,s*2),(12.3,-1.2,s*5.5),(10.6,-1.25,s*7),(6.1,-1.35,s*4),(9.2,-1.8,s*2.8)],(0,-1,0),.35,'bone',.1)
        pathbone('scapular spine',[(11.8,-1.9,s*2.5),(10.8,-1.95,s*4.6),(11.8,-1.6,s*6.5)],.21,.3,'bone',8,3)
        # Flat leather harness conforms to the rib cage.
        plate('diagonal harness',[(13.6,3.5,s*6.3),(13.2,3.8,s*7.3),(7.4,7.4,s*2.5),(8.1,7.65,s*1.6)],(0,1,0),.24,'strap',.08)
    plate('chest clasp',[(11.3,8.6,0),(9.8,8.4,-1.25),(8.3,8.1,0),(9.8,8.4,1.25)],(0,1,0),.3,'armor',.13)
    plate('clasp inlay',[(10.7,8.95,0),(9.8,8.74,-.65),(8.95,8.57,0),(9.8,8.74,.65)],(0,1,0),.13,'trim',.12)
    if species==2:
        for s in (-1,1):
            plate('warden gorget',[(12.9,7.9,s*.6),(14,6.4,s*2.5),(13.7,4.9,s*5.2),(12,6.1,s*5),(10.7,7.9,s*2)],(0,1,0),.42,'armor',.09)
            pathbone('gorget chased edge',[(12.8,8.35,s*.8),(13.5,6.85,s*2.5),(13.25,5.65,s*4.8)],.1,.1,'trim',6,3)

def longbone(length,radius,paired=False):
    if paired:
        for s in (-1,1):
            rings=[(-.6,0,radius*.63,radius*.4),(.1,0,radius*.82,radius*.45),(length*.17,.05,radius*.5,radius*.25),(length*.5,-.15,radius*.4,radius*.21),(length*.8,0,radius*.48,radius*.26),(length,0,radius*.72,radius*.43),(length+.4,0,radius*.55,radius*.36)]
            ob=loft('radius and ulna' if length<12 else 'tibia and fibula',rings,'bone',True,12)
            for v in ob.data.vertices: v.co.z+=s*radius*.58
    else:
        loft('anatomical long bone',[(-.6,0,radius*.8,radius*.86),(0,0,radius,radius*1.1),(radius*.6,.1,radius*.95,radius),(length*.22,.1,radius*.55,radius*.62),(length*.52,-.13,radius*.46,radius*.51),(length*.78,-.07,radius*.57,radius*.62),(length-.3,0,radius*.9,radius*1.12),(length+.55,0,radius*.7,radius*.95)],'bone',True,14)
        for s in (-1,1):
            plate('condylar ridge',[(length-.9,-radius*.6,s*radius*.6),(length+.25,-radius*.68,s*radius*.9),(length+.65,0,s*radius*.82),(length-.55,radius*.3,s*radius*.96)],(0,0,s),.19,'bone',.15)
    pathbone('cortical ridge',[(length*.16,-radius*.61,.2),(length*.44,-radius*.49,.28),(length*.78,-radius*.5,.3)],.07,.13,'bone',6,3)

def armor_shell(name,from_x,to_x,ry,rz,cy=0,cz=0,mat='armor'):
    # Closed curved plate with crisp rolled perimeter and a raised central keel.
    verts=[];slices=12;rows=[(from_x,1.03),((from_x+to_x)*.5,1),(to_x,.86)]
    for layer in range(2):
        for x,scale in rows:
            for i in range(slices+1):
                a=-.22+math.pi*1.14*i/slices;depth=0 if layer==0 else -.28
                verts.append((x,cy+math.sin(a)*(ry*scale+depth),cz+math.cos(a)*(rz*scale+depth)))
    stride=slices+1;count=stride*3;faces=[]
    for layer in range(2):
        for r in range(2):
            for i in range(slices):
                a=layer*count+r*stride+i;faces.append((a,a+1,a+1+stride,a+stride))
    for row in (0,2):
        for i in range(slices):
            a=row*stride+i;faces.append((a,a+count,a+1+count,a+1))
    for i in (0,slices):
        for r in range(2):
            a=r*stride+i;faces.append((a,a+stride,a+stride+count,a+count))
    mesh(name,verts,faces,mat,True)
    for row in (0,2):
        pathbone(name+' rim',[verts[row*stride+i] for i in range(stride)],.105,.11,'trim',6,1)
    plate(name+' central flute',[(from_x+.4,cy+ry*1.08,cz),(from_x+1,cy+ry*1.13,cz-.38),(to_x-.4,cy+ry*.91,cz),(from_x+1,cy+ry*1.13,cz+.38)],(0,1,0),.12,'trim',.1)

def shoulder(left):
    before=len(parts)
    # Overlapping angular lames, no spherical shoulder core.
    for j in range(3):
        x=-.4+j*2.2
        armor_shell('lamellar pauldron',x,x+2.7,3.3-j*.25,4.3-j*.22,2.2,1.7)
    plate('pauldron crown',[(-.3,5.25,-.8),(1,6,-1),(4.8,5.9,-.2),(7.3,4.5,1.7),(4.8,5.9,3.6),(1,6,4.4),(-.3,5.25,3.8)],(0,1,0),.65,'armor',.09)
    if species>=1:
        for x in (1,3.7):
            plate('shoulder heraldry',[(x-.6,6.65,1.7),(x,6.8,.95),(x+.7,6.55,1.7),(x,6.8,2.45)],(0,1,0),.19,'trim',.12)
    if species==2:
        for x in (1,4.2):
            plate('warden shoulder fin',[(x-.8,5.9,1.1),(x-.5,9,1.45),(x+1.2,7.8,1.6),(x+1.6,5.9,1.9)],(0,0,1),.42,'armor',.08)
    if left:
        for ob in parts[before:]:
            for v in ob.data.vertices: v.co.z=-v.co.z
            bm=bmesh.new();bm.from_mesh(ob.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(ob.data);bm.free()

def hand():
    for i in range(4):
        z=-1.7+i*1.15
        pathbone('metacarpal',[(0,.1,z*.62),(.8,1.8,z),(1.4,3,z)],.36,.42,'bone',8,3)
        pathbone('curled phalanges',[(1.4,3,z),(2.6,3.7,z),(2.25,4.8,z),(.5,5.3,z)],.34,.38,'bone',8,3)
        for x,y in [(1.4,3),(2.55,3.8),(2.15,4.8)]:
            plate('phalange joint',[(x-.34,y-.3,z-.35),(x+.35,y-.3,z-.35),(x+.35,y+.3,z-.35),(x-.34,y+.3,z-.35)],(0,0,-1),.66,'bone',.26)
    pathbone('thumb',[(0,0,-1.25),(-1.2,1.7,-2.3),(-1.3,3.8,-2.4),(.15,4.85,-1.3)],.45,.51,'bone',10,4)
    loft('wrist carpals',[(-.7,.1,1.1,1.1),(.15,.35,1.35,1.65),(.8,.4,1.1,1.65)],'bone',True,10)

def shield():
    first_piece=len(parts)
    # Forged octagonal guard with a raised center and a visible layered edge.
    outline=[(math.sin(i*math.tau/12)*11.2,3.8+math.cos(i*math.tau/12)*11.4,6) for i in range(12)]
    plate('forged twelve-sided shield',outline,(0,0,1),1.4,'armor',.095)
    for radius,depth,mat in [(1,6.75,'trim'),(.84,7.45,'trim')]:
        pts=[(p[0]*radius,3.8+(p[1]-3.8)*radius,depth) for p in outline];pathbone('shield chased rim',pts+[pts[0]],.19 if radius==1 else .085,None,mat,8,1)
    # Original angular wing-and-blade heraldry, built as fitted metal inlays.
    for sign in (-1,1):
        for j in range(3):
            y=1.1+j*2.4
            plate('wing heraldry',[(sign*.9,y,7.68),(sign*(7.1-j*.85),y-1.6,7.66),(sign*(6.25-j*.7),y+.25,7.7),(sign*1.8,y+1.85,7.74)],(0,0,1),.12,'trim',.08)
    plate('central shield blade',[(0,-4.6,7.7),(-1.1,3.8,7.75),(0,10.75,7.72),(1.1,3.8,7.75)],(0,0,1),.28,'trim',.12)
    plate('heraldic stone',[(0,1.2,8.05),(-1.3,4,8.08),(0,6.8,8.05),(1.3,4,8.08)],(0,0,1),.4,'eye' if species==2 else 'inlay',.13)
    for i in range(12):
        a=(i+.5)*math.tau/12;x=math.sin(a)*10.45;y=3.8+math.cos(a)*10.5
        plate('shield rivet',[(x-.23,y-.23,7.05),(x+.23,y-.23,7.05),(x+.23,y+.23,7.05),(x-.23,y+.23,7.05)],(0,0,1),.25,'trim',.28)
    pathbone('shield rear grip',[(-3.5,2.4,3.8),(-2.7,2.4,3.1),(2.7,2.4,3.1),(3.5,2.4,3.8)],.45,.55,'strap',10,3)
    if species>=1:
        for s in (-1,1):
            plate('shield edge reinforcement',[(s*6.4,11.5,6.9),(s*9.8,9.3,6.9),(s*8.2,8.7,7.7),(s*6.1,10,7.75)],(0,0,1),.2,'steel',.1)
    if species==2:
        plate('warden shield crest',[(-2.2,13.4,6.9),(-2,16,6.3),(0,14.9,7),(2,16,6.3),(2.2,13.4,6.9)],(0,0,1),.4,'trim',.08)
    # Native shield-local X points upward in the guard pose. Rotate the
    # decorative face around its center; its rear grip stays fitted to the hand.
    for ob in parts[first_piece:]:
        if 'shield rear grip' in ob.name: continue
        for v in ob.data.vertices:
            x,y,z=v.co;v.co=(y-3.8,3.8-x,z)

def sword():
    ob=loft('wrapped sword grip',[(-4.2,2.8,.7,.7),(-3.7,2.8,.79,.79),(4.9,2.8,.69,.69)],'strap',False,12)
    for j in range(10): band('grip binding',-3.5+j*.85,.79,.79,.07,2.8,'trim')
    widths=[1.4,2.6,3.0,2.25,1.1,.018];xs=[5.2,7.1,12.5,22,30.5,34];verts=[]
    for x,w in zip(xs,widths):
        verts.extend([(x,2.8-w,0),(x,2.8-w*.72,.28),(x,2.8,.64),(x,2.8+w*.72,.28),(x,2.8+w,0),(x,2.8+w*.72,-.28),(x,2.8,-.64),(x,2.8-w*.72,-.28)])
    faces=[tuple(reversed(range(8)))];inks=['steel']
    for j in range(5):
        for k in range(8): faces.append((j*8+k,j*8+(k+1)%8,(j+1)*8+(k+1)%8,(j+1)*8+k));inks.append('edge' if k in (0,3,4,7) else 'steel')
    faces.append(tuple(40+i for i in range(8)));inks.append('edge');mesh('forged ridged blade',verts,faces,'steel',False,inks)
    plate('swept crossguard',[(4.5,-2.2,0),(6,.3,0),(5.6,2.8,0),(6,5.3,0),(4.5,7.8,0),(4.4,6.3,0),(4.65,2.8,0),(4.4,-.7,0)],(0,0,1),.65,'trim',.14)
    plate('faceted pommel',[(-4.5,1.8,0),(-5.8,2.8,0),(-4.5,3.8,0),(-3.9,2.8,0)],(0,0,1),.85,'trim',.16)
    for s in (-1,1):
        plate('recessed fuller',[(8.8,2.8,s*.65),(10,2.47,s*.57),(28.8,2.8,s*.63),(10,3.13,s*.57)],(0,0,s),.018,'shadow',.03)
        if species>=1:
            for x in (10.3,14.2,18.1):
                plate('blade sigil',[(x,2.8,s*.69),(x+.5,2.3,s*.61),(x+1,2.8,s*.69),(x+.5,3.3,s*.61)],(0,0,s),.025,'eye' if species==2 else 'trim',.08)

def foot():
    loft('calcaneus and talus',[(-1.8,-.05,1.5,1.6),(-.2,0,2.2,2.05),(1.6,-.5,1.7,2.0),(3,-.8,1,1.8)],'bone',True,12)
    for i in range(5):
        z=-2.45+i*1.2;length=9.6-abs(i-2)*.85
        pathbone('metatarsal',[(1,-.7,z*.6),(4,-1.5,z),(6.3,-1.95,z)],.35,.44,'bone',8,3)
        pathbone('toe phalanges',[(6.1,-1.95,z),(7.2,-2.2,z),(length,-2.7,z)],.4,.42,'bone',8,3)
    armor_shell('ankle guard',-.75,2.3,2.6,2.45)

def pelvis():
    for s in (-1,1):
        # Broad iliac wings and separate lower arches retain real negative space.
        plate('iliac wing',[(1,-2,s*1.8),(.2,.9,s*4.8),(3.4,4.15,s*7.25),(6.8,3.7,s*7.1),(8.4,1.1,s*4.35),(6.5,-1.5,s*2.1)],(0,0,s),.65,'bone',.13)
        pathbone('iliac crest',[(.2,.8,s*4.8),(3.2,4.5,s*7.35),(6.7,4.1,s*7.35),(8.6,1.1,s*4.35)],.49,.56,'bone',10,4)
        pathbone('obturator arch',[(1,-2.2,s*1.3),(3,-4.4,s*3.8),(6,-3,s*5.2),(7,-.9,s*2.1)],.49,.58,'bone',10,4)
    band('war belt',5.6,5.6,7.5,.52,0,'strap')
    for z in (-4.5,-1.5,1.5,4.5):
        plate('torn layered tabard',[(4.8,5.9,z-1.35),(4.8,5.9,z+1.35),(-2.8,4.45,z+1.1),(-4.2,4.2,z+.3),(-3.2,4.3,z-.35),(-4.7,4.05,z-.9)],(0,1,0),.22,'cloth',.025)
        pathbone('tabard embroidered edge',[(4.25,6.07,z-1.05),(.5,5.34,z-.88),(-2.8,4.65,z-.8)],.06,.07,'trim',6,2)
    plate('belt buckle',[(4.5,6,-1.4),(6.7,6,-1.4),(6.7,6,1.4),(4.5,6,1.4)],(0,1,0),.35,'trim',.18)
    if species==2: plate('belt stone',[(6.5,6.4,0),(5.6,6.5,-.55),(4.7,6.4,0),(5.6,6.5,.55)],(0,1,0),.25,'eye',.1)

for species in range(3):
    for limb in (10,11,12,13,14,15,18,21,26,27,28,29,34,35,36,37,38,48,49,50,57,58,59,60):
        if limb==10: jaw()
        elif limb==11: skull()
        elif limb in (12,13,14,38): vertebrae({12:4.8,13:5.4,14:6.5,38:6.9}[limb])
        elif limb==15: chest()
        elif limb in (18,21): shoulder(limb==21)
        elif limb in (26,35): hand()
        elif limb==27: shield()
        elif limb==34: sword()
        elif limb in (28,36):
            longbone(9.65,1.45,True);armor_shell('forearm guard',2.1,8.4,2.7,2.7);band('forearm strap',1.8,1.9,1.9,.23)
        elif limb in (29,37): longbone(12.31,1.55)
        elif limb in (48,57): foot()
        elif limb in (49,58):
            longbone(16.65,1.9,True);armor_shell('fluted greave',2.2,13.5,3.1,2.95);armor_shell('pointed knee guard',-.5,2,4.0,3.6)
        elif limb in (50,59): longbone(10.87,1.45);band('thigh strap',4,1.65,1.65,.23)
        elif limb==60: pelvis()
# Bone shafts retain their anatomically placed ridges; only the continuous
# vault/long-bone topology receives a modest subdivision surface.
for ob in parts:
    if any(w in ob.name for w in ('anatomical long bone','radius and ulna','tibia and fibula','calcaneus and talus')):
        mod=ob.modifiers.new('Cortical surface refinement','SUBSURF');mod.subdivision_type='CATMULL_CLARK';mod.levels=1;mod.render_levels=1
world=bpy.data.worlds.get('ARPG Stalfos World') or bpy.data.worlds.new('ARPG Stalfos World');scene.world=world;world.use_nodes=True
bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs[0].default_value=(.06,.07,.08,1);bg.inputs[1].default_value=.5
for name,location,power,size in [('Key',(-85,-95,130),340000,75),('Fill',(100,-70,75),250000,65),('Rim',(0,90,135),460000,60)]:
    data=bpy.data.lights.new('Stalfos '+name,'AREA');data.energy=power;data.shape='DISK';data.size=size
    ob=bpy.data.objects.new(data.name,data);scene.collection.objects.link(ob);ob.location=location;ob.rotation_euler=(Vector((0,0,27))-ob.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Stalfos Camera');ob=bpy.data.objects.new(data.name,data);scene.collection.objects.link(ob)
ob.location=(35,-240,88);ob.rotation_euler=(Vector((0,0,28))-ob.location).to_track_quat('-Z','Y').to_euler();data.type='ORTHO';data.ortho_scale=170;scene.camera=ob
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1700;scene.render.resolution_y=900;scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG'
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D': area.spaces.active.shading.type='MATERIAL';area.spaces.active.region_3d.view_perspective='CAMERA'
bpy.context.view_layer.update()
for ob in scene.objects: ob.select_set(ob.type=='MESH')
bpy.ops.export_scene.gltf(filepath=str(ROOT/'stalfos.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_apply=True,export_extras=True)
print('Authored',len(parts),'Stalfos mesh pieces across all three ranks')
