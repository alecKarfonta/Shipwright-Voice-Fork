#pragma once

// [TIME] | [INFO] | ItemModifierSystem.h | Procedural prefix/suffix gear modifiers

#include <stdint.h>
#include <string>
#include "ARPGSave.h"
#include "LootCatalog.h"

struct Actor;
struct PlayState;

namespace ItemModifierSystem {
// [TIME] | [INFO] | ItemModifierSystem.ResetDefeatCredit | Call only after a native enemy completes a new life, never on stun, healing or an interrupted death.
void ResetDefeatCredit(Actor* actor);

// Tuning default (CVAR: gEnhancements.ARPG.DropChance)
constexpr int DROP_CHANCE_DEFAULT = 18; // % chance an affixed enemy drops gear

// Display name for a modifier id ("Unknown" for invalid)
const char* ModName(uint8_t modId);
std::string DescribeModifier(const ARPGItemMod& mod);
struct PowerRange { int min; int max; };
PowerRange GetPowerRange(uint8_t modId, uint8_t rarity, bool defaults = false);
void SetPowerRange(uint8_t modId, uint8_t rarity, PowerRange range);

// ---- Combat-time effect queries (called from the damage pipeline) ----------

// Multiplier from equipped sword/hammer mods + relevant skills
float GetMeleeDamageMult();
// Multiplier from equipped bow mods
float GetRangedDamageMult();
// Rolls on-hit elemental procs (flame/frost/storm). Returns bonus multiplier
// (1.0 = no proc). May apply extra hit reactions to the target.
float RollElementalProcMult(Actor* target, uint32_t dmgFlags);
// Armor reduction percentage points for the player (Aegis mod), capped at 25
int GetFlatDefense();
// Movement speed bonus (Swift mod), as an additive multiplier (0.0 = none)
float GetMoveSpeedBonus();
// Loot find bonus (Lucky mod), percentage points
int GetLuckyBonus();
// Magic cost reduction (Magic Affinity mod), fraction 0..0.75
float GetMagicCostReduction();

// ---- Loot ------------------------------------------------------------------

// Rolls a new item instance for the slot the player currently has equipped
// (uses ARPG RNG for determinism). Caller collects it into the backpack.
ARPGItemInstance RollItemDrop(int tier, int requestedSlot = -1, int itemLevel = 0);
std::string ItemName(const ARPGItemInstance& item);
bool IsEquipped(const ARPGItemInstance& item);
bool WeaponOwned(int slot);
void CaptureWeapon(Actor* actor,int slot);
void CopyWeapon(Actor* child,Actor* parent);
const ARPGItemInstance* WeaponForActor(Actor* actor);
const ARPGItemInstance* WeaponForHit(Actor* source,uint32_t flags);
bool AcceptsElement(Actor* target,uint32_t flags);
float WeaponDamageMultiplier(Actor* target,Actor* source,uint32_t flags,bool* critical);
float BossWeaponMultiplier(Actor* target,Actor* source,uint32_t flags);
int GetModPower(uint8_t modId);
uint8_t EquippedMask();
ARPG::Loot::BuildStyle GetStyleBonuses();
bool GiveMaterial(int material, int count = 1);
void SpawnMaterialDrop(Actor* source, int material);
void SpawnItemDrop(Actor* source, const ARPGItemInstance& item);

// Builds + shows a pickup notification describing the dropped gear and
// [TIME] | [INFO] | ItemModifierSystem.GiveItemDrop | Returns false without consuming loot when the backpack is full.
bool GiveItemDrop(const ARPGItemInstance& inst);

// Hook registration (loot on defeat)


} // namespace ItemModifierSystem
