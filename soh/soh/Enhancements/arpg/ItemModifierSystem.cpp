#include "ItemModifierSystem.h"
#include "GearVisuals.h"
#include "AdventureItems.h"
#include "AdventureItemRules.h"

// [TIME] | [INFO] | ItemModifierSystem.cpp | Modifier tables, combat queries,
// loot rolls on enemy defeat, backpack collection + notification delivery

#include <algorithm>
#include <cmath>
#include <string>

#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Notification/Notification.h"
#include "ARPG.h"
#include "ARPGSave.h"
#include "AffixSystem.h"
#include "EnemySystem.h"
#include "AffixData.h"
#include "SkillSystem.h"
#include "ARPGRules.h"
#include "InventoryRules.h"
#include "CraftingRules.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/Enhancements/nametag.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"

extern "C" {
#include "z64.h"
#include "variables.h"
#include "functions.h"
#include "macros.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
extern PlayState* gPlayState;
}

// SECTION: modifier metadata

// [TIME] | [INFO] | sModDefs | name + power roll ranges per rarity
struct ModDef {
    const char* name;
    int16_t minPower[ARPG_RARITY_COUNT_]; // scale with rarity tier
    int16_t maxPower[ARPG_RARITY_COUNT_];
};

// Power semantics per mod (documented in tooltips):
//   BRUTAL  : +power% melee damage          ARCHERY: +power% ranged damage
// [TIME] | [INFO] | ItemModifierSystem.Power | Aegis reduces damage by percentage points.
//   KEEN    : +power% crit chance           AEGIS  : armor reduction (%)
//   FLAME/FROST/STORM: +power% proc chance  VITALITY: +power% max hearts
//   SWIFT   : +power*2% move speed          LUCKY  : +power% loot chance
//   MAGICAFF: -power*5% magic cost
static const ModDef sModDefs[ARPG_MOD_COUNT_] = {
    { "None", { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
    { "Brutal", { 5, 10, 15, 25 }, { 10, 20, 30, 45 } },
    { "Keen", { 2, 4, 6, 10 }, { 4, 8, 12, 20 } },
    { "Flame-Touched", { 5, 10, 15, 25 }, { 10, 20, 30, 45 } },
    { "Frost-Touched", { 5, 10, 15, 25 }, { 10, 20, 30, 45 } },
    { "Storm-Touched", { 5, 10, 15, 25 }, { 10, 20, 30, 45 } },
    { "Archery", { 5, 10, 15, 25 }, { 10, 20, 30, 45 } },
    { "Aegis", { 1, 2, 3, 5 }, { 2, 4, 6, 10 } },
    { "Vitality", { 4, 8, 12, 20 }, { 8, 16, 24, 40 } },
    { "Swift", { 3, 6, 9, 15 }, { 6, 12, 18, 30 } },
    { "Lucky", { 5, 10, 15, 25 }, { 10, 20, 30, 45 } },
    { "Magic Affinity", { 1, 2, 3, 4 }, { 2, 3, 4, 5 } },
    { "Wideburst", { 5,10,15,25 }, { 10,20,30,45 } },
    { "Far-reaching", { 5,10,15,25 }, { 10,20,30,45 } },
    { "Binding", { 5,10,15,25 }, { 10,20,30,50 } },
    { "Opportunist", { 5,10,15,25 }, { 10,20,30,50 } },
};

const char* ItemModifierSystem::ModName(uint8_t modId) {
    if (modId >= ARPG_MOD_COUNT_) {
        return "Unknown";
    }
    return sModDefs[modId].name;
}

// [TIME] | [INFO] | ItemModifierSystem.PowerRange | Overrides tune future rolls; existing saved equipment keeps its power.
static std::string PowerRangeKey(uint8_t modId, uint8_t rarity, const char* bound) {
    return spdlog::fmt_lib::format(CVAR_ENHANCEMENT("ARPG.ModifierPower.{}.{}.{}"), modId, rarity, bound);
}

ItemModifierSystem::PowerRange ItemModifierSystem::GetPowerRange(uint8_t modId, uint8_t rarity, bool defaults) {
    if (modId == ARPG_MOD_NONE || modId >= ARPG_MOD_COUNT_ || rarity >= ARPG_RARITY_COUNT_) { return { 0, 0 }; }
    const auto& def = sModDefs[modId];
    if (defaults) { return { def.minPower[rarity], def.maxPower[rarity] }; }
    int min = std::clamp(CVarGetInteger(PowerRangeKey(modId, rarity, "Min").c_str(), def.minPower[rarity]), 1, 100);
    int max = std::clamp(CVarGetInteger(PowerRangeKey(modId, rarity, "Max").c_str(), def.maxPower[rarity]), min, 100);
    return { min, max };
}

void ItemModifierSystem::SetPowerRange(uint8_t modId, uint8_t rarity, PowerRange range) {
    if (modId == ARPG_MOD_NONE || modId >= ARPG_MOD_COUNT_ || rarity >= ARPG_RARITY_COUNT_) { return; }
    range.min = std::clamp(range.min, 1, 100);
    range.max = std::clamp(range.max, range.min, 100);
    CVarSetInteger(PowerRangeKey(modId, rarity, "Min").c_str(), range.min);
    CVarSetInteger(PowerRangeKey(modId, rarity, "Max").c_str(), range.max);
}

std::string ItemModifierSystem::DescribeModifier(const ARPGItemMod& mod) {
    switch (mod.modId) {
        case ARPG_MOD_BRUTAL: return spdlog::fmt_lib::format("+{}% melee damage", mod.power);
        case ARPG_MOD_KEEN: return spdlog::fmt_lib::format("+{}% critical chance", mod.power);
        case ARPG_MOD_FLAME: return spdlog::fmt_lib::format("{}% fire-hit chance", mod.power);
        case ARPG_MOD_FROST: return spdlog::fmt_lib::format("{}% frost-hit chance", mod.power);
        case ARPG_MOD_STORM: return spdlog::fmt_lib::format("{}% light-hit chance", mod.power);
        case ARPG_MOD_ARCHERY: return spdlog::fmt_lib::format("+{}% ranged damage", mod.power);
        case ARPG_MOD_AEGIS: return spdlog::fmt_lib::format("{}% armor reduction (25% combined cap)", mod.power);
        case ARPG_MOD_VITALITY: return spdlog::fmt_lib::format("+{}% heart capacity (cap 20)", mod.power);
        case ARPG_MOD_SWIFT: return spdlog::fmt_lib::format("+{}% movement speed", mod.power * 2);
        case ARPG_MOD_LUCKY: return spdlog::fmt_lib::format("+{}% gear-drop chance", mod.power);
        case ARPG_MOD_MAGICAFF: return spdlog::fmt_lib::format("-{}% magic cost", mod.power * 5);
        case ARPG_MOD_BLAST: return spdlog::fmt_lib::format("+{}% blast radius (cap 75%)",mod.power);
        case ARPG_MOD_REACH: return spdlog::fmt_lib::format("+{}% tool travel (cap 100%)",mod.power);
        case ARPG_MOD_CONTROL: return spdlog::fmt_lib::format("+{}% setup duration (cap 100%)",mod.power);
        case ARPG_MOD_FOLLOWUP: return spdlog::fmt_lib::format("+{}% setup finisher damage (cap 100%)",mod.power);
        default: return "No modifier";
    }
}

// SECTION: combat effect queries

// [TIME] | [INFO] | SumModPower | sums power of equipped mods of a given id
int ItemModifierSystem::GetModPower(uint8_t modId) {
    if (!ARPG::ItemsEnabled()) {
        return 0;
    }
    int total=ARPG::Loot::TotalPower(gARPGSave,modId,EquippedMask());
    if(modId==ARPG_MOD_LUCKY)for(int slot=ARPG_SLOT_LENS;slot<=ARPG_SLOT_BOTTLE;++slot) {
        const auto& item=gARPGSave.weapons[slot-ARPG_SLOT_COUNT_];
        if(ARPG::InventoryRules::Valid(item) && WeaponOwned(slot))total+=ARPG::Loot::Power(item,modId);
    }
    return total;
}
uint8_t ItemModifierSystem::EquippedMask() {
    if(!ARPG::ItemsEnabled()) { return 0; }
    uint8_t mask=0; for(int slot=0;slot<ARPG_SLOT_COUNT_;++slot) { if(IsEquipped(gARPGSave.equipment[slot])) { mask|=1<<slot; } } return mask;
}
ARPG::Loot::BuildStyle ItemModifierSystem::GetStyleBonuses() { return ARPG::Loot::StyleBonuses(gARPGSave,EquippedMask()); }

bool ItemModifierSystem::WeaponOwned(int slot) {
    int item=ARPG::AdventureItems::NativeItem(slot);
    if(item==ITEM_NONE)return false;
    if(slot==ARPG_SLOT_OCARINA)return INV_CONTENT(ITEM_OCARINA_FAIRY)==ITEM_OCARINA_FAIRY || INV_CONTENT(ITEM_OCARINA_FAIRY)==ITEM_OCARINA_TIME;
    if(slot==ARPG_SLOT_BOTTLE) { for(int i=SLOT_BOTTLE_1;i<=SLOT_BOTTLE_4;++i)if(gSaveContext.inventory.items[i]!=ITEM_NONE)return true;return false; }
    return INV_CONTENT(item)==item || (slot==ARPG_SLOT_HOOKSHOT && INV_CONTENT(ITEM_HOOKSHOT)==ITEM_LONGSHOT);
}
struct WeaponImpact { Actor* source=nullptr;uint32_t flags=0,frame=UINT32_MAX;float multiplier=1; };
static ObjectExtension::Register<WeaponImpact> weaponImpactRegistration;
struct WeaponShot { ARPGItemInstance item{}; };
static ObjectExtension::Register<WeaponShot> weaponShotRegistration;
void ItemModifierSystem::CaptureWeapon(Actor* actor,int slot) {
    if(!actor || !ARPG::Loot::WeaponSlot(slot))return;
    ARPGItemInstance item{};item.slot=slot;
    if(ARPG::ItemsEnabled() && WeaponOwned(slot))item=gARPGSave.weapons[slot-ARPG_SLOT_COUNT_];
    ObjectExtension::GetInstance().Set<WeaponShot>(actor,WeaponShot{item});
}
void ItemModifierSystem::CopyWeapon(Actor* child,Actor* parent) {
    if(!child || !parent)return;
    if(auto* shot=ObjectExtension::GetInstance().Get<WeaponShot>(parent))ObjectExtension::GetInstance().Set<WeaponShot>(child,WeaponShot{*shot});
}
const ARPGItemInstance* ItemModifierSystem::WeaponForActor(Actor* actor) {
    if(!ARPG::ItemsEnabled() || !actor)return nullptr;
    auto* shot=ObjectExtension::GetInstance().Get<WeaponShot>(actor);
    return shot && ARPG::InventoryRules::Valid(shot->item) && WeaponOwned(shot->item.slot)?&shot->item:nullptr;
}
const ARPGItemInstance* ItemModifierSystem::WeaponForHit(Actor* source,uint32_t flags) {
    if(!ARPG::ItemsEnabled() || !source)return nullptr;
    int slot=source->id==ACTOR_EN_ARROW && (flags&DMG_ARROW)?ARPG_SLOT_BOW:
             source->id==ACTOR_EN_ARROW && (flags&DMG_SLINGSHOT)?ARPG_SLOT_SLINGSHOT:
             source->id==ACTOR_EN_BOM && (flags&DMG_EXPLOSIVE)?ARPG_SLOT_BOMBS:
             source->id==ACTOR_ARMS_HOOK && (flags&DMG_HOOKSHOT)?ARPG_SLOT_HOOKSHOT:
             source->id==ACTOR_EN_BOOM && (flags&DMG_BOOMERANG)?ARPG_SLOT_BOOMERANG:
             source->id==ACTOR_EN_M_FIRE1 && (flags&DMG_DEKU_NUT)?ARPG_SLOT_NUTS:
             source->id==ACTOR_PLAYER && (flags&DMG_HAMMER)?ARPG_SLOT_HAMMER:
             source->id==ACTOR_PLAYER && (flags&DMG_DEKU_STICK)?ARPG_SLOT_STICK:-1;
    if(slot==ARPG_SLOT_BOMBS) { auto* shot=WeaponForActor(source);if(shot && shot->slot==ARPG_SLOT_BOMBCHU)slot=ARPG_SLOT_BOMBCHU; }
    if(slot<0 || !WeaponOwned(slot))return nullptr;
    // A launched projectile retains its roll, including an empty roll, across gear swaps.
    if(ObjectExtension::GetInstance().Has<WeaponShot>(source)) {
        auto* item=WeaponForActor(source);return item && item->slot==slot?item:nullptr;
    }
    // Legacy/synthetic bow and hammer contacts keep their existing direct lookup.
    if(slot!=ARPG_SLOT_BOW && slot!=ARPG_SLOT_HAMMER && slot!=ARPG_SLOT_STICK)return nullptr;
    const auto* item=&gARPGSave.weapons[slot-ARPG_SLOT_COUNT_];
    return ARPG::InventoryRules::Valid(*item) && item->slot==slot ? item:nullptr;
}
bool ItemModifierSystem::AcceptsElement(Actor* target,uint32_t flags) {
    if(!target || AffixSystem::ResistanceForFlags(target,flags)<=0)return false;
    if(!target->colChkInfo.damageTable)return true;
    int bit=0;for(auto f=flags;f>1;f>>=1)++bit;
    return bit<32 && (target->colChkInfo.damageTable->table[bit]&15)>0;
}
float ItemModifierSystem::WeaponDamageMultiplier(Actor* target,Actor* source,uint32_t flags,bool* critical) {
    const auto* item=WeaponForHit(source,flags);if(!item)return 1;
    auto power=[&](int mod){return ARPG::Loot::Power(*item,mod);};
    float scale=1+std::clamp(power(ARPG::Loot::RangedSlot(item->slot)?ARPG_MOD_ARCHERY:ARPG_MOD_BRUTAL),0,200)/100.f;
    bool hadCritical=critical && *critical,rolledCritical=false;
    if(power(ARPG_MOD_KEEN)>0 && ARPG::RandInt(1,100)<=uint32_t(std::clamp(power(ARPG_MOD_KEEN),0,75))) {
        rolledCritical=true;
        if(!critical || !*critical)scale*=2;
        if(critical)*critical=true;
    }
    float elemental=0;
    for(auto pair:{std::pair{ARPG_MOD_FLAME,uint32_t(DMG_MAGIC_FIRE)},std::pair{ARPG_MOD_FROST,uint32_t(DMG_MAGIC_ICE)},std::pair{ARPG_MOD_STORM,uint32_t(DMG_MAGIC_LIGHT)}}) {
        if(power(pair.first)>0 && ARPG::RandInt(1,100)<=uint32_t(std::clamp(power(pair.first),0,100))) {
            // Seed variants use native arrow elements; magic-only immunity must not be silently bypassed.
            if(item->slot==ARPG_SLOT_SLINGSHOT) {
                if(pair.first==ARPG_MOD_FLAME)pair.second=DMG_ARROW_FIRE;
                if(pair.first==ARPG_MOD_FROST)pair.second=DMG_ARROW_ICE;
                if(!AcceptsElement(target,pair.second))continue;
            }
            float resistance=AffixSystem::ResistanceForFlags(target,pair.second);elemental+=.5f*resistance;
            if(target->colChkInfo.damageTable && resistance>0) {
                int bit=0;for(auto f=pair.second;f>1;f>>=1)++bit;
                auto entry=target->colChkInfo.damageTable->table[bit];if(entry&15)target->colChkInfo.damageEffect=entry>>4;
            }
        }
    }
    float result=scale*(1+elemental);
    if(target && (target->id==ACTOR_BOSS_GOMA || target->id==ACTOR_BOSS_DODONGO)) {
        ObjectExtension::GetInstance().Set<WeaponImpact>(target,WeaponImpact{source,flags,gPlayState->gameplayFrames,result*(hadCritical && rolledCritical?2.f:1.f)});
    }
    return result;
}
float ItemModifierSystem::BossWeaponMultiplier(Actor* target,Actor* source,uint32_t flags) {
    if(!ARPG::ItemsEnabled() || !source || !target)return 1;
    // These bosses resolve health in their own phase functions. Reuse the collision roll.
    if(auto* impact=ObjectExtension::GetInstance().Get<WeaponImpact>(target)) {
        if(impact->source==source && impact->flags==flags && impact->frame==gPlayState->gameplayFrames)return impact->multiplier;
    }
    bool critical=false;return WeaponDamageMultiplier(target,source,flags,&critical);
}
static RegisterShipInitFunc weaponShotInit([](){
    COND_HOOK(OnActorInit,true,[](void* ref){
        auto* actor=static_cast<Actor*>(ref);
        if(actor->id==ACTOR_EN_ARROW && actor->params>=0)ItemModifierSystem::CaptureWeapon(actor,actor->params==ARROW_SEED?ARPG_SLOT_SLINGSHOT:actor->params==ARROW_NUT?ARPG_SLOT_NUTS:ARPG_SLOT_BOW);
    });
});
bool ItemModifierSystem::IsEquipped(const ARPGItemInstance& item) {
    if(ARPG::Loot::WeaponSlot(item.slot))return ARPG::InventoryRules::Valid(item) && WeaponOwned(item.slot);
    return item.slot < ARPG_SLOT_COUNT_ && CUR_EQUIP_VALUE(item.slot) != 0 &&
           (item.baseEquip == 0 || CUR_EQUIP_VALUE(item.slot) == item.baseEquip);
}

float ItemModifierSystem::GetMeleeDamageMult() {
    return 1.0f + std::clamp(GetModPower(ARPG_MOD_BRUTAL), 0, 200) / 100.0f;
}

float ItemModifierSystem::GetRangedDamageMult() {
    return 1.0f + std::clamp(GetModPower(ARPG_MOD_ARCHERY), 0, 200) / 100.0f;
}

float ItemModifierSystem::RollElementalProcMult(Actor* target, uint32_t dmgFlags) {
    // Procs only trigger on melee hits in v1 (flame/frost/storm are sword mods)
    if (!(dmgFlags & (DMG_SWORD | DMG_HAMMER | DMG_DEKU_STICK))) {
        return 1.0f;
    }
    float bonus = 1.0f;
    struct ProcDef {
        uint8_t modId;
        uint32_t element;
    };
    static const ProcDef procs[] = {
        { ARPG_MOD_FLAME, DMG_MAGIC_FIRE },
        { ARPG_MOD_FROST, DMG_MAGIC_ICE },
        { ARPG_MOD_STORM, DMG_MAGIC_LIGHT },
    };
    for (const ProcDef& p : procs) {
        int power = GetModPower(p.modId);
        if (power <= 0) {
            continue;
        }
        if ((int)ARPG::RandInt(1, 100) <= power) {
            float resistance = AffixSystem::ResistanceForFlags(target, p.element);
            bonus += 0.5f * resistance;
            if (resistance > 0 && target->colChkInfo.damageTable != nullptr) {
                int bit = 0;
                for (uint32_t flags = p.element; flags > 1; flags >>= 1) { ++bit; }
                uint8_t entry = target->colChkInfo.damageTable->table[bit];
                if ((entry & 15) > 0) {
                    target->colChkInfo.damageEffect = entry >> 4;
                }
            }
        }
    }
    return bonus;
}

int ItemModifierSystem::GetFlatDefense() {
    return std::clamp(GetModPower(ARPG_MOD_AEGIS), 0, 25);
}

float ItemModifierSystem::GetMoveSpeedBonus() {
    return std::clamp(GetModPower(ARPG_MOD_SWIFT) * 2, 0, 60) / 100.0f;
}

int ItemModifierSystem::GetLuckyBonus() {
    return std::clamp(GetModPower(ARPG_MOD_LUCKY), 0, 100);
}

float ItemModifierSystem::GetMagicCostReduction() {
    int steps = std::clamp(GetModPower(ARPG_MOD_MAGICAFF), 0, 5);
    return steps * 0.05f;
}

// SECTION: loot

// [TIME] | [INFO] | RollItemDrop | rarity from source tier; mod pool filtered
// by slot so swords roll offensive mods, shields/tunics defensive
static ARPGItemInstance RollInternal(uint8_t rarity, int requestedSlot, int itemLevel) {
    ARPGItemInstance inst{};
    inst.rarity = rarity;
    inst.itemLevel = std::clamp(itemLevel, 1, 50);
    inst.modCount = 0;

    // slot: weighted toward the player's progression (swords most common)
    int slotRoll = (int)ARPG::RandInt(1, 100);
    int slot = slotRoll <= 45   ? ARPG_SLOT_SWORD
               : slotRoll <= 70 ? ARPG_SLOT_SHIELD
               : slotRoll <= 90 ? ARPG_SLOT_TUNIC
                                : ARPG_SLOT_BOOTS;
    int weapons[ARPG_WEAPON_SLOT_COUNT],weaponCount=0;
    for(int candidate=ARPG_SLOT_BOW;candidate<ARPG_LOOT_SLOT_COUNT;++candidate)if(ItemModifierSystem::WeaponOwned(candidate))weapons[weaponCount++]=candidate;
    if(requestedSlot<0 && weaponCount && ARPG::RandInt(1,100)<=30)slot=weapons[ARPG::RandInt(0,weaponCount-1)];
    if (requestedSlot >= 0 && requestedSlot < ARPG_LOOT_SLOT_COUNT) { slot = requestedSlot; }
    if(ARPG::Loot::WeaponSlot(slot) && !ItemModifierSystem::WeaponOwned(slot))return {};
    for (int i = 0; !ARPG::Loot::WeaponSlot(slot) && i < ARPG_SLOT_COUNT_ && CUR_EQUIP_VALUE(slot) == 0; ++i) {
        slot = (slot + 1) % ARPG_SLOT_COUNT_;
    }
    inst.slot = (uint8_t)slot;
    inst.baseEquip = ARPG::Loot::WeaponSlot(slot)?1:CUR_EQUIP_VALUE(slot);
    // [TIME] | [INFO] | RollInternal.Base | Roll among unlocked bases so the backpack offers real equipment choices.
    uint8_t bases[3];
    int baseCount = 0;
    for (int base = 1; !ARPG::Loot::WeaponSlot(slot) && base <= 3; ++base) {
        if (CHECK_OWNED_EQUIP(slot, base - 1)) { bases[baseCount++] = base; }
    }
    if (baseCount > 0) { inst.baseEquip = bases[ARPG::RandInt(0, baseCount - 1)]; }

    // mod count by rarity: common 0, magic 1, rare 2, unique 3
    int count = (int)rarity; // 0..3
    if (count > ARPG_ITEM_MOD_COUNT) {
        count = ARPG_ITEM_MOD_COUNT;
    }

    auto pool=ARPG::Loot::Pool(slot);
    int poolSize=pool.count;
    auto& available=pool.ids;
    for (int m = 0; m < count; ++m) {
        uint8_t modId = ARPG::Rules::TakeDistinct(gARPGSave.rngState, available.data(), poolSize, m);
        const auto range = ItemModifierSystem::GetPowerRange(modId, rarity);
        inst.mods[inst.modCount++] = { modId, (int16_t)ARPG::Enemies::LootPower(ARPG::RandInt(range.min, range.max), inst.itemLevel) };
    }
    ARPG::Loot::RollIdentity(inst,gARPGSave.rngState);
    return inst;
}

ARPGItemInstance ItemModifierSystem::RollItemDrop(int tier, int requestedSlot, int itemLevel) {
    // Rarity ladder by source tier: 1=Champion -> mostly Magic, 2=Elite ->
    // Rare bias, 3=Boss -> guaranteed Rare, 25% Unique
    uint8_t rarity = ARPG_RARITY_MAGIC;
    int roll = (int)ARPG::RandInt(1, 100);
    switch (tier) {
        case ARPG_TIER_NORMAL:
            rarity = roll <= 55 ? ARPG_RARITY_COMMON : roll <= 95 ? ARPG_RARITY_MAGIC : ARPG_RARITY_RARE;
            break;
        case ARPG_TIER_CHAMPION:
            rarity = roll <= 20 ? ARPG_RARITY_COMMON : (roll <= 85 ? ARPG_RARITY_MAGIC : ARPG_RARITY_RARE);
            break;
        case ARPG_TIER_ELITE:
            rarity = roll <= 90 ? ARPG_RARITY_RARE : ARPG_RARITY_UNIQUE;
            break;
        default: // boss
            rarity = roll <= 75 ? ARPG_RARITY_RARE : ARPG_RARITY_UNIQUE;
            break;
    }
    if (itemLevel <= 0) { itemLevel = ARPG::Enemies::Enabled() ? ARPG::Enemies::CurrentRegion().minLevel : 12; }
    return RollInternal(rarity, requestedSlot, itemLevel);
}

// [TIME] | [INFO] | GroundLoot | Item ownership remains on the collectible until the normal pickup predicate succeeds.
struct GroundLoot { ARPGItemInstance item; bool warnedFull = false; uint8_t material = 0; };
static ObjectExtension::Register<GroundLoot> groundLootRegister;
struct LootRewarded {};
static ObjectExtension::Register<LootRewarded> lootRewardedRegister;

void ItemModifierSystem::ResetDefeatCredit(Actor* actor) {
    if (actor != nullptr) { ObjectExtension::GetInstance().Remove<LootRewarded>(actor); }
}

static const Color_RGBA8 rarityColors[] = {
    { 210, 210, 210, 255 }, { 100, 150, 255, 255 }, { 255, 225, 90, 255 }, { 220, 160, 75, 255 }
};

std::string ItemModifierSystem::ItemName(const ARPGItemInstance& item) {
    static const char* suffixes[ARPG_MOD_COUNT_] = {
        "", "Slaughter", "Precision", "the Flame", "Frost", "Radiance", "Marksmanship",
        "Warding", "Vitality", "Haste", "Fortune", "the Magus", "Widebursts", "Reach", "Binding", "Opportunity"
    };
    static const char* bases[4][4] = {
        { "Sword", "Kokiri Sword", "Master Sword", "Giant's Sword" },
        { "Shield", "Deku Shield", "Hylian Shield", "Mirror Shield" },
        { "Tunic", "Kokiri Tunic", "Goron Tunic", "Zora Tunic" },
        { "Boots", "Kokiri Boots", "Iron Boots", "Hover Boots" }
    };
    if (item.slot >= ARPG_LOOT_SLOT_COUNT || item.baseEquip > 3) {
        return "Unknown equipment";
    }
    std::string name = ARPG::Loot::WeaponSlot(item.slot)?ARPG::Loot::WeaponNames[item.slot-ARPG_SLOT_BOW]:bases[item.slot][item.baseEquip];
    if(auto unique=ARPG::Loot::Unique(item)) { return unique->name; }
    if(auto set=ARPG::Loot::Set(item)) { return std::string(set->name)+" "+name; }
    if(int word=ARPG::Loot::Runework(item)) { return std::string(ARPG::Loot::Runeworks[word].name)+" "+name; }
    if (item.modCount > 0) {
        name = std::string(ModName(item.mods[0].modId)) + " " + name;
    }
    if (item.modCount > 1 && item.mods[1].modId > ARPG_MOD_NONE && item.mods[1].modId < ARPG_MOD_COUNT_) {
        name += std::string(" of ") + suffixes[item.mods[1].modId];
    }
    return name;
}

static constexpr uint16_t ARPG_PICKUP_TEXT = 0x9F00;
static bool hasPickupText = false;

bool ItemModifierSystem::GiveItemDrop(const ARPGItemInstance& inst) {
    if (!ARPG::ItemsEnabled() || !ARPG::InventoryRules::Valid(inst)) { return false; }
    if (!ARPG::InventoryRules::Collect(gARPGSave, inst)) {
        Notification::Emit({ .message = "Backpack full - make room before collecting gear.", .remainingTime = 4.0f });
        return false;
    }
    const char* colors[] = { "%w", "%b", "%y", "%r" };
    std::string text = std::string(colors[inst.rarity]) + ItemName(inst) + "%w";
    for (int m = 0; m < inst.modCount; ++m) { text += "&" + DescribeModifier(inst.mods[m]); }
    text += "^Added to your backpack.&Open Inventory to compare and equip.";
    CustomMessageManager::Instance->ClearMessageTable("arpg");
    CustomMessageManager::Instance->CreateMessage("arpg", ARPG_PICKUP_TEXT, CustomMessage(text));
    hasPickupText = true;
    if (CVarGetInteger(CVAR_ENHANCEMENT("ARPG.PickupText"), 0) && GameInteractor::IsPlayerInControl()) {
        Message_StartTextbox(gPlayState, ARPG_PICKUP_TEXT, nullptr);
    }
    auto color = ARPG::Loot::Set(inst)?Color_RGBA8{95,235,136,255}:rarityColors[inst.rarity];
    Notification::Emit({
        .message = "Found: " + ItemName(inst),
        .messageColor = ImVec4(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, 1.0f),
        .suffix = " | Backpack  [I]",
        .remainingTime = 6.0f,
    });
    return true;
}

static void DrawGroundLoot(Actor* actor, PlayState* play) {
    const auto* loot = ObjectExtension::GetInstance().Get<GroundLoot>(actor);
    if (loot == nullptr) { return; }
    if(loot->material) { ARPG::GearVisuals::DrawMaterial(play,actor,loot->material); return; }
    static const int models[4][4] = {
        { GID_SWORD_KOKIRI, GID_SWORD_KOKIRI, GID_SWORD_KOKIRI, GID_SWORD_BGS },
        { GID_SHIELD_HYLIAN, GID_SHIELD_DEKU, GID_SHIELD_HYLIAN, GID_SHIELD_MIRROR },
        { GID_TUNIC_GORON, GID_TUNIC_GORON, GID_TUNIC_GORON, GID_TUNIC_ZORA },
        { GID_BOOTS_IRON, GID_BOOTS_IRON, GID_BOOTS_IRON, GID_BOOTS_HOVER }
    };
    if (ARPG::GearVisuals::DrawLoot(play, actor, loot->item)) { return; }
    Matrix_Scale(15.0f, 15.0f, 15.0f, MTXMODE_APPLY);
    GetItem_Draw(play, ARPG::Loot::WeaponSlot(loot->item.slot)?ARPG::AdventureItems::PickupModel(loot->item.slot):models[loot->item.slot][loot->item.baseEquip]);
}

void ItemModifierSystem::SpawnItemDrop(Actor* source, const ARPGItemInstance& item) {
    if (gPlayState == nullptr || source == nullptr || !ARPG::InventoryRules::Valid(item)) { return; }
    Vec3f pos = source->world.pos;
    if (source->category == ACTORCAT_BOSS) {
        pos = GET_PLAYER(gPlayState)->actor.world.pos;
        pos.x += 45.0f;
    }
    EnItem00* collectible = Item_DropCollectible(gPlayState, &pos, ITEM00_RUPEE_BLUE);
    if (collectible == nullptr) {
        GiveItemDrop(item);
        return;
    }
    ObjectExtension::GetInstance().Set<GroundLoot>(&collectible->actor, GroundLoot{ item });
    collectible->actor.draw = DrawGroundLoot;
    collectible->unk_15A = 20 * 120;
    NameTagOptions options{};
    options.tag = "ARPGGear";
    options.yOffset = 30;
    options.scale = 0.33f;
    options.textColor = ARPG::Loot::Set(item)?Color_RGBA8{95,235,136,255}:rarityColors[item.rarity];
    NameTag_RegisterForActorWithOptions(&collectible->actor, ItemName(item).c_str(), options);
}

// [TIME] | [INFO] | ItemModifierSystem.Materials | Crafting ingredients are physical collectibles with an independent, bounded saved satchel.
bool ItemModifierSystem::GiveMaterial(int material,int count) {
    if(!ARPG::ItemsEnabled() || !ARPG::Crafting::Receive(gARPGSave,material,count)) { return false; }
    Notification::Emit({.message="Found: "+ARPG::Loot::MaterialName(material),.suffix=" | Forge [I]",.remainingTime=4.0f}); return true;
}
void ItemModifierSystem::SpawnMaterialDrop(Actor* source,int material) {
    if(!gPlayState || !source || !ARPG::Loot::SocketMaterial(material)) { return; }
    Vec3f pos=source->category==ACTORCAT_BOSS?GET_PLAYER(gPlayState)->actor.world.pos:source->world.pos;
    pos.x+=25;
    auto* collectible=Item_DropCollectible(gPlayState,&pos,ITEM00_RUPEE_BLUE);
    if(!collectible) { GiveMaterial(material); return; }
    GroundLoot loot{}; loot.material=material;
    ObjectExtension::GetInstance().Set<GroundLoot>(&collectible->actor,std::move(loot));
    collectible->actor.draw=DrawGroundLoot; collectible->unk_15A=20*120;
    NameTagOptions options{}; options.tag="ARPGGear"; options.yOffset=24; options.scale=.33f; options.textColor={150,235,230,255};
    NameTag_RegisterForActorWithOptions(&collectible->actor,ARPG::Loot::MaterialName(material).c_str(),options);
}

// [TIME] | [INFO] | UpdateVitality | Track only the added capacity so toggles and equipment switches preserve earned hearts.
static void UpdateVitality() {
    if (!GameInteractor::IsSaveLoaded(true)) { return; }
    int base = std::clamp<int>(gSaveContext.healthCapacity - gARPGSave.vitalityBonus, FULL_HEART_HEALTH, MAX_HEALTH);
    int power = ItemModifierSystem::GetModPower(ARPG_MOD_VITALITY);
    int bonus = std::min(MAX_HEALTH - base, ((base * power + 100 * FULL_HEART_HEALTH - 1) / (100 * FULL_HEART_HEALTH)) * FULL_HEART_HEALTH);
    gSaveContext.healthCapacity = base + bonus;
    gARPGSave.vitalityBonus = bonus;
    gSaveContext.health = std::min(gSaveContext.health, gSaveContext.healthCapacity);
}

static void RewardLoot(void* ref, bool boss) {
    Actor* actor = static_cast<Actor*>(ref);
    if (actor && !ARPG::Enemies::RewardEligible(actor)) { return; }
    auto& extensions = ObjectExtension::GetInstance();
    if (!ARPG::ItemsEnabled() || actor == nullptr || extensions.Has<LootRewarded>(actor)) { return; }
    extensions.Set<LootRewarded>(actor, LootRewarded{});
    const auto* affix = ARPG::AffixesEnabled() ? AffixSystem::GetAffixData(actor) : nullptr;
    int tier = boss ? ARPG_TIER_BOSS : (affix != nullptr ? affix->tier : ARPG_TIER_NORMAL);
    int level = affix != nullptr && affix->level ? affix->level : 12;
    if(int heal=ItemModifierSystem::GetStyleBonuses().lifeOnKill) { Health_ChangeBy(gPlayState,heal); }
    if(int material=ARPG::Loot::RollMaterial(gARPGSave.rngState,level,tier)) { ItemModifierSystem::SpawnMaterialDrop(actor,material); }
    if (!boss) {
        int chance = std::clamp(CVarGetInteger(CVAR_ENHANCEMENT("ARPG.DropChance"), ItemModifierSystem::DROP_CHANCE_DEFAULT), 0, 100);
        chance = ARPG::Enemies::GearChance(chance, tier, level, ItemModifierSystem::GetLuckyBonus());
        if (affix != nullptr) { chance += std::max(0, int(affix->lootBudget) - tier) * 2; }
        if (ARPG::SkillsEnabled()) { chance = std::lround(chance * SkillSystem::GetLootBonusMult()); }
        if ((int)ARPG::RandInt(1, 100) > std::clamp(chance, 0, tier == ARPG_TIER_ELITE ? 45 : 25)) { return; }
    }
    ItemModifierSystem::SpawnItemDrop(actor, ItemModifierSystem::RollItemDrop(tier, -1, level));
}

static void UpdateGroundLoot(void* ref) {
    auto* actor = static_cast<Actor*>(ref);
    const auto* loot = ObjectExtension::GetInstance().Get<GroundLoot>(actor);
    if (loot == nullptr || !ARPG::ItemsEnabled() || gPlayState->gameplayFrames % 8 != 0) { return; }
    Vec3f pos = actor->world.pos;
    pos.y += 10.0f;
    Vec3f velocity = { 0, 0.4f, 0 };
    Vec3f accel = {};
    auto prim = loot->material?Color_RGBA8{140,225,225,255}:ARPG::Loot::Set(loot->item)?Color_RGBA8{95,235,136,255}:rarityColors[loot->item.rarity];
    Color_RGBA8 env = { 30, 30, 70, 255 };
    EffectSsKiraKira_SpawnSmall(gPlayState, &pos, &velocity, &accel, &prim, &env);
}

static void RegisterItemHooks() {
    COND_ID_HOOK(OnActorUpdate, ACTOR_EN_ITEM00, true, UpdateGroundLoot);
    CustomMessageManager::Instance->AddCustomMessageTable("arpg");
    COND_ID_HOOK(OnOpenText, ARPG_PICKUP_TEXT, true, [](uint16_t*, bool* loadFromMessageTable) {
        if (!hasPickupText) { return; }
        auto message = CustomMessageManager::Instance->RetrieveMessage("arpg", ARPG_PICKUP_TEXT);
        message.AutoFormat();
        message.LoadIntoFont();
        *loadFromMessageTable = false;
    });
    COND_HOOK(OnEnemyDefeat, true, [](void* ref) {
        Actor* actor = static_cast<Actor*>(ref);
        if (EnemyDifficulty::IsEnemyActor(actor) && actor->category != ACTORCAT_BOSS) { RewardLoot(ref, false); }
    });
    COND_HOOK(OnBossDefeat, true, [](void* ref) { RewardLoot(ref, true); });
    COND_ID_HOOK(OnVanillaBehavior, VB_GIVE_ITEM_FROM_ITEM_00, true, [](GIVanillaBehavior, bool* should, va_list args) {
        auto* actor = va_arg(args, EnItem00*);
        auto* loot = ObjectExtension::GetInstance().Get<GroundLoot>(actor);
        if (loot == nullptr) { return; }
        *should = false;
        if (!ARPG::ItemsEnabled()) { return; }
        if(loot->material) {
            if(ItemModifierSystem::GiveMaterial(loot->material)) { Actor_Kill(&actor->actor); }
            else { actor->unk_15A=20*120; if(!loot->warnedFull) { loot->warnedFull=true; Notification::Emit({.message="This material stack is full. Spend ingredients in the forge.",.remainingTime=4.0f}); } }
            return;
        }
        if (ARPG::InventoryRules::FreeSlot(gARPGSave) < 0) {
            if (!loot->warnedFull) {
                Notification::Emit({ .message = "Backpack full - this item stays on the ground.", .remainingTime = 4.0f });
                loot->warnedFull = true;
            }
            actor->unk_15A = 20 * 120;
            return;
        }
        if (ItemModifierSystem::GiveItemDrop(loot->item)) { Actor_Kill(&actor->actor); }
    });
    COND_HOOK(OnPlayerUpdate, true, UpdateVitality);
}

static RegisterShipInitFunc initFunc(RegisterItemHooks);

namespace ARPG::AdventureItems {
int NativeItem(int slot) {
    constexpr int items[]={ITEM_BOW,ITEM_HAMMER,ITEM_BOMB,ITEM_SLINGSHOT,ITEM_HOOKSHOT,ITEM_BOOMERANG,ITEM_NUT,ITEM_BOMBCHU,ITEM_STICK,ITEM_LENS,ITEM_OCARINA_FAIRY,ITEM_BOTTLE};
    return Loot::WeaponSlot(slot)?items[slot-ARPG_SLOT_BOW]:ITEM_NONE;
}
int PickupModel(int slot) {
    constexpr int models[]={GID_BOW,GID_HAMMER,GID_BOMB,GID_SLINGSHOT,GID_HOOKSHOT,GID_BOOMERANG,GID_NUTS,GID_BOMBCHU,GID_STICK,GID_LENS,GID_OCARINA_FAIRY,GID_BOTTLE};
    return Loot::WeaponSlot(slot)?models[slot-ARPG_SLOT_BOW]:GID_BOMB;
}
}
extern "C" void ARPG_AdventureLaunch(Actor* actor,int slot) { ItemModifierSystem::CaptureWeapon(actor,slot); }
extern "C" void ARPG_AdventureCopy(Actor* child,Actor* parent) { ItemModifierSystem::CopyWeapon(child,parent); }
extern "C" int ARPG_AdventureTravel(Actor* actor,int frames) {
    auto* item=ItemModifierSystem::WeaponForActor(actor);return item?ARPG::AdventureItems::Travel(*item,frames):frames;
}
extern "C" float ARPG_AdventureBlastScale(Actor* actor) {
    auto* item=ItemModifierSystem::WeaponForActor(actor);return item?ARPG::AdventureItems::BlastScale(*item):1;
}

extern "C" int ARPG_AdventureSeedElement(Actor* actor) {
    const auto* item=ItemModifierSystem::WeaponForActor(actor);
    return ARPG::AdventureItems::IsUnique(item,13)?1:ARPG::AdventureItems::IsUnique(item,14)?2:0;
}
