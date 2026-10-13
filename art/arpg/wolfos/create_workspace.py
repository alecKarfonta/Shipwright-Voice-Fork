"""Build an animated, textured native reference and editable Wolfos master.

blender --background --factory-startup --python-exit-code 1 --python THIS_FILE

Outputs go to workspace/, separate from the procedural GLB/runtime assets. An
existing master is never overwritten. Use -- --output /new/directory for another
import. Original game assets are read only from the local user's oot.o2r.
"""
import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from native_asset import NativeWolfos

parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, default=ROOT / 'workspace')
parser.add_argument('--archive', type=Path)
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
assert bpy.app.background and not bpy.data.filepath, 'Use an isolated factory-startup Blender process'
args.output.mkdir(parents=True, exist_ok=True)
master = args.output / 'wolfos-authoring.blend'
assert not master.exists(), f'{master} already exists; open it to edit, or choose a new --output'
native = NativeWolfos(args.archive)
bind_error = native.validate_bind()
AXIS = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
scene = bpy.context.scene
scene.name = 'Wolfos animation workshop'
for ob in list(scene.objects):
    bpy.data.objects.remove(ob, do_unlink=True)
scene.render.fps = 20

def collection(name):
    value = bpy.data.collections.new(name)
    scene.collection.children.link(value)
    return value

def link_object(name, data, group):
    ob = bpy.data.objects.new(name, data)
    group.objects.link(ob)
    return ob

def bone_name(joint):
    return f'limb_{joint:02}'

def rig(name, species, group, x):
    data = bpy.data.armatures.new(name)
    ob = link_object(name, data, group)
    ob.location.x = x
    ob.show_in_front = True
    ob['blink'] = 0
    ob.id_properties_ui('blink').update(min=0, max=3, description='Native eye index: open, half, narrow, half')
    ob['source_species'] = species
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    world, local = native.pose('gWolfosWaitingAnim', 0, species)
    for limb in native.rigs[species]:
        bone = data.edit_bones.new(bone_name(limb['joint']))
        bone.length = 3
        bone.matrix = AXIS @ Matrix(world[limb['joint']])
        bone['native_joint'] = limb['joint']
    for limb in native.rigs[species]:
        if limb['parent']:
            data.edit_bones[bone_name(limb['joint'])].parent = data.edit_bones[bone_name(limb['parent'])]
    bpy.ops.object.mode_set(mode='OBJECT')
    for joint, matrix in world.items():
        actual = data.bones[bone_name(joint)].matrix_local
        expected = AXIS @ Matrix(matrix)
        error = max(abs(actual[r][c]-expected[r][c]) for r in range(4) for c in range(4))
        assert error < .001, (name, joint, 'rest matrix', error, str(actual), str(expected))
    ob.select_set(False)
    return ob

images = {}
def native_image(name):
    if name not in images:
        data = native.texture(name)
        image = bpy.data.images.new(name.rsplit('/', 1)[-1], width=data['width'], height=data['height'], alpha=True)
        # [TIME] | [INFO] | Wolfos.Workspace | N64 image rows start at the top; Blender pixel rows start at the bottom.
        pixels = [component for y in reversed(range(data['height'])) for x in range(data['width'])
                  for component in data['rgba'][y*data['width']+x]]
        image.pixels.foreach_set(pixels)
        image.pack()
        images[name] = image
    return images[name]

def driver(socket, armature, expression):
    curve = socket.driver_add('default_value')
    variable = curve.driver.variables.new()
    variable.name = 'blink'
    variable.targets[0].id = armature
    variable.targets[0].data_path = '["blink"]'
    curve.driver.expression = expression

def material(texture, tile, armature):
    mat = bpy.data.materials.new(armature.name + ' / ' + texture.rsplit('/', 1)[-1] + ' / ' + str(tile))
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    shader = next(n for n in nodes if n.type == 'BSDF_PRINCIPLED')
    shader.inputs['Roughness'].default_value = .85
    uv = nodes.new('ShaderNodeTexCoord')
    split = nodes.new('ShaderNodeSeparateXYZ'); links.new(uv.outputs['UV'], split.inputs[0])
    combine = nodes.new('ShaderNodeCombineXYZ')
    for axis, wrap in enumerate(tile[:2]):
        source = split.outputs[axis]
        if wrap == 1:
            ping = nodes.new('ShaderNodeMath'); ping.operation = 'PINGPONG'; ping.inputs[1].default_value = 1
            links.new(source, ping.inputs[0]); source = ping.outputs[0]
        elif wrap == 2:
            clamp = nodes.new('ShaderNodeClamp'); links.new(source, clamp.inputs[0]); source = clamp.outputs[0]
        links.new(source, combine.inputs[axis])
    states = ('Open', 'Half', 'Narrow') if 'EyeOpenTex' in texture else ('Open',)
    samplers = []
    for state in states:
        tex = nodes.new('ShaderNodeTexImage'); tex.image = native_image(texture.replace('EyeOpenTex', 'Eye'+state+'Tex'))
        tex.interpolation = 'Closest'; tex.extension = 'REPEAT'
        links.new(combine.outputs[0], tex.inputs['Vector']); samplers.append(tex)
    color, alpha = samplers[0].outputs['Color'], samplers[0].outputs['Alpha']
    if len(samplers) == 3:
        for index, expression in ((1, 'blink == 1 or blink == 3'), (2, 'blink == 2')):
            mix = nodes.new('ShaderNodeMixRGB'); links.new(color, mix.inputs[1]); links.new(samplers[index].outputs['Color'], mix.inputs[2])
            driver(mix.inputs[0], armature, expression); color = mix.outputs[0]
            blend = nodes.new('ShaderNodeMixRGB'); links.new(alpha, blend.inputs[1]); links.new(samplers[index].outputs['Alpha'], blend.inputs[2])
            driver(blend.inputs[0], armature, expression); alpha = blend.outputs[0]
        links.new(color, shader.inputs['Emission Color']); shader.inputs['Emission Strength'].default_value = .5
        links.new(alpha, shader.inputs['Alpha'])
    links.new(color, shader.inputs['Base Color'])
    return mat

def native_mesh(species, armature, group, name):
    world, _ = native.pose('gWolfosWaitingAnim', 0, species)
    vertices, faces, normals, uvs, joints, owners, material_keys, face_materials = [], [], [], [], [], [], [], []
    lookup = {}
    for limb in native.rigs[species]:
        for triangle in native.mesh(species, limb['joint']):
            key = (triangle['texture'], triangle['tile'])
            if key not in material_keys:
                material_keys.append(key)
            face_materials.append(material_keys.index(key)); owners.append(limb['joint'])
            face = []
            texture = native.texture(triangle['texture'])
            for v in triangle['vertices']:
                key = (v['joint'], *v['position'])
                matrix = AXIS @ Matrix(world[v['joint']])
                if key not in lookup:
                    lookup[key] = len(vertices)
                    vertices.append(matrix @ (Vector(v['position']) * .01)); joints.append(v['joint'])
                face.append(lookup[key])
                uvs.append((v['uv'][0]/(32*texture['width']), 1-v['uv'][1]/(32*texture['height'])))
                normals.append((matrix.to_3x3() @ Vector(v['normal'])).normalized())
            faces.append(face)
    data = bpy.data.meshes.new(name); data.from_pydata(vertices, [], faces); data.update()
    uv = data.uv_layers.new(name='Native UV')
    for loop, value in zip(uv.data, uvs): loop.uv = value
    data.normals_split_custom_set(normals)
    owner_attribute = data.attributes.new('native_limb', 'INT', 'FACE')
    for value, owner in zip(owner_attribute.data, owners): value.value = owner
    ob = link_object(name, data, group); ob.location = armature.location
    for key in material_keys: data.materials.append(material(*key, armature))
    for polygon, index in zip(data.polygons, face_materials):
        polygon.material_index = index; polygon.use_smooth = True
    for limb in native.rigs[species]:
        vertex_group = ob.vertex_groups.new(name=bone_name(limb['joint']))
        indices = [i for i, joint in enumerate(joints) if joint == limb['joint']]
        if indices: vertex_group.add(indices, 1, 'REPLACE')
    modifier = ob.modifiers.new('Original Wolfos skeleton', 'ARMATURE'); modifier.object = armature
    ob['source_species'] = species; ob['source_kind'] = 'native reference'
    return ob, vertices, joints

order = ['gWolfosWaitingAnim', 'gWolfosRunningAnim', 'gWolfosSidesteppingAnim', 'gWolfosSlashingAnim',
         'gWolfosBlockingAnim', 'gWolfosBackflippingAnim', 'gWolfosDamagedAnim', 'gWolfosRearingUpFallingOverAnim']
schedule, cursor = [], 1
for animation in order:
    frames = len(native.animations[animation])
    schedule.append(dict(name=animation, start=cursor, end=cursor+frames-1, animation=animation))
    cursor += frames
schedule.append(dict(name='Eye states — held for inspection', start=cursor, end=cursor+19, animation='gWolfosWaitingAnim', diagnostic='blink'))
cursor += 20
schedule.append(dict(name='Head turn attachment check', start=cursor, end=cursor+19, animation='gWolfosWaitingAnim', diagnostic='head'))
scene.frame_start, scene.frame_end = 1, cursor+19
for clip in schedule: scene.timeline_markers.new(clip['name'], frame=clip['start'])

def frame_pose(clip, frame, species):
    index = frame-clip['start']
    diagnostic = clip.get('diagnostic')
    head = round(math.sin(index/19*math.tau)*0x1800) if diagnostic == 'head' else 0
    blink = (index//5) % 4 if diagnostic == 'blink' else (index-12 if clip['animation'] == 'gWolfosWaitingAnim' and 13 <= index <= 15 else 0)
    return native.pose(clip['animation'], 0 if diagnostic else index, species, head), blink

def animate(armature, species):
    bind_world, bind_local = native.pose('gWolfosWaitingAnim', 0, species)
    for clip in schedule:
        for frame in range(clip['start'], clip['end']+1):
            (world, local), blink = frame_pose(clip, frame, species)
            for limb in native.rigs[species]:
                joint = limb['joint']; bone = armature.pose.bones[bone_name(joint)]
                bone.rotation_mode = 'QUATERNION'
                bone.matrix_basis = Matrix(bind_local[joint]).inverted() @ Matrix(local[joint])
                for channel in ('location', 'rotation_quaternion', 'scale'): bone.keyframe_insert(channel, frame=frame)
            armature['blink'] = blink; armature.keyframe_insert('["blink"]', frame=frame)
    action = armature.animation_data.action; action.name = armature.name + ' / native animation reel'; action.use_fake_user = True
    # [TIME] | [INFO] | Wolfos.Workspace | Constant keys reproduce native integer frames without quaternion curve overshoot.
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for point in curve.keyframe_points: point.interpolation = 'CONSTANT'

reference_group = collection('01 Original reference — locked')
reference_rig = rig('Original Brown Wolfos', 'Normal', reference_group, -42)
reference, bind_vertices, vertex_joints = native_mesh('Normal', reference_rig, reference_group, 'Original Brown Wolfos mesh')
animate(reference_rig, 'Normal')
for ob in reference_group.objects: ob.hide_select = True

working_group = collection('02 Editable master — original anatomy')
working_rig = rig('Editable Brown Wolfos', 'Normal', working_group, 42)
working, _, _ = native_mesh('Normal', working_rig, working_group, 'EDIT THIS Wolfos mesh')
working['source_kind'] = 'editable master'; animate(working_rig, 'Normal')

white_group = collection('03 White Wolfos reference — hidden')
white_rig = rig('Original White Wolfos', 'White', white_group, 0)
white_mesh, _, _ = native_mesh('White', white_rig, white_group, 'Original White Wolfos mesh')
animate(white_rig, 'White')
white_group.hide_render = True; white_group.hide_viewport = True

# [TIME] | [INFO] | Wolfos.Workspace | Validate actual Blender deformation against independently decoded native matrices.
maximum_matrix_error = maximum_vertex_error = 0
bind_world, _ = native.pose('gWolfosWaitingAnim', 0)
local_vertices = [Matrix(bind_world[joint]).inverted() @ AXIS.inverted() @ position
                  for position, joint in zip(bind_vertices, vertex_joints)]
for clip in schedule:
    for frame in range(clip['start'], clip['end']+1):
        scene.frame_set(frame)
        (world, _), blink = frame_pose(clip, frame, 'Normal')
        for joint, matrix in world.items():
            actual = reference_rig.pose.bones[bone_name(joint)].matrix
            expected = AXIS @ Matrix(matrix)
            maximum_matrix_error = max(maximum_matrix_error, max(abs(actual[r][c]-expected[r][c]) for r in range(4) for c in range(4)))
        evaluated = reference.evaluated_get(bpy.context.evaluated_depsgraph_get()); data = evaluated.to_mesh()
        for vertex, local, joint in zip(data.vertices, local_vertices, vertex_joints):
            expected = AXIS @ Matrix(world[joint]) @ local
            maximum_vertex_error = max(maximum_vertex_error, (vertex.co-expected).length)
        evaluated.to_mesh_clear()
        assert reference_rig['blink'] == blink
assert maximum_matrix_error < .001 and maximum_vertex_error < .002, (maximum_matrix_error, maximum_vertex_error)

studio = collection('04 Preview studio')
world = scene.world; world.use_nodes = True
background = next(n for n in world.node_tree.nodes if n.type == 'BACKGROUND')
background.inputs[0].default_value = (.09, .11, .14, 1); background.inputs[1].default_value = .5
for name, position, power, size in [('Key', (-75, -95, 135), 300000, 75), ('Fill', (100, -40, 75), 150000, 65), ('Rim', (0, 100, 130), 350000, 70)]:
    light = bpy.data.lights.new(name, 'AREA'); light.energy = power; light.size = size
    ob = link_object(name, light, studio); ob.location = position
    ob.rotation_euler = (Vector((0, -5, 30))-ob.location).to_track_quat('-Z', 'Y').to_euler()
camera = link_object('Review camera', bpy.data.cameras.new('Review camera'), studio)
camera.location = (95, -235, 100)
camera.rotation_euler = (Vector((0, -5, 30))-camera.location).to_track_quat('-Z', 'Y').to_euler()
camera.data.type = 'ORTHO'; camera.data.ortho_scale = 175; scene.camera = camera
scene.render.engine = 'CYCLES'; scene.cycles.samples = 24
scene.render.resolution_x = 1440; scene.render.resolution_y = 960; scene.render.resolution_percentage = 100
scene.view_settings.view_transform = 'Standard'
scene.render.image_settings.file_format = 'PNG'
scene.frame_set(1)
for ob in scene.objects: ob.select_set(False)
working.select_set(True); bpy.context.view_layer.objects.active = working
for area in bpy.context.screen.areas:
    if area.type == 'VIEW_3D':
        area.spaces.active.shading.type = 'MATERIAL'; area.spaces.active.region_3d.view_perspective = 'CAMERA'
text = bpy.data.texts.new('START HERE')
text.write('''Wolfos authoring workspace\n\nLeft: locked original reference. Right: independent editable master, initially identical.\nCollection 03 contains the white species with its own original textures.\nTimeline markers identify all eight native clips, held eye states, and a head-turn check.\nSpace plays the 20 fps reel. The rig custom property "blink" selects the eye state.\n\nEdit the right-hand mesh and save this .blend. Keep the original reference intact.\nThis file is the authoring master; running the old procedural builder does not update it.\nThe importer refuses to overwrite it. Runtime export from this armature is not implemented yet.\nNative lighting/combiners and game motion/AI are not reproduced by the studio.\nAnimation clips are raw native poses; the game can vary their speed, blend them, and move the actor.\n''')
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(master), compress=True)
report = dict(blender=bpy.app.version_string, master=str(master),
              animations={name: len(frames) for name, frames in native.animations.items()}, timeline=schedule,
              native_triangles={s: sum(len(native.mesh(s, limb['joint'])) for limb in native.rigs[s]) for s in native.rigs},
              native_textures=len(native.texture_cache), bind_matrix_error=bind_error,
              evaluated_frames=scene.frame_end, maximum_blender_matrix_error=maximum_matrix_error,
              maximum_deformed_vertex_error=maximum_vertex_error, sources=native.hashes,
              limitations=['Studio shading approximates native materials', 'Raw clips omit AI movement and animation blending',
                           'Armature-to-runtime export pending', 'Editable master starts as unmodified original anatomy'])
(args.output / 'import-report.json').write_text(json.dumps(report, indent=2)+'\n')
scene.render.filepath = str(args.output / 'native-workspace.png')
bpy.ops.render.render(write_still=True)
print(json.dumps({k: v for k, v in report.items() if k != 'sources'}, indent=2))
