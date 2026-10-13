"""Compile the Blender GLB into checked, indexed native limb data.

No Blender or external Python packages are needed for this step. Object transforms
are the authoring pose only: vertex positions are exported in each native joint's
local space, with the Blender/glTF axis conversion undone.
"""
import hashlib
import json
import math
import struct
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / 'poe.glb'
DEST = ROOT.parents[2] / 'soh/soh/Enhancements/arpg/PoeBlenderData.h'
MATERIALS = ['cloth','fold','recess','seam','skin','knuckle','bronze','edge','eye']
LIMBS = {4,5,9,10,11,16,18,19,20}

RIG=json.loads((ROOT/'native_rig.json').read_text())
REST=RIG['animations']['gPoeFloatAnim'][0]
def tx(m,p,normal=False):return tuple(sum(m[i][j]*p[j] for j in range(3))+(0 if normal else m[i][3]) for i in range(3))
def parent_space(child,parent,p,normal=False):
 w=tx(REST[str(child)],p,normal);m=REST[str(parent)]
 if not normal:w=[w[i]-m[i][3] for i in range(3)]
 return tuple(sum(m[i][j]*w[i] for i in range(3)) for j in range(3))
def world(v):return tx(REST[str(v[6])],[x/100 for x in v[:3]])
def compile_asset():
    raw=SOURCE.read_bytes()
    assert struct.unpack_from('<III',raw)==(0x46546c67,2,len(raw)), 'Expected glTF 2 GLB'
    chunks={};offset=12
    while offset<len(raw):
        size,kind=struct.unpack_from('<II',raw,offset);offset+=8
        chunks[kind]=raw[offset:offset+size];offset+=size
    doc=json.loads(chunks[0x4e4f534a]);blob=chunks[0x004e4942]
    def accessor(index):
        a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
        assert not a.get('sparse') and v['buffer']==0
        code={5126:'f',5125:'I',5123:'H',5121:'B'}[a['componentType']]
        count={'SCALAR':1,'VEC3':3}[a['type']];fmt='<'+code*count
        stride=v.get('byteStride',struct.calcsize(fmt));start=v.get('byteOffset',0)+a.get('byteOffset',0)
        return [struct.unpack_from(fmt,blob,start+i*stride) for i in range(a['count'])]
    vertices=[];lookup={};groups=defaultdict(list);collapsed=0
    for node in doc['nodes']:
        extra=node.get('extras',{})
        assert 'arpg_limb' in extra, 'Unrelated object in source export'
        sp,limb=[extra[k] for k in ('arpg_variant','arpg_limb')]
        assert sp in (0,) and limb in LIMBS
        assert not node.get('children'), 'Export each piece directly in its native limb space'
        # [TIME] | [INFO] | Poe.Asset | Native squash may be present in the preview transform; joint-local geometry is unscaled.
        for prim in doc['meshes'][node['mesh']]['primitives']:
            assert prim.get('mode',4)==4
            mat_name=doc['materials'][prim['material']]['name'].split()[2].split('.')[0]
            mat=MATERIALS.index(mat_name)
            points=accessor(prim['attributes']['POSITION']);normals=accessor(prim['attributes']['NORMAL'])
            remap=[]
            for p,n in zip(points,normals):
                p=(p[0],-p[2],p[1]);n=(n[0],-n[2],n[1]);joint=limb
                if 'arpg_parent_joint' in extra:
                    if any(math.dist(p,q)<.0001 for q in extra['arpg_parent_ring_points']):
                        joint=extra['arpg_parent_joint'];p=parent_space(limb,joint,p);n=parent_space(limb,joint,n,True)
                    else:assert any(math.dist(p,q)<.0001 for q in extra['arpg_child_ring_points']), 'Missing bridge assignment'
                assert all(math.isfinite(v) for v in (*p,*n))
                length=math.sqrt(sum(v*v for v in n));assert length>.99
                key=(*[round(v*100) for v in p],*[round(v/length*127) for v in n],joint)
                assert all(abs(v)<32000 for v in key[:3])
                if key not in lookup:
                    lookup[key]=len(vertices);vertices.append(key)
                remap.append(lookup[key])
            indices=[remap[i[0]] for i in accessor(prim['indices'])]
            assert len(indices)%3==0
            for i in range(0,len(indices),3):
                a,b,c=[world(vertices[j]) for j in indices[i:i+3]]
                ab=[b[j]-a[j] for j in range(3)];ac=[c[j]-a[j] for j in range(3)]
                cross=[ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0]]
                if sum(v*v for v in cross)<1e-12:
                    collapsed+=1;continue  # Sub-pixel bevel tips cannot survive native integer quantization.
                groups[sp,limb,mat].extend(indices[i:i+3])
    assert collapsed<100, f'Unexpected topology loss during integer conversion: {collapsed}'
    indices=[];parts=[];counts={}
    for key,values in sorted(groups.items()):
        parts.append((len(indices),len(values),*key));indices.extend(values)
    for sp in (0,):
        assert {l for (s,l,m) in groups if s==sp}==LIMBS
        counts[str(sp)]=sum(len(v)//3 for (s,l,m),v in groups.items() if s==sp)
        assert 500<counts[str(sp)]<16000, 'Bound per-actor native rendering cost'
    lines=['#pragma once','#include <cstdint>',
           '// [TIME] | [INFO] | PoeBlenderData.Export | Generated by art/arpg/poes/export_runtime.py from the editable Blender GLB.',
           'namespace ARPG::PoeBlenderData {',
           'struct Vertex { int16_t x,y,z; int8_t nx,ny,nz; uint8_t joint; };',
           'struct Part { uint32_t first,count; uint8_t variant,limb,material; };',
           'inline constexpr Vertex vertices[]={']
    lines.extend('{'+','.join(map(str,v))+'},' for v in vertices);lines.append('};')
    lines.append('inline constexpr uint32_t indices[]={')
    lines.extend(','.join(map(str,indices[i:i+24]))+',' for i in range(0,len(indices),24));lines.append('};')
    lines.append('inline constexpr Part parts[]={')
    lines.extend('{'+','.join(map(str,p))+'},' for p in parts);lines.extend(['};','}'])
    generated='\n'.join(lines)+'\n'
    if not DEST.exists() or DEST.read_text()!=generated:
        DEST.write_text(generated)
    report={'source_sha256':hashlib.sha256(raw).hexdigest(),'vertices':len(vertices),'triangles':len(indices)//3,'parts':len(parts),
            'subpixel_triangles_removed':collapsed,'triangles_per_actor':counts,'materials':MATERIALS,
            'space':'Native joint-local XYZ, positions x100; native normals retained; per-vertex native joint indices; authoring-pose transforms omitted'}
    (ROOT/'export-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':
    compile_asset()
