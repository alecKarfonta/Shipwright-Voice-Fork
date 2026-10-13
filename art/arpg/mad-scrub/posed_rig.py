"""Reconstruct the native pre-limb snout callback, including its matrix order."""
import copy,math

def mm(a,b):return [[sum(a[i][k]*b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]
def inverse(m):
 out=[[m[j][i] if i<3 and j<3 else int(i==j) for j in range(4)] for i in range(4)]
 for i in range(3):out[i][3]=-sum(out[i][j]*m[j][3] for j in range(3))
 return out
def posed(rig):
 clips=copy.deepcopy(rig['animations']);parents={}
 def visit(i,parent):
  if i==255:return
  l=rig['rig'][i];parents[i+1]=parent;visit(l['child'],i+1);visit(l['sib'],parent)
 visit(0,0)
 for name,clip in clips.items():
  for frame,pose in enumerate(clip):
   pose['0']=[[int(i==j) for j in range(4)] for i in range(4)]
   if name=='gDekuNutsSpitAnim' and frame<=10:
    sx,sy=(1-frame*.0833,1+frame*.1167) if frame<=6 else (.5+frame-6,1.7-(frame-6)*.7) if frame<=7 else (1.5-(frame-7)*.1667,1)
    scale=[[sx,0,0,0],[0,sy,0,0],[0,0,sy,0],[0,0,0,1]]
    parent=pose[str(parents[7])];pose['7']=mm(mm(mm(parent,scale),inverse(parent)),pose['7'])
 return clips
