"""Apply authored image files to an existing master without rebuilding its mesh.

Run in isolated background Blender with -- --master input.blend --output
revision.blend --manifest paints.json. The manifest maps full native texture
resource paths to PNG paths relative to the manifest. Output must be new.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import bpy

parser = argparse.ArgumentParser()
parser.add_argument('--master', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--manifest', type=Path, required=True)
parser.add_argument('--size', type=int, default=256)
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
assert bpy.app.background and not bpy.data.filepath
assert not args.output.exists(), 'Choose a new master revision; existing work is never overwritten'
assert 16 <= args.size <= 2048
manifest = json.loads(args.manifest.read_text())
assert manifest and isinstance(manifest, dict)
bpy.ops.wm.open_mainfile(filepath=str(args.master))
images = {image['native_path']: image for image in bpy.data.images
          if image.get('paintable_master') and image.get('native_path')}
assert set(manifest).issubset(images), 'Every supplied map must have a prepared paint image in the master'
report = {}
for native_path, filename in manifest.items():
    source = (args.manifest.parent / filename).resolve()
    assert source.is_file()
    paint = images[native_path]
    loaded = bpy.data.images.load(str(source), check_existing=False)
    width, height = loaded.size
    assert loaded.channels == 4 and width > 0 and height > 0, 'Paint maps must be RGBA'
    # [TIME] | [INFO] | Wolfos.PaintMaps | Resize for the native pack while retaining the full-resolution authored source.
    factor = args.size / max(width, height)
    loaded.scale(max(1, round(width*factor)), max(1, round(height*factor)))
    loaded['native_path'] = native_path
    loaded['paintable_master'] = True
    loaded['paint_source_sha256'] = hashlib.sha256(source.read_bytes()).hexdigest()
    loaded.name = 'Paint / ' + native_path.rsplit('/', 1)[-1]
    loaded.pack()
    users = 0
    for ob in bpy.context.scene.objects:
        if ob.get('source_kind') != 'editable master':
            continue
        for mat in ob.data.materials:
            for node in mat.node_tree.nodes:
                if node.type == 'TEX_IMAGE' and node.image == paint:
                    node.image = loaded
                    node.interpolation = 'Linear'
                    users += 1
    assert users, 'Unused paint resource'
    paint['paintable_master'] = False
    report[native_path] = dict(source=str(source), sha256=loaded['paint_source_sha256'],
                               size=list(loaded.size), replaced_nodes=users)
bpy.ops.file.pack_all()
args.output.parent.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(args.output), compress=True)
args.output.with_suffix('.paint.json').write_text(json.dumps(dict(master=str(args.master),
    output=str(args.output), maps=report), indent=2)+'\n')
print(json.dumps(report, indent=2))
