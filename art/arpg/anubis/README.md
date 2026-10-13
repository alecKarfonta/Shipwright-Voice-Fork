# Original Anubis upgrade

`En_Anubice` keeps its floating, armless mummy silhouette and native mirror puzzle. Eight original visible joints now carry fitted linen strips with folded edges, a beveled jackal mask, separate jaw and lower canines, lapis-and-brass collar, and a faceted turquoise pendant. The native translucent spiral eyes remain on their original head matrix. The new geometry introduces no new movement, attack timing or collision.

The references are the original `gAnubiceSkel` geometry, `gAnubiceEyesDL` facial planes, and the extracted `object_anubiceTex_000F90`, `001110`, `001590` and `001610` textures for aged linen, ornament and dark mask colors. `native-reference.png` is an untextured silhouette diagnostic. Native texture previews are stored alongside the source. The opaque mask is inset from its original facial planes so the original translucent eyes stay visible.

The editable `anubis.blend` contains all five native clips, totaling 169 frames. Idle, attack and falling death are used by the original actor; laughing and standing are preserved for authoring inspection but are not additional gameplay behaviors. The runtime exports 7,009 triangles, 11,433 indexed vertices and 24 joint/material parts. All parts are rigidly assigned to their original joints. Animation bounds at the original .015 actor scale extend the native envelope by at most 0.60 world units; this is an envelope check, not proof against every triangle intersection.

Eight explicit material slots map to Cloth, Wood, Metal and Gem. The adapter uses native scene lighting and fog; the pendant uses the compatible scene-lit Metal treatment, without added emissive geometry. Only the original eyes retain their original translucent material. Geometry and display lists are cached across actors. Both ARPG feature gates and the 1,500-unit camera fallback restore original meshes.

The original controller reflects Link's position around an invisible anchor. The mummy rises, hovers, answers sword swings with its frame-twelve fireball, spins away from non-fire hits, retreats when Link leaves range, and burns when hit by fire or an active native flame circle. Its native fireball, eyes, damage table, collision, death animation and collectible drop remain authoritative.

The shared enemy classifier now excludes Anubis fireballs from scaling, affixes, nameplates and rewards. The body remains reward-eligible after its native death switches it into the prop category. Controller and body teardown validate actor-list membership before reading cached pointers. Removing a live controller removes its owned body; removing the controller during death preserves the burning body until its native drop completes. Removed flame-circle pointers are cleared before use.

Practice lives in the collapsed **Upgraded Anubis** section. It spawns the original controller with a 360-unit trigger radius, checks a 390-unit floor perimeter and 160-unit headroom, separates nearby anchors, and caps them at four. It does not add a flame circle to the scene. Fire arrows or Din's Fire provide the counter outside the original puzzle rooms. Mirror-shield fire behavior continues to follow the existing `AnubisFix` preference.

## Rebuild

Run from the repository root with isolated Blender processes:

```sh
python3 art/arpg/anubis/extract_native_rig.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/anubis/author_blender.py
python3 art/arpg/anubis/export_runtime.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/anubis/package_blend.py
python3 art/arpg/anubis/validate_animation.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/anubis/preview_states.py
```

Blender MCP was unavailable for this pass. Blender 5.1.2 ran in isolated background processes; the live scene was not modified. Studio renders omit the game's native translucent eye display list. In-game captures establish the final eye presentation.

The native QA harness is compiled only with `SOH_ARPG_RUNTIME_TESTS`. Set `SOH_ARPG_ANUBIS_QA=1` and a separate `SHIP_HOME` basename starting with `soh-arpg-qa-`. `SOH_ARPG_ANUBIS_UI=1` leaves the completed dungeon fixture paused. The fixture pins adult Link, camera, health and time; sets Link's melee state to initiate one attack; spawns real ordinary/fire arrows and invokes their native launch function; and changes a real flame-circle switch. All collision, mirroring, hit reactions, death, reward and teardown then use native updates. It also tests both interrupted cleanup orders, removed cached circles, four simultaneous mummies, feature fallbacks and a scene transition.

All 43 portable suites, geometry ASAN/UBSAN, five-clip bounds, semantic archive checks and 55 native checks passed. Ordinary play verified spawning, mirroring, visible fireball retaliation, fire-item assignment and a close-range Din's Fire defeat: XP rose from 31,160 to 31,275 and a Rough Ruby dropped. Another champion Anubis is ready in the separate practice adventure, with fire arrows and Din's Fire equipped. The normal player save is unchanged.

See [verification](../../../tests/arpg/anubis-verification-macos.json), [native results](../../../tests/arpg/anubis-runtime-results-macos.json), [animation bounds](animation-validation.json) and [export counts](export-report.json). Four-actor capped draw cadence is a smoke comparison, not an uncapped GPU benchmark. Original Spirit Temple puzzle layouts, child Link, mirror-shield settings, broader terrain and third-party texture packs remain additional acceptance coverage.
