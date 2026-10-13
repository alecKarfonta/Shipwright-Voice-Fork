#pragma once

// [TIME] | [INFO] | AffixSystem.h | Rolls + applies random affixes to enemies/bosses

#include <stdint.h>
#include <string>
#include "AffixData.h"

struct Actor;

namespace AffixSystem {
float ResistanceForFlags(Actor* actor, uint32_t dmgFlags);

// Tuning defaults (mirrored as CVARs in the ARPG window)
constexpr int ELITE_CHANCE_DEFAULT = 12; // % chance a spawn is Champion+
constexpr int CHAMPION_RATIO_DEFAULT = 70; // % of special spawns that are Champions (rest Elite)

// Enemy predicate shared with EnemyDifficulty (via EnemyUtil::IsEnemyActor)


// [TIME] | [INFO] | AffixSystem.GetAffixData | Regional rolls include normal enemies; null means the actor has not been configured.
ARPGAffixData* GetAffixData(Actor* actor);
std::string AffixNames(Actor* actor);
void Configure(Actor* actor, int tier = -1, uint16_t pack = 0, uint8_t member = 0);

// Damage multiplier lookup for the OnDamageActor hook:
// combines actor affix resist/vuln with player item/skill bonuses.
// Returns the combined multiplier (1.0f = unmodified).
// wasCrit/resistanceOut feed the floating damage numbers (yellow crits, grey resists).
float ComputeDamageMultiplier(Actor* target, uint32_t dmgFlags, bool* wasCrit = nullptr, float* resistanceOut = nullptr);

// Element group lookup for a DMG_* bit
int ElementGroupForFlags(uint32_t dmgFlags);

} // namespace AffixSystem
