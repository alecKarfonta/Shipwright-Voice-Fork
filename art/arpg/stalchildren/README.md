# Original Stalchild upgrade

The original Stalchild now has Blender-authored open ribs and pelvic anatomy, separate skull and mandible, recessed ember eyes, vertebrae, paired lower-arm and shin bones, and individual fingers and toes. It retains the recognizable oversized skull and hunched native proportions. The regular, large and giant variants all use the original size progression.

The original twenty-entry animation rig drives seventeen visible rigid bones. Emergence, walking, swipes, shield recoil, damage, stun, headless pursuit, daylight burial and body breakup remain native. The model is 9,198 triangles with six material roles. Native lighting, fog and damage filters shade the bone; only the inset eyes use the original environment-color pulse.

## Source and rebuild

- `stalchildren.blend`: editable weighted meshes with all five original clips, 82 frames, and named timeline markers.
- `stalchildren.glb`: authored source geometry in native joint spaces.
- `author_blender.py`: anatomy and local joint placement. Run in an isolated Blender process.
- `extract_native_rig.py`: native skeleton, source bounds and animation extraction from the local game archive.
- `export_runtime.py`: deterministic integer geometry compiler with material, joint, normal, topology and budget checks.
- `validate_animation.py`: 434,928 authored vertex transforms across all original animation frames. Maximum per-axis envelope extension is 5.43 world units at regular scale and 10.85 at giant scale; this is an envelope check, not collision proof.
- `preview_motion.py`: original emergence, walking, attack, recoil and dying stills.

From the repository root:

```sh
python3 art/arpg/stalchildren/extract_native_rig.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/stalchildren/author_blender.py
python3 art/arpg/stalchildren/export_runtime.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/stalchildren/package_blend.py
python3 art/arpg/stalchildren/validate_animation.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/stalchildren/preview_motion.py
```

## Runtime and testing

`StalchildVisuals` replaces the original display lists after the native head/jaw visibility logic. Geometry is cached for the process lifetime: detached parts can outlive their parent without referencing per-frame memory or a flex-matrix palette. Original collision-sphere updates and BodyBreak callbacks remain active. Pending native debris allocations are now released on interruption, replacement and actor destruction. Completed breakup buffers are not freed twice.

Both ARPG feature switches and a 1,500-unit camera-distance fallback restore original anatomy. The practice menu uses original parameters 0, 5 and 10, checks floor/footprint/sightline/headroom, and caps nearby Stalchildren at six.

Practice path: **I → More → Settings & testing → Testing shortcuts → Upgraded Stalchildren**. Choose a size, use **Set night**, then summon. Daylight still sends them underground. In the tested keyboard profile, **C** is sword, **X** is action, **Z** targets and **R** raises the shield. Debug controls do not pause gameplay; the adventure journal does.

## Verification

See `tests/arpg/stalchild-verification-macos.json` for source hashes, fixture limits and acceptance details. All 40 portable suites passed; the final geometry suite and ASAN/UBSAN passed after the art refinements. The final native run passes 72 assertions, with 947 live-actor draw-end observations, 87 distinct poses, 108 headless observations, seven headless yaw values and four earned defeats. Its 812 debris observations count individual live parts at draw end, not unique rendered frames.

Native checks cover head/jaw detachment, continued headless motion, shield recoil, immunity/stun/fire responses, size-dependent drops, burial, water reaction, practice cap and interrupted scene cleanup. Collision flags and deep-water conditions are explicit fixtures. The full native field-spawner escalation and bunny-mask route remain untested.

Ordinary play verified the three size choices, night shortcut, targeting, movement, swipes and a regular Stalchild sword defeat. Journal XP rose from 31,160 to 31,206. A fresh giant encounter is open and paused in the separate practice build at `/tmp/soh-stalchild-playtest/Stalchild Adventure.app`. A near-bridge attempt encountered moat knockback; final practice moved into open field. Full manual headless encounters, repeated large/giant kills, crowd performance and wider regional acceptance remain open.

The normal `player1` save checksum is unchanged. The broader replayability goal is still incomplete.
