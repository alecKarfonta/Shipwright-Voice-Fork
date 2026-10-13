# Native art workflow

Use this process for every new or changed ARPG model, world prop, equipment appearance, icon or combat effect. The reference is the existing game, including its painted textures, silhouette scale, lighting and animation. A studio render alone does not establish a match.

## Authoring

1. Identify two or three existing assets that fit the same role. Record their resource names and what to match: silhouette, palette, surface detail and size at normal gameplay distance. Prefer an existing native asset when its shape already communicates the action.
2. Choose materials by meaning. `GearGeometry::Color` carries a `Surface` tag; `Paint(color, Surface::Metal)` makes that choice explicit. `Shade` preserves it. Primitive geometry must call `FinishMaterials` with its authored base before returning. Blender material slots use the named `Surfaces` table in their geometry adapter. Never infer material from RGB, brightness, rarity or item element.
3. Separate materials even when their RGB is identical. Render batches use `MaterialKey`, which includes the surface tag. Preserve semantic slot order on export; update `native-style/material-slots.json` when deliberately changing it.
4. Use `NativeMaterials` for scene-lit surfaces and `NativeEffects` for trails, dust, flames, lightning and glints. Keep flat eyes, apertures and warning pulses only where intentional. Reserve unlit `Glow` for small emitters. Gems retain facet lighting; they are not full-bright lamps. Verify native texture format, width and height against the resource archive.
5. Preserve native joints, attachment pivots, normals, collision, effect lifetime and warning boundaries. Keep geometry and display lists cached. Record triangle and material counts; increasing texture batches or translucent coverage requires a crowded-scene check.

## Review in the game

Use an isolated `SHIP_HOME` whose basename starts with `soh-arpg-qa-`. Keep runtime fixtures out of the installed production build.

- Inspect the asset next to its native references at ordinary gameplay distance and close range. Check daylight, night or a dark interior, and the relevant room lighting and fog.
- For equipment: inspect pickup, held, sheathed and worn versions. Run, roll, attack and swim using the normal controls. Check child/adult variants where supported, scabbard/hand changes, and clipping at the shoulders, feet and back. Compare the inventory icon's identity and colors at its actual display size.
- For enemies: inspect idle, movement, attack, damage, stun/freeze, death and detached parts. Check variant palettes, off toggles and distance fallback. Preserve native eyes, mouths, aura anchors and warning pulses.
- For effects: inspect startup, impact and fade in a dark interior and outdoors. Overlap multiple effects; the enemy, weapon, floor and warning boundary must remain readable. Test all changed runes.
- For props: inspect incomplete/completed states and interaction distance, plus slope contact and nearby native scenery. Verify appearance without the target marker.
- Check performance with the same scene, camera, resolution and population before/after. Record material batches and triangle counts as well as runtime behavior. A render smoke test is not a frame-rate benchmark.

Save screenshots and results under `art/arpg/<asset>/` or `art/arpg/native-style/`. Record exactly which actions and environments were observed, source hashes, build flags and limitations. Do not label unobserved motion, all rooms or third-party texture packs as verified.

## Repeatable checks

```sh
cmake -S tests/arpg -B /tmp/arpg-art-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/arpg-art-tests -j 6
ctest --test-dir /tmp/arpg-art-tests --output-on-failure
/tmp/arpg-art-tests/arpg_material_workload > /tmp/arpg-art-workload.json
python3 art/arpg/native-style/check_assets.py --archive build-cmake/soh/oot.o2r
```

`arpg_material_identity` exercises authored meshes and recolors, verifies surface tags survive shading, and prevents equally colored gem/leather batches from merging. Existing geometry tests check native rigs, scale and triangle validity. `arpg_material_workload` records geometry/palette hashes, vertex counts and material batches for 273 equipment and enemy limb variants, so a material-only change can demonstrate unchanged geometry and quantify extra batches. Use the same compiler and optimization flags for before/after fingerprints, because floating-point code generation can differ. This measures authored workload, not GPU frame rate. The asset checker compares Blender slot names and runtime surfaces and verifies the referenced native resources.

For a native test build, enable `SOH_ARPG_RUNTIME_TESTS`. `SOH_ARPG_ART_QA` selects the isolated preview:

| Mode | Review |
| --- | --- |
| 0 | Equipment pickups; `SOH_ARPG_ART_RELICS=1` or `2` selects relic pages |
| 1 | Completed quest props |
| 2 | Representative attacks; `SOH_ARPG_ART_NIGHT=1` selects night |
| 3 | Every active attack and rune through its lifetime |
| 4 | Waystone |
| 5 | Close equipped relics |
| 6 | Equipped relics in the field with normal movement controls |
| 7 | Equipped relics in the Deku Tree with normal movement controls |
| 8 | Equipped relics in Lake Hylia water with normal movement controls |

For a dark combat preview add `SOH_ARPG_ART_INTERIOR=1` to mode 2 or 3.

In modes 6–8, write `run`, `roll`, `attack` or `swim` into `$SHIP_HOME/native-art-capture.txt`, then perform that action using the normal controls. The fixture freezes after three matching native animation frames for inspection. Write `resume` or a different capture command to continue.

Modes 6–8 record moving, rolling, attacking and in-water frames in `native-art-results.json`; these counters establish runtime coverage, not visual acceptance. Capture and inspect the actual animation as well. `SOH_ARPG_OCTOROK_QA=1`, `SOH_ARPG_SKULLTULA_QA=1` and `SOH_ARPG_BUBBLE_QA=1` run the respective integration tests; adding the matching `_UI=1` leaves a visual fixture after completion.

Before installing, build with both runtime and journal fixtures disabled, verify a normal startup, and retain a recoverable executable backup. Link the new verification record from the ARPG README.
