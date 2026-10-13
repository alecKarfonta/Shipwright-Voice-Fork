#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/ARPGSaveCodec.h"
#include "soh/Enhancements/arpg/ExpeditionSystem.h"
#include "soh/Enhancements/arpg/WardenCombat.h"
#include "soh/Enhancements/arpg/EnemySystem.h"
#include "soh/Enhancements/arpg/AffixSystem.h"
#include "soh/Enhancements/arpg/InventorySystem.h"
#include "soh/Enhancements/arpg/SkillSystem.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "soh/SaveManager.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
void Sram_InitDebugSave(void);
void CollisionCheck_ApplyDamage(PlayState*,CollisionCheckContext*,Collider*,ColliderInfo*);
extern PlayState* gPlayState;
extern GameState* gGameState;
}
namespace {
using namespace ARPG;
using namespace ARPG::ExpeditionRules;
int phase=0,frames=0;
Vec3f fixture{},target{};
Actor* warden=nullptr;
std::array<Waystone::WardView,3> sites{};
nlohmann::json checks=nlohmann::json::array();
std::filesystem::path output;
void Check(bool condition,const char* message) {
    if(!condition) { throw std::runtime_error(message); }
    checks.push_back(message); SPDLOG_INFO("[Ritual QA] PASS: {}",message);
}
void Result(const char* status,const std::string& error="") {
    std::ofstream(output)<<nlohmann::json{{"status",status},{"checks",checks},{"error",error}}.dump(2);
    phase=99; if(status==std::string("failed")) { SPDLOG_ERROR("[Ritual QA] {}",error); }
}
int RouteForRitual() {
    for(int i=0;i<3;++i) if(RouteObjective(gARPGSave.expedition,i)==ExpeditionRules::Ritual) return i;
    return -1;
}
std::vector<Actor*> Members() {
    std::vector<Actor*> out;
    for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)
        if(a->update && Expeditions::DamageMultiplier(a,DMG_SLASH_KOKIRI)!=1.f) out.push_back(a);
    return out;
}
// [TIME] | [INFO] | RitualQA.Damage | Exercise native collision calculation, hook scaling, integer accumulation and health subtraction on the live warden.
int Hit(Actor* actor) {
    auto backup=actor->colChkInfo;
    Collider source{},victim{}; ColliderInfo strike{},surface{};
    source.actor=&GET_PLAYER(gPlayState)->actor; source.atFlags=AT_TYPE_PLAYER;
    victim.actor=actor; victim.acFlags=AC_HIT;
    strike.toucher.dmgFlags=DMG_SLASH_KOKIRI; strike.toucher.damage=8;
    surface.acHit=&source; surface.acHitInfo=&strike; surface.bumperFlags=BUMP_HIT;
    actor->colChkInfo.damage=0;
    CollisionCheck_ApplyDamage(gPlayState,&gPlayState->colChkCtx,&victim,&surface);
    Actor_ApplyDamage(actor); int lost=backup.health-actor->colChkInfo.health;
    actor->colChkInfo=backup; return lost;
}
void AtWard(int index) { const auto& p=sites[index]; target={p.x,p.y,p.z+38}; }
void Next(int next) { phase=next; frames=0; }
void Tick() {
    if(phase==99) return;
    try {
        ++frames;
        if(phase==0 && frames>60 && gGameState) {
            CVarSetInteger(CVAR_DEVELOPER_TOOLS("DebugSaveFileMode"),2);
            gSaveContext.gameMode=GAMEMODE_NORMAL; gSaveContext.fileNum=0xFE; Sram_InitDebugSave();
            gSaveContext.gameMode=GAMEMODE_NORMAL; gSaveContext.fileNum=0xFF; gSaveContext.linkAge=0;
            gSaveContext.cutsceneIndex=0; gSaveContext.sceneLayer=0; gSaveContext.entranceIndex=ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;
            gSaveContext.respawnFlag=0; gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;
            gGameState->running=false; SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);
            for(const char* key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.Tactics"}) CVarSetInteger(key,1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"),0); CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Skills"),0);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.PickupText"),0); CVarSetInteger(CVAR_WINDOW("ARPG"),0);
            SeedRng(0x8170A1); Next(1); return;
        }
        if(phase>=2 && gPlayState && GameInteractor::IsPlayerInControl()) {
            auto* p=GET_PLAYER(gPlayState); gSaveContext.health=gSaveContext.healthCapacity; p->invincibilityTimer=20;
            p->actor.world.pos=target; p->actor.prevPos=target; p->actor.velocity={}; p->actor.speedXZ=0;
        }
        auto& r=gARPGSave.expedition;
        if(phase==1 && frames>100 && GameInteractor::IsPlayerInControl()) {
            fixture=target=GET_PLAYER(gPlayState)->actor.world.pos;
            Check(Expeditions::Start(0) && RouteForRitual()>=0 && Expeditions::Choose(RouteForRitual()),"Native route offers include a selectable ritual objective");
            Next(2); return;
        }
        if(phase==2 && r.phase==Combat && frames>220) {
            auto members=Members(); auto view=Expeditions::GetWaystoneView(); sites=view.sites;
            Check(members.size()==4 && Expeditions::Remaining()==4,"Ritual spawns one warden and three guards with ambient packs disabled");
            Check(view.wards && view.state.ritual && view.state.seals==0,"Three world wards start sealed and the shrine tracks the ritual");
            for(int i=0;i<3;++i) for(int j=i+1;j<3;++j)
                Check(std::hypot(sites[i].x-sites[j].x,sites[i].z-sites[j].z)>350,"Ward placements are separated on reachable field ground");
            uint32_t attacks=0;
            for(auto* a:members) { if(Expeditions::DamageMultiplier(a,DMG_SLASH_KOKIRI)==0) warden=a; attacks+=Enemies::AttackCount(a); }
            Check(warden && attacks>0,"Protected warden and guards execute native combat tactics");
            Check(Hit(warden)==0,"Native collision and Actor_ApplyDamage preserve protected warden health");
            auto before=r; GameInteractor_ExecuteOnEnemyDefeat(warden);
            Check(r.defeatedMask==before.defeatedMask && r.phase==Combat,"An invalid protected-warden defeat cannot award a clear");
            AtWard(1); Next(3); return;
        }
        if(phase==3 && frames>25) {
            auto view=Expeditions::GetWaystoneView();
            Check(view.sites[1].progress>0 && !r.sealMask,"Standing still beside a real ward advances channel progress");
            auto* player=GET_PLAYER(gPlayState); auto flags=player->stateFlags1;
            player->stateFlags1|=PLAYER_STATE1_READY_TO_FIRE; GameInteractor_ExecuteOnPlayerUpdate(); player->stateFlags1=flags;
            Check(Expeditions::GetWaystoneView().sites[1].progress==0,"Aiming a ranged weapon cancels the ritual channel");
            for(int i=0;i<10;++i) GameInteractor_ExecuteOnPlayerUpdate();
            Check(Expeditions::GetWaystoneView().sites[1].progress>0,"Channel can resume after putting the weapon away");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Skills"),1);
            gARPGSave.level=30; gARPGSave.xp=XpForLevel(29); SkillSystem::PrepareTestBuild(1);
            gSaveContext.isMagicAcquired=1; gSaveContext.magicCapacity=96;
            gSaveContext.magic=gSaveContext.magicCapacity; gSaveContext.magicState=MAGIC_STATE_IDLE;
            Check(SkillSystem::TryCastActive(gPlayState,ARPG_SKILL_MAGIC_INFUSE) && SkillSystem::IsPerformingAction(),"A real spell cast exposes the skill action window");
            GameInteractor_ExecuteOnPlayerUpdate();
            Check(Expeditions::GetWaystoneView().sites[1].progress==0,"Active skill casting cancels the ward channel");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Skills"),0); SkillSystem::ResetRuntime();
            for(int i=0;i<10;++i) GameInteractor_ExecuteOnPlayerUpdate();
            gSaveContext.health-=8; GameInteractor_ExecuteOnPlayerUpdate();
            Check(Expeditions::GetWaystoneView().sites[1].progress==0 && !r.sealMask,"Actual player health loss interrupts the unfinished native channel");
            Next(4); return;
        }
        if(phase==4 && (r.sealMask&2)) {
            auto view=Expeditions::GetWaystoneView();
            Check(r.sealMask==2 && view.sites[1].broken && Waystone::LampTone(view.state,1)==5 && Waystone::LampTone(view.state,0)==r.mutator,
                  "Second ward can break first and lights the matching shrine lens gold");
            Check(Hit(warden)==0,"One broken seal leaves the warden protected");
            AtWard(0); Next(5); return;
        }
        if(phase==5 && frames>20) {
            Check(Expeditions::GetWaystoneView().sites[0].progress>0,"A second ward starts a fresh channel");
            target=fixture; Next(6); return;
        }
        if(phase==6 && frames>8) {
            Check(Expeditions::GetWaystoneView().sites[0].progress==0 && r.sealMask==2,"Leaving a ward cancels its progress while retaining the broken seal");
            AtWard(0); Next(7); return;
        }
        if(phase==7 && (r.sealMask&1)) { Check(r.sealMask==3,"Returning and holding ground completes the first ward"); AtWard(2); Next(8); return; }
        if(phase==8 && r.sealMask==7) {
            Check(Expeditions::DamageMultiplier(warden,DMG_SLASH_KOKIRI)>0 && Hit(warden)>0,"All three wards expose the warden to native collision damage");
            float baseline=IncomingScale(r)*(r.mutator==0?.65f:1.f)*ARPG::Warden::OpeningMultiplier(warden);
            Check(std::abs(Expeditions::DamageMultiplier(warden,DMG_SLASH_KOKIRI)-baseline*1.5f)<.0001f,"Exposed warden takes the promised 50 percent bonus after oath scaling");
            Check(r.phase==Combat && r.clears==0,"Breaking the ritual alone cannot grant the cache");
            for(auto* a:Members()) { a->colChkInfo.health=0; GameInteractor_ExecuteOnEnemyDefeat(a); GameInteractor_ExecuteOnEnemyDefeat(a); Actor_Kill(a); }
            Check(r.phase==Reward && r.clears==1 && r.bestDepth==1,"Defeating the registered guard unlocks exactly one ritual cache");
            target=fixture; Next(9); return;
        }
        if(phase==9 && frames>40) {
            Check(Expeditions::GetWaystoneView().open==1,"The earned ritual cache opens the physical coffer");
            gSaveContext.fileNum=0; auto snapshot=SaveCodec::Encode(gARPGSave);
            SaveManager::Instance->SaveFile(0); SaveManager::Instance->ThreadPoolWait(); gARPGSave.expedition={}; SaveManager::Instance->LoadFile(0);
            if(SaveCodec::Encode(gARPGSave)!=snapshot) SPDLOG_ERROR("[Ritual QA] Save diff: {}",nlohmann::json::diff(snapshot,SaveCodec::Encode(gARPGSave)).dump());
            Check(SaveCodec::Encode(gARPGSave)==snapshot && r.objective==ExpeditionRules::Ritual,"Version-eight asynchronous save restores ritual objective and earned cache");
            Expeditions::OnFileLoaded(); Next(10); return;
        }
        if(phase==10 && frames>80) {
            auto view=Expeditions::GetWaystoneView();
            Check(view.visible && view.state.seals==7 && view.state.cache==Waystone::Earned && !view.wards,"Reload restores earned shrine state without respawning combat wards");
            Check(Expeditions::Claim(1) && !Expeditions::Claim(1),"Ritual rune cache transfers only once");
            Check(Expeditions::Continue() && Expeditions::Choose(RouteForRitual()),"Next depth can start another deterministic ritual route");
            Next(11); return;
        }
        if(phase==11 && r.phase==Combat && frames>80) {
            auto members=Members(); Check(members.size()==4 && !r.sealMask,"A new depth starts with four live enemies and all seals restored");
            Actor_Kill(members[0]); Next(12); return;
        }
        if(phase==12 && frames>180 && r.phase==Combat) {
            Check(Expeditions::Remaining()==4 && r.sealMask==0 && r.clears==1,"Despawn retries the entire ritual without a false clear or stale seal progress");
            Expeditions::End(); Check(!Expeditions::GetWaystoneView().visible && Enemies::LiveCount()==0,"Ending the ritual releases enemies and world presentation");
            Result("passed"); return;
        }
        if(frames>1800) throw std::runtime_error("Timed out in ritual phase "+std::to_string(phase));
    } catch(const std::exception& e) { Result("failed",e.what()); }
}
void Register() {
    const char* enabled=std::getenv("SOH_ARPG_RITUAL_QA"),*home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-")) return;
    output=std::filesystem::path(home)/"ritual-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
}
static RegisterShipInitFunc init(Register);
}
