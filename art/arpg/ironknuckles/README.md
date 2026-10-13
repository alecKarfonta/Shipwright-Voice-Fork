# Native Iron Knuckle upgrade

The ordinary ivory, black and silver guards use new Blender-authored armor and an axe on their original animation rig. The helmet has a recessed visor and cheek vents; the body has independent shoulder plates, articulated gauntlets, fluted leg armor and segmented boots. Detachable front/back armor reveals a quilted red torso. The native metal and chain-mail textures preserve the game's lighting and material character.

The native wake-up, walk, run, block, axe attack windows, recovery, damage rules and death remain authoritative. Armor shedding uses the original BodyBreak matrices and physics. Nabooru's story and cutscene models remain original. The ARPG and remastered-enemy settings gate the replacement, with original geometry beyond 1,500 world units.

## Assets

- `ironknuckles.blend`: editable meshes with two displayed armor states and all twelve native animation clips, totaling 279 frames.
- `ironknuckles.glb`: source geometry and joint-connection metadata.
- `IronKnuckleBlenderData.h` in the ARPG source directory: checked integer geometry generated from the GLB.
- `ironknuckles-studio.png` and six animation stills: authoring previews.
- `ironknuckle-native-preview.png`: final model rendered by the game in an isolated QA fixture.

One shared geometry set supplies three runtime palettes. The armored state has 14,926 triangles; the exposed state has 12,918. Eight native-joint bridges connect hips, knees, elbows and wrists. Both detachable chest plates are rigid and require no other actor's matrix palette.

## Rebuild

From the repository root, using an installed Blender in separate background processes:

```sh
python3 art/arpg/ironknuckles/extract_native_rig.py
blender --background --factory-startup --python art/arpg/ironknuckles/author_blender.py
python3 art/arpg/ironknuckles/export_runtime.py
blender --background --factory-startup --python art/arpg/ironknuckles/package_blend.py
blender --background --factory-startup --python art/arpg/ironknuckles/preview_motion.py
python3 art/arpg/ironknuckles/validate_animation.py
```

Rig extraction reads the local `build-cmake/soh/oot.o2r` game archive. Geometry compilation and animation validation use the Python standard library. These scripts do not modify a live Blender scene.

## Practice and verification

Open **More → Settings & testing → Testing shortcuts → Upgraded Iron Knuckles**. Select armor and summon in a wide, uncleared room. Strike the armor to wake the guard. Bait the axe and attack during recovery; removed armor means faster pursuit and vulnerability to arrows. The placement helper checks floor, headroom and nearby enemies and caps active guards at three. Native cleared-room despawn is explained instead of bypassed.

Verification is recorded in `tests/arpg/ironknuckle-verification-macos.json` and `ironknuckle-runtime-results-macos.json`: 33 portable suites, geometry ASAN/UBSAN, all 279 native animation frames, 53,568 bridge endpoint checks, and 79 native runtime assertions. Ordinary menu summon, sword wake-up, targeting, axe damage/knockback and reopening the journal after the encounter were also checked manually.

The native test supplies damage/collision stimuli and selects attack setup functions to make both attack types repeatable. Its player/camera are fixed, and the interrupted-break test explicitly calls the real destructor before inspecting cleared ownership. This does not substitute for a full Spirit Temple playthrough, Nabooru cutscene coverage, varied terrain or crowded-scene performance testing.

The native integration also fixes rear-armor resource identity, the no-switch sentinel, leaked axe-trail effects and cleanup of interrupted debris allocations. The normal player save is unchanged; test profiles and executables are isolated under `/tmp/soh-ironknuckle*`.
