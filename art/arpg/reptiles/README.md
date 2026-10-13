# Blender reptile assets

Original Lizalfos and Dinolfos replacement geometry, authored and reviewed in the
local Blender 5.1.2 MCP workshop. These replace the earlier overlapping rounded
primitives with shaped anatomical surfaces, angular brow/frill silhouettes,
beveled cuirasses and greaves, fitted scales, and ridge-forged weapons.

- `reptiles.blend`: standalone editable Blender document with studio lighting.
  `package_blend.py` packages it in a separate background Blender process.
- `reptiles.glb`: editable Blender export, with native limb/species/rank/weapon
  metadata on every mesh object. It contains only the ARPG workshop objects.
- `build_reptiles.py`: reproducible authoring source. Run in Blender's Text Editor
  with the script's `__file__` pointing to this file, or with Blender's `--python`.
  It rebuilds only the named ARPG workshop scene and exports the GLB.
- `native_pose.json`: native joint matrices for the authoring pose, in units of
  100 native model units. This is a pose reference, not replacement animation.
- `export_runtime.py`: standalone Python compiler for the game header. Run
  `python3 art/arpg/reptiles/export_runtime.py` from the repository root after
  exporting edits from Blender.
- `export-report.json`: geometry totals and the exact exported source hash.
- `reptiles-preview.png`: Blender studio render, not an in-game screenshot.

Edit geometry in mesh space, retaining object transforms and `arpg_*` metadata.
The game takes the local positions and corner normals; the displayed authoring
pose and side-by-side offsets are not included. `arpg_rank` is the minimum rank
(0 ordinary, 1 champion, 2 elite). `arpg_state` is 0 always visible, 1 drawn sword,
or 2 stored sword. Export only the active workshop scene, with custom properties
and applied modifiers. Material names use `ARPG <species> <semantic-name>`.

The runtime uses the native skeleton, animation, collision, lighting, fog, and
damage colors. Blender's metallic/roughness preview is not a PBR game shader;
the game uses the corresponding semantic colors and exported surface normals.
The exporter deduplicates vertices, rejects unrelated objects and unsupported
metadata, and removes only triangles that collapse after native integer rounding.
The native combat fixture and portable geometry suite cover both species, three
ranks, drawn/stored weapons, fades, and all 20 visible limbs.

MCP safe mode remains enabled. For live MCP authoring, pass the script text with
the pose JSON inlined instead of using filesystem reads or `exec` in Blender.
The connected user's other scenes and saved Blender project are preserved.
