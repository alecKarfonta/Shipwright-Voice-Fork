"""Give editable master textures independent image data and native export metadata.

Run in a separate background Blender process with -- --master path.blend.
Already prepared images are reused; this migration does not reset mesh edits.
"""
import argparse
import sys
from pathlib import Path
import bpy

parser = argparse.ArgumentParser()
parser.add_argument('--master', type=Path, default=Path(__file__).parent/'workspace/wolfos-authoring.blend')
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
assert bpy.app.background and not bpy.data.filepath
bpy.ops.wm.open_mainfile(filepath=str(args.master))
paint_images = {}
for ob in bpy.context.scene.objects:
    if ob.get('source_kind') != 'editable master':
        continue
    for mat in ob.data.materials:
        textures = [n for n in mat.node_tree.nodes if n.type == 'TEX_IMAGE' and n.image]
        for node in textures:
            source = node.image
            native_path = source.get('native_path', 'objects/object_wf/' + source.name.split('.')[0])
            assert native_path.rsplit('/', 1)[-1].startswith('gWolfos')
            if source.get('paintable_master'):
                paint = source
            elif native_path in paint_images:
                paint = paint_images[native_path]
            else:
                paint = source.copy(); paint.name = 'Paint / ' + source.name
                paint['paintable_master'] = True
                paint['native_path'] = native_path
                paint.pack()
            paint_images[native_path] = paint
            node.image = paint
        assert textures
        mat['native_texture'] = textures[0].image['native_path']
        # [TIME] | [INFO] | Wolfos.Master | Existing workshop materials encode the tile tuple in their final name field.
        if 'native_tile' not in mat:
            import ast
            mat['native_tile'] = list(ast.literal_eval(mat.name.rsplit(' / ', 1)[-1]))
    ob['export_schema'] = 'native-wolfos-pack-v1'
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(args.master), compress=True)
print('Prepared independently paintable images:', len(paint_images))
