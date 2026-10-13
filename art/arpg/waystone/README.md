# Oath shrine and reward coffer

Original Blender-authored world assets for waystone expeditions. The cut stone
pylons, bronze astrolabe, suspended crystal, five heraldic crests, three wave
lenses and walnut coffer replace the earlier rounded landmark. The coffer has
separate base, hinged lid and unclaimed contents.

`waystone.blend` is the standalone editable source. `build_waystone.py` rebuilds
only `ARPG Waystone Workshop` through local Blender MCP or Blender's Text Editor.
`waystone.glb` is the evaluated interchange asset. `export_runtime.py` compiles it
with standard Python; `package_blend.py` saves the standalone source and studio
preview in a separate `--background --factory-startup` Blender process.

Meshes carry `arpg_part` from 0 through 14: shrine, crystal, three lenses, coffer,
lid, contents, Iron/Wild/Cinder/Winter/Storm crests, ritual ward and separate seal. Export selected meshes
from the active workshop scene with modifiers and extras enabled. Semantic
material names must retain the `Waystone ` prefix. The compiler checks part
coverage, positions, normals, topology after integer quantization and budgets.

Vertices are authored in native X/Y-up/Z-forward space. Studio object transforms
are omitted by the compiler. The crystal's runtime pivot is `(0,36,0)` and the
lid's hinge is `(0,14,15)`; both use their own local mesh coordinates. One authoring
unit becomes 100 integer mesh units and 1.6 game-world units. The four detached
crests in the studio are alternate emblems, not extra world objects.

The selected oath changes the crest and crystal color. Wave lenses become gold
only after their corresponding wave clears; the current wave uses the oath color
and future lenses stay dark. Clearing the third wave opens the lid. Claiming any
cache removes its displayed contents and keeps the lid open. Contents symbolize
the cache choices, not a separate rupee pickup or additional reward.

Presentation derives from the existing expedition save data and cannot grant
loot. An earned cache reappears on safe wilderness ground after reload. Claiming
still uses the expedition journal and its existing atomic capacity checks.
The shrine faces Link when it appears; placement and combat radius rules remain
in the expedition system.

Native rendering uses lit vertex colors. Studio metallic/roughness materials
are preview only. The landmark has no blocking collision. Crowded-scene
performance and coverage beyond the tested wilderness scene remain unmeasured.

The ritual variant places three smaller ward monoliths on independently checked
ground around the shrine. Each uses 972 stone/frame triangles and a separate
128-triangle seal, at 1.8 world units per authored unit. The seal rises with
channel progress, then settles and turns gold when broken. Shrine lenses track
the exact corresponding ward, so wards can be broken in any order. Completing
all three exposes the warden; defeating the guard earns the existing cache.
The fifteen-component pack totals 11,622 triangles before selecting one crest.
