# Original roaming Poe upgrade

Normal and rupee `En_Poh` variants keep the original floating silhouette, pointed face, native rig and combat. The authored replacement adds folded and frayed cloth, a shaped hood opening, articulated hands and a bronze frame around the original glowing lantern. Sharp, Flat and the separate riding/Big Poe actor retain their existing art.

References are `gPoeSkel`, `gPoeLanternDL` and the original `object_poh` cloth, face and lamp textures. `native-reference.png` is an untextured geometry diagnostic; `native-texture-reference.png` records the native green-gray cloth, dark face, bronze fittings and warm lantern panels. The runtime lamp draws the original luminous core before the fitted frame, retaining its flicker and point light.

The editable `poe.blend` contains all six original clips, totaling 98 frames. Nine original visible joints carry 7,905 authored triangles, 4,255 indexed vertices and 22 joint/material parts. The original body uses eight of these joints; the lamp is drawn separately on its captured native matrix. Quantized geometry passes every native clip frame at the original .01 scale. Maximum envelope extension is 6.78 world units during attack, principally from fuller sleeves and hands. Bounds do not establish triangle-level clearance in every pose.

Nine explicitly named material slots map to Cloth, Skin, Metal and small emissive eyes. The compatibility renderer uses the shared native Bone texture for the green hands, retaining their authored color and scene lighting. Cloth and bronze use shared native material bindings. Heavy geometry and vertex buffers are cached; small frame-owned wrappers preserve each ghost's independent native opacity and color. The original fully transparent body remains hidden. Both feature gates and the 1,500-unit camera fallback restore native body and lantern geometry.

The native actor retains emergence, circling pursuit, lantern windup and attack, hit recoil, zigzag escape, disappearance and return. Damage tables, collision, frame timing, original burn effect, lantern fall/break, soul and bottle conversation are unchanged. The visual frame follows the lantern both while held and during its fall. Native fragment and soul display lists remain authoritative. The normal object's lookup uses the port's existing spawn fallback so the practice option can initialize outside usual Poe rooms; composers keep their original object lookup.

Practice is under the collapsed **Upgraded Poes** section. It checks level ground, room above the spawn point, line of sight and nearby enemy spacing. The four-actor limit includes living Poes, falling lamps and lingering souls.

## Rebuild

Run from the repository root:

```sh
python3 art/arpg/poes/extract_native_rig.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/poes/author_blender.py
python3 art/arpg/poes/export_runtime.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/poes/package_blend.py
python3 art/arpg/poes/validate_animation.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/poes/preview_states.py
```

Blender MCP was unavailable. Blender 5.1.2 ran in isolated background processes, leaving the live scene untouched. Studio previews show the authored frame without the game's original glowing lantern core; runtime captures show the composite.

The native harness is gated by `SOH_ARPG_RUNTIME_TESTS`, `SOH_ARPG_POE_QA=1` and an isolated `SHIP_HOME` basename starting with `soh-arpg-qa-`. `SOH_ARPG_POE_UI=1` leaves its completed dungeon fixture paused. It pins Link, camera, health and time, uses native pursuit to trigger the lantern attack, injects a contact flag to exercise recoil, and shortens a visibility timer to exercise disappearance. Real arrows then exercise native collision, damage and death. These controls are QA fixtures and are excluded from the ordinary adventure executable.

All 44 portable suites, geometry ASAN/UBSAN, six-clip animation bounds, semantic archive checks and 45 native assertions passed. The live run includes 34 damage frames, 25 fleeing frames, 28 burning frames, seven falling-lamp frames and the complete 200-frame soul wait. Two real arrows produced exactly one native and ARPG defeat. Ordinary practice, circling attacks, day/night appearance and journal pause were observed; a manual defeat was not confirmed. The separate practice adventure remains open and paused, and the normal player save checksum is unchanged. See [verification](../../../tests/arpg/poe-verification-macos.json), [native results](../../../tests/arpg/poe-runtime-results-macos.json), [animation bounds](animation-validation.json) and [export counts](export-report.json). Natural graveyard spawns, a manual bottle capture, composer encounters, child Link, unusual terrain and third-party texture packs need further acceptance coverage. Capped four-actor draw cadence is a smoke comparison, not an uncapped GPU benchmark.
