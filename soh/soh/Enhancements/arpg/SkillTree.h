#pragma once
#include "SkillDefinitions.h"
#include <array>
#include <algorithm>
#include <limits>

namespace ARPG::SkillTree {
struct Requirement { int skill = -1; int rank = 1; };
struct Node {
    int skill, column, row;
    std::array<Requirement, 2> parents{};
};
// [TIME] | [INFO] | SkillTree.Nodes | Stable skill IDs are arranged by branch rather than their historical save order.
inline constexpr std::array<Node, ARPG_SKILL_COUNT_> nodes = {{
    {ARPG_SKILL_SWORD_CLEAVE,0,0},
    {ARPG_SKILL_SWORD_COMBO,1,0},
    {ARPG_SKILL_SWORD_CRIT,2,0},
    {ARPG_SKILL_SWORD_CROSSCUT,0,1,{{{ARPG_SKILL_SWORD_CLEAVE,1}}}},
    {ARPG_SKILL_SWORD_SHOCKWAVE,1,1,{{{ARPG_SKILL_SWORD_COMBO,1}}}},
    {ARPG_SKILL_SWORD_LUNGE,2,1,{{{ARPG_SKILL_SWORD_CRIT,1}}}},
    {ARPG_SKILL_SWORD_QUICKSPIN,3,1,{{{ARPG_SKILL_SWORD_COMBO,1}}}},
    {ARPG_SKILL_SWORD_CYCLONE,0,2,{{{ARPG_SKILL_SWORD_CROSSCUT,1}}}},
    {ARPG_SKILL_SWORD_BEAM,1,2,{{{ARPG_SKILL_SWORD_SHOCKWAVE,1}}}},
    {ARPG_SKILL_SWORD_RESOLVE,2,2,{{{ARPG_SKILL_SWORD_LUNGE,1}}}},
    {ARPG_SKILL_SWORD_MASTERY,3,2,{{{ARPG_SKILL_SWORD_QUICKSPIN,1}}}},
    {ARPG_SKILL_SWORD_BLADESTORM,0,3,{{{ARPG_SKILL_SWORD_CYCLONE,1}}}},
    {ARPG_SKILL_SWORD_EXECUTE,2,3,{{{ARPG_SKILL_SWORD_MASTERY,1},{ARPG_SKILL_SWORD_RESOLVE,1}}}},
    {ARPG_SKILL_SWORD_TEMPO,3,3,{{{ARPG_SKILL_SWORD_MASTERY,1}}}},
    {ARPG_SKILL_SWORD_FINESSE,3,4,{{{ARPG_SKILL_SWORD_TEMPO,1}}}},
    {ARPG_SKILL_MAGIC_ECONOMY,0,0},
    {ARPG_SKILL_MAGIC_RECOVERY,2,0},
    {ARPG_SKILL_MAGIC_FLAME_BURST,0,1,{{{ARPG_SKILL_MAGIC_ECONOMY,1}}}},
    {ARPG_SKILL_MAGIC_FROST_NOVA,1,1,{{{ARPG_SKILL_MAGIC_RECOVERY,1}}}},
    {ARPG_SKILL_MAGIC_RECALL,2,1,{{{ARPG_SKILL_MAGIC_RECOVERY,1}}}},
    {ARPG_SKILL_MAGIC_INFUSE,3,1,{{{ARPG_SKILL_MAGIC_ECONOMY,1}}}},
    {ARPG_SKILL_MAGIC_METEOR,0,2,{{{ARPG_SKILL_MAGIC_FLAME_BURST,2}}}},
    {ARPG_SKILL_MAGIC_CHAIN,1,2,{{{ARPG_SKILL_MAGIC_FROST_NOVA,1}}}},
    {ARPG_SKILL_MAGIC_CHANNEL,2,2,{{{ARPG_SKILL_MAGIC_RECALL,1}}}},
    {ARPG_SKILL_MAGIC_RESONANCE,3,2,{{{ARPG_SKILL_MAGIC_INFUSE,1}}}},
    {ARPG_SKILL_MAGIC_HOLY_AURA,2,3,{{{ARPG_SKILL_MAGIC_CHANNEL,1}}}},
    {ARPG_SKILL_GUARD_TOUGHNESS,0,0},
    {ARPG_SKILL_GUARD_DODGE,1,0},
    {ARPG_SKILL_GUARD_PROFIT,3,0},
    {ARPG_SKILL_GUARD_BULWARK,0,1,{{{ARPG_SKILL_GUARD_TOUGHNESS,1}}}},
    {ARPG_SKILL_GUARD_RIPOSTE,1,1,{{{ARPG_SKILL_GUARD_DODGE,1}}}},
    {ARPG_SKILL_GUARD_BLOODSTONE,2,1,{{{ARPG_SKILL_GUARD_TOUGHNESS,1}}}},
    {ARPG_SKILL_GUARD_FORTUNE,3,1,{{{ARPG_SKILL_GUARD_PROFIT,1}}}},
    {ARPG_SKILL_GUARD_IRON_WILL,0,2,{{{ARPG_SKILL_GUARD_BULWARK,1}}}},
    {ARPG_SKILL_GUARD_VENGEANCE,1,2,{{{ARPG_SKILL_GUARD_RIPOSTE,1}}}},
    {ARPG_SKILL_GUARD_SECOND_WIND,2,2,{{{ARPG_SKILL_GUARD_BLOODSTONE,1}}}},
    {ARPG_SKILL_GUARD_WARCRY,0,3,{{{ARPG_SKILL_GUARD_IRON_WILL,1}}}},
}};
inline const Node* Find(int skill) {
    for (const auto& node : nodes) { if (node.skill == skill) { return &node; } }
    return nullptr;
}
inline bool Prerequisites(const ARPGSaveData& save, int skill) {
    const auto* node = Find(skill);
    if (!node) { return false; }
    for (auto parent : node->parents) {
        if (parent.skill >= 0 && save.skillRanks[parent.skill] < parent.rank) { return false; }
    }
    return true;
}
struct Synergy { int source, target, percent; };
inline constexpr Synergy synergies[] = {
    {ARPG_SKILL_SWORD_CROSSCUT,ARPG_SKILL_SWORD_CLEAVE,4},
    {ARPG_SKILL_SWORD_CLEAVE,ARPG_SKILL_SWORD_CROSSCUT,5},
    {ARPG_SKILL_SWORD_LUNGE,ARPG_SKILL_SWORD_CROSSCUT,4},
    {ARPG_SKILL_SWORD_CROSSCUT,ARPG_SKILL_SWORD_CYCLONE,4},
    {ARPG_SKILL_SWORD_QUICKSPIN,ARPG_SKILL_SWORD_CYCLONE,10},
    {ARPG_SKILL_SWORD_CRIT,ARPG_SKILL_SWORD_LUNGE,3},
    {ARPG_SKILL_SWORD_MASTERY,ARPG_SKILL_SWORD_SHOCKWAVE,5},
    {ARPG_SKILL_SWORD_SHOCKWAVE,ARPG_SKILL_SWORD_BEAM,5},
    {ARPG_SKILL_MAGIC_INFUSE,ARPG_SKILL_SWORD_BEAM,4},
    {ARPG_SKILL_SWORD_CYCLONE,ARPG_SKILL_SWORD_BLADESTORM,6},
    {ARPG_SKILL_SWORD_BEAM,ARPG_SKILL_SWORD_BLADESTORM,5},
    {ARPG_SKILL_SWORD_MASTERY,ARPG_SKILL_SWORD_EXECUTE,6},
    {ARPG_SKILL_SWORD_CROSSCUT,ARPG_SKILL_SWORD_EXECUTE,5},
    {ARPG_SKILL_MAGIC_ECONOMY,ARPG_SKILL_MAGIC_FLAME_BURST,4},
    {ARPG_SKILL_MAGIC_RECOVERY,ARPG_SKILL_MAGIC_FROST_NOVA,4},
    {ARPG_SKILL_MAGIC_FROST_NOVA,ARPG_SKILL_MAGIC_CHAIN,5},
    {ARPG_SKILL_MAGIC_INFUSE,ARPG_SKILL_MAGIC_CHAIN,4},
    {ARPG_SKILL_MAGIC_FLAME_BURST,ARPG_SKILL_MAGIC_METEOR,6},
    {ARPG_SKILL_MAGIC_RESONANCE,ARPG_SKILL_MAGIC_METEOR,4},
    {ARPG_SKILL_GUARD_BULWARK,ARPG_SKILL_GUARD_RIPOSTE,4},
    {ARPG_SKILL_SWORD_RESOLVE,ARPG_SKILL_GUARD_RIPOSTE,4},
    {ARPG_SKILL_GUARD_RIPOSTE,ARPG_SKILL_GUARD_VENGEANCE,5},
};
// [TIME] | [INFO] | SkillTree.Synergy | Only learned skill ranks contribute; bonuses are additive and never recursively feed one another.
inline int SynergyPercent(const ARPGSaveData& save, int skill) {
    int bonus = 0;
    for (auto link : synergies) {
        if (link.target == skill) { bonus += link.percent * std::clamp<int>(save.skillRanks[link.source],0,ARPG_SKILLS[link.source].maxRank); }
    }
    return bonus;
}
inline float Power(const ARPGSaveData& save, int skill) { return 1 + SynergyPercent(save,skill) / 100.0f; }
inline int Navigate(int skill, int dx, int dy) {
    const auto* current = Find(skill);
    if (!current || (!dx && !dy)) { return skill; }
    int best = skill, score = std::numeric_limits<int>::max();
    for (const auto& node : nodes) {
        if (ARPG_SKILLS[node.skill].tree != ARPG_SKILLS[skill].tree) { continue; }
        int x = node.column-current->column, y = node.row-current->row;
        if ((dx && x*dx<=0) || (dy && y*dy<=0)) { continue; }
        int distance = dx ? std::abs(x)*10+std::abs(y)*16 : std::abs(y)*10+std::abs(x)*16;
        if (distance < score) { best = node.skill; score = distance; }
    }
    return best;
}
}
