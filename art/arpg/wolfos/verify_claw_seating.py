"""Measure exported claw-root seating against the paws through all native clips.

Run in background Blender for its BVH implementation. Positive signed distance
means a root sample is outside the nearest paw surface. This local surface test
supplements, rather than replaces, visual inspection of the claw/paw junction.
"""
import argparse
import json
from pathlib import Path
import sys
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0,str(Path(__file__).resolve().parent))
from native_asset import NativeWolfos
parser=argparse.ArgumentParser()
parser.add_argument('--pack',type=Path,required=True)
parser.add_argument('--previous',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
native=NativeWolfos()
assets={'previous':NativeWolfos(overlay=args.previous),'candidate':NativeWolfos(overlay=args.pack)}
report={}
for label,asset in assets.items():
    roots={}
    for joint in (15,21):
        points={tuple(v['position']) for t in asset.mesh('Normal',joint) for v in t['vertices'] if v['uv'][1]<=26}
        assert len(points)==26,(joint,len(points))
        roots[joint]=[Vector(p)*.01 for p in points]
    samples=0;outside=0;worst=-1e9;worst_pose=None
    for animation,frames in native.animations.items():
        for frame in range(len(frames)):
            matrices,_=native.pose(animation,frame,'Normal')
            matrices={j:Matrix(m) for j,m in matrices.items()}
            for paw,claw in ((14,15),(20,21)):
                points=[];faces=[]
                for t in asset.mesh('Normal',paw):
                    start=len(points)
                    points.extend(matrices[v['joint']]@(Vector(v['position'])*.01) for v in t['vertices'])
                    faces.append((start,start+1,start+2))
                bvh=BVHTree.FromPolygons(points,faces,all_triangles=True)
                for root in roots[claw]:
                    point=matrices[claw]@root
                    nearest,normal,_,distance=bvh.find_nearest(point)
                    signed=distance if normal.dot(point-nearest)>0 else -distance
                    samples+=1;outside+=signed>.03
                    if signed>worst:
                        worst=signed;worst_pose=dict(animation=animation,frame=frame,claw=claw)
    report[label]=dict(samples=samples,outside_samples=outside,maximum_signed_distance=worst,worst_pose=worst_pose)
report['status']='passed' if report['candidate']['outside_samples']==0 else 'needs_art_revision'
report['method']='Nearest oriented paw surface at 52 exported root vertices across all 125 native frames; tolerance 0.03 authoring units'
args.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
assert report['status']=='passed','Claw root samples leave the paw surface'
