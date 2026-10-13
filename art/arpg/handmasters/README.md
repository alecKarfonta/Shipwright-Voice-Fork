# Original Wallmaster / Floormaster upgrade

Blender-authored replacements for the original animated hands. Both variants use the original 24-bone skeleton (25 animation entries), fourteen visible flex matrices and separately transformed fifth digit. Wallmaster keeps a dusty violet palette; Floormaster keeps an olive palette. The palm, knuckles, curved nails, folds and wrist anatomy share thirteen native-joint skin connections.

Native crawl, turn, finger tapping, hover, armored lunge, split, merge, ceiling warning/drop, recovery, stun and death remain driven by the original actors. The native floor-shadow warning and green armored-state fog are retained. No new creature AI or actor layout was introduced.

## Editable source and rebuild

- `handmasters.blend`: two editable weighted native rigs and all twelve original clips, 346 frames. Frame 1 is the authoring pose; clips begin at frame 2 and have timeline markers.
- `author_blender.py`: authored surfaces, original-joint bridges, boundary-normal reconciliation and material palettes. Run in an isolated Blender process.
- `extract_native_rig.py`: extract native rig and animation matrices from the local game archive.
- `export_runtime.py`: deterministic GLB-to-integer runtime compiler; 5,914 triangles per hand, eight material roles, no collapsed triangles.
- `package_blend.py`: editable rigs and studio preview. The separately drawn fifth digit retains its native scale of two in its mesh; scale is removed from preview bone transforms so it is not applied twice.
- `validate_animation.py`: all original frames, 149,472 bridge endpoint checks per variant. Maximum quantization error is 0.00866 world units; maximum per-axis envelope extension is 4.67 units at native large scale. Root callback offsets are shared by original and replacement geometry and are separately checked in-engine.
- `preview_motion.py`: six native motion stills.

Typical rebuild, from the repository root:

```sh
python3 art/arpg/handmasters/extract_native_rig.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/handmasters/author_blender.py
python3 art/arpg/handmasters/export_runtime.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/handmasters/package_blend.py
python3 art/arpg/handmasters/validate_animation.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/handmasters/preview_motion.py
```

## Runtime and practice

`HandmasterVisuals` replaces display lists inside each original actor's existing override callback. All root movement and post-draw fifth-digit transforms remain native. Skin bridges load only the fourteen real flex matrices; the extra digit never indexes that palette. Native lighting, fog and damage filters remain active. Feature switches and a 1,500-unit camera-distance fallback restore all original parts together. The invisible Floormaster uses the native Lens draw path.

Practice: **I → More → Settings & testing → Testing shortcuts → Upgraded handmasters**. Floormaster spawns its original complete three-actor family; Wallmaster uses its proximity ambush. Placement checks clear floor, footprint, sightline and headroom. The nearby six-hand cap counts hidden siblings. In the tested keyboard profile, **C** is sword, **X** is the action button, **Z** is targeting and **R** is shield.

## Verification and limits

See `tests/arpg/handmaster-verification-macos.json` for reproducible evidence, hashes, fixtures and limits. All 39 portable suites, geometry ASAN/UBSAN, and 99 native assertions pass. The native test observes 1,317 draw-end frames with live actors, 189 distinct finger poses, 50 charge frames, 143 small-hand frames, 81 Lens-state frames, 131 shadow-warning frames, 16 drop frames, and exactly five earned defeats.

The merge fixture explicitly absorbs siblings through the native wait-state transition; it is not a claim of an unassisted capture/merge encounter. Native grab/carry and respawn were unchanged but not harness-tested. Ordinary menu summons, targeting, green armor, motion, lunge knockback, sword/technique effects and journal pause were inspected; a full manual kill, full Lens fight, dungeon traversal and crowd performance remain open. An inherited nameplate in the isolated baseline can reveal a hidden Wallmaster warning. The broader replayability goal remains incomplete.

The isolated practice build is `/tmp/soh-handmaster-playtest/Handmaster Adventure.app`, left open and paused. The original `player1` save checksum is unchanged. The shared build directory was not rebuilt.
