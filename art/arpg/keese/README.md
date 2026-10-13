# Native Keese upgrade

Original replacement art for the existing normal, fire and ice Keese. Authored
through the local Blender MCP in the separate **ARPG Keese Workshop** scene.
The user's open Blender document is not overwritten.

The tapered chest, face, pointed ears, fangs, claws and thin wing membranes use
the original ten visible limb spaces. Internal wing boundaries follow the native
hinges; only exposed silhouette edges are scalloped. Fire uses charred colors
and ember seams; ice uses pale fur, blue membranes and small frost ridges.

- `keese.blend`: standalone editable source with all eight native flight poses.
- `keese.glb`: source export with semantic materials and `arpg_variant` /
  `arpg_limb` metadata. Object transforms are for the studio pose only.
- `author_blender.py`: regenerates only the Keese workshop scene. For MCP safe
  mode, pass the native rig as inline JSON and use literal export paths.
- `extract_native_rig.py`: reads the local extracted game archive to recover
  joint transforms and reference bounds; no ROM is included.
- `export_runtime.py`: compiles the GLB into checked integer vertices, normals
  and indexed triangles in `KeeseBlenderData.h`.
- `validate_animation.py`: verifies exported hinge positions and actor bounds
  through all eight native poses.
- `package_blend.py`: imports the GLB and restores its animation preview in a
  separate `Blender --background --factory-startup` process.

The game still uses EnFirefly's original skeleton, flight, perch, dive, collision,
stun, freeze, death, flame and frost code. The renderer selects the live element,
so ignition and extinguishing change the model immediately. Lens visibility is
handled before replacement, and the native extra eye list is suppressed only
when an upgraded head was actually drawn. Disabling ARPG/remastered enemies or
moving beyond the existing 1,500-unit detail distance restores the native model.

In a temporary testing adventure, open **More → Arsenal & enemy counters →
Practice encounters → Upgraded Keese**, choose a variant, and summon it ahead.
No new enemy species or flight controller is introduced by this pass.
