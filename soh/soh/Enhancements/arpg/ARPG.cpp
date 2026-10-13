#include "ARPG.h"
#include "ExpeditionSystem.h"
#include "ARPGBridge.h"
#include "ARPGSave.h"
#include "ARPGRules.h"
#include "AffixSystem.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
#include "variables.h"
}

#include "ItemModifierSystem.h"
#include "SkillSystem.h"
#include "SkillCombat.h"
#include "EnchantedSystem.h"
#include "ArsenalSystem.h"

// [TIME] | [INFO] | ARPG.cpp | Central module state: CVAR gates, seeded RNG,
// logging, and the C bridge used by engine code (z_parameter.c)

#include <algorithm>
#include <cmath>
#include <random>
#include <spdlog/spdlog.h>

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/cvar_prefixes.h"

namespace ARPG {

// SECTION: CVAR gates

// [TIME] | [INFO] | ARPG.Enabled | master switch; all sub-systems gated on this
bool Enabled() {
    return CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"), 1) != 0 &&
           GameInteractor::IsSaveLoaded(true) &&
           (gSaveContext.ship.quest.id != QUEST_RANDOMIZER ||
            CVarGetInteger(CVAR_ENHANCEMENT("ARPG.AllowRandomizer"), 0) != 0);
}

bool AffixesEnabled() {
    return Enabled() && CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Affixes"), 1) != 0;
}

bool ItemsEnabled() {
    return Enabled() && CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Items"), 0) != 0;
}

bool SkillsEnabled() {
    return Enabled() && CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Skills"), 0) != 0;
}

// SECTION: seeded RNG

// [TIME] | [INFO] | ARPG.RNG | Persist the integer generator state so loading continues the same roll stream.
uint32_t RandInt(uint32_t min, uint32_t max) {
    return Rules::RandomRange(gARPGSave.rngState, min, max);
}

float RandFloat(float min, float max) {
    if (max <= min) {
        return min;
    }
    float unit = (Rules::NextRandom(gARPGSave.rngState) >> 8) * (1.0f / 16777216.0f);
    return min + (max - min) * unit;
}

void SeedRng(uint32_t seed) {
    gARPGSave.seed = seed;
    gARPGSave.rngState = seed;
}

uint32_t CurrentSeed() {
    return gARPGSave.seed;
}

// SECTION: logging

void Log(const std::string& level, const std::string& msg) {
    if (level == "error") {
        SPDLOG_ERROR("[ARPG] {}", msg);
    } else if (level == "warn") {
        SPDLOG_WARN("[ARPG] {}", msg);
    } else {
        SPDLOG_INFO("[ARPG] {}", msg);
    }
}

// SECTION: save lifecycle

void InitFile() {
    // New file: derive a seed from the global RNG so each new adventure differs,
    // then persist it via the save section (ARPGSave.cpp handles storage).
    uint32_t seed = std::random_device{}();
    SeedRng(seed);
}

void OnFileLoaded() {
    Expeditions::OnFileLoaded();
    SkillSystem::ResetRuntime();
}

} // namespace ARPG

// SECTION: C bridge (called from engine .c code)

// [TIME] | [INFO] | ARPG_PlayerDamageReduction | Bulwark % + Toughness/Aegis
// percentage armor applied to incoming damage; ordinary hits always retain damage
extern "C" int ARPG_PlayerDamageReduction(int16_t* healthChange) {
    if (healthChange == nullptr || *healthChange >= 0) {
        return 0;
    }
    bool anySystem = ARPG::SkillsEnabled() || ARPG::ItemsEnabled();
    if (!anySystem) {
        return 0;
    }

    float mult = 1.0f;
    if (ARPG::SkillsEnabled()) {
        mult *= SkillSystem::GetDamageTakenMult();
    }
    int flat = 0;
    if (ARPG::SkillsEnabled()) {
        flat += SkillSystem::GetDefenseBonus();
    }
    if (ARPG::ItemsEnabled()) {
        flat += ItemModifierSystem::GetFlatDefense();
    }

    // Shared rules cap armor and combined mitigation; small hits remain meaningful.
    int scaled = ARPG::Rules::ReducedDamage(-(*healthChange), mult, flat);
    SkillSystem::OnDefendedHit();
    if (scaled <= 0) {
        return 1; // fully absorbed
    }
    *healthChange = (int16_t)-scaled;
    return 0;
}

extern "C" float ARPG_ActorMovementMultiplier(Actor* actor) {
    if (!ARPG::Enabled() || actor == nullptr) {
        return 1.0f;
    }
    if (actor->id == ACTOR_PLAYER) {
        float bonus = ARPG::ItemsEnabled() ? 1.0f + ItemModifierSystem::GetMoveSpeedBonus() : 1.0f;
        // [TIME] | [INFO] | ARPG_ActorMovementMultiplier.Chill | Frost Enchanted hits slow the player until the chill expires.
        return bonus * ARPG::Enchanted::PlayerChillMultiplier();
    }
    const auto* affix = AffixSystem::GetAffixData(actor);
    if (!ARPG::AffixesEnabled() || affix == nullptr || !EnemyDifficulty::IsCurated(actor->id)) {
        return ARPG::SkillCombat::MovementMultiplier(actor) * ARPG::Arsenal::MovementMultiplier(actor);
    }
    return ARPG::SkillCombat::MovementMultiplier(actor) * ARPG::Arsenal::MovementMultiplier(actor) * std::min(affix->speedMult / 256.0f, 2.25f / EnemyDifficulty_MovementMultiplier(actor));
}

extern "C" uint8_t ARPG_AccumulateDamage(float damage, uint8_t accumulated) {
    if (!ARPG::Enabled()) {
        return static_cast<uint8_t>(accumulated + damage);
    }
    return ARPG::Rules::ClampDamage(damage, accumulated);
}

extern "C" int16_t ARPG_MagicCost(int16_t amount) {
    return amount > 0 && (ARPG::SkillsEnabled() || ARPG::ItemsEnabled()) ? SkillSystem::ScaledMagicCost(amount) : amount;
}
