# Skill combat and progression

The skill journal (`K`, or **Skills** in the inventory) contains 37 skills across Sword, Magic, and Guardian. Six character-specific action slots use keyboard `1`–`6` or controller `L` + D-pad Up/Right/Down/Left and C-Left/C-Right. Drag a learned technique onto a slot, or select it and click the slot. Keyboard bindings are configurable under **Build and controls**; click a binding to cycle the available keys. Assigning an occupied key exchanges the two bindings.

## Combat loop

Crescent Cleave and Evasion start learned. Basic sword damage gains 3.5% per character level after level 1, in addition to Battle Flow, weapon mastery, gear, and critical hits. Successful basic sword hits build up to five Momentum stacks; the next sword technique consumes them for +8% damage each. Combat Rhythm increases the value of each stack. Momentum expires after eight seconds without a hit. Channeling returns magic on basic sword hits.

Sword techniques cost no magic. Each skill owns its own cooldown, independent of its action slot, with a 0.4 second action recovery between casts. Multi-stage sword sequences must finish before starting another sword sequence; defensive and magical actions can still be used after the action recovery. Cooldowns advance at the engine's 20 Hz gameplay simulation rate and stop during pause, dialogue, and cutscenes. Scene changes end effects while preserving remaining cooldowns. Loading a file starts a fresh combat session.

| Technique | Base recharge | Pattern |
| --- | ---: | --- |
| Crescent Cleave | 2.5 s | Forward sweeping arc |
| Crosscut | 6 s | Two crossing cuts, separated in time |
| Cyclone | 10 s | Three circular sweeps that follow Link |
| Gale Thrust | 7 s | Forward dash with a narrow piercing trail |
| Blade Orbit | 20 s | Three revolving blades with repeated strike windows |
| Execution | 16 s | Heavy forward strike, double damage below 35% enemy health |
| Jump Shockwave | 9 s | Expanding ground impact; also activates on jump-attack landing |
| Sword Beam | 8 s | Piercing wave; no full-health restriction |
| Flame Burst | 9 s | Expanding fire burst |
| Frost Nova | 12 s | Ice burst that chills movement for six seconds |
| Chain Lightning | 11 s | Up to four nearby targets connected by clear paths |
| Meteor | 22 s | Visible ground warning followed by a delayed fire impact |
| Holy Aura | 45 s | Eight seconds of native protection and retaliation |
| Elemental Infusion | 25 s | Fifteen seconds of elemental melee damage |
| Wind Recall | 30 s | Return to the existing same-room Farore's Wind point |
| Evasion | 5 s | Brief invulnerability and a safe backward step |
| Rallying Cry | 25 s | Heal, gain Momentum, and reduce incoming damage |
| Riposte | 9 s | Brief defensive stance that converts one incoming hit into a counter |

All 18 active techniques have two optional rune variants, unlocked at character levels 6 and 14. Examples include a full-circle Cleave, triple Crosscut, five-sweep Cyclone, stationary Impaler, delayed Aftershock, a three-wave Beam fan or returning Beam, six-target lightning, and a three-impact Meteor Shower. The journal describes each tradeoff. Runes can change only when their skill is ready and its buff has ended. Ranks increase offensive power by 18% and reduce base cooldown by 6% per additional rank; utility ranks improve their applicable healing/duration. Sword Tempo, Channeling, and Iron Will specialize cooldown recovery by tree.

## Progression and saves

Each level awards a passive point; every even level awards a technique point. Each distinct boss adds two technique points. New and migrated characters receive one starting technique point. Existing characters receive the even-level points retroactively once. Their earlier ranks, gear, backpack, XP, and other save data remain intact.

The journal arranges all 37 nodes into connected branches. Advanced nodes require a character level, investment in other nodes of their tree, and the prerequisite ranks shown on each branch. Existing learned skills remain learned even when an older character lacks a newly introduced prerequisite. Arrow keys move between nodes; Enter learns or improves the selected skill. The point pools, unmet requirements, first/current/next-rank stats, and rune tradeoffs are visible in the inspector.

Twenty-two directed synergies add power from learned ranks of related skills. For example, Crosscut gains 5% per Cleave rank and 4% per Gale Thrust rank; Meteor gains 6% per Flame Burst rank and 4% per Elemental Resonance rank. Bonuses add together and never recursively multiply each other. A cast snapshots its synergy power. The inspector lists incoming and outgoing links, including cross-tree links, with buttons to inspect the related node. Strike power includes rank, rune and synergy, before character level, gear, Momentum and conditional target bonuses. Utility previews use the actual cast duration/healing rules; Quick Return displays its doubled magic cost before discounts.

 Free respec refunds purchased ranks and preserves the free rank of Cleave/Evasion. Respec and slot changes do not clear cooldowns. Skill IDs are append-only. ARPG save section version 5 persists ranks, runes, action slots, and the one-time migration marker in the existing asynchronous SaveContext snapshot.

## Native integration

`SkillPatterns.h` samples deterministic local-space attack geometry. `SkillCombat.cpp` uses native player-aligned colliders attached to an owned effect actor, native sword animation entry points, and original translucent meshes rendered through the game engine. Native damage tables, shield collisions, vulnerabilities, damage reactions, death rewards, and gear modifiers remain in the damage pipeline. A collision gate rejects duplicate limb contacts per strike and attacks through walls before native hit flags are set. Native invulnerable phases still apply.

`SkillVfx.cpp` builds distinct crescent and crossing ribbons, helical Cyclone trails, spectral orbiting swords, a descending Execution blade, crystalline shockwaves, traveling crescent beams, flame petals, ice crystals, jagged lightning links, and meteor telegraphs/descent/impact fragments. Rune geometry also changes the corresponding effects. Each actor has a bounded frame-owned mesh; rendering consumes no gameplay RNG.

`AuraGeometry.h` and `SkillAuras.cpp` add seven original forms to utility skills: elemental hand seals and orbiting crystals for Infusion, a root-carved frontal ward for Riposte, three orbiting heraldic shields for Holy Aura, a jade-and-gold rally crest, feather trails for Evasion, spiraling departure/arrival rings for Recall, and rising healing feathers for Second Wind. They share the jade, ivory, gold and elemental colors of the custom gear. Fire/Frost/Light can be chosen in Infusion's journal entry when it is ready. The effects render as depth-tested world geometry and scale for child Link. Persistent forms follow the actual buff timers; the frontal ward ends when its counter is consumed. Pause freezes their lifetimes. Respec, death, transitions and master-off cancel them. These cosmetic actors never register an attack collider, use no RNG, and have a separate eight-actor limit.

The journal, inventory, and action bar share OoT's actual glyphs, pause textures, selection corners, and circular button art, with original technique icons. The pause pages use carved blue stone, an open item grid, native arrow choices, and message-box details over a dimmed world. Rank pips sit below each skill painting. All 37 skills have their own original transparent paintings, shared by journal nodes and action slots. Their silhouettes depict the mechanics: Crosscut crossing arcs, Blade Orbit's three swords, Infusion's elemental crystals, Riposte's root ward, and Recall's spiraling feathers. The painted equipment art is reserved for equipment; no native item paintings are substituted. A bronze journal crest anchors the heading. The action bar is transparent and scales with the viewport. **Quest & map** returns to the original pause pages. Native pause drawing still initializes its cursor geometry before its display lists are suppressed, preserving the engine's update dependencies.

Magic debits retain the native animated meter. The skill system releases its transaction after payment instead of holding the meter throughout a spell's lifetime; utility actor cleanup cannot cancel a subsequent spell's debit. The native two-unit drain clamps to its target so odd discounted costs do not overspend.

Gale Thrust and Evasion validate each movement step against walls, static ground, height changes, water, hazardous surfaces, and exits. They cannot jump across a pit or teleport through a wall. Live technique effects are capped. Transitions, death, master-off, and respec terminate effects; actor-owned collider and extension data are destroyed with their actor.

The temporary testing adventure starts with the Swordmaster build. **Testing builds** in the journal and **Settings & testing** offer Swordmaster, Spellblade, and Warden presets, point refills, and resource/cooldown resets. These presets are restricted to the explicitly launched temporary adventure.

## Verification

Portable tests cover all 54 active/rune combinations, bounded geometry, strike counts, telegraph timing, returning waves, progression gates, point migration, save roundtrips, corrupted bindings, and cooldown modifiers. On the earlier macOS arm64 skill build (2026-10-11), all 241 native skill checks passed, including all 54 casts, offensive AT-vs-AC collision probes, per-strike deduplication, consecutive spell debits with odd costs and one remaining magic unit, asynchronous loadout saving, actual champion-pack combat, child Link, and master-off cleanup. The 79 gear/enemy regression checks also passed on the final implementation. Records: [skill checks](../../../../tests/arpg/runtime-results-skills-macos.json) and [gear/enemy regression](../../../../tests/arpg/runtime-results-skills-regression-macos.json). Portable tests also passed under AddressSanitizer and UndefinedBehaviorSanitizer. The final native executable was built with runtime harnesses disabled.

Manual native-app inspection verified the cohesive inventory/journal, native pause-page access, cooldown HUD, spectral orbiting swords, Cyclone, and a consecutive Meteor/Frost Nova/Chain Lightning rotation against real enemies. Automated native collision probes verify damage plumbing, not every enemy-specific interaction. Full boss and campaign balance remain playtesting work.

The connected-tree and aura pass passed all three portable CTest targets, also under AddressSanitizer and UndefinedBehaviorSanitizer. Tests prove branch coverage/reachability within earned point budgets, prerequisite gates, preservation of legacy ranks, bounded hard-rank synergies, side-effect-free next-rank previews, and 5,094 finite, nondegenerate aura meshes across valid lifetimes/runes/elements. The expanded native skill suite passed 370 checks, including all 54 casts, actual attack power matching the displayed rank/synergy values, valid testing presets, owned aura spawning/cleanup, save/load, champion combat, and child/master-off coverage. Results: [tree/skill regression](../../../../tests/arpg/runtime-results-tree-skills-macos.json).

The focused native aura harness was rerun successfully after the final branch prerequisites and icon integration; all 39 checks pass: Second Wind activation, the eight-effect cap, normal update/drawing, respec cleanup, adult-to-child scene transition, initialized pause/resume with frozen lifetimes, natural expiration, child rendering and master-off. [Aura lifecycle results](../../../../tests/arpg/runtime-results-auras-macos.json).

The final gear/enemy regression also passed all 79 checks: [tree-pass regression](../../../../tests/arpg/runtime-results-tree-regression-macos.json).
