# Original Guay upgrade

Regular and giant `En_Crow` enemies keep their original circling flight, dives, flapping skeleton and wagging tail. The replacement adds a continuous tapered body, hooked bill, inset orange eyes, layered coverts and separated flight feathers. Seven visible joints carry the model; six native connections span neighboring wing and tail joints. The original purple feather texture is reused on the authored plumage, with explicit material roles for feathers, bone and small emissive eyes.

`guays.blend` is the editable animated source, with the eight-frame native fly clip. The exported model has 3,988 indexed vertices, 7,304 triangles and 28 joint/material parts. Quantized runtime geometry is checked in all eight poses with both the living tail oscillation and the neutral death callback. The maximum bounds extension is 0.73 world units at normal scale and 2.19 at giant scale. Connection endpoints are checked in every pose; bounds do not establish triangle-level clearance. `native-reference.png` is an untextured shape diagnostic and `native-texture-reference.png` records the original paint.

The renderer caches geometry and display lists and uses the original seven flex matrices. The original callbacks continue to position head, wing and tail effects. Actor tint, native fire and ice effects, attention, collision and wing timing stay native. Both feature gates and a 1,500-unit camera-distance fallback restore original meshes.

Two lifecycle fixes support ARPG health scaling. A damaging hit that leaves health now enters a 35-frame native escape instead of starting a fatal fall. Deku Nuts retain their original 100-frame escape. A completed native respawn restores the existing scaled maximum health and clears only the XP/loot credit guards for the new life. It does not reroll affixes, stack health multipliers or grant rewards on its own. The first real death of each life remains eligible for one reward; duplicate defeat callbacks remain ignored. The original 300-frame hidden delay, growth, ten-defeat giant transformation, giant red rupee and randomized-enemy removal are preserved.

Practice is under the collapsed **Upgraded Guays** section, with regular and giant summons. Placement requires a broad floor envelope, headroom, sight lines and space from nearby enemies. Hidden respawning Guays count toward the four-bird limit. Giant practice starts at the native threefold scale and uses the same sphere calculation as natural giant respawns.

## Rebuild

Run from the repository root:

```sh
python3 art/arpg/guays/extract_native_rig.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/guays/author_blender.py
python3 art/arpg/guays/export_runtime.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/guays/package_blend.py
python3 art/arpg/guays/validate_animation.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/guays/preview_states.py
```

Blender MCP was unavailable. Blender 5.1.2 ran in isolated background processes, leaving the live scene untouched.

## Validation scope

The native harness requires `SOH_ARPG_RUNTIME_TESTS`, `SOH_ARPG_GUAY_QA=1` and an isolated `SHIP_HOME` basename starting with `soh-arpg-qa-`. `SOH_ARPG_GUAY_UI=1` leaves its completed dungeon fixture paused. It pins Link, camera, health and time. Real native arrows and a Deku Nut shockwave exercise collision and damage; no defeat is injected to advance the lifecycle. Extra defeat notifications deliberately test duplicate-credit protection. The first complete 300-frame respawn is observed; subsequent delays are shortened to test ten-defeat giant escalation. One ice fixture starts at two health. Native contact with Link is not asserted. The harness is excluded from the ordinary adventure executable.

The isolated executable uses the verified Poe snapshot for save/material ABI compatibility. Current Guay code, native actor and practice source are compiled directly. The exact new credit-reset functions are applied to the snapshot's older XP and item modules; their unrelated current work is not copied into that executable. All four current Guay/reward modules also compile against the current repository headers. See the verification record for source and executable hashes.

All 45 portable suites, final geometry ASAN/UBSAN, native animation/connection checks and 80 native runtime assertions passed. The final live run records twelve earned defeats, eleven ignored duplicate notifications, ten fire deaths, one ice death, the complete first respawn delay and the natural giant/red-rupee cycle. Both practice summons, original targeting, dive contact and player weapon inputs were observed in the ordinary build. A manual defeat was not confirmed. The separate practice adventure is open and paused; the normal player save checksum is unchanged. See [verification](../../../tests/arpg/guay-verification-macos.json), [native results](../../../tests/arpg/guay-runtime-results-macos.json), [animation bounds](animation-validation.json) and [export counts](export-report.json). Natural scene placements, child Link, Skull Mask/horse conditions, randomized sizes, broader terrain and texture packs need further acceptance coverage. Four-actor capped cadence is a smoke comparison, not an uncapped GPU benchmark.
