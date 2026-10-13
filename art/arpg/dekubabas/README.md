# Native Deku Baba upgrade

Original Blender-authored art for the existing normal and Big Deku Baba. Work is
isolated in the **ARPG DekuBaba Workshop** scene; the unrelated open Blender
project is never overwritten. The standalone source is packaged in a separate
background Blender process.

The flower has a flared blue cap, recessed palate, hooked interlocking teeth,
raised mouth rim and surface-fitted ivory markings. Its three slender stem
sections carry growth fibers and small thorns. Curved root leaves have raised
veins, and the severed plant becomes a matching woody stalk pickup. Big plants
keep their native 2.5× size with an indigo palette and additional cap markings.
Normal/Big exports contain 6,512 / 6,632 triangles, including the separate stick.

- `dekubabas.blend`: standalone editable meshes with the native jaw animation
  frames on an illustrative rooted stem preview.
- `dekubabas.glb`: semantic materials and native part metadata, exported in
  joint-local space. Object transforms are used only for the studio preview.
- `author_blender.py`: generates the isolated workshop scene. MCP safe mode uses
  inline rig JSON and a literal export path. Cap markings are projected onto the
  actual sculpted surface to avoid clipping.
- `extract_native_rig.py`: reads the local extracted game archive for both jaw
  animations, reference bounds and the separately drawn stem/leaf/stick pieces.
  No ROM is included.
- `export_runtime.py`: quantizes vertices and normals into `DekuBabaBlenderData.h`.
- `validate_animation.py`: checks head bounds through all 140 native jaw frames,
  exported stem endpoints, and studio stem continuity. Runtime growth and stem
  movement are checked separately in the game harness.
- `package_blend.py`: saves the standalone editable file and studio render.

Native visible jaw limbs are 4 and 7. Renderer part IDs 8–10 address the three
stem sections, 11 the rooted leaves, and 12 the collectible stalk. All draw paths
use the original actor matrices, including retraction and pruning. The plant's
AI, seven collision spheres, attacks, damage rules, root attachment, stun,
severing, fire, shrinking, stick collection and rewards remain native.

The shared renderer applies painted surface detail with a lighter texture tint
on the flower, palate and teeth. The native scene still supplies lighting, fog
and damage flashes. Disabling ARPG/remastered enemies or exceeding the existing
1,500-unit camera distance restores all original parts.

Temporary adventures expose **More → Arsenal & enemy counters → Practice
encounters → Upgraded Deku Babas**. Choose normal or Big, summon, then resume and
approach. The summon requires clear ground and caps the practice population.
