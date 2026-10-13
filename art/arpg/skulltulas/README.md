# Native Skulltula upgrade

Original hanging Skulltulas (`ACTOR_EN_ST`, normal, big and invisible) receive a skull shield, recessed sockets, separate moving mandibles, exposed rust-colored rear abdomen and eight tapered legs. Wall/gold Skulltulas (`ACTOR_EN_SW`) are outside this pass.

The original 29-limb skeleton remains authoritative. Ten visible limbs carry 4,067 triangles. Leg bends are sculpted into the original rigid leg meshes; their native root rotations drive motion. The upgrade does not invent knee animation or change native AI, collisions, drop/retreat timing, armor, stun, death, blur, Lens flags or rewards. The native tooth-color warning drives the new mandible through a cached material, with no per-frame mesh generation.

Authoring coordinates are native units divided by 100. The native actor scale is 0.04 (one authoring unit = four world units), or 0.056 for the large variant. All 111 native frames are previewed in the editable Blender source. Geometry stays within 0.76 world units of the original whole-body animation envelope; 888 leg-root checks verify the original attachments.

Files:

- `skulltulas.blend`: editable source, native animation preview, CPU studio setup.
- `author_blender.py`: independent geometry construction in its own scene.
- `skulltulas.glb`: joint-local export with limb metadata.
- `export_runtime.py`: indexed C++ export, normal validation and bounded triangle budget.
- `extract_native_rig.py`: extract local native skeleton/animation reference from `build-cmake/soh/oot.o2r`.
- `validate_animation.py`: whole-pose envelopes and leg attachments.
- `package_blend.py` and `preview_motion.py`: isolated Blender source packaging and CPU renders.

Run the authoring script with the installed Blender in a separate `--background --factory-startup` process, then run `python3 art/arpg/skulltulas/export_runtime.py`, `python3 art/arpg/skulltulas/validate_animation.py`, and the packaging script. The native archive is a local reference input, not a redistributed requirement for runtime geometry.

In the journal: **More → Arsenal & enemy counters → Practice encounters → Upgraded Skulltulas**. Face a clear room beneath a ceiling. The practice placement requires safe floor, ceiling and approach clearance, avoids nearby enemies, and limits the scene to eight Skulltulas. The invisible variant requires the Lens of Truth. ARPG/remastered-enemy toggles and the distant-model fallback restore original geometry.
