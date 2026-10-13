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
SOURCE = ROOT / 'wolfos.glb'
DEST = ROOT.parents[2] / 'soh/soh/Enhancements/arpg/WolfosBlenderData.h'
MATERIALS = ['fur','mane','muzzle','skin','claw','metal','eye','mouth','pupil','highlight','silver']
LIMBS = {2,3,4,5,6,7,8,9,10,11,12,13,14,15,17,18,19,20,21}
POSE = {int(k):v for k,v in json.loads((ROOT/'native_pose.json').read_text()).items()}
LINKS = {int(k):v for k,v in json.loads((ROOT/'native_links.json').read_text()).items()}
def transform(m,p,normal=False):
    return tuple(sum(m[r*4+c]*p[c] for c in range(3))+(0 if normal else m[r*4+3]) for r in range(3))
def world(v):
    m=[x for row in POSE[v[6]][:3] for x in row]
    return transform(m,[x*.01 for x in v[:3]])

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
        count={'SCALAR':1,'VEC2':2,'VEC3':3}[a['type']];fmt='<'+code*count
        stride=v.get('byteStride',struct.calcsize(fmt));start=v.get('byteOffset',0)+a.get('byteOffset',0)
        return [struct.unpack_from(fmt,blob,start+i*stride) for i in range(a['count'])]
    vertices=[];lookup={};groups=defaultdict(list);collapsed=0
    for node in doc['nodes']:
        extra=node.get('extras',{})
        assert 'arpg_limb' in extra, 'Unrelated object in source export'
        sp,limb,rank,blink=[extra[k] for k in ('arpg_species','arpg_limb','arpg_rank','arpg_blink')]
        assert sp in (0,1) and limb in LIMBS and rank in (0,1,2) and blink in (-1,0,1,2)
        assert not node.get('children'), 'Export each piece directly in its native limb space'
        assert all(abs(s-1)<1e-5 for s in node.get('scale',[1,1,1])), 'Apply edits to mesh data, not object scale'
        bridge='arpg_parent_joint' in extra
        if bridge:
            assert limb in LINKS and extra['arpg_parent_joint']==LINKS[limb]['parent']
            assert all(abs(a-b)<1e-6 for a,b in zip(extra['arpg_parent_transform'],LINKS[limb]['transform']))
            assert rank==0 and blink==-1
        for prim in doc['meshes'][node['mesh']]['primitives']:
            assert prim.get('mode',4)==4
            mat_name=doc['materials'][prim['material']]['name'].split()[2].split('.')[0]
            mat=MATERIALS.index(mat_name)
            points=accessor(prim['attributes']['POSITION']);normals=accessor(prim['attributes']['NORMAL'])
            assert 'TEXCOORD_0' in prim['attributes'], 'Every surface needs authored UV coordinates'
            uvs=accessor(prim['attributes']['TEXCOORD_0'])
            assert len(points)==len(normals)==len(uvs)
            remap=[]
            for p,n,uv in zip(points,normals,uvs):
                p=(p[0],-p[2],p[1]);n=(n[0],-n[2],n[1])
                joint=limb
                if bridge:
                    if any(sum((a-b)**2 for a,b in zip(p,q))<1e-8 for q in extra['arpg_parent_ring_points']):
                        joint=extra['arpg_parent_joint']
                        p=transform(LINKS[limb]['transform'],p)
                        n=transform(LINKS[limb]['transform'],n,True)
                    else:
                        assert any(sum((a-b)**2 for a,b in zip(p,q))<1e-8 for q in extra['arpg_child_ring_points']), 'Bridge vertex has no native joint assignment'
                assert all(math.isfinite(v) for v in (*p,*n))
                length=math.sqrt(sum(v*v for v in n));assert length>.99
                assert all(math.isfinite(v) and abs(v)<31 for v in uv), 'UV exceeds signed native coordinate range'
                key=(*[round(v*100) for v in p],*[round(v/length*127) for v in n],joint,*[round(v*1024) for v in uv])
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
                if sum(x*x for x in cross)<1e-10:
                    collapsed+=1;continue  # Sub-pixel bevel tips cannot survive native integer quantization.
                groups[sp,limb,rank,blink,mat].extend(indices[i:i+3])
    assert collapsed<100, 'Unexpected topology loss during integer conversion'
    indices=[];parts=[];counts={}
    for key,values in sorted(groups.items()):
        parts.append((len(indices),len(values),*key));indices.extend(values)
    for sp in (0,1):
        for rank in (0,1,2):
            counts[f'{sp}:{rank}']=sum(len(v)//3 for (s,l,r,st,m),v in groups.items() if s==sp and r<=rank and st in (-1,0))
            assert counts[f'{sp}:{rank}']<40000
    lines=['#pragma once','#include <cstdint>',
           '// [TIME] | [INFO] | WolfosBlenderData.Export | Generated by art/arpg/wolfos/export_runtime.py from the editable Blender GLB.',
           'namespace ARPG::WolfosBlenderData {',
           'struct Vertex { int16_t x,y,z; int8_t nx,ny,nz; uint8_t joint; int16_t s,t; };',
           'struct Part { uint32_t first,count; uint8_t species,limb,rank; int8_t blink; uint8_t material; };',
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
            'uv':'Authored glTF UVs retained, 32-texel logical tile with 5 fractional bits; split UV seams retained',
            'space':'Native joint-local XYZ, positions x100; per-vertex joint index preserves 17 mixed-matrix connections; authoring-pose transforms omitted'}
    (ROOT/'export-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':
    compile_asset()
