"""Render isolated posed paw/claw assemblies from a saved master, without edits."""
import argparse
from pathlib import Path
import sys
import bpy
import bmesh
from mathutils import Vector

parser = argparse.ArgumentParser()
parser.add_argument('--master', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
assert bpy.app.background and not bpy.data.filepath and not args.output.exists()
args.output.mkdir(parents=True)
bpy.ops.wm.open_mainfile(filepath=str(args.master))
bpy.data.collections['01 Original reference — locked'].hide_render = True
scene = bpy.context.scene; original = bpy.data.objects['EDIT THIS Wolfos mesh']
original.hide_render = True
camera = scene.camera
scene.render.resolution_x = 960; scene.render.resolution_y = 720
scene.cycles.samples = 16
for joints, name in (({14,15},'right'),({20,21},'left'),({5},'hind')):
    ob = original.copy(); ob.data = original.data.copy(); scene.collection.objects.link(ob)
    ob.hide_render = False
    bm = bmesh.new(); bm.from_mesh(ob.data); owner = bm.faces.layers.int['native_limb']
    bmesh.ops.delete(bm,geom=[f for f in bm.faces if f[owner] not in joints],context='FACES')
    bm.to_mesh(ob.data); bm.free()
    for frame, label in ((1,'waiting'),(60,'slash'),(78,'block')):
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        evaluated = ob.evaluated_get(bpy.context.evaluated_depsgraph_get()); mesh = evaluated.to_mesh()
        points = [ob.matrix_world @ v.co for v in mesh.vertices]
        evaluated.to_mesh_clear()
        low = Vector([min(p[i] for p in points) for i in range(3)])
        high = Vector([max(p[i] for p in points) for i in range(3)])
        target = (low+high)/2
        camera.location = target+Vector((70,-135,70))
        camera.rotation_euler = (target-camera.location).to_track_quat('-Z','Y').to_euler()
        camera.data.ortho_scale = max((high-low).length*1.45,25)
        scene.render.filepath = str(args.output/(name+'-'+label+'.png'))
        bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(ob,do_unlink=True)
