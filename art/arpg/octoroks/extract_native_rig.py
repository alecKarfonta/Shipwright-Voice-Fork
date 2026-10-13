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
 r=R(z.read('objects/object_okuta/gOctorokSkel'));r.read('bbIIb');limbs=[r.s() for _ in range(r.read('I'))];rig=[]
 for i,n in enumerate(limbs):
  r=R(z.read(n));r.read('bb');r.s();r.read('H');assert r.read('I')==0;r.s();r.read('fffHHH');r.s();r.s();dl=r.s();r.s();pos=r.read('hhh');c,s=r.read('BB')
  vs=verts(dl) if dl else []
  rig.append(dict(limb=i+1,pos=pos,child=c,sib=s,dl=dl,vertices=vs,bounds=[[min(p[k] for p in vs),max(p[k] for p in vs)] for k in range(3)] if vs else None))
 def mm(a,b):return [[sum(a[i][k]*b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]
 def local(pos,rot):
  x,y,z=[a*math.pi/32768 for a in rot];cx,sx,cy,sy,cz,sz=math.cos(x),math.sin(x),math.cos(y),math.sin(y),math.cos(z),math.sin(z)
  return [[cz*cy,cz*sy*sx-sz*cx,cz*sy*cx+sz*sx,pos[0]/100],[sz*cy,sz*sy*sx+cz*cx,sz*sy*cx-cz*sx,pos[1]/100],[-sy,cy*sx,cy*cx,pos[2]/100],[0,0,0,1]]
 def scale_for(name,f,limb):
  if limb==5:
   if name=='gOctorokAppearAnim':
    if f<8:return (1,1,1)
    if f<10:return (1,(f-7)*.4+1,1)
    if f<14:return ((f-9)*.075+1,1.8-(f-9)*.25,(f-9)*.075+1)
    return (1.3-(f-13)*.05,(f-13)*.0333+.8,1.3-(f-13)*.05)
   if name=='gOctorokHideAnim':return (1,1 if f<3 else f-1 if f<4 else 2-(f-3)*.333,1)
   if name=='gOctorokShootAnim':
    if f<5:return (f*.125+1,)*3
    if f<7:return (1.5-(f-4)*.35,)*3
    if f<17:return ((f-6)*.05+.8,.8,(f-6)*.05+.8)
    return (1.3-(f-16)*.1,(f-16)*.0666+.8,1.3-(f-16)*.1)
   if name=='gOctorokFloatAnim':return (1,math.sin(math.pi*f/16)*.2+1,1)
  if limb==8:
   if name=='gOctorokFloatAnim':return (1,math.sin(math.pi*f/16)*.4+1,1)
   if name=='gOctorokShootAnim':
    if f<5:return (1,f*.25+1,f*.25+1)
    if f<7:return ((f-4)*.5+1,2-(f-4)*.5,2-(f-4)*.5)
    return (2-(f-6)*.0769,1,1)
   if name=='gOctorokDieAnim' and 25<=f<35:
    if f<27:return (1,(f-24)*.5+1,(f-24)*.5+1)
    if f<30:return ((f-26)*.333+1,2-(f-26)*.333,2-(f-26)*.333)
    return (2-(f-29)*.2,1,1)
  return (1,1,1)
 animations={};unscaled={};frame_labels=[]
 for asset in sorted(n for n in z.namelist() if n.startswith('objects/object_okuta/') and n.endswith('Anim')):
  name=asset.rsplit('/',1)[-1]
  r=R(z.read(asset));r.read('I');frames=r.read('h');values=[r.read('h') for _ in range(r.read('I'))];indices=[r.read('HHH') for _ in range(r.read('I'))];static=r.read('h')
  poses=[];originals=[]
  for frame in range(frames+(11 if name=='gOctorokDieAnim' else 0)):
   sample=min(frame,frames-1);joints=[tuple(values[j+(sample if j>=static else 0)] for j in idx) for idx in indices];matrices={};orig={}
   def visit(i,parent,oldparent):
    if i==255:return
    l=rig[i];p=list(joints[0] if i==0 else l['pos']);s=scale_for(name,frame,i+1)
    scale=[[s[x] if x==y and x<3 else int(x==y) for y in range(4)] for x in range(4)]
    m=local(p,joints[i+1]);world=mm(mm(parent,scale),m);old=mm(oldparent,m)
    matrices[i+1]=world;orig[i+1]=old;visit(l['child'],world,old);visit(l['sib'],parent,oldparent)
   identity=[[int(i==j) for j in range(4)] for i in range(4)];visit(0,identity,identity);poses.append(matrices);originals.append(orig)
  animations[name]=poses;unscaled[name]=originals
 order=['gOctorokFloatAnim','gOctorokShootAnim','gOctorokAppearAnim','gOctorokHideAnim','gOctorokHitAnim','gOctorokDieAnim']
 preview=[p for a in order for p in animations[a]]
 out={'frames':len(preview),'native_animation_frames':100,'death_hold_frames':11,'rig':rig,'poses':preview,'preview':preview,'animations':animations,'unscaled':unscaled,'labels':[(a,f) for a in order for f in range(len(animations[a]))]}
 (repo/'art/arpg/octoroks/native_rig.json').write_text(json.dumps(out,indent=2)+'\n')
 print('animations including held death frames',{key:len(value) for key,value in animations.items()})
 print('preview frames',len(preview))
 for l in rig:
  if l['dl']:print(l['limb'],l['pos'],'bounds',l['bounds'])
