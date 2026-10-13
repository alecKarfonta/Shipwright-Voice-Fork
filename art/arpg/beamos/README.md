# Original Beamos upgrade

The original Beamos now has a carved, tapered pedestal, a metal bearing, two separate fitted shutters, radial seams, rivets and an inset ember lens. The four visible meshes remain attached to the original rigid skeleton. Native scanning, head pitch, warning blinks, laser tracking, eye-stun recovery and flying-head death use the original actor code.

## Source and art references

- `beamos.blend`: editable meshes and a weighted rig with the original eight-frame eyelid clip.
- `beamos.glb`: authored source geometry, with native joint assignments in object metadata.
- `author_blender.py`, `export_runtime.py`, `package_blend.py`: reproducible Blender authoring, integer geometry export and editable scene packaging.
- `extract_native_rig.py`: original Beamos skeleton, geometry and animation extraction from the local game archive.
- `preview_motion.py`: original untextured geometry reference and authored open/closed shutter previews.
- `validate_animation.py`: all eight native clip frames plus 256 callback-driven pitch/yaw poses. The maximum per-axis envelope extension is 0.52 world units for the small variant and 0.73 for the large variant. This is an envelope check, not a claim of watertight surfaces or collision proof.

References are the original Beamos body, shutters and eye silhouette; the existing Armos upgrade's carved stone; native dungeon stone and Hookshot metal textures. Runtime stone uses `gUnusedStoneTex` (RGBA16, 32×32) and metal uses `gLinkAdultHookshotMetalTex` (RGBA16, 8×8), verified directly in the local archive. Only the small inset lens is emissive. Native scene lighting, fog and damage filters remain active on the other materials.

The model has 7,412 triangles, 4,760 indexed vertices, 18 limb/material batches and seven explicit material slots. The adapter records semantic surfaces separately from RGB for compatibility with the isolated build's older material API. There is no color-based material inference. Geometry is cached once per visible joint and shared by both sizes.

## Rebuild

From the repository root:

```sh
python3 art/arpg/beamos/extract_native_rig.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/beamos/author_blender.py
python3 art/arpg/beamos/export_runtime.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/beamos/package_blend.py
python3 art/arpg/beamos/validate_animation.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/beamos/preview_motion.py
```

## Runtime

`BeamosVisuals` runs after the original head rotations and death visibility rules. The original post-limb callback still places the beam origin, charge flash, terrain impact, attack quad and eye quad. The native laser display list is unchanged. Destruction now also calls the native cleanup functions for both quad colliders.

Both ARPG feature gates and the 1,500-unit distance fallback restore the original meshes. Small and large practice variants preserve the native health, scale and collision dimensions. Placement checks floor, footprint, headroom and line of sight, and caps nearby Beamos at four. Practice parameters include a 400-unit scan range; omitting the high-byte range would silently prevent detection.

Practice path: **I → More → Settings & testing → Testing shortcuts → Upgraded Beamos**. Shoot the eye to interrupt the laser. Bombs break the armored body: one for the small variant, two for the large variant.

Verification is recorded in `tests/arpg/beamos-verification-macos.json`. The ordinary app uses a separate practice profile; the player's normal save is preserved. The broader replayability goal remains incomplete.

## Verified result

All 41 portable suites, the geometry ASAN/UBSAN run, 14 semantic material adapters and 50 native assertions pass. The animation sweep checks 1,218,560 authored vertex transforms. The native run includes a real arrow eye-stun, three real bombs, two earned defeats, 64 flying-head observations, terrain-clipped lasers and a darker dungeon encounter. Four-actor capped frame cadence stayed near 50 ms for native and upgraded models; this does not establish uncapped rendering performance.

Ordinary practice verified both size choices, targeting, native player damage and a small Beamos bomb defeat. Journal XP rose from 31,160 to 31,188. A fresh large encounter is open and paused at `/tmp/soh-beamos-playtest/Beamos Adventure.app`. The ordinary large defeat, broader dungeon coverage and uncapped performance remain open.
