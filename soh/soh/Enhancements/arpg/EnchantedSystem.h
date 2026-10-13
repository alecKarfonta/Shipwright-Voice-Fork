#pragma once

// [TIME] | [INFO] | EnchantedSystem.h | Diablo-2 elemental elite auras
//
// D2's most iconic monster modifiers are the elemental "Enchanted" flags:
// Fire Enchanted burns on hit and explodes on death, Frost Enchanted chills,
// Storm Enchanted zaps. This header holds the pure, engine-free rules so the
// portable test suite can verify tuning; EnchantedSystem.cpp owns the hooks.
//
// Co-op parity: enchanted affixes are ordinary pool entries, so the shared
// room seed already guarantees both clients roll the same enchant. Procs and
// novas resolve locally against each player's own client, exactly like D2
// where every partymember is struck independently.

#include <algorithm>
#include <cstdint>
#include "AffixData.h"

struct Actor;

namespace ARPG::Enchanted {

// [TIME] | [INFO] | Enchanted.AffixIds | Indices into AffixSystem's name/def tables.
constexpr uint8_t AFFIX_FIRE_ENCHANTED = 11;
constexpr uint8_t AFFIX_FROST_ENCHANTED = 12;
constexpr uint8_t AFFIX_STORM_ENCHANTED = 13;

enum class Element : uint8_t { None = 0, Fire, Frost, Storm };

// [TIME] | [INFO] | Enchanted.ElementForAffix | One affix id -> element; unknown ids are None.
inline Element ElementForAffix(uint8_t affixId) {
    switch (affixId) {
        case AFFIX_FIRE_ENCHANTED: return Element::Fire;
        case AFFIX_FROST_ENCHANTED: return Element::Frost;
        case AFFIX_STORM_ENCHANTED: return Element::Storm;
        default: return Element::None;
    }
}

// First enchanted affix on the actor (an Elite can only roll one enchant because
// affixes are drawn without replacement from the pool).
inline Element EnchantOf(const ARPGAffixData& a) {
    for (int i = 0; i < a.affixCount && i < ARPG_AFFIX_SLOTS; ++i) {
        Element e = ElementForAffix(a.affixIds[i]);
        if (e != Element::None) {
            return e;
        }
    }
    return Element::None;
}

// SECTION: tuning (quarter-hearts and world units)

// [TIME] | [INFO] | Enchanted.NovaRadius | D2 corpse nova reach: Elite packs punish melee range.
inline int NovaRadius(int tier) {
    return tier == ARPG_TIER_ELITE ? 220 : tier == ARPG_TIER_CHAMPION ? 160 : 120;
}

// [TIME] | [INFO] | Enchanted.NovaDamage | Base nova damage (quarter-hearts) + level scaling.
inline int NovaDamage(int tier, int level) {
    int base = tier == ARPG_TIER_ELITE ? 20 : tier == ARPG_TIER_CHAMPION ? 12 : 8;
    return base + std::clamp(level, 0, 50) / 5;
}

// Fire Enchanted adds flat damage to every hit that connects (folded into the
// single hit so defense applies once and one damage number shows).
inline int FireBonusDamage(int tier) {
    return tier == ARPG_TIER_ELITE ? 6 : tier == ARPG_TIER_CHAMPION ? 4 : 2;
}

// Storm Enchanted zaps for bonus damage on a percentage of hits.
inline int StormBonusDamage() { return 6; }
inline int StormProcChancePct(int tier) { return tier == ARPG_TIER_ELITE ? 30 : 20; }

// Frost Enchanted chills movement; a refresh extends but cannot exceed the cap.
inline float ChillMultiplier() { return 0.62f; }
inline int ChillDurationFrames() { return 90; }
inline int ChillStackCapFrames() { return 150; }

// SECTION: engine-side API (implemented in EnchantedSystem.cpp)

// CVAR gate for enchanted auras (checks ARPG.Enchanted, default on).
bool EnabledGate();

// Called from AffixSystem's OnDamageActor when an enchanted enemy strikes the player.
// May fold bonus damage into *damage, apply chill, or trigger a storm zap.
void OnPlayerStruck(const ARPGAffixData* affix, float* damage);

// Death nova on defeat: element ring + damage to the local player if in radius.
void OnEnemyDefeat(::Actor* actor, const ARPGAffixData* affix);

// Player movement multiplier while chilled (1.0f when inactive).
float PlayerChillMultiplier();

} // namespace ARPG::Enchanted
