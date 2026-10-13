# Hyrule Reborn

Hyrule Reborn adds 58 authored NPC quests in 11 regional chapters alongside Ocarina of Time's main campaign. The early chapters help families keep their homes and livelihoods; adult chapters reunite those communities and rebuild their regions; the final chapter gathers support before the confrontation with Ganon. The main game's dungeons, sages, carpenters, castle trials, and ending retain their normal progression.

## Playing

- Find a resident with a gold **!**. Approach and press **J**, or open **Start → Quests → Campaign quests**. Talking normally or pressing **F7** also offers **Local quests** in the native textbox, with explanations, acceptance, deliveries, tracking and reward choices. The quest system works without the dialogue service.
- Choose an available quest, read the request and rewards, and accept it while near its contact. Ready quests have a **?**. Return to the contact and choose one of the two equipment rewards.
- Use **In progress**, **Available**, **All quests**, **Completed**, and the chapter selector to browse. **Track this quest** puts objectives and a nearby worksite direction on the HUD. Controller users can use the existing pause-menu navigation throughout.
- Hunts count the specified species and region. Named pack hunts count only the marked Milk-road ambushers or Thornbound stalkers. Recoveries give one guaranteed quest item for each eligible defeat; these items use a separate quest satchel and do not consume backpack slots.
- Gathering and work objectives appear as distinct objects with green rings on safely reachable ground in the requested region. Walk into a ring and stand still for one second to gather or three seconds to work. Moving, taking damage, or nearby enemies interrupt the work; pausing freezes it. Supply gathering must finish before the corresponding construction begins.
- Restored saplings, beacons, supply stations, memorials, and spring filters remain in their saved locations. Rest beside them outside combat to recover a quarter heart every two seconds. Gathered resources do not respawn for duplicate credit.
- Use **Save** in the journal to retain quests and rebuilt sites with the rest of your adventure. A full backpack leaves the complete reward waiting with the NPC. Rupees use the normal wallet capacity.

**Hyrule Reborn quests** and **Quest objective tracker** are enabled by default under Settings & testing. Disabling quests preserves saved progress and removes their live markers, pack encounters, and scenery. Re-enable to continue. The normal ARPG/randomizer master gate still applies. Quest progress belongs to the local character; network party XP does not manufacture quest kills.

The Available list accounts for child/adult Link, previous hand-ins, and main-story milestones. Already completed native bosses and story milestones count when accepting a relevant quest. Adult regional arcs can be started without completing their optional childhood chains. The well recovery remains accessible as a child; its journal hints describe the narrow entrance without giving the cucco keeper knowledge of time travel. Accepting and turning in age-specific quests requires the corresponding age; their objectives survive time travel. Daytime contacts are identified in the journal. Malon returns to the ranch after Talon returns from the castle. Before the Forest Temple is cleared, find the Kokiri inside their houses or speak to Mido in the Lost Woods. Unfinished named packs respawn only their uncredited members after leaving or reloading.

## Chapters and contacts

| Chapter | Contact | Progression |
| --- | --- | --- |
| Roots of Courage | Kokiri residents, Kokiri Forest | Protect paths, gather dewleaf, defeat Gohma, plant guardian saplings. |
| The Long Road Home | Malon, Lon Lon Ranch | Clear the ambusher pack, recover feed, rebuild roadside stations, carry good news. |
| Bread of the Mountain | Gorons, Goron City | Find edible ore, hunt Dodongos, defeat their king, relight the mountain trail. |
| A River Remembers | Zora residents or King Zora | Clear river Tektites, recover filter pearls, end Barinade's sickness, restore filters. |
| The Village Watch | Cucco keeper, Kakariko | Quiet the graveyard, shelter travelers, recover patrol badges, restore memorials. |
| What the Forest Kept | Kokiri residents as adult Link | Defeat Thornbound stalkers, awaken Saria, plant the sacred meadow. |
| Embers of Hope | Gorons as adult Link | Recover prisoners' meal tokens, defeat Volvagia, rebuild relief stations. |
| The Thaw | Scientist, Lakeside Laboratory | Sample the Ice Cavern's cursed ice, defeat Morpha, restore lakeshore spring filters. |
| Names in the Dark | Cucco keeper as adult Link | Recover keepsakes from the well as a child, defeat Bongo Bongo, light Kakariko. |
| Across the Divide | Head carpenter and Gerudo guards | Rescue the bridge crew, recover tools, rebuild stations, light the wasteland, free Nabooru. |
| Hyrule Reborn | Cucco keeper after the five sages awaken | Unite the regions, recover castle relief seals, break six trials, raise dawn beacons before the finale. |

Every quest grants XP, rupees, and a choice of equipment with an explicit preview. Forest, fire, and water rebuilding finales award Briarwatch, Cinderbound, and Tidecaller set pieces. Late campaign rewards include Moonwell Fang/Dawnglass, Cinderwake/Briar Oath, and Veil of the Deep/Redstep.

## Implementation

`QuestCatalog.h` contains append-only quest IDs, the quest graph, authored offers and thank-you dialogue, and objectives. `QuestRules.h` implements progression and atomic item claims without engine dependencies. `QuestSystem.cpp` adapts native enemy/boss events, story flags, contacts, controls, markers, safe ground placement, interactions, journal, and HUD. `QuestWorld.cpp` provides original native geometry for the gathering objects and broken/restored scenery.

The version-14 ARPG save section stores quest states, counters, defeated-pack/gathered-node identities, tracked quest, historical milestones, and bounded world coordinates in `SaveContext`. Older characters gain an empty campaign. Save workers read the immutable snapshot, not gameplay globals. Existing native story rewards backfill milestone objectives; NPC acceptance/claiming rechecks range, scene, room, and line of sight. Ordinary enemy callbacks are deduplicated by the existing XP reward lifecycle before reaching quests.

Worksites use the same hazard checks as the encounter system, including static ground, floor slope, water, voids, exits, line of sight, and samples along the walking approach. Placement is bounded and retries while exploring open terrain. Saved positions are checked again on room entry; invalid unfinished positions may be replaced without losing completed node identities. Scenery is nonblocking, so it cannot obstruct original puzzles or exits. The valley bridge quest uses the actual carpenter-rescue milestone and the native rebuilt bridge.

## Validation

Portable checks:

```sh
cmake -S tests/arpg -B /tmp/arpg-quest-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/arpg-quest-tests -j 6
ctest --test-dir /tmp/arpg-quest-tests --output-on-failure
```

`quest_test.cpp` exercises all 58 catalog entries, every reward choice in both ages, exact enemy/region matching, named pack identities, prerequisite and age gates, physical node deduplication, supply-before-building order, atomic full-backpack handling, old saves, malformed saves, and campaign save round trips.

`quest_runtime_test.cpp` is compiled only with `SOH_ARPG_RUNTIME_TESTS=ON`. It requires `SOH_ARPG_QUEST_QA=1` and an isolated `SHIP_HOME` whose final directory name begins `soh-arpg-qa-`. It starts a fresh debug adventure and writes only save slot 0 in that isolated profile. `SOH_ARPG_QUEST_UI=1` leaves the journal open for inspection. Native results are reported to `quest-results.json` inside that profile. It does not load ordinary saves.


`quest_world_runtime_test.cpp` requires `SOH_ARPG_QUEST_SWEEP=1` and the same isolated-profile naming convention. It visits 41 child/adult regional cases, checks native contact availability (including Kokiri indoors before the Forest Temple), places physical objectives on real collision geometry, and defeats both named pack types through the engine's reward callbacks. It reports to `quest-world-results.json`. The test starts with the relevant story flags, uses normal region entrances, and can move onto already validated worksites when an entrance ledge is too small for all three objects.

The native harnesses exercise engine integration with controlled fixtures; they do not replace a full manual campaign playthrough. Build ordinary game binaries with `SOH_ARPG_RUNTIME_TESTS=OFF`.


## More journeys, more conversations

The 18 added quests (IDs 41–58) extend the existing regional chapters:

| Quest | New activity |
| --- | --- |
| The Forest Has an Echo | Play Saria's Song, then examine sunlit plants. |
| Special Delivery: Do Not Eat | Deliver lunch-wrapping leaves to Gorons who might eat the wrapping. |
| Lullaby for a Light Sleeper | Play Epona's Song and investigate a nighttime ranch noise. |
| Feathers, Not Sneezes | Bring Malon's clean bedding to the allergic cucco keeper. |
| A Rubbing, Not a Robbery | Survey memorial marks and consult the lakeside scientist. |
| The Gravest Night Shift | Investigate the marks at night, then play the Sun's Song. |
| A Rock With Rhythm | Bring forest music to the Gorons and test a drumstone. |
| The Lunch Bell Rings Again | Inspect stone and rebuild communal supper stands. |
| The River's Two Faces | Compare daylight and nighttime filters, then deliver observations. |
| Strictly Not for Soup | Carry a sample through Malon and the Zoras in order. |
| Rain Check | Call rain with the Song of Storms and inspect shore filters. |
| A Reply Without Words | Play Saria's melody and listen quietly in the restored meadow. |
| The Bridge Inspector's Bad Day | Inspect timber, obtain a guard's report, repair approach lights. |
| Reading the Dunes | Study signal posts and observe the wasteland wind. |
| Water Without a War | Consult the scientist, gather supplies, deliver them to Gerudo scouts. |
| The Quiet Watch | Check night lanterns, keep watch, restore remembrance lights. |
| No Rocks in the Milk | Deliver Malon's greetings to forest and mountain neighbors. |
| A Table With Room for Everyone | Gather promises of benches, water jars, and supper before the tower. |

Songs require a learned melody played freely in the exact region **after acceptance and preceding steps**. Lessons, playback demonstrations, wrong songs, and performances elsewhere do not count. Deliveries require an explicit journal conversation with the correct nearby recipient; entering the destination alone does not hand over a parcel. The nearest pending delivery gets priority when opening **J**, and its recipient has a **?** marker.

Survey rings require three uninterrupted seconds and the stated day/night condition. Each of the three sites counts once. Observations leave no healing station or duplicate pickup. A watch requires 20 or 30 continuous seconds standing still, unharmed, and grounded in the requested place and time. Movement, damage, changing regions, and loading reset unfinished watches; completed watches stay complete. Pausing freezes time. Where a region's sky is fixed, change the time outside and return.

Every quest has authored **Tell me about it**, **Why does it matter?**, and **Any advice?** topics, plus ready and completion responses. These work offline. The unified NPC conversation's suggested questions also use this authored local dialogue for quest contacts. Free-text conversation keeps the existing optional dialogue service. Named speakers have their own greetings; greetings change with Link's age and relevant temple milestones. Saria's personal version of her melody request speaks in the first person. Generic villagers do not impersonate their leaders, and frozen King Zora cannot receive deliveries.

`QuestDialogue.h` is the source for conversation branches and recipient responses. Lore was checked against the [original game's English dialogue transcript](https://www.zeldadungeon.net/wiki/TextDump:Ocarina_of_Time) and native actor logic: Kokiri do not grow up or recognize adult Link; Malon's mother composed Epona's melody; the cucco keeper is allergic to her birds; the restored lake does not automatically thaw Zora's Domain. New events are original side stories. They do not change sage identities, grant unseen knowledge to villagers, or replace the canonical ending.

## Voice production

`voice-production/queue_quest_dialogue.py` compiles an exporter against the actual C++ catalog, preserving exact authored text and character overrides. It adds quest jobs to the existing TTS/STT pipeline using approved voices, including individual Kokiri, Goron, Zora, and Gerudo variants and separate child/adult Malon. It creates `quest-dialogue-script.json` and `quest-playback-map.json` for source coverage and later playback lookup. All existing stock jobs and completed audio are preserved. The normal renderer checkpoint refreshes these jobs after subsequent script edits or approved recasts.

Queuing/rendering audio does not by itself install prerecorded playback in the native game. The existing voice-production normalization manifest remains the source for finished playback assets.

## Forest quest plants

Herb patches reuse the original `gameplay_field_keep/gHanaDL` wildflowers in three offset rosettes. Saplings use the original `gHanaLeafTex` painted leaf and `object_wood02_Tex_002F90` bark on tapered branching stems and folded, two-sided leaves. Native field grass softens the base. There are three deterministic shapes for both seedlings and restored saplings; saved site positions select the shape and rotation without changing quest saves. The plants use the scene's ambient and local lights, fog, and depth-tested cutout foliage. They load textures from the game's existing resource paths, including compatible replacement texture packs.

`QuestPlantGeometry.h` contains the sapling meshes; `QuestWorld.cpp` binds the original materials and models. `arpg_quest_plants` validates all six meshes, texture coordinates, normals, world bounds and triangles after N64 coordinate quantization. A test-only native comparison can be opened with `SOH_ARPG_QUEST_QA=1 SOH_ARPG_PLANTS_REVIEW=1` in a `soh-arpg-qa-*` profile. Optional `SOH_ARPG_PLANTS_NIGHT=1` and `SOH_ARPG_PLANTS_REVERSE=1` switches check night lighting and reverse views. The production build excludes this fixture.

## Expansion verification

The expanded catalog passes **1,054 quest-rule checks**, including every quest and both reward choices, ordered deliveries, wrong songs/places, time-of-day conditions, watch interruptions, dialogue coverage, save round trips, and migration from the original 40-quest array. The portable regression run passed **21 suites**. Native isolated-profile runs passed **36 engine checks** and **41 regional cases**, including real recipient actors, song hooks, observation channels, pause/damage behavior, and collision-safe sites. Eight voice-export tests and the complete stock-dialogue inventory verification passed after queue expansion.

Large and 640×480 journal layouts were inspected, including topic selection, readable wrapping, reward buttons, and Saria's first-person request. The normal game build excludes the test hooks. These checks exercise controlled fixtures, not a full manual playthrough of all 58 quests. Current evidence is recorded in [quest-verification-macos.json](../../../../tests/arpg/quest-verification-macos.json).
