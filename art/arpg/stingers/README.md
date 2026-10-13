# Native Stinger upgrade

Land and water Stingers share a new Blender-authored ray body with thin cambered fins, swept tips, segmented tail, gill slits and amber eyes. Teal and blue habitat palettes distinguish the two original enemies. The eleven visible limbs follow the original nineteen-joint skeleton and all five native animation clips: backflip, dive, hit, idle and pop-out.

The original underground fins, circling decoys, ambush, glide, diving strike, water pursuit, breach, stun and sinking death remain authoritative. Native scale, root offsets, colliders and damage rules are preserved. A compatible native flex traversal supplies eleven matrices for eight joint connections without changing the skeleton or animation assets. Original geometry remains available through both feature gates, at distance and for incompatible replacement rigs.

## Assets and rebuild

- `stingers.blend`: editable land and water presentations, weighted to native animation matrices across 208 frames.
- `stingers.glb`: shared source geometry with joint-connection metadata.
- `StingerBlenderData.h` in the ARPG source directory: generated integer geometry, 9,434 triangles per actor.
- `stingers-studio.png` and five clip stills: Blender authoring previews.
- `stinger-native-preview.png`: final meshes in the native renderer, captured in the isolated QA fixture.
- `animation-validation.json`: native pose envelopes and 79,872 quantized connection endpoint checks.

From the repository root, with an installed Blender:

```sh
python3 art/arpg/stingers/extract_native_rig.py
blender --background --factory-startup --python art/arpg/stingers/author_blender.py
python3 art/arpg/stingers/export_runtime.py
blender --background --factory-startup --python art/arpg/stingers/package_blend.py
blender --background --factory-startup --python art/arpg/stingers/preview_motion.py
python3 art/arpg/stingers/validate_animation.py
```

Extraction reads the local `build-cmake/soh/oot.o2r` archive. Authoring uses isolated background Blender processes; it does not modify the user's live project.

## Practice

Open **More → Settings & testing → Testing shortcuts → Upgraded Stingers**. Choose **Land wanderer**, **Circling decoys** or **Water swimmer**. Land placement needs open ground; water placement needs a clear view of nearby deep water. A circling encounter counts as four actors toward the eight-actor practice limit.

Land Stingers expose only their dorsal fins underground. Approach or use a utility hit to provoke the native ambush, then watch for their diving attack. Water Stingers use their original swimming pursuit and breach. The practice guide stays inside the collapsed section.

## Verification scope

The portable geometry suite checks both palettes, eleven visible limbs, eight joint connections, matrix assignments, normals and triangle limits. The animation validator checks all 208 native frames against the original posed bounds; it does not assert watertight topology. Native QA runs in an isolated debug profile and supplies collision/damage stimuli and attack setup calls for repeatable coverage. The breach check explicitly supplies near-surface positions, water distances, facing and a fresh floor query before calling the native attack function; it holds the player update for 21 ticks while observing the resulting native airborne updates. It is not a substitute for a full Jabu-Jabu playthrough or crowded-scene performance testing.

The integration also fixes missing defeat rewards from the original underground instant-kill rule and removes circling decoys when their parent is interrupted. Decoys are excluded from enemy scaling, affixes and defeat rewards. The normal player save is outside all test profiles.

Final results are recorded in `tests/arpg/stinger-verification-macos.json` and `stinger-runtime-results-macos.json`: 34 portable suites, geometry ASAN/UBSAN and 66 native runtime checks pass. Ordinary land-menu summon, targeting, backflip/flight, attack damage and journal reopening were verified manually. Manual water and circling combat acceptance remain open. Both isolated test applications exited successfully; the normal save checksum is unchanged.
