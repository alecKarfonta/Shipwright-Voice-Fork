#pragma once
#include "SkillPatterns.h"
#include <set>

namespace ARPG::SkillDetails {
// [TIME] | [INFO] | SkillDetails.Inspect | The journal samples the same strike geometry as combat; rank previews never mutate the character.
struct Stats { float power = 0; int strikes = 0, cooldown = 0, duration = 0, healing = 0, cost = 0; };
inline Stats Inspect(const ARPGSaveData& character, int id, bool next = false) {
    if (!SkillRules::Castable(id)) { return {}; }
    auto save = character;
    save.skillRanks[id] = std::clamp(int(save.skillRanks[id]) + int(next), 1, int(ARPG_SKILLS[id].maxRank));
    int rune = save.skillRunes[id];
    Stats stats;
    stats.cooldown = SkillRules::Cooldown(save,id);
    stats.duration = SkillRules::BuffDuration(save,id);
    stats.healing = SkillRules::Healing(save,id);
    stats.cost = SkillRules::BaseMagicCost(id,rune);
    std::set<int> strikes;
    for (int age = 0; age < SkillPatterns::Duration(id,rune); ++age) {
        auto frame = SkillPatterns::Sample(id,rune,age);
        for (int i = 0; i < frame.count; ++i) {
            if (frame.shapes[i].active) {
                if (!stats.power) { stats.power = frame.shapes[i].power; }
                strikes.insert(frame.shapes[i].strike);
            }
        }
    }
    stats.strikes = static_cast<int>(strikes.size());
    if (id == ARPG_SKILL_MAGIC_CHAIN) {
        stats.power = 1.4f * (rune == 1 ? .8f : rune == 2 ? 1.8f : 1);
        stats.strikes = 1;
    }
    stats.power *= SkillRules::RankPower(save,id) * SkillTree::Power(save,id);
    return stats;
}
}
