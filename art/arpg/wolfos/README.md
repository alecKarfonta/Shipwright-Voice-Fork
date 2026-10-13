# Blender Wolfos

The procedural replacement below is **not visually accepted**. User feedback
identified chunky anatomy, reversed palms, poorly seated teeth/claws, floating
eyes, missing pupils and unconvincing blinking. Passing runtime checks does not
resolve those art issues. Work has shifted to an editable master with animated
original references; do not expand the enemy roster from this recipe.

## Animated authoring workspace

`native_asset.py` reads the original local `build-cmake/soh/oot.o2r`: both
skeletons, eight animation clips, per-vertex matrix assignments, UV coordinates,
texture wrapping and all 16 original texture maps. It checks the imported bind
pose against the existing native fixture and rejects unknown geometry commands.

`create_workspace.py` creates `workspace/wolfos-authoring.blend`. The left mesh
is a locked original reference; the right mesh is an independent editable master
that initially has identical anatomy. A hidden collection contains white Wolfos
with its own textures. Standard Blender armatures deform the meshes. Timeline
markers identify all 125 native animation frames followed by held eye states and
head-turn diagnostics. The latter are inspection sequences, not native blink
timing or AI. Native eye textures are driven by the rig's `blink` property.

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup \
  --python-exit-code 1 --python art/arpg/wolfos/create_workspace.py
```

The importer refuses to overwrite an existing master. Open and edit that `.blend`
directly; use `-- --output /new/directory` only to make another fresh import. The
workspace packs its textures and is ignored by Git because it contains assets
decoded from the user's own game. Neither this importer nor its preview tool
changes the runtime GLB, generated headers, app executable or player save.

`preview_workspace.py` opens the saved master in a separate background process
and renders a 20 fps animation reel plus eye close-ups. It checks that every
animated vertex stays inside the review camera. Use `-- --master path.blend
--output /new/preview/directory` for another master or preview. Assemble the PNG
frames with `ffmpeg -framerate 20 -i frames/%04d.png -c:v libx264 -pix_fmt yuv420p
native-animation-reel.mp4` from the preview directory.

The importer validates Blender's actual evaluated mesh across 165 frames against
decoded native transforms. Studio shading approximates the original rendering;
native fog, lighting combiners, actor movement, speed changes and transitions
between animations still need in-game review. The editable master starts with
original anatomy. The existing procedural replacement's pupil/blink issue
remains open; the authoring workflow now supports replacing it without rebuilding
anatomy in Python.

The user accepted the animation preview on October 13. Preserve that movement
reference during the art pass; this feedback does not approve the replacement
anatomy or materials.

## Painted eye revision

The eye-only milestone is `workspace/wolfos-painted-eyes-v1.blend`, exported as
`workspace/export/wolfos-painted-eyes-v1.o2r`. Its brown Wolfos has a painted amber
iris, dark round pupil, limbal ring, and upper eyelid that covers the eye through
open, half and fully closed states. The original `EyeNarrowTex` resource now
contains the closed painting; the actor still selects its original
open/half/closed/half blink sequence. White Wolfos remains unchanged.

The three original paintings live in `paint/eyes/` with the exact built-in
imagegen prompts in `prompts.json`. They are 1254px RGBA sources, reduced to 256px
when imported into the master. Their alpha contour seats the artwork on the
native eye surfaces. No separate eyeballs or attachment geometry were added.
All other exported resources are byte-identical to the baseline pack.

`apply_paint_maps.py` applies a JSON map of native texture paths to authored PNG
files. It reads the saved master and saves a new revision, preserving its mesh,
rig and locked reference images. For example:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup \
  --python-exit-code 1 --python art/arpg/wolfos/apply_paint_maps.py -- \
  --master art/arpg/wolfos/workspace/wolfos-authoring.blend \
  --output /new/path/wolfos-painted-eyes.blend \
  --manifest art/arpg/wolfos/paint/eyes/brown-eyes-v1.json
```

Use `preview_workspace.py --closeups-only` to review the four held blink frames.
`workspace/painted-eyes-preview-v1/painted-blink-review.mp4` is a slowed inspection
loop, not native blink timing. The native preview launcher supports `--mode eyes`
for a closer camera and four-second eye-state holds. This mode is diagnostic;
ordinary combat retains the original blink timing.

Open, half and closed states were inspected in Blender and in the actual game.
This is the first painted material pass on the native master, not a completed
modern character model. Body anatomy, fur, mouth and claws still need art work;
the older procedural replacement still has its previous eye implementation.

## Painted coat and body revision

The first body milestone is `workspace/wolfos-body-refined-v2.blend`, exported as
`workspace/export/wolfos-painted-body-v2.o2r`. `workspace/current-master.json`
records the current revision. Earlier masters remain available for comparison.
This revision adds four paintings: directional gray-brown fur, graded ivory
keratin, a recessed nostril, and a darker mouth interior. Source images and exact
built-in imagegen prompts are in `paint/material-prompts.json`. The tongue map
remains original after its generation request was rejected by the image tool.

Simply replacing the old fur image exposed fourteen collapsed UV triangles and
long stretched bands. `author_coat_uv.py` gives the 266 coat triangles consistently
scaled limb-local projections and repeat wrapping on both axes. The exporter
supports a material `export_wrap` pair (S, T); omitting it preserves the original
template exactly. Only the final render-tile command changes, retaining texture
loading, lighting and the original logical texture size. The coat imports at
256×512, with three new face maps at 256×256 and the earlier 256×256 eye maps.

`refine_body_edges.py` bevels selected rigid coat edges on the forearms, hind paws
and tail. The resulting master has 913 triangles. Its 108 mixed-joint triangles,
head, eyes, teeth and claw geometry remain unchanged. It updates normals without
reorienting the original mouth surfaces. All 114,125 triangle/pose combinations
across the 125 native frames remain finite and nondegenerate after export.

`preview_workspace.py --keyposes-only` renders eight representative body poses
plus four eye states for faster art review. The full 165-frame reel is in
`workspace/body-refined-motion-v2/`. This pass improves materials and some rigid
edges; it is not a finished modern character. Face and torso topology, anatomical
detail, coat-region variation and UV seams still need work. White Wolfos and rank
fittings also remain pending. Continue editing the saved master rather than
regenerating the rejected procedural anatomy.

## Head, torso and regional coat revision

The regional-coat milestone is `workspace/wolfos-sculpt-regions-v3.blend`, exported as
`workspace/export/wolfos-sculpt-regions-v3.o2r`. `refine_head_torso.py` edits the
previous saved master with two subdivision passes and bounded relaxation inside
rigid skull, torso and abdomen coat regions. Joint boundaries, eye seating, ear
tips and mouth borders are pinned. The master has 1,951 triangles. All 108 original
mixed-joint triangles and 61 non-coat triangles retain their geometry and winding;
the other limbs retain the previous body revision's shape.

`paint/coat/wolfos-coat-regions-v1.png` is a new built-in imagegen painting with
four equal strips: short facial fur, longer body fur, pale chest fur and dark
muzzle fur. The full prompt is in `paint/coat/region-prompt.json`; the import
manifest is `paint/coat/brown-regions-v1.json`. The master uses a 1024×512 copy.
`author_coat_regions.py` assigns a persistent `coat_region` face attribute and
continuous cylindrical UVs per limb. Its second revision moves the pale patch
onto the underside of the chest and reduces per-triangle fur direction changes.
Both texture axes clamp; the UVs stay inset inside each atlas strip. The third
revision adds end-cap projections and minimally separates collapsed UVs on the
native integer texture grid. All exported coat triangles now retain nonzero UV
area; this prevents sampling one line of fur over an entire face.

`verify_master_geometry.py --pack PACK --previous PREVIOUS_PACK --output REPORT`
checks protected geometry and winding, all 243,875 triangle/pose combinations,
and the shared native head/eye transform. It verifies the exported game geometry,
including integer vertex quantization. The sculpt tool separately measures eye
seating in Blender at 180 surface samples. Neither check establishes art quality.

The full animation and diagnostic reel is in `workspace/sculpt-regions-motion-v3/`.
This is an intermediate asset: the native paws, claws and shoulder silhouette
remain visibly angular, atlas boundaries need painted transitions, and the
original tongue is still present. White Wolfos and rank variants remain pending.
Preserve the accepted native animation while improving these details in the saved
master. Current appearance has not received human approval.

## Curved front claws

Continue from **`workspace/wolfos-curved-claws-v1.blend`**, exported as
`workspace/export/wolfos-curved-claws-v1.o2r`. The original brown Wolfos has two
large claw wedges on each hand. `refine_front_claws.py` reads each connected
component from the saved master and uses its root pair, knuckle and tip as
landmarks for a closed, curved shell. Eleven elliptical cross sections taper
to the original tip; the existing keratin painting follows each claw's length.
This retains the native two-claw design and reach while replacing flat facets.

The master contains 2,975 triangles. Twelve native root/tip anchors are preserved
after integer export. All other positions, UVs, material assignments and joint
assignments remain unchanged, including the 108 original joint connections.
All paint resources are byte-identical to the previous pack. Re-saving custom
normals rounds thirteen unchanged corners differently by at most one signed
normal unit; those are the only incidental changes outside the claws.

Use `verify_master_geometry.py --edited-limbs 15 21 --refined-claws` with the
previous regional-coat pack to check native anchors, closed claw shells, UV area,
and all 371,875 triangle/pose combinations. `preview_paws.py` produces isolated
right/left assemblies and hind paws in waiting, slash and block poses. Final
detail renders are in `workspace/curved-claws-detail-v1/`; the full animation and
eye/head diagnostics are in `workspace/curved-claws-motion-v1/`.

A denser paw-smoothing experiment (`workspace/wolfos-paws-claws-uv-v1.blend`)
was not promoted: it added several thousand triangles without adequately
improving the paw silhouette. The current master retains the prior paws.
`refine_head_torso.py --limbs ...` and `author_coat_regions.py --limbs ...` support
selective further experiments without repainting unrelated regions. Continue
with deliberate paw/toe, fang, muzzle and shoulder shaping and painted seam
transitions; the complete character and white/rank variations remain unfinished.

## Master-to-game export

Run `prepare_master.py` once after importing a workspace. It copies the editable
mesh's paint maps so painting cannot alter the locked reference and records each
material's native texture and wrapping template. Re-running it reuses prepared
images and does not reset mesh edits.

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup \
  --python-exit-code 1 --python art/arpg/wolfos/prepare_master.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup \
  --python-exit-code 1 --python art/arpg/wolfos/export_master.py
```

The exporter reads the saved `.blend` and writes `workspace/export/wolfos-master.o2r`.
Choose a fresh `-- --output /path/version.o2r` for each revision; existing packs
are not overwritten. Only objects marked `source_kind = editable master` export.
The initial workspace exports the brown species; white remains a reference until
an independent editable copy is made. Every visible limb of an edited species
must be included.

Mesh positions, split normals, UVs, face limb assignments and bone groups come
from Blender. Non-armature modifiers are evaluated. The native renderer requires
one bone per vertex but supports triangles spanning multiple bones, including
the original joint seams. Blended weights and changes to the native rest skeleton
fail explicitly before writing a pack. Keep the `native_limb` face attribute when
editing topology. Do not apply the armature modifier.

Paint the `Paint / ...` images and pack/save them in the master. Higher-resolution
images retain their original logical tile dimensions and UV placement; RGBA32
resource metadata supplies the additional pixels. Open, half and narrow eye maps
keep the actor's native blink selection. Export uses the original material
lighting/combiner templates, with `native_texture` and `native_tile` as material
metadata. Blender PBR shader changes are preview-only. New material templates
need explicit exporter support; they are not silently converted.

The `.o2r` contains alternate native display lists and textures plus private
vertex resources. `WolfosMasterAssets.h` lets a mounted master pack take priority
for its species while other ARPG enemy remasters remain enabled. Disabling
alternate assets restores the procedural fallback. No normal profile has had the
experimental pack installed.

`launch_master_preview.py --executable /path/to/wolfos-qa --config /path/to/qa-window-template.json`
creates a fresh temporary profile and preview app; `--mode combat` runs the
native combat/teardown checks. It requires an executable built with
`wolfos_models_runtime_test.cpp`. The comparison camera holds the body still but
cycles the native eye states slowly enough to inspect.

`python3 tests/arpg/wolfos_master_pipeline_test.py` verifies a baseline round trip,
then edits a vertex, a UV and a doubled-resolution paint map in disposable copies.
It verifies those edits reach the pack, locked reference images stay unchanged,
and unsupported blended weights cannot produce an archive. Original-derived
packs and working files remain local in the ignored workspace.

The baseline pack passes 32 native checks, including pack priority and fallback,
all 19 brown limbs and eight texture resources, native combat, blinking, damage,
fire attachment points, death and scene cleanup. Seven round-trip checks verify
saved authoring edits. See
[`wolfos-master-export-verification-macos.json`](../../../tests/arpg/wolfos-master-export-verification-macos.json)
for exact files, hashes and remaining limits. These tests establish a working
pipeline; the art pass is still pending.

## Existing procedural replacement

These meshes replace all 19 visible native Wolfos limbs for both brown and white
Wolfos. They include a tapered canine skull and muzzle, inset eyes and ears,
separate teeth and jaw, painted fur, modeled claws, and champion/elite fittings.
The game's skeleton, attack colliders, blink timing, head tracking, damage,
fire attachments and death behavior remain native.

- `wolfos.blend`: standalone editable meshes, native reference pose, studio lights.
- `wolfos.glb`: runtime export, with semantic materials and joint/rank/blink metadata.
- `build_wolfos.py`: editable mesh and modifier recipe. Run in a separate
  `blender --background --factory-startup --python ...` process.
- `native_pose.json` and `native_links.json`: native rig reference transforms in
  one-hundredth model units, matching the checked-in bind fixture and WolfosRig.h.
- `export_runtime.py`: standard-library compiler that writes WolfosBlenderData.h.
- `fur-albedo.png`: authored grayscale fur map generated with built-in imagegen;
  the exact prompt is in `fur-prompt.txt`.
- `pack_texture.py`: resamples and encodes the map at 512×512. Requires Pillow.
  `WolfosTexture.h` registers the pixels with the game's resource manager, so
  neither a separate texture pack nor Blender is needed to play.
- `package_blend.py`: creates the standalone Blender file and studio PNG from the
  GLB in a separate `--background --factory-startup` Blender process.

Edit mesh vertices in their limb's local coordinates. Object transforms only
arrange the reference pose and two species for inspection; runtime ignores them.
Apply mesh changes before exporting. Keep all mesh objects with `arpg_limb`,
including the hidden alternate blink shapes, in the GLB. Use the active scene and
selected meshes, evaluated modifiers and custom extras. Do not export cameras or
lights. Run `python3 art/arpg/wolfos/export_runtime.py` from the repository.

The 17 flex bridges have a ring assigned to each adjoining native joint. Their
metadata records each ring's source positions and the native parent transform;
the compiler converts the parent ring into parent-local space. If a bridge is
edited, regenerate its ring metadata in `build_wolfos.py` too. An unassigned
vertex fails export instead of silently attaching to the wrong bone. Other
pieces remain rigid on their native limb. The Blender document is a posed mesh
workshop, not a replacement animation rig.

`arpg_rank` is the minimum rank (0 ordinary, 1 champion, 2 elite). `arpg_blink`
is -1 for always visible, otherwise 0 open, 1 half, 2 closed. Only one blink shape
renders at a time. Runtime palettes distinguish species and ranks; the studio
preview shows both species with all rank fittings under studio lighting.

Portable validation checks all 126 limb/blink/rank/species variants, integer
triangle area, unit normals, native bounds, matrix assignments, every flex
connection and the per-actor triangle budget. Native runtime tests cover combat,
blinks, head tracking, fire attachments, damage, death and scene teardown. The
normal player save is not used by the QA fixtures.

The coat now retains authored per-corner UVs and split seams through GLB and the
native vertex exporter. Its 512px texture uses a 32-texel logical tile; the port's
high-resolution image metadata supplies the full pixels. The combiner adds the
texture detail to a species-colored base before applying original actor lighting
and fog. This avoids the first texture pass's excessively dark coat. Non-coat
materials still use the existing native material renderer.

The October 13 quality revision removes dense raised coat chunks, smooths facial
contours and reduces ear height. Both front paws previously used the same
unmirrored anatomy. Right-hand geometry now mirrors Z to match the native left /
right convention, and both wrists bend down 18 degrees. Palm pads remain beneath
the furred backs; the original wrist matrices and attack colliders remain intact.
The portable test checks opposite thumb handedness and pad/back ordering in the
native waiting pose. The current GLB reports 24,990 triangles for normal actors
and 25,542 for elites after the face-attachment tessellation pass.

`native-in-game.png` and `wolfos-in-game.png` show the original and revised models
from an earlier comparison fixture. The saved 27-check native run covers an
earlier revision's texture resource, combat, blink state, damage, burn, death and
teardown; it does not validate the newest attachment geometry or pending eye fix.
These are engineering checks, not a claim that art quality is final. Other enemy
families have not received this authored-texture correction. Face readability,
style consistency across scenes and crowded-scene rendering cost still need
further visual review. Studio metallic/roughness values are preview-only.
