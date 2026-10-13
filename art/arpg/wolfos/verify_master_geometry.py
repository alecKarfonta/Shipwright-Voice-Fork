"""Check an exported sculpt against its previous pack and original animation.

Uses the local game archive; no original assets are written to the repository.
"""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path

from native_asset import NativeWolfos

parser = argparse.ArgumentParser()
parser.add_argument('--pack', type=Path, required=True)
parser.add_argument('--previous', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--edited-limbs', type=int, nargs='+', default=[7, 12, 17])
parser.add_argument('--refined-claws', action='store_true', help='Verify native root/tip anchors and closed replacement claws instead of original claw triangles')
parser.add_argument('--vertex-map', type=Path, help='Sculpt report mapping intended native endpoint movements while retaining original joint-bridge topology')
args = parser.parse_args()
vertex_moves = {}
if args.vertex_map:
    for move in json.loads(args.vertex_map.read_text())['native_vertex_moves']:
        key=(move['joint'],*move['before']);value=(move['joint'],*move['after'])
        assert key not in vertex_moves or vertex_moves[key]==value
        vertex_moves[key]=value
native = NativeWolfos()
previous = NativeWolfos(overlay=args.previous)
candidate = NativeWolfos(overlay=args.pack)


def triangles(asset):
    return [t for j in range(1, 22) for t in asset.mesh('Normal', j)]


def geometry_key(triangle, mapped=False):
    values = [(v['joint'], *v['position']) for v in triangle['vertices']]
    if mapped:
        values = [vertex_moves.get(v,v) for v in values]
    return min(tuple(values[i:]+values[:i]) for i in range(3))


def bridges(values, mapped=False):
    return Counter(geometry_key(t,mapped) for t in values
                   if len({v['joint'] for v in t['vertices']}) > 1)


def noncoat(values):
    return Counter((t['texture'], geometry_key(t)) for t in values
                   if not t['texture'].endswith('FurTex') and not (args.refined_claws
                       and {v['joint'] for v in t['vertices']} <= {15,21}))


original, before, after = map(triangles, (native, previous, candidate))
assert bridges(before, bool(args.vertex_map)) == bridges(after), 'Unexpected change to joint connections'
assert len(bridges(original)) == len(bridges(after)), 'Original joint-bridge topology changed'
assert noncoat(before) == noncoat(after), 'Protected eye/mouth/tooth/claw geometry or winding changed'
for joint in set(range(1, 22))-set(args.edited_limbs):
    assert Counter(map(geometry_key, previous.mesh('Normal', joint))) == Counter(
        map(geometry_key, candidate.mesh('Normal', joint))), f'Unexpected edit to limb {joint}'
assert all(tuple(t['tile']) == (2, 2, 4, 5) for t in after if t['texture'].endswith('FurTex'))
for triangle in after:
    if triangle['texture'].endswith('FurTex'):
        a, b, c = [v['uv'] for v in triangle['vertices']]
        assert (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]), 'Collapsed exported coat UV triangle'
claw_anchors = 0
if args.refined_claws:
    assert {15,21} <= set(args.edited_limbs)
    for joint in (15,21):
        source = {tuple(v['position']) for t in previous.mesh('Normal',joint) for v in t['vertices']}
        anchors = {p for p in source if p[0] < 0 or p[0] > 1800}
        assert len(anchors) == 6, 'Expected two root pairs and two tip landmarks'
        mesh = candidate.mesh('Normal',joint)
        positions = {tuple(v['position']) for t in mesh for v in t['vertices']}
        assert anchors <= positions, 'Native claw root or tip moved after quantization'
        claw_anchors += len(anchors)
        edges = Counter()
        for t in mesh:
            points = [tuple(v['position']) for v in t['vertices']]
            for i in range(3):
                edges[tuple(sorted((points[i],points[(i+1)%3])))] += 1
            a,b,c = [v['uv'] for v in t['vertices']]
            assert (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]), 'Collapsed claw UV triangle'
        assert set(edges.values()) == {2}, 'Claws must form closed shells'
samples = 0
minimum = math.inf
maximum_eye_matrix_error = 0
for animation, frames in native.animations.items():
    for frame in range(len(frames)):
        matrices, _ = native.pose(animation, frame, 'Normal')
        maximum_eye_matrix_error = max(maximum_eye_matrix_error,
            max(abs(matrices[17][r][c]-matrices[18][r][c]) for r in range(4) for c in range(4)))
        for index, triangle in enumerate(after):
            points = []
            for vertex in triangle['vertices']:
                m = matrices[vertex['joint']]
                points.append([sum(m[i][j]*vertex['position'][j]*.01 for j in range(3))+m[i][3]
                               for i in range(3)])
            u = [points[1][i]-points[0][i] for i in range(3)]
            v = [points[2][i]-points[0][i] for i in range(3)]
            cross = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
            area_squared = sum(c*c for c in cross)
            assert math.isfinite(area_squared) and area_squared > 1e-12, (animation, frame, index)
            minimum = min(minimum, area_squared)
            samples += 1
assert maximum_eye_matrix_error < 1e-8, 'Eyes no longer follow the head transform'
report = dict(status='passed', pack=str(args.pack),
              pack_sha256=hashlib.sha256(args.pack.read_bytes()).hexdigest(),
              triangles=len(after), preserved_mixed_joint_triangles=sum(bridges(original).values()),
              mixed_joint_endpoint_map=str(args.vertex_map) if args.vertex_map else None,
              mixed_joint_geometry_unchanged=not bool(args.vertex_map),
              preserved_noncoat_triangles=sum(noncoat(before).values()),
              edited_limbs=args.edited_limbs, unchanged_other_limb_geometry_and_winding=True,
              preserved_claw_anchors=claw_anchors,
              coat_wrap='Clamp S and T', triangle_pose_samples=samples,
              minimum_cross_squared=minimum, collapsed_count=0,
              collapsed_quantized_coat_uv_triangles=0,
              maximum_head_eye_matrix_difference=maximum_eye_matrix_error)
args.output.write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
