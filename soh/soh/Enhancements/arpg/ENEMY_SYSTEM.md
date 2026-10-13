# Regional enemy system

The requested outcome is a Diablo-inspired encounter loop: larger packs, leveled regions, randomized enemies, champion/elite groups, level-appropriate drops, and more challenging combat AI.

Acceptance requirements:

- Fixed region level bands across wilderness and dungeons, with settlement activity rules and separate boss scaling; show the active region and levels in the HUD.
- Region-wide wilderness patrols and expanded dungeon combat groups, varied by biome, with a bounded live population, validated ground/line of sight, no water/pit/exit spawns, and cleanup on region changes or disabling the feature.
- Normal enemies get rolled level/HP/damage/speed; blue champion groups and named elite leaders with guards get stronger stats, visible labels, and working affixes.
- Kills award level/tier-scaled XP and physical loot; elite leaders guarantee better gear. Item level affects power, is inspectable, and persists with older-save migration.
- Native melee enemies use purposeful pressure, flanking, defensive reactions, attack coordination, and recovery while retaining telegraphs, hit reactions, and collision checks. Behavior must be observable in gameplay, not only a stats multiplier.
- The testing panel can spawn/clear packs, pick encounter tiers, and quickly visit representative regions. Settings expose density and aggression.
- Portable tests cover rules and save compatibility; native checks cover actual spawns, scaling, rewards, tactics, cleanup, and toggles; inspect packs in the running game and verify combat behavior. Keep ordinary saves intact.

Implementation is complete for this encounter slice. The evidence and remaining playtesting limits are recorded below.

## Controls and behavior

ARPG, Monster Affixes, regional enemies, region populations, and tactical behavior default on in new normal and testing profiles. Existing explicit opt-outs remain respected; the current player and preview profiles have been enabled. Open **Skills & testing → Regional enemies & encounters** to change density (50–150%), the wilderness safety ceiling (4–96, default 64), the dungeon/test enemy limit (4–20, default 18), and aggression (50–150%). Wilderness population targets derive from usable floor area; the ceiling is not a fixed population.

In the temporary test adventure, **Testing shortcuts** offers a warband, champion pack, or elite leader with guards; clear added enemies; or visit Hyrule Field, Death Mountain Trail, and Desert Colossus without cinematics. Ordinary save slots do not expose these testing mutations.

| Region example | Child level | Adult level | Summoned pack / dungeon group |
| --- | --- | --- | --- |
| Hyrule Field | 4–7 | 12–15 | 5–8 |
| Lost Woods | 3–6 | 10–13 | 4–6 |
| Death Mountain Trail | 8–11 | 20–23 | 4–7 |
| Lake Hylia | 8–11 | 18–21 | 4–7 |
| Desert Colossus | 24–27 | 34–37 | 5–8 |
| Forest Temple | 18–22 | 18–22 | Expanded existing combat groups |
| Ganon's Castle | 40–44 | 40–44 | Expanded existing combat groups |

The complete table is in `EnemyRegions.h`. Champions add one level, elite leaders three, and bosses four, capped at 50. These are fixed region bands, so leveling does not make every region scale alongside Link. The HUD shows the current region band; targeting a configured enemy shows its level, health, tactical state, affix names, and rolled stat multipliers. Compact overhead labels identify level and champion tier or the elite’s name.

Wilderness patrols roll 55% warband, 25% champion pack, and 20% elite with guards. The first three members of a champion pack are champions; an elite pack has one named leader. Forests favor Wolfos, plains mix adult Stalfos, mountains mix Dinolfos and Tektites, water regions combine White Wolfos and Blue Tektites, and desert/fortress regions mix Dinolfos with Stalfos. Crypts use Stalfos. The remaining enemies use their native combat logic with regional stats.

Wilderness populations are placed across the region as soon as Link gains control after entry. The director samples safe ground across scene collision bounds, with pairs and three-member groups spread across scene-exit corridors and validated floor candidates instead of spawning around Link. Enemies remain present beyond the former 2,400-unit cleanup radius and remain drawable across the region (subject to scene fog and terrain). Generated Wolfos and Stalfos emerge independently of Link's proximity. Curated melee enemies roam around their home positions until Link or a watch patrol comes within engagement range. The area-based target and performance ceiling bound the region; existing actor occupancy can reduce the available budget. Cleared patrols stay cleared for that scene visit; moving around does not generate replacement waves.

Wilderness monster populations cover configured regions independently of human patrols. Hyrule Field prioritizes the ranch and castle approaches. Actual floor polygons supplement the terrain grid so narrow paths and islands can host encounters. Streamed areas use the loaded room and fixed entry/inhabitant positions, rather than following Link's later movement.

Added Hylian watchmen are restricted to Hyrule Field and Lon Lon Ranch in both ages, plus the child-era castle and castle-town gate, alleys and market. The field, ranch and child castle support border skirmishes. Other cultures, wilderness regions, Kakariko, temples and interiors retain their authored inhabitants without generated Hylian soldiers. Regional names do not stand in for appropriate NPC models. New civilian schedules and region-specific combatants remain unimplemented.

Native Hylian guards walk patrols, approach monsters, and perform spear attacks through native enemy damage colliders. Monsters choose a closer watchman over Link and retaliate through the guards' own damage colliders. Overwhelmed watchmen retreat and recover before rejoining combat. These are additional patrol NPCs; authored story NPCs retain their behavior. Native Wolfos, Tektites, ordinary ground Stalfos and Dinolfos in applicable regions also join the active population. Turning the feature off restores their prior culling configuration instead of deleting them.

Private homes, shops, fairy fountains, minigames, cinematic maps, scripted stealth rooms, collapse sequences and boss arenas retain their authored population. No watch allies are added inside dungeons to complete combat puzzles for Link.

Unassisted patrol kills grant Link no ARPG XP, equipment, materials, or on-kill healing. Damaging an enemy makes it eligible for the usual rewards if the watch helps finish it. Guard attacks do not use Link's equipment or skill bonuses. Disabling the population feature removes both added monsters and watchmen, and enabling it again rebuilds the population. Scene transitions discard the encounter state. Expeditions retain their separate wave and reward rules.

Dungeon combat rooms populate once on room entry, using the home positions of existing eligible enemies as anchors, without the former 650-unit proximity requirement or line of sight to Link. Multiple separated combat groups can expand up to the shared cap. Empty puzzle rooms, cleared rooms and scripted Lizalfos miniboss pairs are excluded. Test-pack and expedition spawns retain their explicit nearby placement.

Ground checks allow ordinary soft sand and reject steep slopes, dynamic platforms, lava, deep quicksand, void floors, water, scene exits, blocked lines of sight, nearby NPCs/doors, and crowded spawn positions. The additional population limit is also constrained by a 24-enemy room total and 180-actor safety threshold. Generated Wolfos use switch flag `0xFF`; pack membership never uses actor parents, avoiding quest-switch and parent-cast side effects. Actor destruction frees all pack and AI state through ObjectExtension.

## Combat and rewards

Regional rolls vary health, outgoing damage, and supported movement speed. Affixes add their existing resistances, immunities, weaknesses, and bonuses. Health uses EnemyDifficulty's shared helper and existing limits; combined movement remains capped. Manual-health bosses retain their documented restrictions.

Wolfos, ordinary Stalfos, and Dinolfos receive the tactical controller only in neutral movement/decision states. Vanguard, flanker, and guard roles close on ranged attackers, circle toward shield openings, separate from nearby allies, and briefly retreat when injured. Defensive reactions have a six-frame observation delay and a cooldown. New coordinated attacks reserve at most two attack slots with an 18-frame stagger; native follow-up combos remain native. Aggression changes recovery between attacks. Native attack animations, collisions, stun, damage, and death states remain in charge after commitment. Movement probes reject unsupported ground and obstacles. This is local steering, not a full navigation mesh; enemies do not solve paths through rooms or around long walls.

XP scales with enemy level, then champion/elite bonuses. Normal enemies have modest gear chances; champions have higher chances; elite leaders and bosses guarantee Rare-or-better physical gear. Item level records the source enemy's level and scales future modifier rolls within the existing power cap. Version 4 saves preserve item level; old gear remains unchanged and is labeled legacy. Pickup, equipment, and full-backpack behavior continue through the inventory system.

## Verification record

Verified on macOS arm64, 2026-10-11:

| Requirement | Evidence |
| --- | --- |
| Leveled regions | Portable child/adult/safe/boss region assertions; native Hyrule Field, Desert Colossus, and Deku Tree transitions; region HUD visually inspected. |
| Larger, bounded packs | Native champion and elite packs spawned on real collision; population cap rejects further packs; dungeon room expansion passed; disable/clear/scene cleanup passed. |
| Random stats and tiers | 10,000 deterministic stat-roll cases; live champion counts, level ranges, HP composition, and parent/switch-safe generated actors checked. |
| Level-appropriate loot and XP | Native elite defeat granted XP and created physical Rare-or-better loot; item level passed the save codec; full SaveManager snapshot/reload and inventory tests passed. |
| Combat AI | Live Wolfos packs moved and committed attacks; adult Dinolfos/Stalfos desert encounter committed native attacks; disabled tactics returned native control. Portable decisions cover shield flanking, ranged pressure, evasion, injury recovery, attack slots, and leash. Combat and compact labels inspected in the running app. |
| Testing controls | Native UI inspection confirmed the testing panel. Spawn-pack created eight enemies, clear removed them, refill restored health, and the region shortcut returned to the Hyrule Field entrance. Density, aggression, and cap controls are in the configuration panel. |
| Regression and saves | All 79 native checks passed; portable suite passed normally and under AddressSanitizer/UndefinedBehaviorSanitizer; normal player1 save hash unchanged. |

Native evidence: `tests/arpg/runtime-results-enemies-macos.json`. The QA adventure uses `/tmp/soh-arpg-qa-enemies-20261011`; the ordinary player1 save was never loaded by the harness. Its SHA-256 remains `ff60ad4ddfdd6b78e2233d453b067ee6e20cf3eb4d28f4c74bf28c003cbed309`.

These checks cover representative wilderness and dungeon encounters, not every room or a complete boss playthrough. Boss choreography and non-curated enemy AI stay native. Long-wall pathfinding and network replication remain outside this local encounter implementation; see the main README for existing multiplayer and manual-health-boss limitations.


### Living world update — 2026-10-12

All eight portable suites and fifteen dedicated native living-world checks pass on macOS arm64. The native harness verifies multiple stationary-entry populations, enemies beyond 2,400 units completing emergence, both ranch/castle watch posts, automatic spear attacks, real damage in both directions while Link remains elsewhere, player-participation reward eligibility, persistence, no idle replacement waves, toggle cleanup/rebuild, and a transition to Desert Colossus. Evidence: [`living-world-results-macos.json`](../../../../tests/arpg/living-world-results-macos.json).

The harness requires `SOH_ARPG_RUNTIME_TESTS=ON`, `SOH_ARPG_WORLD_QA=1`, and an isolated `SHIP_HOME` beginning with `soh-arpg-qa-`. `SOH_ARPG_WORLD_PREVIEW=1` holds a remote camera over the ranch watch for visual inspection. The ranch skirmish was also visually inspected through a remote camera while Link remained at the castle approach: watchmen and enemies moved and attacked, Wolfos fell, and a surviving Stalfos continued fighting. The production build disables runtime harnesses. These checks cover Hyrule Field and Desert Colossus; placement across every terrain/room combination and extended combat balance remain manual playtesting work. Scene populations are bounded encounter groups, not a persistent simulation saved between visits. This first verification record predates the broader world rollout below.

### Broader world rollout — 2026-10-12

The expansion is enabled through the existing ARPG, affix, regional-enemy and population switches. All nine portable suites pass, including world activity policy checks for both ages and protected scenes. The native macOS arm64 sweep passed 33 scene/age visits across every configured wilderness scene, representative settlements and inhabited interiors, three dungeon entrances, and protected interiors. Each visit holds Link at the entry for 120 controllable frames and checks the actual loaded age, population/watch caps and protected-scene exclusions. Both ranch ages and the child castle exterior produced two-monster conflicts with four watchmen. The small guardhouse also received its single sentry. The sweep completed in two batches of 26 and seven visits. Evidence: [`world-population-sweep-macos.json`](../../../../tests/arpg/world-population-sweep-macos.json).

All 19 native combat/lifecycle checks pass, retaining the earlier distant-emergence, real two-way NPC damage, persistence, rewards and toggle checks, and adding native-resident adoption/restoration checks. Disabling populations preserves the original enemy actor and restores its previous culling flags and distance. Evidence: [`world-population-combat-macos.json`](../../../../tests/arpg/world-population-combat-macos.json).

The sweep requires `SOH_ARPG_RUNTIME_TESTS=ON`, `SOH_ARPG_WORLD_SWEEP=1`, and an isolated `SHIP_HOME` beginning with `soh-arpg-qa-`. `SOH_ARPG_WORLD_START` selects a zero-based visit; `SOH_ARPG_WORLD_HOLD=1` holds the first visited scene with a camera on a watch post. QA uses a debug adventure and does not load ordinary saves. The normal executable was rebuilt with runtime harnesses OFF. These are representative entry/room checks, not a full quest playthrough or proof that every terrain variant fits a skirmish; bounded safe placement can leave a settlement peaceful.

The child castle exterior was visually inspected in the native app with a remote camera while Link remained at entry. Added watchmen were visible and animated on validated ground around the gate and stairs. The earlier field skirmish inspection and the native damage checks provide the combat evidence; the castle view was a placement/presentation check.

### Patrol movement correction — 2026-10-12

Idle enemies and watchmen now keep a fixed patrol destination until arrival or obstruction. Candidate routes sample ground and lateral clearance along the entire segment; unavailable routes retry rather than continuously pushing toward a rotating point. Wilderness melee patrols can travel up to 550 units from home, watch patrols 420, and settlement monsters 220. Guard spawn points require lateral clearance. Guard wall checks now use a 40-unit height with a 20-unit radius, avoiding low collision probes that pinned guards against terrain. Guard walk animations follow measured movement after collision resolution.

All 21 native living-world checks passed. The regression captures actual positions, not just animation or requested speed: all four field watchmen moved during the combat interval, nine distant monsters travelled over 100 units, and the four guards travelled 345–551 units after enemies were cleared. Existing collision damage, rewards, persistence, toggle and native-resident restoration checks still passed. Evidence: `tests/arpg/patrol-motion-results-macos.json`. This is local collision-aware patrol routing; long-wall navigation and civilian daily schedules remain outside this system.

### Encounter and watch refinement — 2026-10-12

Generated pack leaders now identify their activity: roaming warbands patrol within 500 units of home, champion hunting packs range up to 650, and elite groups hold a tighter 200-unit area. Settlement patrols remain capped at 220. This varies existing encounters without adding proximity-triggered replacement waves.

Watch partners use opposing approach offsets to spread around monsters, face the opponent during close combat, stagger briefly on hits, and regroup when critically injured. A watchman who is not immediately threatened can approach a wounded partner within 500 units and restore eight health, shortening recovery to at most 60 frames. Assistance has a 180-frame cooldown, requires clear sight and compatible ground height, and stays inside the watchman's home leash. Labels show patrolling, engaging, spear strikes, regrouping and helping an ally. Quest NPCs retain their authored behavior.

All 23 native living-world checks passed, including measured distant travel, actual two-way combat damage, post-combat patrols, a controlled wounded-partner assistance case, reward participation, toggle cleanup and preservation of native residents. Evidence: `tests/arpg/world-refinement-results-macos.json`. The normal executable and living-world preview were rebuilt with runtime harnesses disabled.

### Battle aftermath — 2026-10-12

When a watchman loses contact with monsters, they spend up to 180 gameplay frames securing the last fighting position and looking along the road before returning to patrol. They keep only the position, not a pointer to a potentially destroyed enemy. The approach uses the same ground and clearance validation as patrols; an unreachable site is observed from the current position. New threats interrupt the aftermath immediately, while injury recovery and helping a partner take priority. Lookout duty uses idle animation and a scanning facing direction.

A watchman who observed Link participate in the fight turns toward him and briefly displays “Thanks for the help!” during the final lookout interval if Link is within 300 units. This is an overhead acknowledgment, not a blocking dialogue or reward. Guards also mend two health after each 120 quiet frames; hits and visible nearby threats reset that interval. Regional population limits, player reward eligibility and default-on behavior are unchanged.

All 27 native living-world checks passed in an isolated macOS app, including aftermath completion, resumed movement beyond 100 units, quiet recovery, partner aid, native combat, region transitions and resident restoration. Evidence: `tests/arpg/world-aftermath-results-macos.json`. The playable preview executable was built with runtime harnesses disabled. The conditional thank-you label is implemented but was not directly asserted by this runtime suite.

### Regional identity correction — 2026-10-12

The earlier world-wide watch rollout incorrectly reused Hylian soldiers for labels such as forest wardens, domain watch, and desert expeditions. Those labels did not provide region-appropriate NPC models or behavior. Added Hylian patrols now use an explicit scene/age allowlist: Hyrule Field and Lon Lon Ranch in both ages, plus Hyrule Castle and Castle Town's gate, alleys and market in the child era. No generated Hylian patrols appear in Kokiri Forest, Kakariko, Goron City, Zora's Domain, Gerudo territory, other wilderness, temples, or interiors. Authored NPCs remain intact; this correction does not claim to add new Kokiri/Goron/Zora/Gerudo combatants. Wilderness monster populations continue independently of the watch. Border skirmishes no longer introduce Hylian guards at non-human settlement approaches.

The portable policy regression checks the exact guard allowlist across every scene ID in both ages, including explicit exclusions for the reported Kokiri case. The runtime sweep expectations now require zero generated guards outside the allowlist, and the living-world desert assertion checks that Hylian watchmen do not follow Link there. Previous sweep records document the superseded broad-watch policy.

### Wilderness coverage — 2026-10-12

The default additional-enemy budget increases from 12 to 18 (existing explicit limits are respected). Wilderness placement first considers supported positions along scene-exit-to-center corridors, interleaved across exits, then falls back to the existing floor candidates. All candidates are fixed at region entry, independent of later player movement. Border fights retain their three-member groups; other wilderness encounters use mobile pairs or three-member elite groups, separated by at least 900 units. This spreads the budget across more locations instead of four large clumps. Existing actor limits, safe-floor checks, regional guard allowlist, and no-replacement-wave behavior remain intact. These are geometric travel corridors, not a navigation mesh or authored road paths; this change does not add peaceful civilian schedules.

All 28 native living-world checks passed with the new budget: 18 enemies occupied six of nine broad field sectors, and 13 distant enemies moved more than 100 units while Link remained still. Combat damage, partner aid, aftermath, cleanup, resident restoration and zero added Hylian guards in Desert Colossus also passed. Evidence: `tests/arpg/world-coverage-results-macos.json`. This verifies population coverage and activity, not a complete civilian-life simulation or a performance benchmark.

### Area-scaled populations — 2026-10-12

Wilderness populations now estimate usable area from the projected area of static floor triangles whose centroids pass the existing ground, hazard, water and loaded-room checks. Walls, steep surfaces, unsupported levels and invalid ground do not contribute. The density rule requests one roaming pair per three million square world units at 100%, rounded up. The estimate is conservative around overlapping floors and checks triangle centroids rather than integrating partially hazardous surfaces exactly.

The population target is limited by the separate wilderness safety ceiling (default 64, adjustable 4–96) and available actor slots with a reserve. The old enemy limit only governs dungeon/test encounters. Native enemies also count against a 112-enemy hard limit, and spawning still stops at 180 total actors. These are safety limits, not guarantees that every requested actor can fit on validated ground.

After regional border encounters, each group goes into the largest uncovered gap among the terrain/exit candidates. Density also adjusts minimum group spacing. This replaces the old fixed 18 target and first-candidate filling order. Protected settlements retain their activity rules and region-appropriate guard restrictions; this does not flood civilian towns with enemies. Populations still appear on region entry, without proximity waves or mid-visit refills.

At higher populations, distant noncombat actors retain update activity and extended view distance but can skip drawing when outside the view. Drawing remains forced within 1,000 units of Link or a live watchman so native skeleton callbacks maintain combat hitboxes. Authored draw-culling exemptions are preserved and restored for adopted residents. Runtime damage verification samples health throughout combat rather than relying only on the final snapshot, which can miss damage followed by healing.

Scaling past the original small population exposed native collision table saturation: the original AC table held only 60 colliders, and several enemies register multiple colliders each. Late-registered enemies could therefore stop receiving hits. The port's bounded tables are expanded to 256 AT, 512 AC, and 256 OC entries, with a native regression checking their peak occupancy during a populated field battle. This changes runtime collision-context storage, not save data. The original spear timing is retained; damage remains native collision-based.

All 31 native checks passed. Hyrule Field measured 146,978,780 square world units of accepted floor area, targeted 64 under the safety ceiling, and placed 63 enemies across all nine broad field sectors. Desert Colossus measured 47,330,192, targeted and placed 32. During the field battle, 57 distant enemies moved more than 100 units, native collision usage peaked at 122 AC / 3 AT / 77 OC entries, and watch damage was observed. Recovery, rewards, toggles, native-resident restoration and desert guard exclusions passed. Evidence: `tests/arpg/area-population-results-macos.json`. The preview uses the exact tested executable with QA environment flags absent; its opt-in test harnesses remain inactive in normal play. Build configuration is restored to disable runtime test compilation on subsequent normal builds.

### Population performance safeguards — 2026-10-12

Route planning shares an eight-attempt budget per simulation tick across enemies and watchmen, with at most two attempts per actor per tick. Unfinished searches resume their direction and radius sequence later; completed searches that find no route back off for 20 ticks. Existing patrol movement, animation and native combat still update every tick. The budget resets with the scene and never accumulates unused work, limiting synchronized route-search spikes without proximity spawning or freezing distant residents.

Population placement caches each candidate's nearest occupied-group distance and updates it only after a successful spawn. This preserves stable largest-gap selection while removing repeated distance scans inside the sort comparator. Melee coordination counts active attackers only at the eight-tick decision cadence; target selection is no longer duplicated inside a tactics update. Existing actor ceilings, combat-aware draw culling and native collision headroom remain in place.

The opt-in `SOH_ARPG_WORLD_PERF=1` harness requires a build with `SOH_ARPG_RUNTIME_TESTS=ON` and a `SHIP_HOME` basename beginning `soh-arpg-qa-`. It writes `world-performance-results.json` after a fixed-camera Hyrule Field comparison with populations off, the default 64 ceiling, and the 96 ceiling. Each mode warms up for 180 ticks and samples 300 ticks at 60 FPS interpolation. Measurements separate game-state CPU wall time up to the benchmark hook from full simulation-tick intervals (including rendering, frame pacing and scheduling); hooks executed after the benchmark hook are excluded from its CPU wall time. These are host-specific measurements, not a guarantee for every GPU, resolution or hardware configuration.

The native benchmark exposed a larger graphics bottleneck: remastered limb meshes were interpreted at full detail for every distant actor on each interpolated render. A three-second native sample placed 2,172 of 2,223 main-thread samples in graphics-command execution. With the field facing the camera, the default population averaged 215.43 ms per simulation tick despite only 3.18 ms of measured game-state CPU work. Evidence: `tests/arpg/world-performance-before-lod-macos.json` (partial run; default population completed before stopping the stress phase).

Remastered Wolfos, Stalfos and Lizalfos now use their lightweight native limb display lists beyond 1,500 world units from the active camera, retaining detailed replacements nearby. This preserves continuous visibility, native skeletal animation and collider callbacks rather than removing distant inhabitants. Region draw-culling rules apply immediately after placement instead of waiting another director cycle. The native living-world check explicitly exercises both near and far model selection.

On this Apple M2 Max with Metal, a 1280×720 window and 60 FPS interpolation, the fixed-camera benchmark measured 50.00 ms mean simulation ticks with populations off, 50.00 ms with 64 actors, and 50.00 ms with 95 actors under the 96 ceiling. The populated runs' 95th-percentile tick times were 51.01 and 50.96 ms; measured game-state CPU work averaged 4.19 and 4.71 ms. Both maintain the normal 20 Hz simulation cadence while rendering interpolated frames. Concurrent compilation was running, so these are practical host-specific timings, not an isolated lab comparison or a universal FPS guarantee. Cold population initialization still produced one 215.64 ms tick at the default ceiling; the subsequent stress initialization peaked at 78.22 ms. The change fixes sustained rendering slowdown, not all scene/resource-loading hitches. Evidence: `tests/arpg/world-performance-results-macos.json` includes the tested executable hash.

All 33 native living-world checks passed on the same executable as the benchmark, including camera-distance model selection, 46 distant moving enemies, all four moving guards, native combat damage, collision headroom, healing, aftermath, rewards, toggles and region identity restrictions. The field placed 63 actors across nine sectors and Colossus placed 32. Evidence: `tests/arpg/world-performance-functional-results-macos.json`. The exact tested executable was copied to the playable preview, with QA environment flags absent during normal launch.

### Neutral wildlife — 2026-10-12

`ARPG.Wildlife` defaults on, with a checkbox under regional encounters. Green outdoor regions receive loose flocks of native brown birds and small butterfly clusters. Rivers and Lake Hylia can also receive native swimming fish; Zora's Domain and Fountain receive fish only. Frozen adult Zora's Domain, desert/volcanic regions, interiors and dungeons are excluded. Authored animals and quest actors are never adopted, replaced or removed.

Wildlife shares the regional entry plan and validated floor candidates. The budget is two animals per 12 million square units of usable area, bounded to 6–24 and further limited by free actor slots below 168, preserving the existing reserve. Animal groups are spread apart; native bird flight paths require open space and fish require a water pocket at least 40 units deep with a 120-unit clearance check in four directions. Butterflies use the non-fairy-transforming native variant. Fish retain native swimming and bottle capture behavior and are not replenished mid-visit.

Generated animals remain active while distant, using lightweight native meshes and normal view culling. Birds have extended 12,000-unit visibility; butterflies/fish use 6,000. The unused native bird only updates animation and steering, so a scoped wrapper applies its native velocity. Its dynamically allocated skeleton tables are freed on destruction. Wildlife remains neutral and is not part of monster packs, watch targets, XP or loot. Disabling the wildlife setting removes only generated animals; enabling it repopulates once. Disabling regional populations also removes them, and expedition encounters clear them to preserve encounter actor capacity.

Wildlife verification: all 41 native living-world checks passed, including 12 moving birds and 12 moving butterflies in Hyrule Field, six submerged fish in child Zora's Domain, desert exclusions, independent toggles, existing combat/rewards and cleanup. Portable habitat/budget policies passed. Evidence: `tests/arpg/wildlife-results-macos.json`. On this M2 Max/Metal host at 1280×720 and 60 FPS interpolation, the default population with wildlife averaged 50.000 ms per simulation tick (p99 51.494 ms); populations off averaged 50.055 ms. The 94–95 enemy stress case averaged 50.505 ms, p99 66.273 ms and maximum 92.669 ms, so stress-load spikes remain possible. Other development builds were running during measurement. Evidence: `tests/arpg/wildlife-performance-results-macos.json`. This is a fixed-camera native benchmark, not a guarantee for every scene or machine.
