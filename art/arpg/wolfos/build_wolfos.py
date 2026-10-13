"""Blender-authored Wolfos in the native flex-joint spaces.

Run via the local Blender MCP or Blender's Text Editor. Only this workshop scene
is rebuilt. The export contains anatomy, accessories, blink shapes and bridges.
"""
import bpy
import bmesh
import json
import math
import struct
from pathlib import Path
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parent
assert bpy.app.background and not bpy.data.filepath, 'Run with --background --factory-startup'
fur_image=bpy.data.images.load(str(ROOT/'fur-albedo.png'),check_existing=True)
scene=bpy.data.scenes.get('ARPG Wolfos Workshop') or bpy.data.scenes.new('ARPG Wolfos Workshop')
bpy.context.window.scene=scene
for obj in list(scene.objects): bpy.data.objects.remove(obj,do_unlink=True)
pose={int(k):Matrix(v) for k,v in json.loads((ROOT/'native_pose.json').read_text()).items()}
links={int(k):v for k,v in json.loads((ROOT/'native_links.json').read_text()).items()}
axis=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
MATERIALS=['fur','mane','muzzle','skin','claw','metal','eye','mouth','pupil','highlight','silver']
palettes=[[(137,122,96),(89,85,69),(186,171,138),(41,40,37),(216,205,168),(159,126,69),(245,161,63),(103,48,49),(17,20,17),(237,245,227),(109,124,119)],
          [(193,210,217),(133,156,174),(226,227,211),(40,52,65),(215,225,225),(146,168,183),(116,228,243),(111,62,76),(19,29,39),(239,249,250),(199,216,224)]]
materials={};parts=[];context={'rank':0,'blink':-1};species=0;limb=7;face_surfaces={};attachments=[]
for sp in range(2):
 for i,(name,rgb) in enumerate(zip(MATERIALS,palettes[sp])):
  mat=bpy.data.materials.get(f'Wolfos {sp} {name}') or bpy.data.materials.new(f'Wolfos {sp} {name}')
  mat.diffuse_color=(*[v/255 for v in rgb],1);mat.use_nodes=True
  node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
  node.inputs['Base Color'].default_value=(*[((v/255+.055)/1.055)**2.4 if v>10 else v/3294.6 for v in rgb],1)
  node.inputs['Metallic'].default_value=.65 if name in ('metal','silver') else 0
  node.inputs['Roughness'].default_value=.4 if name in ('metal','silver') else .82
  if name in ('fur','mane','muzzle'):
   tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=fur_image;tex.extension='REPEAT'
   tint=mat.node_tree.nodes.new('ShaderNodeMixRGB');tint.blend_type='MULTIPLY';tint.inputs[0].default_value=1
   tint.inputs[2].default_value=(*[min(1,v/255*1.55) for v in rgb],1)
   mat.node_tree.links.new(tex.outputs['Color'],tint.inputs[1]);mat.node_tree.links.new(tint.outputs[0],node.inputs['Base Color'])
  materials[sp,i]=mat

def mesh(name, vertices, faces, mat='fur', smooth=False, face_mats=None):
    data = bpy.data.meshes.new(name)
    data.from_pydata(vertices, [], faces)
    data.update()
    bm = bmesh.new(); bm.from_mesh(data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(data); bm.free()
    uv=data.uv_layers.new(name='Coat flow')
    for poly in data.polygons:
        coords=[]
        for index in poly.loop_indices:
            p=data.vertices[data.loops[index].vertex_index].co
            coords.append([math.atan2(p.z,p.y)/math.tau+.5,p.x/32])
        if max(p[0] for p in coords)-min(p[0] for p in coords)>.5:
            for p in coords:
                if p[0]<.5:p[0]+=1
        for index,p in zip(poly.loop_indices,coords):uv.data[index].uv=p
    for i in range(len(MATERIALS)):
        data.materials.append(materials[species,i])
    for i,p in enumerate(data.polygons):
        p.material_index = MATERIALS.index(face_mats[i] if face_mats else mat)
        p.use_smooth = smooth
    obj = bpy.data.objects.new(f'{"Frost" if species else "Timber"}.{limb:02}.{name}',data)
    scene.collection.objects.link(obj)
    obj['arpg_species'] = species; obj['arpg_limb'] = limb
    obj['arpg_rank'] = context['rank']; obj['arpg_blink'] = context['blink']
    obj.matrix_world = Matrix.Translation(((-45 if species==0 else 45),0,0)) @ axis @ pose[limb]
    parts.append(obj)
    return obj

def loft(name, rings, mat='fur', smooth=True, sections=12, stripe=False):
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
            inks.append('mane' if stripe and i in (5,6) else mat)
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

def surface_tree(ob):
    bpy.context.view_layer.update()
    evaluated=ob.evaluated_get(bpy.context.evaluated_depsgraph_get());data=evaluated.to_mesh()
    tree=BVHTree.FromPolygons([v.co.copy() for v in data.vertices],[list(p.vertices) for p in data.polygons])
    evaluated.to_mesh_clear();return tree

def attached_horn(name,host,origin,direction,length,radius,bend=(0,0,0),sides=10):
    out=Vector(direction).normalized();tree=surface_tree(host)
    hit,normal,_,_=tree.ray_cast(Vector(origin),-out,80)
    assert hit is not None, f'{name}: no attachment surface'
    inset=max(radius*2,1.2);curve=Vector(bend)
    ob=horn(name,[hit-normal*inset,hit+out*.25,hit+out*(length*.6)+curve*.45,hit+out*length+curve],
            [radius,radius*.88,radius*.43,.035],'claw',sides)
    attachments.append((ob,host,sides));return ob

def face_patch(name,side,height,mat,offset):
    trees=[surface_tree(ob) for ob in face_surfaces[species]]
    vertices=[];rings=8;steps=32
    for ring in range(rings+1):
        for step in range(steps if ring else 1):
            a=step*math.tau/steps;r=ring/rings
            x=13.4+3.4*r*math.cos(a)
            y=-4.05+.22*(x-13.4)+height*r*math.sin(a)
            hits=[tree.ray_cast(Vector((x,y,side*40)),Vector((0,0,-side)),80) for tree in trees]
            hits=[h for h in hits if h[0] is not None]
            assert hits, f'{name}: no face beneath eye at {x},{y}'
            hit,normal,_,_=min(hits,key=lambda h:h[3])
            vertices.append(tuple(hit+normal*offset))
    faces=[]
    for i in range(steps):faces.append((0,1+i,1+(i+1)%steps))
    for ring in range(rings-1):
        base=1+ring*steps
        for i in range(steps):faces.append((base+i,base+(i+1)%steps,base+steps+(i+1)%steps,base+steps+i))
    return mesh(name,vertices,faces,mat,True)


def wedge(name,rings,mat='fur',smooth=True):
    # x, upper/lower Y, width. Broad skull planes and a flat nasal bridge.
    v=[]
    for x,top,bottom,w in rings:
        middle=(top+bottom)*.5
        v.extend([(x,top,0),(x,top+.4,w*.62),(x,top+1.3,w*.9),(x,middle,w),(x,bottom-.6,w*.85),(x,bottom,w*.5),(x,bottom,0),(x,bottom,-w*.5),(x,bottom-.6,-w*.85),(x,middle,-w),(x,top+1.3,-w*.9),(x,top+.4,-w*.62)])
    f=[tuple(reversed(range(12)))]
    for j in range(len(rings)-1):
        for i in range(12): f.append((j*12+i,j*12+(i+1)%12,(j+1)*12+(i+1)%12,(j+1)*12+i))
    f.append(tuple((len(rings)-1)*12+i for i in range(12)))
    ob=mesh(name,v,f,mat,smooth)
    mod=ob.modifiers.new('Sculpted facial contour','SUBSURF');mod.levels=1;mod.render_levels=1
    return ob

def fur_lock(name,root,tip,width,lift,normal,mat='mane'):
    # A low, swept ridge whose broad root is buried in the skin. Three pointed
    # ends break the silhouette without the beads/cones of the previous model.
    r=Vector(root);t=Vector(tip);n=Vector(normal).normalized()
    d=(t-r).normalized();side=d.cross(n).normalized()*width
    mid=r.lerp(t,.42)+n*lift
    v=[r-side,r+side,mid-side*.75,mid+side*.75,mid+n*lift*.25,
       t-side*.32-d*.7,t,t+side*.34-d*.9,r-n*.3]
    faces=[(0,2,4),(0,4,1),(1,4,3),(2,5,6,4),(4,6,7,3),(0,8,2),(1,3,8),(2,8,5),(5,8,6),(6,8,7),(7,8,3),(0,1,8)]
    return mesh(name,[tuple(p) for p in v],faces,mat,True)

def bridge(start,end,ry,rz,scale=1,mat='fur'):
    # The parent ring is buried in the parent muscle, aligned to that bone.
    # The child ring is buried in the child muscle. This avoids exposed cuffs
    # at sharply bent wrists and elbows while keeping a two-matrix native skin.
    link=links[limb];m=link['transform'];bind=Matrix([m[i:i+4] for i in (0,4,8)]+[[0,0,0,1]])
    anchor=bind.translation.copy()
    center=Vector((0,2.5,-1)) if link['parent']==7 else Vector((anchor.x,0,0)) if link['parent']==12 and limb in (13,19) else Vector((0,0,0))
    direction=(anchor-center).normalized()
    root=anchor-direction*3.2
    u=bind.to_3x3()@Vector((0,1,0));u=(u-direction*u.dot(direction)).normalized();v=direction.cross(u)
    inverse=bind.inverted();verts=[]
    shrink=.72 if link['parent'] not in (7,12) else .78
    for i in range(20):
        a=i*math.tau/20
        verts.append(tuple(inverse@(root+u*math.cos(a)*ry*shrink+v*math.sin(a)*rz*shrink)))
    for i in range(20):
        a=i*math.tau/20
        verts.append((end,math.cos(a)*ry*scale*.78,math.sin(a)*rz*scale*.78))
    faces=[(i,(i+1)%20,20+(i+1)%20,20+i) for i in range(20)]
    ob=mesh('flex skin connection',verts,faces,mat,True)
    ob['arpg_parent_ring_points']=[list(p) for p in verts[:20]]
    ob['arpg_child_ring_points']=[list(p) for p in verts[20:]]
    ob['arpg_parent_joint']=link['parent'];ob['arpg_parent_transform']=m

def skull():
    cranium=wedge('canine cranial planes',[(-5,-6,5,5.6),(-2,-9,6.5,8),(3,-10,6.5,9.4),(8,-9,5.6,9.2),(13,-6.6,3.2,7.4),(17,-5,2.4,5.3)],smooth=True)
    muzzle=wedge('tapered nasal bridge',[(12,-5,2.7,5.5),(18,-5.2,2.5,4.9),(24,-4.7,2.3,4.35),(29,-3.9,1.7,3.65)],'muzzle')
    face_surfaces[species]=[cranium,muzzle]
    wedge('angular nose leather',[(28,-4.3,1.9,3.65),(30.8,-3.5,1.3,3.85),(32,-2.9,.4,2.8)],'skin',False)
    jaw=wedge('separate lower mandible',[(7,5,8,5.4),(12,6.7,10.5,5.3),(21,7.5,10.3,4.6),(28,6.4,8.8,3.65),(30,6.2,7.7,2.7)],'muzzle')
    wedge('recessed mouth cavity',[(12,3.1,6.6,4.8),(21,2.6,7.7,4.1),(28,2.1,6.5,3.1)],'skin',False)
    plate('tongue',[(12,7,-2.6),(23,7.6,-2.8),(27,6.9,-1.3),(27,6.9,1.3),(23,7.6,2.8),(12,7,2.6)],(0,-1,0),.17,'mouth',.1)
    for s in (-1,1):
        plate('nasal cleft',[(30.8,-2.7,s*3.8),(31.8,-2.15,s*2.9),(31.8,-1.05,s*2.9),(30.7,-1.2,s*3.9)],(.2,-.1,s),.06,'pupil',.08)
        face_patch('fitted orbital socket',s,1.6,'skin',.035)
        # One continuous ear shell: the inset follows its rim, so dark cartilage
        # cannot intersect a differently sloped outer panel.
        ear=[Vector((5,-8,s*4.8)),Vector((5,-8,s*9.6)),Vector((1,-18.5,s*10.5)),Vector((0,-14,s*6.7))]
        center=sum(ear,Vector())/4
        inner=[p.lerp(center,.25)+Vector((.45,0,0)) for p in ear]
        back=[p-Vector((1.1,0,0)) for p in ear]
        verts=ear+inner+back+[center+Vector((.3,0,0))]
        faces=[];inks=[]
        for j in range(4):
            k=(j+1)%4
            faces.extend([(j,k,4+k,4+j),(4+j,4+k,12),(j,8+j,8+k,k)])
            inks.extend(['fur','skin','mane'])
        faces.append((8,9,10,11));inks.append('mane')
        mesh('rimmed canine ear',[tuple(p) for p in verts],faces,'fur',False,inks)
        for j in range(2):
            fur_lock('swept cheek ruff',(7-j*1.9,1.5+j*.8,s*8.5),(-3-j*2.5,5+j*1.3,s*(11.3-j*.25)),1.7,1.15,(0,0,s),'fur' if j%2 else 'mane')
        for j in range(5):
            x=14+j*2.45;z=s*(3.55-j*.18)
            attached_horn('upper premolar',muzzle,(x,30,z),(0,1,0),2.1,.52,(-.25,0,0),8)
            if j<4:attached_horn('lower premolar',jaw,(x,-30,z),(0,-1,0),1.65,.43,(.25,0,0),8)
        attached_horn('curved canine',muzzle,(25,30,s*2.8),(0,1,0),4.4,.82,(-1,0,0))
        attached_horn('lower canine',jaw,(12.8,-30,s*3.6),(0,-1,0),2.65,.62,(.8,0,0))
    context['rank']=1
    for s in (-1,1):
        plate('temple guard',[(1,-7.3,s*8.1),(5,-7.6,s*8.5),(7,-4.9,s*9.3),(1,-2.1,s*10.1),(-2,-4.2,s*9.2)],(0,0,s),.5,'silver',.12)
    context['rank']=2
    for s in (-1,1):
        plate('veteran temple crest',[(-2,-5.5,s*9.4),(2,-6.4,s*9.3),(-5,-11.1,s*13.2),(-8,-9.4,s*13.4)],(0,0,s),.5,'metal',.1)
    context['rank']=0

def eyes():
    for blink in range(3):
        context['blink']=blink
        h=[1.25,.5,.095][blink]
        for s in (-1,1):
            face_patch('fitted blink '+str(blink),s,h,'eye',.12)
        context['blink']=-1

def anatomy(length,ry,rz,cy=0,fore=False,lower=False):
    rings=[(-1,0,ry*.72,rz*.75),(length*.1,cy*.6,ry*.9,rz*.91),(length*.31,cy,ry,rz),(length*.5,cy*.7,ry*.91,rz*.95),(length*.7,cy*.35,ry*.71,rz*.77),(length*.92,0,ry*.52,rz*.57),(length+.6,0,ry*.6,rz*.64)]
    loft('lean muscle and tendon',rings,'fur',True,16,stripe=True)
    if fore and lower:
        for s in (-1,1):
            for j in range(2): fur_lock('ulnar fur blade',(5+j*7,1,s*(rz-.4)),(1+j*7,2.4,s*(rz+1.2)),1.1,.35,(0,0,s),'mane')
        context['rank']=1
        # Fitted angular bracer only on the outer forearm; tendon still shows.
        plate('hunter vambrace',[(3,-ry*.5,-rz*.7),(6,-ry*1.04,-rz*.75),(length*.7,-ry*.77,-rz*.48),(length*.91,-ry*.57,0),(length*.7,-ry*.77,rz*.48),(6,-ry*1.04,rz*.75),(3,-ry*.5,rz*.7)],(0,-1,0),.6,'silver',.12)
        context['rank']=2
        plate('vambrace spine',[(5,-ry*1.08,0),(7,-ry*1.2,-.55),(length*.76,-ry*.88,0),(7,-ry*1.2,.55)],(0,-1,0),.25,'metal',.12)
        context['rank']=0

def paw(front):
    first_piece=len(parts)
    y=3 if front else 0;w=5.5 if front else 4.5
    rings=[(-1,0,2.5,w*.55),(2,y*.55,2.8,w*.87),(5,y,2.7,w),(8,y+.5,2.2,w*.97),(10,y+.9,1.7,w*.79)]
    palm=loft('flat metacarpal palm',rings,'fur',True,12)
    mod=palm.modifiers.new('Anatomical refinement','SUBSURF');mod.levels=1
    plate('central paw pad',[(2,y+2.7,-w*.35),(5,y+3.1,-w*.67),(9,y+2.9,-w*.35),(9,y+2.9,w*.35),(5,y+3.1,w*.67),(2,y+2.7,w*.35)],(0,1,0),.3,'skin',.22)
    for j in range(4):
        z=(j-1.5)*w*.5;x=8.9+(1.5-abs(j-1.5))*1.65
        ob=loft('articulated toe',[(x-3,y+.7,1.7,1.2),(x-1,y+.8,1.9,1.35),(x+1.2,y+1.05,1.65,1.1),(x+3,y+1.5,.85,.72)],'fur',True,10)
        for v in ob.data.vertices: v.co.z+=z
        mod=ob.modifiers.new('Anatomical refinement','SUBSURF');mod.levels=1
        attached_horn('fitted ivory talon',ob,(40,y+1.5,z),(1,0,0),3.9,.57,(0,2.1,0))
        fur_lock('knuckle fur',(x-1,y-1,z),(x-4.5,y-1.1,z),.75,.6,(0,-1,0),'mane')
    if front:
        attached_horn('fitted dewclaw',palm,(3,y+.5,20),(0,0,1),1.8,.55,(1.7,1.2,0),8)
        # [TIME] | [INFO] | Wolfos.Hands | Native left/right claws mirror Z. Bend the fingers down from the wrist, keeping the pad on +Y and knuckles on -Y.
        bend=Matrix.Rotation(math.radians(18),3,'Z')
        for ob in parts[first_piece:]:
            for v in ob.data.vertices:
                if limb==15:v.co.z=-v.co.z
                v.co=bend@v.co
            bm=bmesh.new();bm.from_mesh(ob.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(ob.data);bm.free()
            ob['arpg_hand']='right' if limb==15 else 'left'
            ob['arpg_palm_normal']=list(bend@Vector((0,1,0)))

def torso():
    rings=[(-2,0,6.1,6.2),(2,0,6.8,7.1),(7,-.1,7.8,8.8),(13,-.5,10.2,11.8),(19,-.5,11.3,12),(24,0,10.1,10.8),(29,.5,8.5,8.7),(34,.6,6.6,7.3)]
    loft('tapered thorax',rings,'fur',True,20)
    # Separate cream throat panel, contoured to the chest rather than another oval.
    plate('angular throat bib',[(7,7.8,0),(13,9.1,-5.8),(23,9.3,-6),(31,6.8,-2.8),(34,6.1,0),(31,6.8,2.8),(23,9.3,6),(13,9.1,5.8)],(0,1,0),.7,'muzzle',.08)
    # Long dorsal locks form a broken mane silhouette, especially the frost species.
    for j in range(4):
        x=10+j*5
        fur_lock('dorsal ruff',(x,-(9.2 if x<15 else 11 if x<24 else 9),0),(x-7,-(12 if species==0 else 13),0),1.6,.45,(0,-1,0),'mane')
    context['rank']=1
    loft('forged throat collar',[(26,0,9.48,10.25),(26.4,0,9.75,10.52),(27.5,0,9.36,10.13),(27.9,0,9.03,9.8)],'silver',False,20)
    for s in (-1,1):
        plate('collar side fitting',[(25,-3,s*10),(28,-3,s*9.6),(28,3,s*9.6),(25,3,s*10)],(0,0,s),.6,'metal',.17)
    context['rank']=2
    for s in (-1,1):
        plate('shoulder harness plate',[(13,-8,s*6.9),(19,-9,s*8.2),(24,-7,s*8.9),(23,-3,s*11.2),(16,-2,s*12.2),(11,-5,s*10.7)],(0,-.4,s),.7,'silver',.1)
        plate('harness gold seam',[(14,-8.5,s*7.5),(19,-9.5,s*8.8),(22.5,-8,s*9),(22.8,-7.7,s*9.15),(19,-9.3,s*9.05),(14,-8.25,s*7.75)],(0,-.4,s),.14,'metal',.09)
    plate('throat sigil',[(27,10.4,0),(25,11.2,-2),(22,12.2,0),(25,11.2,2)],(0,1,0),.5,'metal',.12)
    context['rank']=0

def pelvis():
    # Pelvis local X spans the body sideways; loft longitudinally in Z instead.
    ob=loft('haunch abdominal taper',[(-10,2.5,5.1,4.7),(-7,2.5,7.8,6.2),(-2,2.2,8.1,6.7),(3,3.6,7.4,5.8),(7,4,6.2,5.4),(10,4.2,4,4)],'fur',True,16)
    for v in ob.data.vertices: v.co=(v.co.z,v.co.y,v.co.x)
    # Reverse transform handedness; refresh winding, keeping outward normals.
    bm=bmesh.new();bm.from_mesh(ob.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(ob.data);bm.free()
    for s in (-1,1):
        for j in range(4): fur_lock('flank coat',(s*5.7,1.2,-4+j*2.6),(s*6.1,-.2,-9+j*2.6),1.5,.9,(s,0,0),'fur')

def tail():
    rings=[(-1,0,3.5,3.5),(3,.3,4.2,4.2),(9,1,4.5,4.3),(17,2.1,4.1,3.9),(25,2,2.9,2.8),(32,1.7,1.7,1.7),(39,1.5,.09,.09)]
    loft('continuous tail',rings,'fur',True,12)

for species in range(2):
    for limb in (2,3,4,5,6,7,8,9,10,11,12,13,14,15,17,18,19,20,21):
        context['rank']=0;context['blink']=-1
        if limb==7: pelvis()
        elif limb==12: torso();bridge(-1.5,8,6.1,6.2,1.5)
        elif limb==17: skull();bridge(-3,3,7,7,1,'mane')
        elif limb==18: eyes()
        elif limb==6: tail();bridge(-1,4,3.3,3.3)
        elif limb in (2,8): anatomy(13.18,5.2,5.4,.3);bridge(-1.5,4,3.8,3.8,1.18)
        elif limb in (3,9): anatomy(16.28,3.4,3.2,0,False,True);bridge(-1,3.5,3.2,3.2)
        elif limb in (4,10): anatomy(11.34,2.65,2.7);bridge(-1.5,3,2.65,2.65)
        elif limb in (5,11): paw(False);bridge(-1,3,2.6,2.6)
        elif limb in (13,19): anatomy(26.45,5.8,5.8,.9,True);bridge(-1,6,4.6,4.6,1.1)
        elif limb in (14,20): anatomy(21.52,4.9,4.6,1.3,True,True);bridge(-1,4,4.3,4.2)
        elif limb in (15,21): paw(True);bridge(-1.5,3,3.3,3.7)
# Smooth anatomy follows its cross sections; the face, ears, fur and metal keep
# their explicit planar breaks. No subdivision of the mixed-joint connections.
for ob in parts:
    if not ob.modifiers and any(w in ob.name for w in ('lean muscle','tapered thorax','continuous tail','haunch abdominal','flat metacarpal','articulated toe')):
        mod=ob.modifiers.new('Anatomical refinement','SUBSURF');mod.subdivision_type='CATMULL_CLARK';mod.levels=1;mod.render_levels=1
attachment_report=[]
for ob,host,count in attachments:
    tree=surface_tree(host);signed=[]
    for v in list(ob.data.vertices)[:count]:
        hit,normal,_,_=tree.find_nearest(v.co)
        signed.append((v.co-hit).dot(normal))
    assert max(signed)<.02, f'{ob.name}: root ring is outside {host.name}: {max(signed)}'
    attachment_report.append({'part':ob.name,'host':host.name,'max_root_signed_distance':max(signed)})
eye_distances=[]
for ob in parts:
    if 'fitted blink' not in ob.name:continue
    trees=[surface_tree(host) for host in face_surfaces[ob['arpg_species']]]
    for v in ob.data.vertices:
        distance=min(tree.find_nearest(v.co)[3] for tree in trees)
        eye_distances.append(distance)
assert max(eye_distances)<.15, 'Eye surface detached from the evaluated face'
(ROOT/'attachment-report.json').write_text(json.dumps({'attachments':attachment_report,
    'root_rings_inside_hosts':len(attachment_report),'eye_vertices':len(eye_distances),
    'max_eye_distance':max(eye_distances),'units':'one-hundredth native model units'},indent=2)+'\n')
world=bpy.data.worlds.get('ARPG Wolfos World') or bpy.data.worlds.new('ARPG Wolfos World');scene.world=world;world.use_nodes=True
bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs[0].default_value=(.055,.065,.085,1);bg.inputs[1].default_value=.5
for name,location,power,size in [('Key',(-80,-90,140),380000,80),('Fill',(100,-60,80),240000,70),('Rim',(0,100,140),440000,65)]:
    data=bpy.data.lights.new('Wolfos '+name,'AREA');data.energy=power;data.shape='DISK';data.size=size
    ob=bpy.data.objects.new(data.name,data);scene.collection.objects.link(ob);ob.location=location
    ob.rotation_euler=(Vector((0,-5,25))-ob.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Wolfos Camera');ob=bpy.data.objects.new(data.name,data);scene.collection.objects.link(ob)
ob.location=(125,-240,120);ob.rotation_euler=(Vector((0,-4,29))-ob.location).to_track_quat('-Z','Y').to_euler();data.type='ORTHO';data.ortho_scale=205;scene.camera=ob
scene.render.engine='CYCLES';scene.cycles.samples=24;scene.render.resolution_x=1600;scene.render.resolution_y=1050;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D': area.spaces.active.shading.type='MATERIAL';area.spaces.active.region_3d.view_perspective='CAMERA'
bpy.context.view_layer.update()
for ob in scene.objects: ob.select_set(ob.type=='MESH')
bpy.ops.export_scene.gltf(filepath=str(ROOT/'wolfos.glb'),export_format='GLB',use_selection=True,export_extras=True,export_apply=True,use_active_scene=True)
# [TIME] | [INFO] | Wolfos.Export | Blender's glTF exporter drops this multiply node's constant tint; retain the same explicit material factor as the game.
path=ROOT/'wolfos.glb';raw=path.read_bytes();json_size=struct.unpack_from('<I',raw,12)[0]
doc=json.loads(raw[20:20+json_size])
for material in doc['materials']:
    _,sp,role=material['name'].split()
    if role in ('fur','mane','muzzle'):
        rgb=palettes[int(sp)][MATERIALS.index(role)]
        material['pbrMetallicRoughness']['baseColorFactor']=[(min(1,v/255*1.5))**2.2 for v in rgb]+[1]
encoded=json.dumps(doc,separators=(',',':')).encode();encoded+=b' '*((-len(encoded))%4)
rest=raw[20+json_size:]
path.write_bytes(struct.pack('<III',0x46546c67,2,20+len(encoded)+len(rest))+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+rest)
# Alternate blinks stay in the editable scene, hidden for the studio render.
for ob in parts:
    if ob['arpg_blink']>0: ob.hide_render=True;ob.hide_set(True)
print('Authored',len(parts),'Wolfos pieces, including 34 native flex bridges')
