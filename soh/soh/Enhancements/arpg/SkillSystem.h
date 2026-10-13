#pragma once

// [TIME] | [INFO] | SkillSystem.h | Hybrid progression: XP passives + boss actives

#include <stdint.h>

struct PlayState;
struct Actor;
#include <string>
#include "ItemPowerRules.h"

namespace SkillSystem {

// ---- Progression ----------------------------------------------------------

// Grants XP (already scaled by the XP-rate cvar). Handles level-ups.
void GrantXp(uint32_t amount);
// [TIME] | [INFO] | SkillSystem.OnBossDefeat | XPSystem calls this once per distinct boss to grant an active point.
void OnBossDefeat();
// Spend one point on a skill (validated against pools + maxRank). Returns
// success.
bool SpendPoint(uint8_t skillId);
// Refund all spent points.
void RespecAll();

uint8_t GetSkillRank(uint8_t skillId);

// ---- Combat-time effect queries -------------------------------------------

float GetMeleeDamageMult();     // Battle Flow + Giant Mastery (weapon-aware)
bool RollCrit();                // Keen Edge ranks + Keen gear mod
int GetDefenseBonus();          // Toughness (armor percentage points)
float GetDamageTakenMult();     // Bulwark (multiplicative reduction)
float GetLootBonusMult();       // Fortune Seeker
float GetMagicCostMult();       // Magic Economy
uint32_t GetHpOnKill();         // Bloodstone (health units (16 per heart) per kill)
uint32_t GetRupeesOnKill();     // Prospector
float GetCritChance();          // percent, for UI display
bool HasSkill(uint8_t skillId); // rank > 0

// Actives: consume magic through the vanilla magic system
bool TryCastActive(PlayState* play, uint8_t skillId);
// Elemental Infusion active-skill window
bool IsInfuseActive();
bool IsPerformingAction();
uint32_t InfusionFlags();
void OnIncomingHit(float damage);
bool IsAuraActive();
int ScaledMagicCost(int cost);
int TechniqueMagicCost(int id);
ARPG::ItemPowers::State ItemPowerState();
bool CanLearn(uint8_t skillId);

// [TIME] | [INFO] | SkillSystem.Update | Upkeep runs on unpaused player simulation updates.
void Update();
void ResetRuntime();
void CancelEffects();
int CooldownRemaining(int id);
int CooldownTotal(int id);
int GlobalCooldown();
int Momentum();
int BuffRemaining(int id);
float InfusionPower();
std::string CastRestriction(PlayState* play, int id);
void RequestCast(int id);
bool AssignSlot(int slot, int id);
bool SelectRune(int id, int rune);
void OnMeleeHit(Actor* source, Actor* target, uint32_t flags, float damage);
void OnDefendedHit();
void PrepareTestBuild(int build);


// Hook registration (XP on kill, defense application, actives)


} // namespace SkillSystem
