# Original Moblin upgrade

Spear guards and the large club fighter now have separately fitted boar faces, recessed eyes, curved tusks, articulated hands and feet, layered armor, leather wraps and rebuilt weapons. The spear has a leaf-shaped forged head and a wrapped wooden shaft; the club has a faceted timber head, iron bands and rivets. Meadow patrols share the spear mesh at their original larger scale.

Both models use their original skeleton and movement. The native look-around, walk, wind-up, charge, braking, lane-aimed club slam, kneeling, recovery and death animations remain intact. Twelve connections per model bridge the original native matrices through the waist, neck, elbows, wrists, hips, knees and ankles. Club and spear skeletons have different visible chest joints and use separate matrix maps.

## Editable assets

- `moblins.blend`: both editable models, two weighted native rigs and all 23 original clips (526 frames total).
- `moblins.glb`: authored joint-local geometry used by the runtime compiler.
- `author_blender.py`, `export_runtime.py`, `package_blend.py`: reproducible authoring, checked integer export and rig packaging.
- `animation-validation.json`: every native frame, 151,488 transformed connection endpoints, and native silhouette comparisons.
- `moblins-studio.png` and six motion stills: authoring previews.
- `moblin-spear-practice.png` and `moblin-club-practice.png`: ordinary adventure screenshots.

From the repository root, with the local game archive available:

```sh
python3 art/arpg/moblins/extract_native_rig.py spear
python3 art/arpg/moblins/extract_native_rig.py club
blender --background --factory-startup --python art/arpg/moblins/author_blender.py
python3 art/arpg/moblins/export_runtime.py
blender --background --factory-startup --python art/arpg/moblins/package_blend.py
python3 art/arpg/moblins/validate_animation.py
blender --background --factory-startup --python art/arpg/moblins/preview_motion.py
```

Extraction reads `build-cmake/soh/oot.o2r` for the original rigs, clips and reference bounds. Original reference triangles are not used in replacement meshes. Run authoring and packaging in isolated background Blender processes; the editable output does not modify a live Blender project.

## Runtime and testing

The cached display lists replace fifteen visible limbs per model. Native lighting, fog and damage filters shade ten authored material colors. Original hitboxes, front protection, spear attack quad, hand-driven club quad, foot shadows, shockwaves and ice attachments stay controlled by the original actor. Feature gates and a 1,500-unit distance fallback restore original geometry. The meshes contain 11,624 spear triangles and 13,092 club triangles.

Lethal ice now enters the same one-time reward transition as ordinary damage. Repeated death setup cannot duplicate a reward. Removing a Moblin releases a player it owns, while leaving another actor's ownership untouched.

Open **More → Settings & testing → Testing shortcuts → Upgraded Moblins**. Choose **Spear guard** or **Club fighter** on broad level ground. Placement checks the floor, footprint, headroom, sightline and nearby enemies, with a four-Moblin limit within 2,000 units. Patrols require scene-authored paths, so they are upgraded in their original locations instead of offered as free-placement actors.

The spear guard deflects frontal hits: sidestep its charge and circle behind. The club fighter commits to a lane before slamming; use its recovery opening. Original combat rules and timing are retained.

## Verification limits

See `tests/arpg/moblin-verification-macos.json` for source hashes, isolated build details and final results. Geometry passed all 38 portable suites and AddressSanitizer/UndefinedBehaviorSanitizer. Animation validation checks quantization and posed envelopes; it does not establish watertight topology or collision coverage for every armor detail.

The final native run passed 79 checks with 1,446 draw frames, 78 ice-attachment frames, three patrol waypoints and four one-time native/ARPG defeats.

The native harness uses a separate debug save, a controlled player/camera and selected native collision effects. It checks all three actor types, ordinary and ice deaths, cleanup, original Meadow path movement and practice placement. The separate ordinary adventure build was used for both menu summons, targeting, pursuit, slam effects, player knockback and journal reopening. Full manual kills, every patrol route, varied terrain and crowded-scene performance remain separate acceptance work.
