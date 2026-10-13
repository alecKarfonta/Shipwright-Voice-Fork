#include "SkillSystem.h"

// [TIME] | [INFO] | SkillSystem.cpp | XP curve, point spend/refund, combat
// queries, active-skill casting via vanilla magic consumption, per-frame upkeep

#include <algorithm>
#include <cmath>

#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Notification/Notification.h"
#include "ARPG.h"
#include "ARPGSave.h"
#include "ItemModifierSystem.h"
#include "ARPGRules.h"
#include "ARPGSpells.h"
#include "SkillRules.h"
#include "CombatInput.h"
#include "InventorySystem.h"
#include "SkillCombat.h"
#include "SkillAuras.h"
#include "SkillPatterns.h"
#include "ArsenalSystem.h"
#include <array>
#include "AffixSystem.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"

extern "C" {
#include "z64.h"
#include "variables.h"
#include "functions.h"
#include "macros.h"
extern PlayState* gPlayState;
}

// Multiplier applied to the next player melee hit (actives/bursts); consumed on use
static uint32_t sCastCooldown = 0;
static uint32_t sReflectCooldown = 0;
static uint32_t sAuraTimer = 0;
static float sPreviousCharge = 0.0f;
static bool sJumping = false;
static uint32_t sInfuseTimer = 0;  // frames remaining for Elemental Infusion
static std::array<int, ARPG_SKILL_COUNT_> sCooldown{}, sCooldownTotal{};
static int sMomentum = 0, sMomentumTimer = 0, sRallyTimer = 0, sRiposteTimer = 0, sWardTimer = 0;
static int sDodgeTimer = 0, sSecondWindCooldown = 0, sSecondWindHealing = 0, sSwordLock = 0;
static int sRequested = -1;
static uint32_t sLastBasicHit = 0, sLastWeaponHit = 0;
static int16_t sDodgeYaw = 0;
static uint32_t sMagicRegenTimer = 0;
static bool sMagicSpendPending = false;
static ARPG::ItemPowers::State sItemPowers;

// [TIME] | [INFO] | SkillSystem.SyncItemPowers | Native equipment, feature gates and gear changes invalidate prepared follow-ups immediately.
static void SyncItemPowers() {
    uint8_t mask=ARPG::SkillsEnabled()?ItemModifierSystem::EquippedMask():0;
    auto style=ARPG::Loot::StyleBonuses(gARPGSave,mask);
    ARPG::ItemPowers::Sync(sItemPowers,style.combatPowers,ARPG::ItemPowers::EquipmentKey(gARPGSave,mask));
}

// SECTION: progression

uint8_t SkillSystem::GetSkillRank(uint8_t skillId) {
    if (skillId >= ARPG_SKILL_COUNT_) {
        return 0;
    }
    return gARPGSave.skillRanks[skillId];
}

bool SkillSystem::HasSkill(uint8_t skillId) {
    return GetSkillRank(skillId) > 0;
}

void SkillSystem::GrantXp(uint32_t amount) {
    int activeBefore = gARPGSave.activePoints;
    uint8_t gained = ARPG::Rules::GrantXp(gARPGSave, amount);
    if (gained > 0) {
        Notification::Emit({
            .message = spdlog::fmt_lib::format("Level {}! +{} passive / +{} technique points", gARPGSave.level, gained, int(gARPGSave.activePoints) - activeBefore),
            .remainingTime = 4.0f,
        });
    }
}

void SkillSystem::OnBossDefeat() {
    gARPGSave.activePoints = std::min<int>(255, gARPGSave.activePoints + 2);
    Notification::Emit({
        .message = "Boss defeated! +2 technique points",
        .remainingTime = 5.0f,
    });
}

// [TIME] | [INFO] | SkillSystem.CanLearn | Level gates and tree investment give each branch a visible progression path.
bool SkillSystem::CanLearn(uint8_t id) {
    return ARPG::SkillsEnabled() && ARPG::SkillRules::CanLearn(gARPGSave, id);
}

bool SkillSystem::SpendPoint(uint8_t skillId) {
    if (!CanLearn(skillId)) {
        return false;
    }
    const ARPGSkillDef& def = ARPG_SKILLS[skillId];
    uint8_t rank = gARPGSave.skillRanks[skillId];
    if (rank >= def.maxRank) {
        return false;
    }
    // passive tree skills cost passive points; actives cost active points
    uint8_t& pool = def.passive ? gARPGSave.passivePoints : gARPGSave.activePoints;
    if (pool == 0) {
        return false;
    }
    pool--;
    gARPGSave.skillRanks[skillId]++;
    ARPG::Log("info", spdlog::fmt_lib::format("skill {} -> rank {}", def.name, gARPGSave.skillRanks[skillId]));
    return true;
}

void SkillSystem::RespecAll() {
    if (!ARPG::SkillsEnabled()) {
        return;
    }
    CancelEffects();
    ARPG::SkillCombat::CancelAll(gPlayState);
    for (int s = 0; s < ARPG_SKILL_COUNT_; s++) {
        uint8_t freeRank = (s == ARPG_SKILL_SWORD_CLEAVE || s == ARPG_SKILL_GUARD_DODGE) ? 1 : 0;
        uint8_t rank = std::max(0, int(gARPGSave.skillRanks[s]) - freeRank);
        gARPGSave.skillRunes[s] = 0;
        if (rank == 0) {
            continue;
        }
        if (ARPG_SKILLS[s].passive) {
            gARPGSave.passivePoints = std::min<int>(255, gARPGSave.passivePoints + rank);
        } else {
            gARPGSave.activePoints = std::min<int>(255, gARPGSave.activePoints + rank);
        }
        gARPGSave.skillRanks[s] = freeRank;
        gARPGSave.skillRunes[s] = 0;
    }
    ARPG::Log("info", "respec complete");
}

// SECTION: combat queries

float SkillSystem::GetMeleeDamageMult() {
    float mult = 1.0f + 0.08f * GetSkillRank(ARPG_SKILL_SWORD_COMBO);
    // Giant Mastery only when wielding the two-hander (Giant's Knife family)
    Player* player = gPlayState != nullptr ? GET_PLAYER(gPlayState) : nullptr;
    if (player != nullptr) {
        s8 sword = player->currentSwordItemId;
        if (sword == ITEM_SWORD_BGS || sword == ITEM_SWORD_KNIFE) {
            mult += 0.10f * GetSkillRank(ARPG_SKILL_SWORD_MASTERY);
        }
    }
    if (gSaveContext.health == gSaveContext.healthCapacity) {
        mult += 0.05f * GetSkillRank(ARPG_SKILL_SWORD_RESOLVE);
    }
    if (sRallyTimer > 0 && gARPGSave.skillRunes[ARPG_SKILL_GUARD_WARCRY] == 1) { mult *= 1.3f; }
    return mult * ARPG::SkillRules::LevelPower(gARPGSave.level);
}

float SkillSystem::GetCritChance() {
    // Keen Edge ranks + Keen gear power (percent)
    float chance = ARPG::SkillsEnabled() ? 3.0f * GetSkillRank(ARPG_SKILL_SWORD_CRIT) : 0.0f;
    chance += ItemModifierSystem::GetModPower(ARPG_MOD_KEEN);
    return std::clamp(chance, 0.0f, 75.0f);
}

bool SkillSystem::RollCrit() {
    float chance = GetCritChance();
    return chance > 0.0f && ARPG::RandFloat(0.0f, 100.0f) < chance;
}

int SkillSystem::GetDefenseBonus() {
    return GetSkillRank(ARPG_SKILL_GUARD_TOUGHNESS); // [TIME] | [INFO] | SkillSystem.GetDefenseBonus | One armor percentage point per rank.
}

float SkillSystem::GetDamageTakenMult() {
    float multiplier = 1.0f - 0.06f * GetSkillRank(ARPG_SKILL_GUARD_BULWARK);
    if (sRallyTimer > 0) { multiplier *= 0.65f - 0.05f * GetSkillRank(ARPG_SKILL_GUARD_IRON_WILL); }
    if (sRiposteTimer > 0) { multiplier *= gARPGSave.skillRunes[ARPG_SKILL_GUARD_RIPOSTE] == 2 ? 0.5f : 0.2f; }
    if (sWardTimer > 0) { multiplier *= 0.8f; }
    return multiplier;
}

float SkillSystem::GetLootBonusMult() {
    return 1.0f + 0.08f * GetSkillRank(ARPG_SKILL_GUARD_FORTUNE);
}

float SkillSystem::GetMagicCostMult() {
    float econ = ARPG::SkillsEnabled() ? 1.0f - 0.15f * GetSkillRank(ARPG_SKILL_MAGIC_ECONOMY) : 1.0f;
    if (ARPG::ItemsEnabled()) {
        econ *= (1.0f - ItemModifierSystem::GetMagicCostReduction());
    }
    return std::clamp(econ, 0.25f, 1.0f);
}

uint32_t SkillSystem::GetHpOnKill() {
    return 8 * GetSkillRank(ARPG_SKILL_GUARD_BLOODSTONE); // half heart per rank
}

uint32_t SkillSystem::GetRupeesOnKill() {
    return 10 * GetSkillRank(ARPG_SKILL_GUARD_PROFIT);
}

int SkillSystem::ScaledMagicCost(int cost) {
    return cost <= 0 ? 0 : std::max(1, (int)std::ceil(cost * GetMagicCostMult()));
}
ARPG::ItemPowers::State SkillSystem::ItemPowerState() { SyncItemPowers(); return sItemPowers; }
int SkillSystem::TechniqueMagicCost(int id) {
    if(!ARPG::SkillRules::Castable(id)) return 0;
    SyncItemPowers();
    return ARPG::ItemPowers::FreeSpell(sItemPowers,id)?0:ScaledMagicCost(ARPG::SkillRules::BaseMagicCost(id,gARPGSave.skillRunes[id]));
}
int SkillSystem::CooldownRemaining(int id) { return id >= 0 && id < ARPG_SKILL_COUNT_ ? sCooldown[id] : 0; }
int SkillSystem::CooldownTotal(int id) { return id >= 0 && id < ARPG_SKILL_COUNT_ ? sCooldownTotal[id] : 0; }
int SkillSystem::GlobalCooldown() { return sCastCooldown; }
int SkillSystem::Momentum() { return sMomentum; }
int SkillSystem::BuffRemaining(int id) {
    switch (id) {
        case ARPG_SKILL_MAGIC_INFUSE: return sInfuseTimer;
        case ARPG_SKILL_MAGIC_HOLY_AURA: return sAuraTimer;
        case ARPG_SKILL_GUARD_WARCRY: return sRallyTimer;
        case ARPG_SKILL_GUARD_RIPOSTE: return sRiposteTimer;
        case ARPG_SKILL_SWORD_BLADESTORM: return sWardTimer;
        default: return 0;
    }
}
float SkillSystem::InfusionPower() { return gARPGSave.skillRunes[ARPG_SKILL_MAGIC_INFUSE] == 2 ? 1.0f : 0.5f; }
std::string SkillSystem::CastRestriction(PlayState* play, int id) {
    using namespace ARPG::SkillRules;
    if (!ARPG::SkillsEnabled()) { return "Skills are disabled."; }
    if (!Castable(id)) { return "Choose an active technique."; }
    if (!HasSkill(id)) { return "Learn this technique in the skill journal [K]."; }
    SyncItemPowers();
    if (CooldownRemaining(id)) { return spdlog::fmt_lib::format("Recharging: {:.1f}s", CooldownRemaining(id) / 20.0f); }
    if (sCastCooldown) { return "Recovering from the last action."; }
    if (Sword(id) && sSwordLock) { return "Finishing the current sword technique."; }
    if (!play || !GameInteractor::IsPlayerInControl() || gSaveContext.health <= 0 || play->transitionTrigger != TRANS_TRIGGER_OFF) { return "Resume gameplay to cast."; }
    auto* player = GET_PLAYER(play);
    if (player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_CARRYING_ACTOR)) { return "Return to open ground first."; }
    if (player->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING || player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK) { return "Finish your spin attack first."; }
    if (!(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) { return "Land before using this technique."; }
    if (Sword(id) && CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == 0) { return "Equip a sword first."; }
    if (id == ARPG_SKILL_MAGIC_HOLY_AURA && gSaveContext.nayrusLoveTimer) { return "Protection is already active."; }
    if (id == ARPG_SKILL_MAGIC_RECALL && (!gSaveContext.fw.set || gSaveContext.fw.entranceIndex != gSaveContext.entranceIndex || gSaveContext.fw.roomIndex != play->roomCtx.curRoom.num)) { return "Set Farore's Wind in this room first."; }
    int cost = BaseMagicCost(id,gARPGSave.skillRunes[id]);
    if (cost && (!gSaveContext.isMagicAcquired || gSaveContext.magic < TechniqueMagicCost(id))) { return "Not enough magic."; }
    if (cost && gSaveContext.magicState != MAGIC_STATE_IDLE) { return "Magic meter is busy."; }
    return {};
}

// [TIME] | [INFO] | SkillSystem.TryCastActive | Cooldowns belong to skills, never slots; failed casts consume no resource or Momentum.
bool SkillSystem::TryCastActive(PlayState* play, uint8_t id) {
    using namespace ARPG::SkillRules;
    if (!CastRestriction(play, id).empty()) { return false; }
    auto* player = GET_PLAYER(play);
    int rune = gARPGSave.skillRunes[id];
    int cost = BaseMagicCost(id,rune);
    // The free spell bypasses only this technique's debit; native spells and other meter transactions retain their usual cost.
    if(ARPG::ItemPowers::FreeSpell(sItemPowers,id)) cost=0;
    if (cost && !Magic_RequestChange(play, cost, MAGIC_CONSUME_NOW)) { return false; }
    auto gearStyle=ItemModifierSystem::GetStyleBonuses();
    float power = RankPower(gARPGSave, id) * (Sword(id) ? MomentumPower(gARPGSave, sMomentum) : 1) * (1+gearStyle.power[ARPG_SKILLS[id].tree]/100.f);
    Vec3f castPosition=player->actor.world.pos;
    bool success = true;
    switch (id) {
        case ARPG_SKILL_MAGIC_INFUSE:
            sInfuseTimer = BuffDuration(gARPGSave,id); break;
        case ARPG_SKILL_GUARD_DODGE:
            sDodgeTimer = rune == 1 ? 8 : 6;
            sDodgeYaw = player->actor.shape.rot.y + 0x8000;
            player->invincibilityTimer = std::max<int>(player->invincibilityTimer, BuffDuration(gARPGSave,id));
            break;
        case ARPG_SKILL_GUARD_WARCRY:
            sRallyTimer = BuffDuration(gARPGSave,id);
            Health_ChangeBy(play, Healing(gARPGSave,id));
            sMomentum = std::min(5, sMomentum + 3); sMomentumTimer = 160;
            break;
        case ARPG_SKILL_GUARD_RIPOSTE: sRiposteTimer = BuffDuration(gARPGSave,id); break;
        case ARPG_SKILL_MAGIC_HOLY_AURA:
            success = ARPG::Spells::Cast(play, id);
            if (success) { sAuraTimer = BuffDuration(gARPGSave,id); }
            break;
        case ARPG_SKILL_MAGIC_RECALL:
            success = ARPG::Spells::Cast(play, id);
            if (success && rune == 1) { Health_ChangeBy(play, Healing(gARPGSave,id)); } break;
        default: success = ARPG::SkillCombat::Cast(play, id, rune, power); break;
    }
    if (!success) {
        if (cost) { gSaveContext.magicTarget = gSaveContext.magic; gSaveContext.magicState = MAGIC_STATE_IDLE; }
        return false;
    }
    ARPG::ItemPowers::Cast(sItemPowers,id);
    if (id==ARPG_SKILL_MAGIC_RECALL) {
        ARPG::SkillAuras::Start(play,id,rune,32,false,&castPosition);
        ARPG::SkillAuras::Start(play,id,rune,32);
    } else if (id==ARPG_SKILL_GUARD_DODGE) {
        ARPG::SkillAuras::Start(play,id,rune,rune==2?26:18,false,&castPosition);
    } else if (BuffRemaining(id)>0) {
        ARPG::SkillAuras::Start(play,id,rune,BuffRemaining(id),true);
    }
    if (cost) { sMagicSpendPending = true; }
    sCastCooldown = 8;
    sCooldown[id] = sCooldownTotal[id] = std::max(1,int(std::lround(Cooldown(gARPGSave, id)*(1-gearStyle.recovery[ARPG_SKILLS[id].tree]/100.f))));
    if (Sword(id)) {
        sMomentum = 0; sMomentumTimer = 0;
        sSwordLock = id == ARPG_SKILL_SWORD_CROSSCUT || id == ARPG_SKILL_SWORD_CYCLONE || id == ARPG_SKILL_SWORD_LUNGE ?
                     ARPG::SkillPatterns::Duration(id, rune) : 12;
        if (id == ARPG_SKILL_SWORD_LUNGE && rune != 2) { player->invincibilityTimer = std::max<int>(player->invincibilityTimer, 8); }
        if (id == ARPG_SKILL_SWORD_BLADESTORM && rune == 2) { sWardTimer = ARPG::SkillPatterns::Duration(id, rune); }
    }
    return true;
}
void SkillSystem::RequestCast(int id) { if (ARPG::SkillRules::Castable(id)) { sRequested = id; } }
bool SkillSystem::AssignSlot(int slot, int id) { return ARPG::SkillsEnabled() && ARPG::SkillRules::Assign(gARPGSave, slot, id); }
bool SkillSystem::SelectRune(int id, int rune) {
    if (!ARPG::SkillsEnabled() || !ARPG::SkillRules::CanRune(gARPGSave, id, rune) || CooldownRemaining(id) || BuffRemaining(id)) { return false; }
    gARPGSave.skillRunes[id] = rune;
    return true;
}
void SkillSystem::OnMeleeHit(Actor* source, Actor*, uint32_t flags, float damage) {
    if ((!ARPG::SkillsEnabled() && !ARPG::ItemsEnabled()) || !gPlayState || !source || damage<=0 ||
        GameInteractor::IsGameplayPaused() || gPlayState->transitionTrigger!=TRANS_TRIGGER_OFF) return;
    bool sword=source->id==ACTOR_PLAYER && (flags&DMG_SWORD) && !sSwordLock;
    bool weapon=sword || (source->id==ACTOR_PLAYER && (flags&(DMG_HAMMER|DMG_DEKU_STICK))) ||
        (source->id==ACTOR_EN_ARROW && (flags&(DMG_ARROW|DMG_SLINGSHOT)));
    if(weapon && gPlayState->gameplayFrames-sLastWeaponHit>=8) {
        sLastWeaponHit=gPlayState->gameplayFrames;SyncItemPowers();
        auto followUp=ARPG::ItemPowers::BasicHit(sItemPowers);
        if(followUp.swordRecovery) for(int id=0;id<ARPG_SKILL_COUNT_;++id)
            if(ARPG::SkillRules::Sword(id)) sCooldown[id]=std::max(0,sCooldown[id]-followUp.swordRecovery);
        if(followUp.healing && gSaveContext.health>0) Health_ChangeBy(gPlayState,followUp.healing);
    }
    if(!sword || gPlayState->gameplayFrames-sLastBasicHit<8) return;
    sLastBasicHit=gPlayState->gameplayFrames;
    if(ARPG::SkillsEnabled()) { sMomentum=std::min(5,sMomentum+1);sMomentumTimer=160; }
    int recovery=(ARPG::SkillsEnabled()?GetSkillRank(ARPG_SKILL_MAGIC_CHANNEL):0)+ItemModifierSystem::GetStyleBonuses().manaOnHit;
    if(recovery && gSaveContext.isMagicAcquired && gSaveContext.magicState==MAGIC_STATE_IDLE)
        gSaveContext.magic=std::min<int>(gSaveContext.magicCapacity,gSaveContext.magic+recovery);
}
void SkillSystem::OnDefendedHit() {
    if (!ARPG::SkillsEnabled() || !sRiposteTimer || !gPlayState) { return; }
    sRiposteTimer = 0;
    ARPG::SkillCombat::Cast(gPlayState, ARPG_SKILL_GUARD_RIPOSTE, gARPGSave.skillRunes[ARPG_SKILL_GUARD_RIPOSTE],
                          ARPG::SkillRules::RankPower(gARPGSave, ARPG_SKILL_GUARD_RIPOSTE) * ARPG::SkillRules::LevelPower(gARPGSave.level) * (1+ItemModifierSystem::GetStyleBonuses().power[ARPG_TREE_GUARDIAN]/100.f));
}

bool SkillSystem::IsInfuseActive() { return ARPG::SkillsEnabled() && sInfuseTimer > 0; }
bool SkillSystem::IsPerformingAction() { return ARPG::SkillsEnabled() && (sCastCooldown>0 || sSwordLock>0 || sDodgeTimer>0); }
bool SkillSystem::IsAuraActive() { return ARPG::SkillsEnabled() && sAuraTimer > 0 && gSaveContext.nayrusLoveTimer != 0; }

uint32_t SkillSystem::InfusionFlags() {
    int element = CVarGetInteger(CVAR_ENHANCEMENT("ARPG.InfusionElement"), 0);
    return element == 1 ? DMG_MAGIC_ICE : element == 2 ? DMG_MAGIC_LIGHT : DMG_MAGIC_FIRE;
}

void SkillSystem::OnIncomingHit(float damage) {
    if (!ARPG::SkillsEnabled() || sReflectCooldown > 0 || gPlayState == nullptr || damage <= 0) { return; }
    auto* player = GET_PLAYER(gPlayState);
    int vengeance = GetSkillRank(ARPG_SKILL_GUARD_VENGEANCE);
    if (IsAuraActive() || (vengeance > 0 && player->invincibilityTimer == 0)) {
        sReflectCooldown = 20;
        ARPG::SkillCombat::Cast(gPlayState, ARPG_SKILL_GUARD_VENGEANCE, 0, (IsAuraActive() ? (gARPGSave.skillRunes[ARPG_SKILL_MAGIC_HOLY_AURA] == 2 ? 2.0f : 1.0f) : 0.2f * vengeance) * (1+ItemModifierSystem::GetStyleBonuses().power[ARPG_TREE_GUARDIAN]/100.f));
    }
}

// SECTION: per-frame upkeep

void SkillSystem::Update() {
    SyncItemPowers();
    ARPG::ItemPowers::Tick(sItemPowers,true);
    // [TIME] | [INFO] | SkillSystem.MagicSpend | Finish our native meter transaction once its debit completes; spell lifetimes use independent timers.
    if (sMagicSpendPending && gSaveContext.magicState == MAGIC_STATE_METER_FLASH_1) {
        Magic_Reset(gPlayState);
        sMagicSpendPending = false;
    } else if (sMagicSpendPending && gSaveContext.magicState != MAGIC_STATE_CONSUME_SETUP && gSaveContext.magicState != MAGIC_STATE_CONSUME) {
        sMagicSpendPending = false;
    }
    if (sCastCooldown > 0) { --sCastCooldown; }
    if (sReflectCooldown > 0) { --sReflectCooldown; }
    if (sAuraTimer > 0 && --sAuraTimer == 0) { ARPG::Spells::CancelAura(gPlayState); }
    for (auto& cooldown : sCooldown) { if (cooldown > 0) { --cooldown; } }
    for (int* timer : { &sMomentumTimer, &sRallyTimer, &sRiposteTimer, &sWardTimer, &sSecondWindCooldown, &sSwordLock }) { if (*timer > 0) { --*timer; } }
    if (!sMomentumTimer) { sMomentum = 0; }
    if (sDodgeTimer > 0) { --sDodgeTimer; ARPG::SkillCombat::TryStep(gPlayState, 9, sDodgeYaw); }
    if (sRequested >= 0) {
        int id = sRequested; sRequested = -1;
        auto reason = CastRestriction(gPlayState, id);
        if (!TryCastActive(gPlayState, id)) {
            Notification::Emit({ .message = reason.empty() ? "No valid target or clear space for this skill." : reason, .remainingTime = 1.5f });
        }
    }
    int wind = GetSkillRank(ARPG_SKILL_GUARD_SECOND_WIND);
    if (wind && sSecondWindCooldown == 0 && gSaveContext.health < gSaveContext.healthCapacity * 0.35f) {
        sSecondWindCooldown = 600; sSecondWindHealing = wind * 16;
        ARPG::SkillAuras::Start(gPlayState,ARPG_SKILL_GUARD_SECOND_WIND,0,wind*80);
    }
    if (sSecondWindHealing > 0 && gPlayState->gameplayFrames % 10 == 0) { Health_ChangeBy(gPlayState, 2); sSecondWindHealing -= 2; }
    Player* player = GET_PLAYER(gPlayState);
    if (HasSkill(ARPG_SKILL_SWORD_QUICKSPIN) && (player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK)) {
        float delta = std::max(0.0f, player->unk_858 - sPreviousCharge);
        player->unk_858 = std::min(1.0f, player->unk_858 + delta);
        sPreviousCharge = player->unk_858;
    } else { sPreviousCharge = 0.0f; }
    if (player->meleeWeaponAnimation == PLAYER_MWA_JUMPSLASH_START && player->meleeWeaponState != 0) {
        sJumping = true;
    }
    if (sJumping && (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        sJumping = false;
        TryCastActive(gPlayState, ARPG_SKILL_SWORD_SHOCKWAVE);
    }
    if (sInfuseTimer > 0) {
        sInfuseTimer--;
    }
    // [TIME] | [INFO] | SkillSystem.Update | One magic unit per four seconds; each rank shortens the interval.
    uint8_t regenRank = GetSkillRank(ARPG_SKILL_MAGIC_RECOVERY);
    if (regenRank > 0 && gSaveContext.isMagicAcquired && gSaveContext.magicState == MAGIC_STATE_IDLE &&
        gSaveContext.magic < gSaveContext.magicCapacity) {
        sMagicRegenTimer++;
        uint32_t interval = 20 * 4 / (uint32_t)regenRank;
        if (sMagicRegenTimer >= interval) {
            sMagicRegenTimer = 0;
            s16 max = gSaveContext.magicCapacity;
            if ((s16)gSaveContext.magic < max) {
                gSaveContext.magic = static_cast<int8_t>(std::min((s16)(gSaveContext.magic + 1), max));
            }
        }
    }
}

void SkillSystem::CancelEffects() {
    sItemPowers={};
    ARPG::SkillAuras::CancelAll(gPlayState);
    if (sMagicSpendPending && gPlayState) {
        if (gSaveContext.magicState == MAGIC_STATE_CONSUME_SETUP || gSaveContext.magicState == MAGIC_STATE_CONSUME) {
            gSaveContext.magic = std::min<int>(gSaveContext.magic, gSaveContext.magicTarget);
            Magic_Reset(gPlayState);
        } else if (gSaveContext.magicState == MAGIC_STATE_METER_FLASH_1) { Magic_Reset(gPlayState); }
    }
    sMagicSpendPending = false;
    if (sAuraTimer > 0) { ARPG::Spells::CancelAura(gPlayState); }
    sReflectCooldown = sAuraTimer = sInfuseTimer = sMagicRegenTimer = 0;
    sPreviousCharge = 0; sJumping = false;
    sMomentum = sMomentumTimer = sRallyTimer = sRiposteTimer = sWardTimer = sDodgeTimer = sSwordLock = 0;
    sSecondWindHealing = 0; sRequested = -1;
}
void SkillSystem::ResetRuntime() {
    CancelEffects();
    sCastCooldown = 0; sCooldown.fill(0); sCooldownTotal.fill(0); sLastBasicHit = sLastWeaponHit = 0; sSecondWindCooldown = 0;
}
// [TIME] | [INFO] | SkillSystem.PrepareTestBuild | Testing presets spend real points through the same prerequisite checks as the journal.
void SkillSystem::PrepareTestBuild(int build) {
    RespecAll();
    gARPGSave.passivePoints = 49; gARPGSave.activePoints = 36;
    const int passives[3][6] = {
        { ARPG_SKILL_SWORD_COMBO, ARPG_SKILL_SWORD_CRIT, ARPG_SKILL_SWORD_TEMPO, ARPG_SKILL_SWORD_FINESSE, ARPG_SKILL_GUARD_BLOODSTONE, ARPG_SKILL_GUARD_TOUGHNESS },
        { ARPG_SKILL_MAGIC_ECONOMY, ARPG_SKILL_MAGIC_RECOVERY, ARPG_SKILL_MAGIC_CHANNEL, ARPG_SKILL_MAGIC_RESONANCE, ARPG_SKILL_SWORD_COMBO, ARPG_SKILL_GUARD_TOUGHNESS },
        { ARPG_SKILL_GUARD_TOUGHNESS, ARPG_SKILL_GUARD_BULWARK, ARPG_SKILL_GUARD_IRON_WILL, ARPG_SKILL_GUARD_SECOND_WIND, ARPG_SKILL_GUARD_BLOODSTONE, ARPG_SKILL_SWORD_COMBO }
    };
    const int loadouts[3][6] = {
        { ARPG_SKILL_SWORD_CLEAVE, ARPG_SKILL_SWORD_CROSSCUT, ARPG_SKILL_SWORD_CYCLONE, ARPG_SKILL_SWORD_LUNGE, ARPG_SKILL_SWORD_BLADESTORM, ARPG_SKILL_SWORD_EXECUTE },
        { ARPG_SKILL_SWORD_CLEAVE, ARPG_SKILL_MAGIC_FROST_NOVA, ARPG_SKILL_MAGIC_CHAIN, ARPG_SKILL_MAGIC_METEOR, ARPG_SKILL_MAGIC_INFUSE, ARPG_SKILL_GUARD_DODGE },
        { ARPG_SKILL_SWORD_CLEAVE, ARPG_SKILL_SWORD_SHOCKWAVE, ARPG_SKILL_GUARD_RIPOSTE, ARPG_SKILL_GUARD_WARCRY, ARPG_SKILL_MAGIC_HOLY_AURA, ARPG_SKILL_GUARD_DODGE }
    };
    build = std::clamp(build, 0, 2);
    auto train = [&](auto&& self, int id, int depth = 0) -> bool {
        if (depth >= ARPG_SKILL_COUNT_) { return false; }
        const auto* node = ARPG::SkillTree::Find(id);
        if (!node) { return false; }
        for (auto parent : node->parents) {
            while (parent.skill >= 0 && GetSkillRank(parent.skill) < parent.rank) {
                if (!self(self, parent.skill, depth + 1)) { return false; }
            }
        }
        return SpendPoint(id);
    };
    for (int pass = 0; pass < 5; ++pass) { for (int id : passives[build]) { train(train,id); } }
    if (build == 2) { for (int i = 0; i < 3; ++i) { SpendPoint(ARPG_SKILL_MAGIC_RECOVERY); } }
    for (int pass = 0; pass < 3; ++pass) { for (int id : loadouts[build]) { train(train,id); } }
    for (int slot = 0; slot < ARPG_ACTION_SLOTS; ++slot) { AssignSlot(slot, loadouts[build][slot]); }
    ResetRuntime();
}

// [TIME] | [INFO] | RegisterSkillHooks | Dynamic gates also cover console toggles and transitions between vanilla/rando saves.
static void RegisterSkillHooks() {
    COND_HOOK(OnPlayerHealthChange, true, [](int16_t amount) {
        if (amount < 0) {
            SyncItemPowers();
            if(gPlayState && gSaveContext.health>0 && !GameInteractor::IsGameplayPaused() && gPlayState->transitionTrigger==TRANS_TRIGGER_OFF)
                ARPG::ItemPowers::HealthLoss(sItemPowers,-int(amount));
            SkillSystem::OnIncomingHit(static_cast<float>(-amount));
        }
    });
    COND_HOOK(OnPlayerUpdate, true, []() {
        if (!ARPG::SkillsEnabled()) {
            SkillSystem::ResetRuntime();
        } else if (GameInteractor::IsPlayerInControl() && !GameInteractor::IsGameplayPaused()) {
            SkillSystem::Update();
        }
        SyncItemPowers();
        if(gPlayState && (gSaveContext.health<=0 || gPlayState->transitionTrigger!=TRANS_TRIGGER_OFF)) sItemPowers={};
    });
    COND_HOOK(OnPlayDestroy, true, []() { SkillSystem::CancelEffects(); });
    COND_HOOK(OnLoadFile, true, [](int32_t) { SkillSystem::ResetRuntime(); });
}

static RegisterShipInitFunc initFunc(RegisterSkillHooks);

// Route the six ability inputs before native item actions, without taking over C buttons.
extern "C" void ARPG_SkillInput(PlayState* play) {
    if (!play) { return; }
    Input* input = &play->state.input[0];
    const bool active=ARPG::SkillsEnabled() && GameInteractor::IsPlayerInControl() &&
        !GameInteractor::IsGameplayPaused() && !ARPG::Inventory::IsOpen();
    const auto routed=ARPG::CombatInput::Route(input->cur.button,input->press.button,input->rel.button,active);
    if(routed.slot>=0)SkillSystem::RequestCast(gARPGSave.actionSlots[routed.slot]);
    input->cur.button=routed.held;
    input->press.button=routed.pressed;
    input->rel.button=routed.released;
}
