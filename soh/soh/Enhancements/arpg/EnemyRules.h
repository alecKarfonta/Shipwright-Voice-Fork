#pragma once
#include "ARPGRules.h"
#include "AffixData.h"
#include "EnemyRegions.h"

namespace ARPG::Enemies {
enum class PackKind : uint8_t { Warband, Champions, EliteGuard };
enum class Role : uint8_t { Vanguard, Flanker, Guard };
enum class Tactic : uint8_t { Approach, Flank, Guard, Evade, Attack, Recover, ReturnHome };
struct Stats { uint8_t level; uint16_t hp, damage, speed; };
inline Stats RollStats(uint32_t& rng, const Region& region, int tier) {
    const bool beginner = region.minLevel == 1 && region.maxLevel <= 3;
    if (beginner && tier != ARPG_TIER_BOSS) { tier = ARPG_TIER_NORMAL; }
    int level = std::clamp<int>(Rules::RandomRange(rng, region.minLevel, region.maxLevel) + (tier == ARPG_TIER_ELITE ? 3 : tier == ARPG_TIER_CHAMPION ? 1 : tier == ARPG_TIER_BOSS ? 4 : 0), 1, 50);
    int hp = (100 + (level - 1) * 4) * Rules::RandomRange(rng, 90, 112) / 100;
    int damage = (100 + (level - 1) * 2) * Rules::RandomRange(rng, 92, 108) / 100;
    int speed = Rules::RandomRange(rng, 94, 106);
    if (tier == ARPG_TIER_CHAMPION) { hp = hp * 155 / 100; damage = damage * 115 / 100; speed += 7; }
    if (tier == ARPG_TIER_ELITE) { hp = hp * 210 / 100; damage = damage * 135 / 100; speed += 10; }
    if (tier == ARPG_TIER_BOSS) { hp = hp * 120 / 100; damage = damage * 110 / 100; speed = 100; }
    if (beginner && tier != ARPG_TIER_BOSS) { hp = hp * 65 / 100; damage = damage * 60 / 100; }
    return { uint8_t(level), uint16_t(std::clamp(hp * 256 / 100, beginner ? 128 : 192, 1792)),
             uint16_t(std::clamp(damage * 256 / 100, beginner ? 128 : 192, 768)), uint16_t(speed * 256 / 100) };
}
inline int PackTier(PackKind kind, int member) {
    if (kind == PackKind::Champions && member < 3) { return ARPG_TIER_CHAMPION; }
    if (kind == PackKind::EliteGuard && member == 0) { return ARPG_TIER_ELITE; }
    return ARPG_TIER_NORMAL;
}
inline int LootPower(int rolled, int level) { return std::clamp((rolled * (75 + std::clamp(level, 1, 50) * 2) + 50) / 100, 1, 100); }
inline int GearChance(int base, int tier, int level, int luck) {
    int chance = tier == ARPG_TIER_NORMAL ? base / 6 : tier == ARPG_TIER_CHAMPION ? base / 2 : base + 7;
    return std::clamp(chance + std::clamp(level, 1, 50) / 15 + std::clamp(luck, 0, 20) / 4,
                      0, tier == ARPG_TIER_ELITE ? 45 : 25);
}
inline uint32_t LevelExperience(uint32_t base, int level) {
    return uint32_t(std::min<uint64_t>(UINT32_MAX, uint64_t(base) * (100 + (std::clamp(level, 1, 50) - 1) * 7) / 100));
}
struct Situation {
    float distance, homeDistance;
    bool lineOfSight, playerAttacking, playerShielding, playerRanged, injured, attackReady, slotAvailable;
    Role role;
};
// [TIME] | [INFO] | Enemies.Decide | Defensive reactions have a cooldown; committed attacks and native hit reactions remain interruptible only by the engine.
inline Tactic Decide(const Situation& s) {
    if (s.homeDistance > 1300) { return Tactic::ReturnHome; }
    if (!s.lineOfSight) { return Tactic::Guard; }
    if (s.playerAttacking && s.distance < 105 && s.attackReady) { return Tactic::Evade; }
    if (s.injured && !s.attackReady && s.distance < 110) { return Tactic::Recover; }
    if (s.distance > (s.playerRanged ? 85 : 120)) { return s.role == Role::Flanker && s.distance < 320 && !s.playerRanged && !s.attackReady ? Tactic::Flank : Tactic::Approach; }
    if (s.playerShielding && s.role != Role::Vanguard) { return Tactic::Flank; }
    if (s.attackReady && s.slotAvailable) { return Tactic::Attack; }
    return s.role == Role::Guard ? Tactic::Guard : Tactic::Flank;
}
}
