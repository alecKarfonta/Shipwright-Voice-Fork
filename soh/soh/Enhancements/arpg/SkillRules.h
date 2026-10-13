#pragma once
#include "SkillDefinitions.h"
#include "SkillTree.h"
#include <algorithm>
#include <cmath>

namespace ARPG::SkillRules {

struct Technique {
    int level = 1;
    int investment = 0;
    int cooldown = 0;
    int duration = 0;
    float potency = 1;
    float radius = 1;
};
struct Rune { const char* name; const char* description; };

inline bool Castable(int id) {
    return id >= 0 && id < ARPG_SKILL_COUNT_ && !ARPG_SKILLS[id].passive && id != ARPG_SKILL_SWORD_QUICKSPIN;
}
inline bool Sword(int id) { return Castable(id) && ARPG_SKILLS[id].tree == ARPG_TREE_SWORD; }
inline Technique Definition(int id) {
    switch (id) {
        case ARPG_SKILL_SWORD_CLEAVE: return { 1, 0, 50, 12, 1.6f, 125 };
        case ARPG_SKILL_SWORD_CROSSCUT: return { 3, 1, 120, 30, 1.4f, 130 };
        case ARPG_SKILL_SWORD_CYCLONE: return { 5, 2, 200, 48, 1.1f, 145 };
        case ARPG_SKILL_SWORD_LUNGE: return { 3, 1, 140, 20, 2.2f, 65 };
        case ARPG_SKILL_SWORD_BLADESTORM: return { 12, 5, 400, 65, 0.9f, 150 };
        case ARPG_SKILL_SWORD_EXECUTE: return { 16, 7, 320, 22, 3.5f, 115 };
        case ARPG_SKILL_SWORD_SHOCKWAVE: return { 3, 1, 180, 22, 2.2f, 185 };
        case ARPG_SKILL_SWORD_BEAM: return { 8, 3, 160, 32, 2.0f, 45 };
        case ARPG_SKILL_MAGIC_FLAME_BURST: return { 3, 1, 180, 24, 2.0f, 180 };
        case ARPG_SKILL_MAGIC_FROST_NOVA: return { 3, 1, 240, 22, 1.2f, 185 };
        case ARPG_SKILL_MAGIC_HOLY_AURA: return { 8, 3, 900, 160, 1, 1 };
        case ARPG_SKILL_MAGIC_INFUSE: return { 5, 2, 500, 300, 1, 1 };
        case ARPG_SKILL_MAGIC_RECALL: return { 3, 1, 600, 1, 1, 1 };
        case ARPG_SKILL_GUARD_DODGE: return { 1, 0, 100, 12, 1, 1 };
        case ARPG_SKILL_MAGIC_CHAIN: return { 8, 3, 220, 30, 1.4f, 70 };
        case ARPG_SKILL_MAGIC_METEOR: return { 16, 7, 440, 50, 4.0f, 200 };
        case ARPG_SKILL_GUARD_WARCRY: return { 5, 2, 500, 120, 1, 1 };
        case ARPG_SKILL_GUARD_RIPOSTE: return { 3, 1, 180, 40, 3.0f, 160 };
        case ARPG_SKILL_GUARD_VENGEANCE: return { 3, 1, 20, 14, 1, 150 };
        case ARPG_SKILL_SWORD_QUICKSPIN: return { 3, 1 };
        case ARPG_SKILL_SWORD_MASTERY: case ARPG_SKILL_SWORD_RESOLVE:
        case ARPG_SKILL_GUARD_BLOODSTONE:
        case ARPG_SKILL_GUARD_FORTUNE: case ARPG_SKILL_GUARD_PROFIT: return { 3, 1 };
        case ARPG_SKILL_SWORD_TEMPO: case ARPG_SKILL_MAGIC_CHANNEL:
        case ARPG_SKILL_GUARD_IRON_WILL: return { 5, 2 };
        case ARPG_SKILL_SWORD_FINESSE: case ARPG_SKILL_MAGIC_RESONANCE:
        case ARPG_SKILL_GUARD_SECOND_WIND: return { 10, 4 };
        default: return {};
    }
}

inline Rune RuneAt(int id, int rune) {
    if (rune == 0) { return { "Original", "The standard technique, with no tradeoff." }; }
    switch (id) {
        case ARPG_SKILL_SWORD_CLEAVE: return rune == 1 ? Rune{ "Reaping Arc", "Full-circle sweep; 20% less damage." } : Rune{ "Heavy Edge", "+60% damage; 40% longer cooldown." };
        case ARPG_SKILL_SWORD_CROSSCUT: return rune == 1 ? Rune{ "Triple Cut", "Adds a third strike; 25% longer cooldown." } : Rune{ "Twin Fangs", "Two cuts travel forward; each deals 15% less damage." };
        case ARPG_SKILL_SWORD_CYCLONE: return rune == 1 ? Rune{ "Hurricane", "+40% radius; 25% less damage." } : Rune{ "Eye of the Storm", "Five sweeps; 50% longer cooldown." };
        case ARPG_SKILL_SWORD_LUNGE: return rune == 1 ? Rune{ "Windrunner", "Dash 50% farther; 25% less damage." } : Rune{ "Impaler", "No dash; a long piercing strike with +60% damage." };
        case ARPG_SKILL_SWORD_BLADESTORM: return rune == 1 ? Rune{ "Outer Ring", "+45% orbit radius; 20% less damage." } : Rune{ "Blade Ward", "20% damage reduction while blades orbit; 30% longer cooldown." };
        case ARPG_SKILL_SWORD_EXECUTE: return rune == 1 ? Rune{ "Reaper", "Full-circle execution; 25% less damage." } : Rune{ "Merciless", "Execute bonus begins below 60% health; 40% longer cooldown." };
        case ARPG_SKILL_SWORD_SHOCKWAVE: return rune == 1 ? Rune{ "Aftershock", "A second delayed shockwave; 35% longer cooldown." } : Rune{ "Fault Line", "A line of three eruptions ahead instead of a radial slam." };
        case ARPG_SKILL_SWORD_BEAM: return rune == 1 ? Rune{ "Prismatic Fan", "Three piercing waves; each deals 35% less damage." } : Rune{ "Returning Edge", "The wave returns for a second hit; 40% longer cooldown." };
        case ARPG_SKILL_MAGIC_FLAME_BURST: return rune == 1 ? Rune{ "Wildfire", "Three fire pulses; each deals 40% less damage." } : Rune{ "Inferno", "+70% damage; 50% longer cooldown." };
        case ARPG_SKILL_MAGIC_FROST_NOVA: return rune == 1 ? Rune{ "Deep Freeze", "Chill lasts twice as long; 40% longer cooldown." } : Rune{ "Shatter", "+100% damage against already chilled enemies; 20% smaller radius." };
        case ARPG_SKILL_MAGIC_HOLY_AURA: return rune == 1 ? Rune{ "Sanctuary", "Protection lasts 50% longer; 50% longer cooldown." } : Rune{ "Retribution", "Double reflected pulse damage; protection lasts 25% less." };
        case ARPG_SKILL_MAGIC_INFUSE: return rune == 1 ? Rune{ "Lingering Edge", "Infusion lasts twice as long; 40% longer cooldown." } : Rune{ "Overload", "Infusion bonus rises to 100%; lasts half as long." };
        case ARPG_SKILL_MAGIC_RECALL: return rune == 1 ? Rune{ "Renewal", "Recall restores three hearts; 30% longer cooldown." } : Rune{ "Quick Return", "40% shorter cooldown; double magic cost." };
        case ARPG_SKILL_GUARD_DODGE: return rune == 1 ? Rune{ "Fleet Foot", "Longer step and 30% shorter cooldown; fewer invulnerable frames." } : Rune{ "Shadow Step", "More invulnerable frames; 50% longer cooldown." };
        case ARPG_SKILL_MAGIC_CHAIN: return rune == 1 ? Rune{ "Forked Lightning", "Jumps to six enemies; 20% less damage." } : Rune{ "Overcharge", "Only two targets; +80% damage." };
        case ARPG_SKILL_MAGIC_METEOR: return rune == 1 ? Rune{ "Meteor Shower", "Three impacts along a line; each deals 40% less damage." } : Rune{ "Cataclysm", "+50% radius and damage; 50% longer cooldown." };
        case ARPG_SKILL_GUARD_WARCRY: return rune == 1 ? Rune{ "Battle Cry", "Adds 30% melee damage; protection lasts half as long." } : Rune{ "Renewing Cry", "Heals four hearts; 40% longer cooldown." };
        case ARPG_SKILL_GUARD_RIPOSTE: return rune == 1 ? Rune{ "Patient Guard", "Brace lasts twice as long; 30% longer cooldown." } : Rune{ "Vengeful Counter", "Counter damage doubled; incoming hit reduced by only 50%." };
        default: return { "Original", "This skill has no rune choices." };
    }
}
inline int Investment(const ARPGSaveData& save, int tree, int except = -1) {
    int points = 0;
    for (int i = 0; i < ARPG_SKILL_COUNT_; ++i) {
        if (i != except && ARPG_SKILLS[i].tree == tree) { points += save.skillRanks[i]; }
    }
    return points;
}
inline bool CanLearn(const ARPGSaveData& save, int id) {
    if (id < 0 || id >= ARPG_SKILL_COUNT_) { return false; }
    auto def = Definition(id);
    return save.level >= def.level && Investment(save, ARPG_SKILLS[id].tree, id) >= def.investment &&
           (save.skillRanks[id] > 0 || SkillTree::Prerequisites(save,id));
}
inline int RuneLevel(int rune) { return rune == 1 ? 6 : rune == 2 ? 14 : 1; }
inline bool CanRune(const ARPGSaveData& save, int id, int rune) {
    return Castable(id) && save.skillRanks[id] > 0 && rune >= 0 && rune <= 2 && save.level >= RuneLevel(rune);
}
inline float CooldownRune(int id, int rune) {
    if (rune == 1) {
        switch (id) {
            case ARPG_SKILL_SWORD_CROSSCUT: return 1.25f;
            case ARPG_SKILL_SWORD_SHOCKWAVE: return 1.35f;
            case ARPG_SKILL_MAGIC_FROST_NOVA: case ARPG_SKILL_MAGIC_INFUSE: return 1.4f;
            case ARPG_SKILL_MAGIC_HOLY_AURA: return 1.5f;
            case ARPG_SKILL_MAGIC_RECALL: case ARPG_SKILL_GUARD_RIPOSTE: return 1.3f;
            case ARPG_SKILL_GUARD_DODGE: return 0.7f;
        }
    } else if (rune == 2) {
        switch (id) {
            case ARPG_SKILL_SWORD_CLEAVE: case ARPG_SKILL_SWORD_EXECUTE:
            case ARPG_SKILL_SWORD_BEAM: case ARPG_SKILL_GUARD_WARCRY: return 1.4f;
            case ARPG_SKILL_SWORD_CYCLONE: case ARPG_SKILL_MAGIC_FLAME_BURST:
            case ARPG_SKILL_GUARD_DODGE: case ARPG_SKILL_MAGIC_METEOR: return 1.5f;
            case ARPG_SKILL_SWORD_BLADESTORM: return 1.3f;
            case ARPG_SKILL_MAGIC_RECALL: return 0.6f;
        }
    }
    return 1;
}
inline int Cooldown(const ARPGSaveData& save, int id) {
    if (!Castable(id)) { return 0; }
    int tree = ARPG_SKILLS[id].tree;
    int passive = tree == ARPG_TREE_SWORD ? ARPG_SKILL_SWORD_TEMPO : tree == ARPG_TREE_MAGIC ? ARPG_SKILL_MAGIC_CHANNEL : ARPG_SKILL_GUARD_IRON_WILL;
    float reduction = 1 - 0.06f * std::max(0, int(save.skillRanks[id]) - 1) - 0.07f * save.skillRanks[passive];
    return std::max(20, int(std::ceil(Definition(id).cooldown * std::max(0.5f, reduction) * CooldownRune(id, save.skillRunes[id]))));
}
inline float RankPower(const ARPGSaveData& save, int id) { return 1 + 0.18f * std::max(0, int(save.skillRanks[id]) - 1); }
inline float LevelPower(int level) { return 1 + 0.035f * (std::clamp(level, 1, 50) - 1); }
inline float MomentumPower(const ARPGSaveData& save, int stacks) {
    return 1 + std::clamp(stacks, 0, 5) * (0.08f + 0.03f * save.skillRanks[ARPG_SKILL_SWORD_FINESSE]);
}
inline int BaseMagicCost(int id, int rune) {
    if (!Castable(id)) { return 0; }
    return ARPG_SKILLS[id].magicCost * (id==ARPG_SKILL_MAGIC_RECALL && rune==2 ? 2 : 1);
}
inline int BuffDuration(const ARPGSaveData& save, int id) {
    if (id < 0 || id >= ARPG_SKILL_COUNT_) { return 0; }
    int ranks = std::max(0,int(save.skillRanks[id])-1), rune = save.skillRunes[id];
    switch (id) {
        case ARPG_SKILL_MAGIC_INFUSE: return int((300+60*ranks)*(rune==1?2:rune==2?.5f:1));
        case ARPG_SKILL_MAGIC_HOLY_AURA: return int((160+20*ranks)*(rune==1?1.5f:rune==2?.75f:1));
        case ARPG_SKILL_GUARD_WARCRY: return (120+20*ranks)/(rune==1?2:1);
        case ARPG_SKILL_GUARD_RIPOSTE: return (40+5*ranks)*(rune==1?2:1);
        case ARPG_SKILL_GUARD_DODGE: return rune==1?7:rune==2?20:12;
        default: return 0;
    }
}
inline int Healing(const ARPGSaveData& save, int id) {
    if (id < 0 || id >= ARPG_SKILL_COUNT_) { return 0; }
    int rune = save.skillRunes[id];
    if (id==ARPG_SKILL_GUARD_WARCRY) { return (rune==2?64:32)+8*std::max(0,int(save.skillRanks[id])-1); }
    return id==ARPG_SKILL_MAGIC_RECALL && rune==1 ? 48 : 0;
}
// [TIME] | [INFO] | SkillRules.Initialize | Migrate once, grant retroactive level milestones, and preserve old learned ranks.
inline int ControllerSlot(int direction, bool modifier) {
    if(direction<0 || direction>3)return -1;
    if(modifier && direction==3)return 4;
    if(modifier && direction==1)return 5;
    return direction;
}
inline void Initialize(ARPGSaveData& save) {
    if (save.skillRevision >= 1) { return; }
    save.skillRevision = 1;
    save.activePoints = std::min(255, int(save.activePoints) + 1 + save.level / 2);
    save.skillRanks[ARPG_SKILL_SWORD_CLEAVE] = std::max<uint8_t>(1, save.skillRanks[ARPG_SKILL_SWORD_CLEAVE]);
    save.skillRanks[ARPG_SKILL_GUARD_DODGE] = std::max<uint8_t>(1, save.skillRanks[ARPG_SKILL_GUARD_DODGE]);
    const uint8_t defaults[ARPG_ACTION_SLOTS] = { ARPG_SKILL_SWORD_CLEAVE, ARPG_SKILL_SWORD_CROSSCUT,
        ARPG_SKILL_SWORD_CYCLONE, ARPG_SKILL_SWORD_LUNGE, ARPG_SKILL_SWORD_BEAM, ARPG_SKILL_GUARD_DODGE };
    std::copy(std::begin(defaults), std::end(defaults), std::begin(save.actionSlots));
}
inline bool Assign(ARPGSaveData& save, int slot, int id) {
    if (slot < 0 || slot >= ARPG_ACTION_SLOTS || (id != ARPG_SKILL_NONE && (!Castable(id) || !save.skillRanks[id]))) { return false; }
    for (int i = 0; i < ARPG_ACTION_SLOTS; ++i) {
        if (i != slot && save.actionSlots[i] == id) { std::swap(save.actionSlots[i], save.actionSlots[slot]); break; }
    }
    save.actionSlots[slot] = id;
    return true;
}
}
