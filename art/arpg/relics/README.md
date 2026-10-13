# Named gear atelier

Twenty original equipment identities: eight uniques and all twelve pieces of the Briarwatch, Cinderbound and Tidecaller sets. `designs.json` specifies each silhouette, palette and emblem. Existing saved unique/set IDs select these designs before socket or elemental powers; no save migration or RNG draw is needed.

- `relics.blend`: standalone editable source, isolated from the user's live Blender project.
- `relics.glb`: only the atelier's 35 semantic meshes, exported through the local Blender MCP.
- `author_blender.py`: reproducible Blender authoring script; creates only its own named scene. Run with the local MCP or in a disposable Blender session. It never saves the user's live file.
- `export_runtime.py`: validates and compiles the GLB into `RelicBlenderData.h`. Run with Python 3 after exporting mesh edits. It strips studio transforms and reverses glTF's axis conversion; dimensions and vertex normals stay in native model space.
- `package_blend.py`: import the GLB in factory-startup background Blender to produce the standalone project and review render.
- `icon-prompts.json`: exact built-in imagegen prompts, source paths, final paths, hashes and the one targeted cleanup edit. Each inventory illustration was generated separately, rather than rendered from a game mesh. Final PNGs live in `soh/assets/custom/textures/arpg/relics/`.

Mesh custom properties are `arpg_identity` (0–19) and `arpg_part`: 0 body/blade, 1 hilt, 2 empty scabbard, 3 fitted torso decoration. Do not apply the studio rotation or translation to mesh data. Export with `use_active_scene=True`, `use_selection=True` and `export_extras=True`. All intended parts, including hidden scabbards, must be selected. Other Blender scenes must not enter this GLB.

`RelicGeometry.h` composes the same parts for pickups and equipment. Sword base length is applied once in that shared adapter; foot attachments use one boot while pickups use a pair. Tunics currently recolor the native body and attach authored chest ornaments. Their pickup model is a complete garment; this pass does not replace the native body's skinning or sleeves.

Portable validation covers all twenty identities, socket stability, distinct painted resources, normal length, nondegenerate triangles, mounting combinations and base lengths. The opt-in native harness also checks resource loading, native equipment changes, rendering gates, palettes and pickup rendering for both ages. The harness requires an isolated `soh-arpg-qa-*` home and `SOH_ARPG_RELIC_QA=1`.

In the temporary testing adventure, open Settings & testing → Testing shortcuts → Relic wardrobe to equip a collection and place matching pickups ahead. Main item inspection keeps the overview short; Details offers expandable affixes, story, special powers and socket contents.
