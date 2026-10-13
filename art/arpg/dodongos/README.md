# Native Dodongo upgrade

The original `ACTOR_EN_DODONGO` now uses an original Blender-authored broad snout, recessed amber eyes, interlocking teeth, independent lower jaw, overlapping dorsal shields, tapered three-part tail, crouched legs, and splayed claws. Twelve native visible limbs carry 7,491 triangles. The existing skeleton, attack controller, collision shapes and attachment points remain authoritative.

The dedicated renderer uses semantic Blender material IDs. Original Dodongo skin textures add painted detail to the authored colors; eyes, mouth and horn use lit pigment. Native scene light, fog, damage flashes, freeze effects, tail blur, mouth position, footsteps and draw-time body swelling remain intact. ARPG/remastered-enemy off gates and the distant fallback return original limbs.

All 405 frames from thirteen native animations are retained in the editable Blender source. The validator checks 2,835 tail/leg joint anchors, posed bounds and eighteen native bomb-swelling scale combinations. The maximum authored envelope extension is 9.02 world units, chiefly protective head details; this does not enlarge hitboxes. Native animations are referenced from the user's local archive and are not a separate distributed dependency.

Bomb swallowing also fixes an existing native error: Bombchus were cast as bomb flowers, writing their fuse outside the Bombchu allocation. The handler now uses the actual explosive type and only accepts native bombs, bomb flowers and Bombchus. Captured Bombchus use the existing actor freeze timer to suspend crawling/collision until the reverse-mouth animation consumes them. Other Bombchu behavior is unchanged.

Rebuild workflow:

1. Run `python3 art/arpg/dodongos/extract_native_rig.py` with the local `build-cmake/soh/oot.o2r` reference available.
2. Run `author_blender.py` in an isolated Blender `--background --factory-startup` process.
3. Run `export_runtime.py` and `validate_animation.py` with Python.
4. Run `package_blend.py` in a separate factory-startup Blender process to save the editable `.blend` and studio preview.
5. Open that saved file with background Blender and run `preview_motion.py` for selected native poses.

The GLB stores joint-local vertices and semantic material IDs. Preview transforms are omitted by the runtime exporter; the game applies the original animated skeleton once. The live Blender connection was unavailable, so authoring used the installed Blender executable without opening or saving an unrelated project.

Practice: **More → Arsenal & enemy counters → Practice encounters → Upgraded Dodongos**. Placement requires a broad, clear, nearly level footprint with headroom and limits active Dodongos to four. The journal stays paused until resumed.
