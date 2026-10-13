# Original creatures

Cairn Sentinel and Lantern Wraith are original meshes authored in the live local
Blender MCP. `author_blender.py` recreates only the **ARPG Original Creatures**
scene; other scenes and the open project's filepath are preserved.

`creatures.glb` is the editable source export. Its 98 objects carry `arpg_part`
and `arpg_material` metadata. Object transforms arrange the studio; mesh-local
coordinates retain native Y-up model space. Modify mesh data rather than object
scale when revising the runtime asset.

Run `python3 art/arpg/creatures/export_runtime.py` to regenerate
`CreatureBlenderData.h` and `export-report.json`. The compiler validates every
component and triangle, quantizes native normals, and ignores studio transforms.
There are 2,492 triangles across 16 articulated components, including the bone
dart. Runtime rendering animates the split shell, legs, arms, tails and core.

Sentinel: plated four-legged crawler with an exposed crystal mechanism. Hammer
and explosions open its shell; ice interrupts its directional ground attack.
Wraith: hollow moth-mask spirit with a hanging open lantern and pointed cloth.
Ranged weapons exploit its core; utility stuns interrupt the spectral volley.

Wraiths also tend wounded melee allies. The raised-arm pose and braided green
lantern tether telegraph a 44-tick healing channel. A hit or accepted stun,
occluding terrain, separation, or the ally's defeat cancels it. Each Wraith
spends one of two charges when beginning a channel; each recipient accepts at
most two heals across all casters. Completed channels restore 20% of maximum
health, capped at six native health points. Wraiths cannot heal one another.
The tether is native procedural geometry, animated alongside the Blender mesh.
The temporary adventure's Practice encounters submenu includes a wounded
Sentinel with a Wraith escort for testing this interaction.
