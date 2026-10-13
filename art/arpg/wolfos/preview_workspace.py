"""Render the saved authoring master without changing it.

Pass -- --master path.blend --output directory. The reel uses native integer
poses at 20 fps; held blink and head-turn sections are explicitly diagnostic.
"""
import argparse
import json
import sys
from pathlib import Path
import bpy
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

parser = argparse.ArgumentParser()
parser.add_argument('--master', type=Path, default=Path(__file__).parent / 'workspace/wolfos-authoring.blend')
parser.add_argument('--output', type=Path, default=Path(__file__).parent / 'workspace/preview')
parser.add_argument('--closeups-only', action='store_true', help='Render the four held eye states without repeating the full animation reel')
parser.add_argument('--keyposes-only', action='store_true', help='Render representative full-body poses and eye states without the full reel')
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
assert bpy.app.background and not bpy.data.filepath
assert not (args.output / 'frames').exists(), 'Choose a fresh preview output directory'
(args.output / 'frames').mkdir(parents=True)
bpy.ops.wm.open_mainfile(filepath=str(args.master))
scene = bpy.context.scene
bpy.data.collections['01 Original reference — locked'].hide_render = True
working = bpy.data.objects['EDIT THIS Wolfos mesh']
camera = scene.camera
target = Vector((42, -3, 33))
camera.location = target + Vector((75, -170, 68))
camera.rotation_euler = (target-camera.location).to_track_quat('-Z', 'Y').to_euler()
camera.data.ortho_scale = 160
camera.data.clip_end = 2000
scene.render.resolution_x = 960; scene.render.resolution_y = 720
scene.cycles.samples = 8
scene.frame_set(1)
bpy.context.view_layer.update()
points = []
for frame in range(scene.frame_start, scene.frame_end+1):
    scene.frame_set(frame)
    evaluated = working.evaluated_get(bpy.context.evaluated_depsgraph_get())
    data = evaluated.to_mesh()
    points.extend(working.matrix_world @ vertex.co for vertex in data.vertices)
    evaluated.to_mesh_clear()
projected = [world_to_camera_view(scene, camera, p) for p in points]
min_x, max_x = min(p.x for p in projected), max(p.x for p in projected)
min_y, max_y = min(p.y for p in projected), max(p.y for p in projected)
rotation = camera.matrix_world.to_3x3()
camera.location += rotation @ Vector((((min_x+max_x)/2-.5)*camera.data.ortho_scale,
                                     ((min_y+max_y)/2-.5)*camera.data.ortho_scale*.75, 0))
camera.data.ortho_scale *= max((max_x-min_x)/.86, (max_y-min_y)/.86)
bpy.context.view_layer.update()
projected = [world_to_camera_view(scene, camera, p) for p in points]
assert all(.03 < p.x < .97 and .03 < p.y < .97 and p.z > 0 for p in projected), 'Animation leaves the review camera'
scene.frame_set(1)
scene.render.image_settings.file_format = 'PNG'
scene.render.filepath = str(args.output / 'frames') + '/'
keyposes = []
if args.keyposes_only:
    for frame, label in ((1, 'waiting'), (33, 'running'), (42, 'sidestep'), (60, 'slash'),
                         (78, 'block'), (87, 'backflip'), (98, 'damaged'), (113, 'rearing')):
        scene.frame_set(frame)
        scene.render.filepath = str(args.output / ('pose-' + label + '.png'))
        bpy.ops.render.render(write_still=True)
        keyposes.append(dict(frame=frame, label=label))
elif not args.closeups_only:
    bpy.ops.render.render(animation=True)

# [TIME] | [INFO] | Wolfos.WorkspacePreview | Close-ups exercise the driven eye materials on the saved rig.
scene.cycles.samples = 24
scene.render.resolution_x = 960; scene.render.resolution_y = 720
closeups = []
for frame, label in ((126, 'open'), (131, 'half'), (136, 'narrow'), (141, 'half-return')):
    scene.frame_set(frame)
    armature = bpy.data.objects['Editable Brown Wolfos']
    head = armature.matrix_world @ armature.pose.bones['limb_17'].matrix.translation
    target = head + Vector((0, -8, -2))
    camera.location = target + Vector((42, -75, 20))
    camera.rotation_euler = (target-camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.ortho_scale = 50
    scene.render.filepath = str(args.output / ('eye-' + label + '.png'))
    bpy.ops.render.render(write_still=True)
    closeups.append(dict(frame=frame, label=label, blink=armature['blink']))
(args.output / 'preview-report.json').write_text(json.dumps(dict(frames=scene.frame_end, fps=scene.render.fps,
    all_vertices_inside_camera=True, closeups=closeups, master=str(args.master),
    keyposes=keyposes, full_reel_rendered=not (args.closeups_only or args.keyposes_only)), indent=2)+'\n')
