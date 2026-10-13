"""Exercise native asset export using baseline, edited and invalid Blender files."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

REPO = Path(__file__).resolve().parents[2]
ART = REPO/'art/arpg/wolfos'
sys.path.insert(0, str(ART))
from native_asset import NativeWolfos

parser = argparse.ArgumentParser()
parser.add_argument('--blender', default='/Applications/Blender.app/Contents/MacOS/Blender')
parser.add_argument('--master', type=Path, default=ART/'workspace/wolfos-authoring.blend')
args = parser.parse_args()
output = Path(tempfile.mkdtemp(prefix='soh-wolfos-master-export-test-'))
master_hash = hashlib.sha256(args.master.read_bytes()).hexdigest()
checks = []

def blender(script, arguments=(), success=True):
    command = [args.blender, '--background', '--factory-startup', '--python-exit-code', '1', '--python', str(script)]
    if arguments: command += ['--', *map(str, arguments)]
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (output/(script.stem+'-'+str(len(list(output.glob('*.log'))))+'.log')).write_text(result.stdout)
    assert (result.returncode == 0) == success, result.stdout[-2500:]
    return result.stdout

def triangle_key(triangle):
    vertices = [tuple([v['joint'], *v['position'], *v['uv']]) for v in triangle['vertices']]
    return triangle['texture'], triangle['tile'], min(tuple(vertices[i:]+vertices[:i]) for i in range(3))

baseline = output/'baseline.o2r'
blender(ART/'export_master.py', ['--master', args.master, '--output', baseline])
native, exported = NativeWolfos(), NativeWolfos(overlay=baseline)
for limb in native.rigs['Normal']:
    expected = native.mesh('Normal', limb['joint']); actual = exported.mesh('Normal', limb['joint'])
    assert Counter(map(triangle_key, expected)) == Counter(map(triangle_key, actual))
checks.append('All 327 native triangles preserve positions, joint assignments, UVs, materials and winding')
for path in native.texture_cache:
    expected, actual = native.texture(path), exported.texture(path)
    assert expected['width'] == actual['width'] and expected['height'] == actual['height']
    assert max(abs(a-b) for p, q in zip(expected['rgba'], actual['rgba']) for a, b in zip(p, q)) <= 1/255+1e-6
checks.append('All eight paint maps survive within RGBA8 quantization error')

fixture = output/'make_edits.py'
fixture.write_text('''import bpy, json
from pathlib import Path
bpy.ops.wm.open_mainfile(filepath=MASTER)
ob=next(o for o in bpy.context.scene.objects if o.get('source_kind')=='editable master')
reference=next(o for o in bpy.context.scene.objects if o.get('source_kind')=='native reference' and o.get('source_species')=='Normal')
reference_images={n.image for mat in reference.data.materials for n in mat.node_tree.nodes if n.type=='TEX_IMAGE'}
paint=next(n.image for mat in ob.data.materials for n in mat.node_tree.nodes if n.type=='TEX_IMAGE' and n.image.get('native_path','').endswith('NormalFurTex'))
assert paint not in reference_images
original_pixels={image.name:list(image.pixels) for image in reference_images}
vertex=next(v for v in ob.data.vertices if any(ob.vertex_groups[g.group].name=='limb_17' for g in v.groups))
vertex.co.x+=.75
face=next(p for p in ob.data.polygons if ob.data.attributes['native_limb'].data[p.index].value==17)
ob.data.uv_layers.active.data[face.loop_start].uv.x+=.125
width,height=paint.size;paint.scale(width*2,height*2)
pixels=list(paint.pixels);pixels[0:4]=[1,0,1,1];paint.pixels[:]=pixels;paint.update();paint.pack()
assert all(list(image.pixels)==original_pixels[image.name] for image in reference_images)
bpy.ops.wm.save_as_mainfile(filepath=EDITED,compress=True)
ob.vertex_groups['limb_18'].add([vertex.index],.5,'REPLACE')
bpy.ops.wm.save_as_mainfile(filepath=INVALID,compress=True)
'''.replace('MASTER', repr(str(args.master))).replace('EDITED', repr(str(output/'edited.blend'))).replace('INVALID', repr(str(output/'invalid.blend'))))
blender(fixture)
checks.append('Working paint is independent of locked reference images')
edited = output/'edited.o2r'
blender(ART/'export_master.py', ['--master', output/'edited.blend', '--output', edited])
changed = NativeWolfos(overlay=edited)
differences = []
for limb in native.rigs['Normal']:
    before = Counter(map(triangle_key, exported.mesh('Normal', limb['joint'])))
    after = Counter(map(triangle_key, changed.mesh('Normal', limb['joint'])))
    if before != after: differences.append(limb['joint'])
assert differences == [17], differences
checks.append('A saved head-vertex and UV edit reaches only the expected exported limb')
fur = 'objects/object_wf/gWolfosNormalFurTex'
before, after = exported.texture(fur), changed.texture(fur)
assert after['width'] == before['width']*2 and after['height'] == before['height']*2
assert after['rgba'][-after['width']] == [1, 0, 1, 1]
checks.append('A saved paint edit and doubled texture resolution reach the native high-resolution resource')
log = blender(ART/'export_master.py', ['--master', output/'invalid.blend', '--output', output/'invalid.o2r'], success=False)
assert 'one bone per vertex' in log and not (output/'invalid.o2r').exists()
checks.append('Unsupported blended weights fail before an asset pack is written')
assert hashlib.sha256(args.master.read_bytes()).hexdigest() == master_hash
checks.append('All fixture edits leave the actual authoring master unchanged')
report = dict(status='passed', checks=checks, master_sha256=master_hash, output=str(output),
              baseline_sha256=hashlib.sha256(baseline.read_bytes()).hexdigest(), edited_pack=str(edited))
(ART/'workspace'/'pipeline-test-results.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
