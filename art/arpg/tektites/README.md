# Native Tektite upgrade

Original replacement art for the existing red and blue Tektites, authored in the
local Blender MCP's separate **ARPG Tektite Workshop** scene. The user's open
Blender document is preserved.

A tapered, layered shell surrounds a large single eye. Shaped upper legs meet
slender lower legs at the original knee pivots. Split claws straddle the native
foot anchors used for dust and ripples. Blue Tektites have pale claws, a cooler
shell and small webbed foot plates. The two variants use 5,612 / 5,692 triangles.

- `tektites.blend`: standalone editable meshes and a 27-frame crouch/jump/landing
  preview on the native skeleton.
- `tektites.glb`: source export with semantic material, species and limb metadata.
  Object transforms describe the studio pose; runtime vertices are joint-local.
- `author_blender.py`: recreates only the named workshop scene. For MCP safe mode,
  inline the extracted rig JSON and use a literal output path.
- `extract_native_rig.py`: reads the local extracted game archive, including all
  eight animations and original mesh bounds. No ROM is included.
- `export_runtime.py`: quantizes the GLB into `TektiteBlenderData.h`.
- `validate_animation.py`: checks all 89 native animation frames, four knee
  connections, actor bounds and exported claw proximity to the foot anchors.
- `package_blend.py`: saves and renders the editable source in an independent
  factory-startup Blender process.

The game uses EnTite's native skeleton, crouch, leap, landing, damage, flip,
collision and break-apart death. Permanent display-list storage also supports
native detached body parts. ARPG and remastered-enemy toggles restore native art;
the existing 1,500-unit camera distance also selects the lightweight native mesh.

With ARPG enabled, blue Tektite upright recovery restores gravity during its
jump and recognizes water as a landing surface. This fixes the documented native
zero-gravity recovery bug, after which normal idle flotation resumes. It is
independent of the visual toggle and leaves ARPG-off behavior unchanged.

In a temporary adventure, open **More → Arsenal & enemy counters → Practice
encounters → Upgraded Tektites**, choose a species and summon it ahead.
Experimental creature controls remain in their own collapsed section.
