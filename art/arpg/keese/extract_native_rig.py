import zipfile,struct,re,json
from pathlib import Path
repo=Path(__file__).resolve().parents[3]
txt=(repo/'libultraship/src/ship/utils/StrHash64.cpp').read_text().split('CRC64_Table[256] = {')[1].split('};')[0]
table=[int(x,16) for x in re.findall(r'0x([0-9a-fA-F]+)',txt)]
def crc(s):
 c=(1<<64)-1
 for b in s.encode(): c=(table[(c>>56)^b]^(c<<8))&((1<<64)-1)
 return c
class R:
 def __init__(self,d):self.d=d;self.o=64
 def read(self,f):
  v=struct.unpack_from('<'+f,self.d,self.o);self.o+=struct.calcsize('<'+f);return v[0] if len(v)==1 else v
 def s(self):
  n=self.read('I');v=self.d[self.o:self.o+n].decode();self.o+=n;return v

import math
with zipfile.ZipFile(repo/'build-cmake/soh/oot.o2r') as z:
 names={crc(n):n for n in z.namelist()}
 def verts(dl):
  d=z.read(dl);o=72;v=[]
  while o+8<=len(d):
   a,b=struct.unpack_from('<II',d,o);o+=8;op=a>>24
   if op in (0x20,0x31,0x32,0x33,0x34,0x35,0x36,0x37):
    h,l=struct.unpack_from('<II',d,o);o+=8
    if op==0x32:
     vd=z.read(names[(h<<32)|l]);v.extend(struct.unpack_from('<hhh',vd,72+b+i*16) for i in range((a>>12)&255))
   if op==0xdf:break
  return v
 r=R(z.read('objects/object_firefly/gKeeseSkeleton'));r.read('bbIIb');limbs=[r.s() for _ in range(r.read('I'))];rig=[]
 for i,n in enumerate(limbs):
  r=R(z.read(n));r.read('bb');r.s();r.read('H');assert r.read('I')==0;r.s();r.read('fffHHH');r.s();r.s();dl=r.s();r.s();pos=r.read('hhh');c,s=r.read('BB')
  vs=verts(dl) if dl else []
  rig.append(dict(limb=i+1,pos=pos,child=c,sib=s,dl=dl,vertices=vs,bounds=[[min(p[k] for p in vs),max(p[k] for p in vs)] for k in range(3)] if vs else None))
 r=R(z.read('objects/object_firefly/gKeeseFlyAnim'));r.read('I');frames=r.read('h');values=[r.read('h') for _ in range(r.read('I'))];indices=[r.read('HHH') for _ in range(r.read('I'))];static=r.read('h')
 def mm(a,b):return [[sum(a[i][k]*b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]
 def local(pos,rot):
  x,y,z=[a*math.pi/32768 for a in rot];cx,sx,cy,sy,cz,sz=math.cos(x),math.sin(x),math.cos(y),math.sin(y),math.cos(z),math.sin(z)
  return [[cz*cy,cz*sy*sx-sz*cx,cz*sy*cx+sz*sx,pos[0]/100],[sz*cy,sz*sy*sx+cz*cx,sz*sy*cx-cz*sx,pos[1]/100],[-sy,cy*sx,cy*cx,pos[2]/100],[0,0,0,1]]
 poses=[]
 for frame in range(frames):
  joints=[tuple(values[j+(frame if j>=static else 0)] for j in idx) for idx in indices];matrices={}
  def visit(i,parent):
   if i==255:return
   l=rig[i];p=list(joints[0] if i==0 else l['pos'])
   if i==0:p[1]+=2300
   world=mm(parent,local(p,joints[i+1]));matrices[i+1]=world;visit(l['child'],world);visit(l['sib'],parent)
  visit(0,[[int(i==j) for j in range(4)] for i in range(4)]);poses.append(matrices)
 out={'frames':frames,'rig':rig,'poses':poses}
 (repo/'art/arpg/keese/native_rig.json').write_text(json.dumps(out,indent=2)+'\n')
 print('frames',frames)
 for l in rig:
  if l['dl']:print(l['limb'],l['pos'],'bounds',l['bounds'])
