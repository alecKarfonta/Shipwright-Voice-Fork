#pragma once
#include "ARPGTypes.h"

inline constexpr ARPGSkillDef ARPG_SKILLS[ARPG_SKILL_COUNT_] = {
    // [TIME] | [INFO] | ARPG_SKILLS | name, desc, maxRank, tree, passive, magicCost
    // Sword tree (passive points)
    { "Keen Edge", "+3% crit chance per rank.", 5, ARPG_TREE_SWORD, 1, 0 },
    { "Battle Flow", "+8% melee damage per rank.", 5, ARPG_TREE_SWORD, 1, 0 },
    { "Jump Shockwave", "Slam the ground in an expanding shockwave. Also triggers on jump-attack landing.", 3, ARPG_TREE_SWORD, 0, 0 },
    { "Quickspin", "Spin attack charges twice as fast after unlocking.", 1, ARPG_TREE_SWORD, 0, 0 },
    { "Sword Beam", "Launch a piercing sword wave. No full-health requirement.", 3, ARPG_TREE_SWORD, 0, 0 },
    { "Giant Mastery", "+10% damage with two-handed weapons per rank.", 3, ARPG_TREE_SWORD, 1, 0 },
    // Magic tree (active points)
    { "Flame Burst", "Release an expanding fire burst around you (5 magic).", 3, ARPG_TREE_MAGIC, 0, 5 },
    { "Frost Nova", "Active: freezing burst around you (10 magic).", 3, ARPG_TREE_MAGIC, 0, 10 },
    { "Holy Aura", "Active: Nayru's Love also reflects damage (6 magic).", 3, ARPG_TREE_MAGIC, 0, 6 },
    { "Elemental Infusion", "Imbue melee hits with the selected element for 15 seconds before rank bonuses (8 magic).", 3, ARPG_TREE_MAGIC, 0, 8 },
    { "Magic Economy", "-15% magic costs per rank.", 3, ARPG_TREE_MAGIC, 1, 0 },
    { "Arcane Recovery", "Regenerate 1 magic every 4 seconds per rank.", 3, ARPG_TREE_MAGIC, 1, 0 },
    // Guardian tree (passive points)
    { "Toughness", "1% armor reduction per rank. Armor caps at 25%.", 5, ARPG_TREE_GUARDIAN, 1, 0 },
    { "Bulwark", "-6% damage taken per rank.", 5, ARPG_TREE_GUARDIAN, 1, 0 },
    { "Vengeance", "Taking a hit releases a retaliatory light pulse, +20% strength per rank.", 3, ARPG_TREE_GUARDIAN, 1, 0 },
    { "Bloodstone", "Heal half a heart per kill per rank.", 3, ARPG_TREE_GUARDIAN, 1, 0 },
    { "Prospector", "+10 rupees per kill per rank.", 3, ARPG_TREE_GUARDIAN, 1, 0 },
    { "Fortune Seeker", "+8% equipment-drop chance per rank.", 3, ARPG_TREE_GUARDIAN, 1, 0 },
    { "Resolve", "+5% melee damage per rank while at full health.", 3, ARPG_TREE_SWORD, 1, 0 },
    { "Wind Recall", "Return to your Farore's Wind point in the same room (12 magic).", 3, ARPG_TREE_MAGIC, 0, 12 },
    { "Evasion", "Sidestep danger with a brief window of invulnerability. No magic cost.", 3, ARPG_TREE_GUARDIAN, 0, 0 },
    { "Crescent Cleave", "A broad forward sweep. Build Momentum with basic hits, then spend it on a technique.", 3, ARPG_TREE_SWORD, 0, 0 },
    { "Crosscut", "Two crossing cuts with a delayed finishing strike. Stay close to land both.", 3, ARPG_TREE_SWORD, 0, 0 },
    { "Cyclone", "Three circular sword sweeps while you move. Punishes enemies surrounding you.", 3, ARPG_TREE_SWORD, 0, 0 },
    { "Gale Thrust", "A guarded forward dash with a piercing blade trail. Stops at walls and ledges.", 3, ARPG_TREE_SWORD, 0, 0 },
    { "Blade Orbit", "Conjure revolving blades that sweep around you for three seconds.", 3, ARPG_TREE_SWORD, 0, 0 },
    { "Execution", "A heavy frontal strike dealing double damage to enemies below 35% health.", 3, ARPG_TREE_SWORD, 0, 0 },
    { "Chain Lightning", "Lightning jumps between up to four nearby enemies, following a clear path.", 3, ARPG_TREE_MAGIC, 0, 10 },
    { "Meteor", "Mark a point ahead, then detonate a fiery impact after a visible delay.", 3, ARPG_TREE_MAGIC, 0, 12 },
    { "Rallying Cry", "Recover two hearts, gain Momentum, and take 35% less damage for six seconds.", 3, ARPG_TREE_GUARDIAN, 0, 0 },
    { "Riposte", "Brace for two seconds. The next damaging hit is reduced by 80% and triggers a powerful counter.", 3, ARPG_TREE_GUARDIAN, 0, 0 },
    { "Sword Tempo", "Sword technique cooldowns recover 7% faster per rank.", 3, ARPG_TREE_SWORD, 1, 0 },
    { "Combat Rhythm", "Each Momentum stack adds another 3% technique damage per rank. Basic hits extend its duration.", 3, ARPG_TREE_SWORD, 1, 0 },
    { "Elemental Resonance", "Spell damage +10% per rank; hit chilled or burning foes for another +10% per rank.", 3, ARPG_TREE_MAGIC, 1, 0 },
    { "Channeling", "Magic skill cooldowns recover 7% faster per rank. Basic sword hits restore 1 magic per rank.", 3, ARPG_TREE_MAGIC, 1, 0 },
    { "Second Wind", "Below 35% health, recover one heart per rank over time. Once every 30 seconds.", 3, ARPG_TREE_GUARDIAN, 1, 0 },
    { "Iron Will", "Guardian cooldowns recover 7% faster per rank. Rally also adds 5% damage reduction per rank.", 3, ARPG_TREE_GUARDIAN, 1, 0 },
};
