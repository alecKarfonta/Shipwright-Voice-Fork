# Native Octorok upgrade

Original Blender meshes replace the existing Octorok's fifteen visible limbs.
The tapered mantle carries surface-fitted pigment patches, inset golden eyes,
sculpted brows and orbital folds. A hollow, fleshy snout expands and recoils with
the native attack. Four flattened swimming arms each use three original moving
segments, with overlapping joints and low suction rims. The replacement contains
9,138 triangles; distant actors retain the lightweight original geometry.

The original 37-joint skeleton, procedural head/snout squash, water attachment,
surface/float/shoot/hide timing, collisions, frozen death, ordinary death,
projectiles and shield-reflection rules stay authoritative. No AI timings or
hitboxes change. Projectile actors share the creature actor ID but are explicitly
excluded from the limb replacement. Scene lighting, fog and damage flashes still
apply; the modeled pigment and eyes use their authored colors.

- `octoroks.blend`: standalone editable source with all 100 original animation
  frames and eleven held death frames, including the native squash transforms.
- `octoroks.glb`: semantic materials and joint-local geometry with limb metadata.
- `author_blender.py`: rebuilds only the `ARPG Octorok Workshop` scene. The initial
  authoring ran through the local Blender MCP. After its viewport renderer
  crashed, refinements and previews used isolated background Blender processes
  and CPU Cycles; the unrelated live project was never overwritten.
- `extract_native_rig.py`: extracts the user's local native rig/animations and
  reproduces the procedural head/nozzle scales for the studio preview. Requires
  `build-cmake/soh/oot.o2r`; no ROM is included.
- `export_runtime.py`: compiles the GLB to `OctorokBlenderData.h` with integer
  geometry checks. Studio object matrices are not baked into limb vertices.
- `validate_animation.py`: checks the quantized mesh through 111 poses and 888
  native arm anchors. The maximum anchor displacement is zero; surface extent
  allows 3.2 world units beyond the original animated envelope for thickness.
- `package_blend.py`: packages the standalone source and studio render in a
  separate `--background --factory-startup` Blender process.
- `preview_motion.py`: renders six poses from the packaged source with CPU Cycles.

Temporary adventures expose **More → Arsenal & enemy counters → Practice
encounters → Upgraded Octoroks**. Face deep water from shore and summon. The
placement checks the water surface, underlying floor, line of sight and actor
caps: four within 700 units and sixteen scene-wide. Native inhabitants far
away do not consume the nearby practice limit. Stay at medium distance and use the original shield-reflection counter.

The native integration harness uses a real water box and shore in Zora's River.
It checks all limb/fallback paths, native motion, close-range hiding, damage,
freezing, deaths, one-time rewards, projectile exclusion and native shield
reflection branches. Collision flags are injected for the shield-branch checks;
this is not a claim that a complete manual reflected-shot kill was playtested.
