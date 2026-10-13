# Blender Stalfos

This authored replacement covers all 24 visible native Stalfos limbs for three
ranks. The skull has open orbital rims, recessed eyes, cheek arches, a nasal
aperture and a separate jaw. Long bones, paired forearm/shin bones, continuous
ribs, vertebral processes, skeletal fingers/toes, layered shoulder armor and
forged sword/shield surfaces replace the previous rounded assemblies.

- `stalfos.blend`: standalone editable limb meshes in the native guard pose,
  organized by rank and joint, with studio camera and lights.
- `stalfos.glb`: evaluated meshes, semantic materials and native limb/rank extras.
- `build_stalfos.py`: the authoring recipe, executed through local Blender MCP or
  Blender's Text Editor. Rebuilds only `ARPG Stalfos Workshop`.
- `native_pose.json`: frame-zero transforms from the native MiddleGuard animation,
  in one-hundredth model units. These arrange the workshop; runtime animation
  comes from the original skeleton.
- `export_runtime.py`: standard-library GLB compiler for StalfosBlenderData.h.
- `package_blend.py`: packages the GLB and renders the PNG in a separate Blender
  `--background --factory-startup` process, leaving the live project intact.

Edit mesh vertices in their local joint space, keeping object transforms intact.
Export only meshes carrying `arpg_limb`, from the active workshop scene, with
modifiers applied and custom extras enabled. `arpg_rank` selects exactly one of
0 (ordinary), 1 (champion) or 2 (elite). Materials use the semantic names in the
compiler. Run `python3 art/arpg/stalfos/export_runtime.py` from the repository to
update the runtime header. Export rejects missing limbs, unrelated objects,
nonfinite coordinates, unexpected metadata and excessive triangle counts.

All native limb transforms and rigid draw behavior are retained. The sword
terminates at native X=3400 for its original trail. Display lists stay cached
independently of actor lifetime, including detached BodyBreak parts. The native
shield collider, Lens gate, damage flash and ice attachments are preserved.

Portable tests cover all 72 limb/rank combinations, integer-safe triangle area,
unit normals, coordinate bounds and the native sword tip. Native tests exercise
animation, attacks, shield motion, Lens visibility, damage, freezing, defeat,
break-apart rendering and scene teardown. All fixtures use an isolated testing
profile rather than the normal player save.

Runtime rendering uses native lit vertex colors and fog. Studio metal/roughness
settings are Blender preview only. Custom skeleton packs, replacement animation,
PBR maps, broad scene/age coverage and crowded-scene performance guarantees are
outside this asset pass.
