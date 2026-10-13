#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGBridge.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/ARPGSaveCodec.h"
#include "soh/Enhancements/arpg/SkillSystem.h"
#include "soh/Enhancements/arpg/SkillCombat.h"
#include "soh/Enhancements/arpg/SkillPatterns.h"
#include "soh/Enhancements/arpg/SkillDetails.h"
#include "soh/Enhancements/arpg/SkillAuras.h"
#include "soh/Enhancements/arpg/AuraGeometry.h"
#include "soh/Enhancements/arpg/InventorySystem.h"
#include "soh/Enhancements/arpg/EnemySystem.h"
#include "soh/Enhancements/arpg/AffixSystem.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
extern PlayState* gPlayState;
extern GameState* gGameState;
void Sram_InitDebugSave(void);
void CollisionCheck_ApplyDamage(PlayState*, CollisionCheckContext*, Collider*, ColliderInfo*);
}
namespace {
int phase = 0, frames = 0, technique = 0, rune = 0;
std::vector<int> techniques;
std::filesystem::path resultPath;
nlohmann::json checks = nlohmann::json::array();
void ProbeUpdate(Actor*, PlayState*) {}
Actor probe{};
DamageTable probeTable{};
bool collisionVerified = false;
bool auraOnly = false;
bool costOnly = false;
uint32_t liveHitsBefore = 0;
int liveCast = 0;
int expectedMagic = 0;
void Check(bool condition, const std::string& label) {
    if (!condition) { throw std::runtime_error(label); }
    checks.push_back(label); SPDLOG_INFO("[ARPG Skills QA] PASS: {}", label);
    std::ofstream(resultPath) << nlohmann::json({ { "status", "running" }, { "checks", checks }, { "error", "" } }).dump(2);
}
void Result(const char* state, const std::string& error = "") {
    std::ofstream(resultPath) << nlohmann::json({ { "status", state }, { "checks", checks }, { "error", error } }).dump(2);
    phase = 99;
}
void ClearEffects() {
    ARPG::SkillCombat::CancelAll(gPlayState);
    SkillSystem::ResetRuntime();
    gSaveContext.magicState = MAGIC_STATE_IDLE; gSaveContext.magic = gSaveContext.magicTarget = gSaveContext.magicCapacity = 96;
    gSaveContext.isMagicAcquired = 1;
}
void LearnAll() {
    SkillSystem::GrantXp(ARPG::Rules::XpThreshold(19));
    for (int id = 0; id < ARPG_SKILL_COUNT_; ++id) { gARPGSave.skillRanks[id] = ARPG_SKILLS[id].maxRank; }
    gARPGSave.skillRanks[ARPG_SKILL_SWORD_CRIT] = 0;
    for (int slot = 0; slot < ARPG_SLOT_COUNT_; ++slot) { gARPGSave.equipment[slot] = {}; gARPGSave.equipment[slot].slot = slot; }
}
// [TIME] | [INFO] | SkillsQA.Collision | Run the production AT-vs-AC narrow phase and damage pipeline against a receiver placed inside a sampled volume.
void ProbeAttack(void* ref) {
    if (phase != 3 || collisionVerified) { return; }
    auto* source = static_cast<Actor*>(ref);
    if (source->id != ACTOR_EN_M_THUNDER) { return; }
    try {
        auto& ctx = gPlayState->colChkCtx;
        ColliderCylinder* strike = nullptr;
        for (int i = 0; i < ctx.colATCount; ++i) {
            if (ctx.colAT[i]->actor == source && ctx.colAT[i]->shape == COLSHAPE_CYLINDER) { strike = reinterpret_cast<ColliderCylinder*>(ctx.colAT[i]); break; }
        }
        if (!strike) { return; }
        probe.update = ProbeUpdate; probe.id = ACTOR_EN_TEST; probe.category = ACTORCAT_ENEMY; probe.colChkInfo.health = 200;
        probe.colChkInfo.damage = 0; probe.colChkInfo.damageTable = &probeTable;
        probe.world.pos = { float(strike->dim.pos.x), float(strike->dim.pos.y - 10), float(strike->dim.pos.z) };
        ObjectExtension::GetInstance().Free(&probe);
        int id=techniques[technique];
        float expected=ARPG::SkillDetails::Inspect(gARPGSave,id).power;
        if(ARPG_SKILLS[id].tree==ARPG_TREE_MAGIC) { expected*=ARPG::SkillRules::LevelPower(gARPGSave.level)*(1+.1f*gARPGSave.skillRanks[ARPG_SKILL_MAGIC_RESONANCE]); }
        if(id==ARPG_SKILL_GUARD_RIPOSTE) { expected*=ARPG::SkillRules::LevelPower(gARPGSave.level); }
        Check(std::abs(ARPG::SkillCombat::DamageMultiplier(source,&probe)-expected)<.001f,
              "Native attack power agrees with rank and synergy preview before target modifiers");
        ColliderCylinder receiver{};
        ColliderCylinderInit init = {
            { COLTYPE_HIT0, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_NONE, OC2_TYPE_1, COLSHAPE_CYLINDER },
            { ELEMTYPE_UNK0, { 0, 0, 0 }, { 0xFFFFFFFF, 0, 0 }, TOUCH_NONE, BUMP_ON, OCELEM_NONE },
            { 25, 70, 0, { 0, 0, 0 } }
        };
        Collider_InitCylinder(gPlayState, &receiver); Collider_SetCylinder(gPlayState, &receiver, &probe, &init);
        Collider_UpdateCylinder(&probe, &receiver);
        auto saved = ctx;
        ctx.colATCount = 1; ctx.colAT[0] = &strike->base;
        ctx.colACCount = 1; ctx.colAC[0] = &receiver.base;
        CollisionCheck_AT(gPlayState, &ctx);
        bool hit = receiver.base.acFlags & AC_HIT;
        CollisionCheck_Damage(gPlayState, &ctx);
        auto damage = probe.colChkInfo.damage;
        bool noDuplicate = !ARPG_SkillCollision(gPlayState, &strike->base, &receiver.base);
        ctx = saved;
        strike->base.at = nullptr; strike->info.atHit = nullptr; strike->info.atHitInfo = nullptr;
        strike->base.atFlags &= ~AT_HIT; strike->info.toucherFlags &= ~(TOUCH_HIT | TOUCH_DREW_HITMARK);
        Collider_DestroyCylinder(gPlayState, &receiver);
        Check(hit && damage > 0, std::string(ARPG_SKILLS[techniques[technique]].name) + " rune " + std::to_string(rune) + " intersects native collision and deals damage");
        Check(noDuplicate, "Repeated limb contact is suppressed for this strike");
        collisionVerified = true;
    } catch (const std::exception& e) { Result("failed", e.what()); }
}
void Run() {
    if (phase == 99) { return; }
    try {
        ++frames;
        if (gPlayState && GameInteractor::IsSaveLoaded(true)) { gSaveContext.health = gSaveContext.healthCapacity; }
        if (phase == 0 && frames > 60 && gGameState) {
            CVarSetInteger(CVAR_DEVELOPER_TOOLS("DebugSaveFileMode"), 2);
            gSaveContext.gameMode = GAMEMODE_NORMAL; gSaveContext.fileNum = 0xFE; Sram_InitDebugSave();
            gSaveContext.gameMode = GAMEMODE_NORMAL; gSaveContext.fileNum = 0xFF; gSaveContext.linkAge = LINK_AGE_ADULT;
            gSaveContext.cutsceneIndex = gSaveContext.sceneLayer = gSaveContext.respawnFlag = 0;
            gSaveContext.entranceIndex = ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;
            gSaveContext.dayTime = gSaveContext.skyboxTime = 0x8000;
            gGameState->running = false; SET_NEXT_GAMESTATE(gGameState, Play_Init, PlayState);
            for (const char* key : { CVAR_ENHANCEMENT("ARPG.Enabled"), CVAR_ENHANCEMENT("ARPG.Items"), CVAR_ENHANCEMENT("ARPG.Skills") }) { CVarSetInteger(key, 1); }
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Affixes"), 0);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"), 0);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.InventoryOnPause"), 0);
            CVarSetInteger(CVAR_WINDOW("ARPG"), 0);
            for (auto& value : probeTable.table) { value = 4; }
            for (int id = 0; id < ARPG_SKILL_COUNT_; ++id) { if (ARPG::SkillRules::Castable(id)) { techniques.push_back(id); } }
            phase = 1; frames = 0; return;
        }
        if (phase == 1 && frames > 90 && GameInteractor::IsPlayerInControl()) {
            Check(gARPGSave.skillRanks[ARPG_SKILL_SWORD_CLEAVE] == 1 && gARPGSave.skillRanks[ARPG_SKILL_GUARD_DODGE] == 1, "New characters start with Cleave and Evasion");
            SkillSystem::GrantXp(ARPG::Rules::XpThreshold(19));
            for(int build=0;build<3;++build) {
                SkillSystem::PrepareTestBuild(build);
                for(int id:gARPGSave.actionSlots) {
                    Check(ARPG::SkillRules::Castable(id) && gARPGSave.skillRanks[id]>0 && ARPG::SkillTree::Prerequisites(gARPGSave,id),
                          "Testing build "+std::to_string(build)+" has a learned action with all branch prerequisites");
                }
            }
            LearnAll(); ClearEffects();
            if(auraOnly) { phase=12;frames=0;return; }
            gSaveContext.isMagicAcquired = 0; gSaveContext.magic = 0;
            Check(SkillSystem::TryCastActive(gPlayState, ARPG_SKILL_SWORD_CLEAVE), "Sword technique works before acquiring magic");
            int cd = SkillSystem::CooldownRemaining(ARPG_SKILL_SWORD_CLEAVE);
            Check(cd > 0 && !SkillSystem::TryCastActive(gPlayState, ARPG_SKILL_SWORD_CLEAVE), "Each skill starts and enforces its own cooldown");
            Check(SkillSystem::AssignSlot(4, ARPG_SKILL_SWORD_CLEAVE) && SkillSystem::CooldownRemaining(ARPG_SKILL_SWORD_CLEAVE) == cd, "Moving a cooling skill to another slot preserves its timer");
            Check(!SkillSystem::SelectRune(ARPG_SKILL_SWORD_CLEAVE, 1), "Cooling techniques cannot change runes");
            int before = cd;
            int pause = gPlayState->pauseCtx.state; gPlayState->pauseCtx.state = 6;
            GameInteractor_ExecuteOnPlayerUpdate();
            Check(SkillSystem::CooldownRemaining(ARPG_SKILL_SWORD_CLEAVE) == before, "Paused player updates do not advance cooldowns");
            gPlayState->pauseCtx.state = pause;
            phase = 2; frames = 0; return;
        }
        if (phase == 2 && frames > 25 && GameInteractor::IsPlayerInControl()) {
            Check(SkillSystem::TryCastActive(gPlayState, ARPG_SKILL_SWORD_CROSSCUT), "A different sword skill casts while Cleave is still recharging");
            Check(SkillSystem::CooldownRemaining(ARPG_SKILL_SWORD_CLEAVE) > 0, "Independent skill timers overlap during a rotation");
            ClearEffects();
            auto savedInput = gPlayState->state.input[0];
            SkillSystem::AssignSlot(4, ARPG_SKILL_SWORD_CLEAVE);
            gPlayState->state.input[0].cur.button = BTN_L | BTN_CLEFT;
            gPlayState->state.input[0].press.button = BTN_CLEFT;
            ARPG_SkillInput(gPlayState);
            Check(!(gPlayState->state.input[0].press.button & BTN_CLEFT) && !(gPlayState->state.input[0].cur.button & BTN_CLEFT), "Controller skill chords consume the native C-button item action");
            SkillSystem::Update();
            Check(SkillSystem::CooldownRemaining(ARPG_SKILL_SWORD_CLEAVE) > 0, "Controller chord queues its assigned technique through the simulation");
            gPlayState->state.input[0] = savedInput;
            ClearEffects();
            auto* player = GET_PLAYER(gPlayState);
            Actor source{}; source.id = ACTOR_PLAYER;
            for (int i = 0; i < 5; ++i) { gPlayState->gameplayFrames += 8; SkillSystem::OnMeleeHit(&source, &probe, DMG_SLASH_MASTER, 4); }
            Check(SkillSystem::Momentum() == 5, "Successful basic sword hits build capped Momentum");
            Check(SkillSystem::TryCastActive(gPlayState, ARPG_SKILL_SWORD_CLEAVE) && SkillSystem::Momentum() == 0, "A sword technique consumes the accumulated Momentum");
            ClearEffects();
            uint32_t retaliation = ARPG::SkillCombat::Casts(ARPG_SKILL_GUARD_VENGEANCE);
            player->invincibilityTimer = 0;
            SkillSystem::OnIncomingHit(16);
            Check(ARPG::SkillCombat::Casts(ARPG_SKILL_GUARD_VENGEANCE) > retaliation && ARPG::SkillPatterns::Duration(ARPG_SKILL_GUARD_VENGEANCE, 0) >= 10, "Vengeance creates a retaliation effect with a complete strike lifetime");
            ClearEffects();
            float mult = SkillSystem::GetMeleeDamageMult(); uint8_t level = gARPGSave.level; gARPGSave.level = 1;
            Check(mult > SkillSystem::GetMeleeDamageMult(), "Character levels strengthen basic melee damage"); gARPGSave.level = level;
            gSaveContext.fw.set = 1; gSaveContext.fw.entranceIndex = gSaveContext.entranceIndex; gSaveContext.fw.roomIndex = gPlayState->roomCtx.curRoom.num;
            gSaveContext.fw.pos = { s32(player->actor.world.pos.x), s32(player->actor.world.pos.y), s32(player->actor.world.pos.z) };
            gSaveContext.fw.yaw = player->actor.shape.rot.y;
            gARPGSave.skillRanks[ARPG_SKILL_MAGIC_RECOVERY] = 0;
            expectedMagic = gSaveContext.magic - SkillSystem::ScaledMagicCost(ARPG_SKILLS[ARPG_SKILL_MAGIC_FROST_NOVA].magicCost);
            Check(SkillSystem::TryCastActive(gPlayState, ARPG_SKILL_MAGIC_FROST_NOVA), "Spell rotation begins with Frost Nova");
            phase = 9; frames = 0; return;
        }
        if (phase == 9 && frames > 35 && GameInteractor::IsPlayerInControl()) {
            Check(gSaveContext.magicState == MAGIC_STATE_IDLE, "Native magic meter returns to idle after the skill debit");
            Check(gSaveContext.magic == expectedMagic, "First spell deducts exactly its discounted magic cost");
            expectedMagic -= SkillSystem::ScaledMagicCost(ARPG_SKILLS[ARPG_SKILL_MAGIC_FLAME_BURST].magicCost);
            Check(SkillSystem::CooldownRemaining(ARPG_SKILL_MAGIC_FROST_NOVA) > 0 && SkillSystem::TryCastActive(gPlayState, ARPG_SKILL_MAGIC_FLAME_BURST), "A second spell casts while the first cools down, without resetting the magic meter");
            phase = 10; frames = 0; return;
        }
        if (phase == 10 && frames > 35 && GameInteractor::IsPlayerInControl()) {
            Check(gSaveContext.magicState == MAGIC_STATE_IDLE, "Consecutive spell transactions both finish normally");
            Check(gSaveContext.magic == expectedMagic, "Odd discounted costs stop at the exact magic target");
            ClearEffects();
            gSaveContext.magic = SkillSystem::ScaledMagicCost(ARPG_SKILLS[ARPG_SKILL_MAGIC_FLAME_BURST].magicCost) + 1;
            Check(SkillSystem::TryCastActive(gPlayState, ARPG_SKILL_MAGIC_FLAME_BURST), "Discounted spell casts with exactly one spare magic unit");
            phase = 11; frames = 0; return;
        }
        if (phase == 11 && frames > 35 && GameInteractor::IsPlayerInControl()) {
            Check(gSaveContext.magicState == MAGIC_STATE_IDLE && gSaveContext.magic == 1, "A low-magic cast preserves the final unspent unit");
            if(costOnly) { ClearEffects();Result("passed");return; }
            gARPGSave.skillRanks[ARPG_SKILL_MAGIC_RECOVERY] = ARPG_SKILLS[ARPG_SKILL_MAGIC_RECOVERY].maxRank;
            ClearEffects(); phase = 4; frames = 0; return;
        }
        if (phase == 4 && frames > 25 && GameInteractor::IsPlayerInControl()) {
            if (technique >= int(techniques.size())) { phase = 5; frames = 0; return; }
            ClearEffects();
            int id = techniques[technique];
            gARPGSave.skillRunes[id] = rune;
            if (id == ARPG_SKILL_MAGIC_CHAIN) {
                auto* p = GET_PLAYER(gPlayState); auto pos = p->actor.world.pos;
                Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_DEKUBABA, pos.x + 160, pos.y, pos.z, 0, 0, 0, 0);
            }
            Check(SkillSystem::TryCastActive(gPlayState, id), std::string(ARPG_SKILLS[id].name) + " rune " + std::to_string(rune) + " casts through the production skill API");
            if (id == ARPG_SKILL_GUARD_RIPOSTE) {
                Check(SkillSystem::GetDamageTakenMult() < 0.4f, "Riposte applies its incoming damage reduction");
                int16_t damage = -64; ARPG_PlayerDamageReduction(&damage);
                Check(SkillSystem::BuffRemaining(id) == 0 && ARPG::SkillCombat::Casts(id) > 0, "Riposte consumes one hit and spawns a counterattack");
            }
            if(ARPG::AuraGeometry::Supported(id)) {
                Check(ARPG::SkillAuras::LiveCount(gPlayState)>0,"Utility technique creates its original in-world mesh actor");
            }
            collisionVerified = !ARPG::SkillRules::Sword(id) && id != ARPG_SKILL_MAGIC_FLAME_BURST && id != ARPG_SKILL_MAGIC_FROST_NOVA && id != ARPG_SKILL_MAGIC_CHAIN && id != ARPG_SKILL_MAGIC_METEOR && id != ARPG_SKILL_GUARD_RIPOSTE;
            phase = 3; frames = 0; return;
        }
        if (phase == 3 && frames > std::clamp(ARPG::SkillPatterns::Duration(techniques[technique], rune) + 6, 35, 85) && GameInteractor::IsPlayerInControl()) {
            Check(collisionVerified, std::string(ARPG_SKILLS[techniques[technique]].name) + " reaches its expected attack or utility state");
            if (ARPG_SKILLS[techniques[technique]].magicCost) {
                Check(gSaveContext.magicState == MAGIC_STATE_IDLE, "Spell and rune release the native magic meter for subsequent actions");
            }
            ClearEffects();
            Check(ARPG::SkillAuras::LiveCount(gPlayState)==0,"Ending the effect releases owned aura actors");
            if (++rune == 3) { rune = 0; ++technique; }
            phase = 4; frames = 0; return;
        }
        if (phase == 5 && frames > 30 && GameInteractor::IsPlayerInControl()) {
            auto snapshot = ARPG::SaveCodec::Encode(gARPGSave);
            gSaveContext.fileNum = 0; SaveManager::Instance->SaveFile(0);
            std::fill(std::begin(gARPGSave.skillRunes), std::end(gARPGSave.skillRunes), 0);
            std::fill(std::begin(gARPGSave.actionSlots), std::end(gARPGSave.actionSlots), ARPG_SKILL_NONE);
            SaveManager::Instance->ThreadPoolWait(); SaveManager::Instance->LoadFile(0);
            auto restored = ARPG::SaveCodec::Encode(gARPGSave);
            if (restored != snapshot) { SPDLOG_ERROR("[ARPG Skills QA] Save difference: {}", nlohmann::json::diff(snapshot, restored).dump()); }
            Check(restored == snapshot, "Asynchronous native saves preserve ranks, runes, loadout, and migration state");
            gSaveContext.fileNum = 0xFF;
            ClearEffects();
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Affixes"), 1); CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"), 1);
            Check(ARPG::Enemies::SpawnPack(ARPG::Enemies::PackKind::Champions, true) >= 3, "Live combat test spawns a real champion pack");
            liveHitsBefore = 0;
            for (int id : techniques) { liveHitsBefore += ARPG::SkillCombat::Hits(id); }
            phase = 6; frames = 0; return;
        }
        if (phase == 6 && GameInteractor::IsPlayerInControl()) {
            if (frames > 120 && frames % 50 == 0) {
                static const int rotation[] = { ARPG_SKILL_SWORD_CYCLONE, ARPG_SKILL_SWORD_CLEAVE, ARPG_SKILL_SWORD_CROSSCUT, ARPG_SKILL_SWORD_BLADESTORM, ARPG_SKILL_SWORD_SHOCKWAVE };
                SkillSystem::TryCastActive(gPlayState, rotation[liveCast++ % 5]);
            }
            if (frames > 480) {
                uint32_t after = 0; for (int id : techniques) { after += ARPG::SkillCombat::Hits(id); }
                Check(after > liveHitsBefore, "Sword rotation hits real native champion-pack enemies during live AI combat");
                ARPG::Enemies::ClearPacks(); ClearEffects();
                gPlayState->linkAgeOnLoad = LINK_AGE_CHILD; gPlayState->nextEntranceIndex = ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;
                gPlayState->transitionTrigger = TRANS_TRIGGER_START; gPlayState->transitionType = TRANS_TYPE_INSTANT;
                phase = 7; frames = 0; return;
            }
        }
        if (phase == 7 && frames > 100 && GameInteractor::IsPlayerInControl()) {
            Check(gSaveContext.linkAge == LINK_AGE_CHILD, "Skill testing transitions safely to child Link");
            ClearEffects();
            Check(SkillSystem::TryCastActive(gPlayState, ARPG_SKILL_SWORD_CLEAVE), "Child Link casts the sword technique with child equipment");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"), 0);
            phase = 8; frames = 0; return;
        }
        if(phase==12 && GameInteractor::IsPlayerInControl()) {
            gSaveContext.health=16;
            SkillSystem::Update();
            Check(ARPG::SkillAuras::LiveCount(gPlayState)==1,"Second Wind creates a healing feather effect at low health");
            SkillSystem::RespecAll();
            Check(ARPG::SkillAuras::LiveCount(gPlayState)==0,"Respec immediately removes the passive healing effect");
            LearnAll();ClearEffects();
            for(int id=0;id<ARPG_SKILL_COUNT_;++id) {
                if(ARPG::AuraGeometry::Supported(id)) { Check(ARPG::SkillAuras::Start(gPlayState,id,0,120),"Original aura mesh starts for "+std::string(ARPG_SKILLS[id].name)); }
            }
            Check(ARPG::SkillAuras::Start(gPlayState,ARPG_SKILL_GUARD_DODGE,2,120),"Eight simultaneous visual actors are allowed");
            Check(!ARPG::SkillAuras::Start(gPlayState,ARPG_SKILL_GUARD_DODGE,2,120),"A ninth aura is rejected by the actor budget");
            phase=20;frames=0;return;
        }
        if(phase==20 && frames>25 && GameInteractor::IsPlayerInControl()) {
            Check(ARPG::SkillAuras::LiveCount(gPlayState)==8,"All seven original forms survive native update and drawing");
            SkillSystem::RespecAll();
            Check(ARPG::SkillAuras::LiveCount(gPlayState)==0,"Respec cancels every original aura actor");
            LearnAll();ClearEffects();
            Check(ARPG::SkillAuras::Start(gPlayState,ARPG_SKILL_MAGIC_INFUSE,0,120),"Transition test begins with an active infusion seal");
            gPlayState->linkAgeOnLoad=LINK_AGE_CHILD;gPlayState->nextEntranceIndex=ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;
            gPlayState->transitionTrigger=TRANS_TRIGGER_START;gPlayState->transitionType=TRANS_TYPE_INSTANT;
            phase=21;frames=0;return;
        }
        if(phase==21 && frames>80 && GameInteractor::IsPlayerInControl()) {
            Check(gSaveContext.linkAge==LINK_AGE_CHILD && ARPG::SkillAuras::LiveCount(gPlayState)==0,"Scene change ends the owned aura before child Link loads");
            for(int id=0;id<ARPG_SKILL_COUNT_;++id) { if(ARPG::AuraGeometry::Supported(id)) { ARPG::SkillAuras::Start(gPlayState,id,1,35); } }
            ARPG::Inventory::Open();
            phase=22;frames=0;return;
        }
        if(phase==22 && frames>60) {
            Check(gPlayState->pauseCtx.state==6,"Aura pause check enters the normal initialized pause menu");
            Check(ARPG::SkillAuras::LiveCount(gPlayState)==7,"Paused simulation preserves all short-lived aura timers");
            ARPG::Inventory::Close();phase=23;frames=0;return;
        }
        if(phase==23 && frames>75 && GameInteractor::IsPlayerInControl()) {
            Check(ARPG::SkillAuras::LiveCount(gPlayState)==0,"Resuming gameplay expires every short-lived aura normally");
            for(int id=0;id<ARPG_SKILL_COUNT_;++id) { if(ARPG::AuraGeometry::Supported(id)) { ARPG::SkillAuras::Start(gPlayState,id,2,120); } }
            phase=24;frames=0;return;
        }
        if(phase==24 && frames>20 && GameInteractor::IsPlayerInControl()) {
            Check(ARPG::SkillAuras::LiveCount(gPlayState)==7,"Child Link updates and draws all seven aura forms");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"),0);phase=8;frames=0;return;
        }
        if (phase == 8 && frames > 25) {
            Check(ARPG::SkillCombat::LiveCount(gPlayState) == 0 && ARPG::SkillAuras::LiveCount(gPlayState)==0 && SkillSystem::Momentum() == 0, "Master-off removes skill effects and transient combat bonuses");
            Result("passed"); return;
        }
        if (frames > 2000) { throw std::runtime_error("Skills QA timed out at phase " + std::to_string(phase)); }
    } catch (const std::exception& e) { SPDLOG_ERROR("[ARPG Skills QA] {}", e.what()); Result("failed", e.what()); }
}
void Register() {
    const char* flag = std::getenv("SOH_ARPG_SKILLS_QA"); const char* home = std::getenv("SHIP_HOME");
    if (!flag || std::string(flag) != "1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-")) { return; }
    auraOnly=std::getenv("SOH_ARPG_AURA_QA") && std::string(std::getenv("SOH_ARPG_AURA_QA"))=="1";
    costOnly=std::getenv("SOH_ARPG_SKILL_COST_QA") && std::string(std::getenv("SOH_ARPG_SKILL_COST_QA"))=="1";
    resultPath = std::filesystem::path(home) / (costOnly?"skill-cost-runtime-results.json":auraOnly?"auras-runtime-results.json":"skills-runtime-results.json");
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Run);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorUpdate>(ProbeAttack);
}
static RegisterShipInitFunc init(Register);
}
