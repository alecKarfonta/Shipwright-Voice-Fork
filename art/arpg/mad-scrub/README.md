# Original Mad Scrub upgrade

The hostile `En_Dekunuts` remains the original enemy. Nine visible native joints carry a mossy bark seed body, hollow fluted snout, narrow amber eyes, folded rust leaves, a separate crown, root shins and branching toes. Its child flower uses a matching rosette. Shop and hint Scrub models are separate and unchanged.

References are `objects/object_dekunuts/gDekuNutsSkel` and `gDekuNutsFlowerDL`: retain the small body, orange leaf umbrella, tall crown, projecting mouth and broad flower footprint. Extracted body/leaf/eye/snout textures and native rig geometry are stored beside the source for palette and silhouette review. The native nut projectile is retained.

`mad-scrub.blend` is editable, with the original 158 frames across ten clips. Each part has a native joint assignment. `posed_rig.py` reproduces the original pre-limb snout squash in parent space. Flower actors are stationary while the enemy escapes; the running/damage studio previews hide the separate flower to expose the feet.

The runtime uses 4,112 body triangles and 632 flower triangles, 5,585 indexed vertices and 27 joint/material parts. Seven explicit material slots map to Wood, Plant and tiny eye Glow. NativeMaterials supplies lighting and fog; Plant pieces load the species' original RGBA16 32×32 leaf texture with matching coordinates. Geometry/display lists are persistent and shared across actors. Original collision, hit reactions, animation, seed origins, shield reflection, burrowing, reward and surviving flower behavior remain authoritative. Both ARPG toggles and the 1,500-unit camera fallback restore original meshes.

The shared enemy classifier excludes decorative flower actors from combat scaling, affixes, nameplates and defeat rewards.

Practice offers single shots and native three-shot volleys in a collapsed submenu. Placement checks floor, space and obstructions, caps nearby live Scrubs at four, and caps surviving flowers at eight. Seeds now use the port's existing object-spawn fallback, so they work when practice creates a Scrub outside its usual scenes. This fallback also applies to the other original Scrub projectile variants; their art and AI are unchanged.

## Rebuild

Run from the repository root using an isolated Blender process:

```sh
python3 art/arpg/mad-scrub/extract_native_rig.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/mad-scrub/author_blender.py
python3 art/arpg/mad-scrub/export_runtime.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/mad-scrub/package_blend.py
python3 art/arpg/mad-scrub/validate_animation.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python art/arpg/mad-scrub/preview_states.py
```

Blender MCP was unavailable for this pass; Blender 5.1.2 ran in isolated background processes. The live Blender scene was not touched.

The native QA fixture is compiled only with `SOH_ARPG_RUNTIME_TESTS`. Set `SOH_ARPG_MADSCRUB_QA=1` and an isolated `SHIP_HOME` basename beginning `soh-arpg-qa-`. `SOH_ARPG_MADSCRUB_UI=1` leaves the finished dungeon fixture paused. Test settings pin adult Link, camera, time and health; seed reflection begins with a synthetic shield-bounce flag, then the real returned seed flies into the native cylinder. Later stun/fire hits are injected at the native damage boundary. The chase, gasp, home return, death, reward and scene teardown run through original actor updates.

See [verification](../../../tests/arpg/madscrub-verification-macos.json), [native results](../../../tests/arpg/madscrub-runtime-results-macos.json), [animation bounds](animation-validation.json) and [export counts](export-report.json). Capped draw cadence is a smoke comparison, not an uncapped performance benchmark. Bounds checks do not establish triangle clearance, and broader regional terrain, texture packs and natural shield timing remain manual acceptance work.
