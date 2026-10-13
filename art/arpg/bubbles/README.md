# Native Bubble upgrade

The original flying skull (`ACTOR_EN_BB`) now has an open cranial vault, recessed sockets, spectral pupils, cheek arches, individual teeth, a snapping mandible, and two articulated membrane wings. Blue, red, white, green and great green variants share the native rig with four restrained color palettes. Flame auras, linked trails, white motion blur, shield reactions, stun, vulnerability cycles and native dismemberment remain in the actor's own update/draw code.

Six visible native limbs carry 3,936 triangles. The skull is authored against the tilted native resting bone, rather than assuming that its local X axis is level. The lower jaw retains its independent native transform. Both wing sections share nine checked seam samples across all 28 animation frames: 504 seam checks. The final envelope extends at most 1.18 world units beyond the original normal-size animation envelope. Great green retains the original three-times scale and collision sphere.

Two native integration fixes accompany the art:

- Flame-trail actors (`params >= 0`) are excluded by `EnemyDifficulty::IsEnemyActor`, preventing combat scaling, affixes, loot and XP for visual effects.
- Fire damage initializes its color filter before entering a lethal state. Drawing a one-hit fire kill while updates are halted can no longer use a zero filter duration.

Source workflow:

1. `extract_native_rig.py` reads the local `build-cmake/soh/oot.o2r` reference skeleton and animations.
2. Run `author_blender.py` with Blender in a separate `--background --factory-startup` process.
3. Run `python3 art/arpg/bubbles/export_runtime.py` and `python3 art/arpg/bubbles/validate_animation.py`.
4. Run `package_blend.py` in another isolated Blender process to write `bubbles.blend` and the CPU studio preview.
5. Open the saved `.blend` in background Blender with `preview_motion.py` for motion renders.

The editable `.blend` includes all native poses. `bubbles.glb` stores joint-local geometry and semantic materials; the generated runtime header does not apply authoring-pose transforms. The local native archive is a reference input, not a redistributed runtime dependency.

Practice controls live under **More → Arsenal & enemy counters → Practice encounters → Upgraded Bubbles**. Blue, red and green can be summoned over safe open floor. White and great green remain on their original scene paths; arbitrary practice placement does not assign them an invalid path. The journal remains paused while placing enemies. Normal ARPG/remastered-enemy toggles and the distant fallback restore original mesh lists.
