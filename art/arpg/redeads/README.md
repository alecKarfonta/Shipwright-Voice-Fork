# Native ReDead and Gibdo upgrade

The original `ACTOR_EN_RD` uses two distinct Blender-authored models on its native 26-joint flex skeleton. ReDeads have lean anatomy, crooked fingers, individual toes and a recessed carved scream mask. Gibdos have broader proportions, continuous helical wrapping strips, raised cloth seams, crossed burial sashes and a wrapped face. The runtime meshes contain 5,236 and 14,512 triangles respectively, distributed across the fifteen original visible joints.

Nine mesh connections per variant join the waist, shoulders, elbows, wrists and knees. Each end is assigned to its native joint in the runtime flex matrix palette; the renderer restores the current limb matrix after mixed-joint batches. These connections deform with the original animations rather than leaving detached limb pieces. Hips, ankles and neck use overlapping forms. This is rigid native-joint attachment, not cloth simulation or a fully watertight skinned character.

The dedicated renderer applies original muscle and linen textures to semantic material IDs, preserves scene lights and fog, and uses native environment alpha for the death fade. Opaque and fading models are cached separately, without allocating another model for each alpha value. Feature gates and a 1,500-unit camera-distance fallback restore original geometry. The original collision cylinder, fear, footsteps, pursuit, grab, damage rules, Sun's Song, fire attachment points, mourning, coffin rise and death controller remain unchanged.

Both variants retain all eleven native animations (277 frames each) in the editable Blender file. The validator checks exported vertex joint assignments, quantized bridge endpoints through all poses, finite coordinates and posed bounds. It measures a maximum 3.46-world-unit extension beyond the native posed envelope and under 0.008-world-unit endpoint quantization error. It does not enlarge collision shapes. Native animation reference data comes from the user's local archive.

Rebuild:

1. `python3 art/arpg/redeads/extract_native_rig.py redead`, then repeat with `gibdo`.
2. Run `author_blender.py` with Blender `--background --factory-startup`.
3. Run `export_runtime.py` and `validate_animation.py` with Python.
4. Run `package_blend.py` in a separate factory-startup Blender process to save the weighted, animated `.blend` and studio preview.
5. Run `preview_motion.py` in background Blender for selected death, grab, crouch, standing and walk poses.

The GLB contains joint-local geometry, preview transforms and explicit connection metadata. The runtime exporter ignores preview transforms and converts parent bridge endpoints back into their own native local spaces, avoiding double transforms. Authoring used the installed Blender executable because the live MCP connection was unavailable; no unrelated open project was modified.

Practice: **More → Arsenal & enemy counters → Practice encounters → Upgraded ReDeads & Gibdos**. The collapsed section offers ReDead, crouching ReDead, Gibdo and rising Gibdo. Placement checks floor, sightline, lateral clearance and headroom, and limits active undead to four. Resume the journal, approach and walk to wake them; idle ReDeads respond to Link's footsteps.
