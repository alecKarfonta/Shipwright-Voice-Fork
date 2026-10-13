# Spirit relay

A third seeded expedition objective: defeat three spirit bearers, gather one spirit at a time, and carry them to three lanterns before defeating the protected warden. Moving and attacking remain available while carrying. Actual health loss drops the spirit, with a brief recovery delay; unattended spirits return to the supported shrine approach after thirty seconds. Delivery requires entering an unlit lantern's radius and consumes exactly one released spirit. No menu or stationary channel is needed.

The three route offers now include gauntlet, ritual and relay once each. Existing ritual offer placement remains stable. Save section version 9 reads versions 1–8; in-progress encounters retry from the route, while earned caches survive reload and transfer once. Spirit positions and live actor pointers are runtime-only and reset with the encounter.

## Original assets

- `author_blender.py`: creates only the `ARPG Spirit Relay` scene through the local Blender MCP. Native mesh space is Y-up; studio object transforms do not enter runtime geometry.
- `relay.glb`: three semantic meshes: lantern cage (896 triangles), floating beacon (64), winged spirit (136).
- `relay.blend`: isolated editable project, packaged from the GLB without saving over the user's live Blender file.
- `package_blend.py`: standalone project and review render generation.
- `export_runtime.py`: dependency-free GLB validation and runtime data compiler. Three semantic parts, 1,096 triangles total; no collapsed triangles.
- `RelayGeometry.h`: native material palette with dark, cyan and lit-gold beacon states.

Native rendering keeps the cage still, turns the beacon at its own pivot, floats loose spirits, and makes the carried spirit orbit Link. Terrain-checked lantern sites share the expedition's safe placement checks. Invalid drop terrain returns the spirit to the waystone. Inventory paintings are unrelated to these world-only models.

In a temporary testing adventure: More → Settings & testing → Testing shortcuts → Oath shrine preview → Test spirit relay. Choose any of the five oaths first to try its different warden attack pattern with the relay.
