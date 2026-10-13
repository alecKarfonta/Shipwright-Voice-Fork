#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/ARPGSaveCodec.h"
#include "soh/Enhancements/arpg/ExpeditionRules.h"
#include "soh/Enhancements/arpg/ExpeditionSystem.h"
#include "soh/Enhancements/arpg/EnemySystem.h"
#include "soh/Enhancements/arpg/AffixSystem.h"
#include "soh/Enhancements/arpg/InventorySystem.h"
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
#include <vector>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
void Sram_InitDebugSave(void);
extern PlayState* gPlayState;
extern GameState* gGameState;
}
namespace {
using namespace ARPG;
using namespace ARPG::ExpeditionRules;
int phase=0,frames=0,clearedWaves=0;
Vec3f fixture{};
uint32_t actions=0;
nlohmann::json checks=nlohmann::json::array();
std::filesystem::path output;
void Check(bool condition,const char* message) {
    if(!condition) { throw std::runtime_error(message); }
    checks.push_back(message); SPDLOG_INFO("[Expedition QA] PASS: {}",message);
}
void Result(const char* status,const std::string& error="") {
    std::ofstream(output)<<nlohmann::json{{"status",status},{"checks",checks},{"error",error}}.dump(2);
    phase=99; if(status==std::string("failed")) { SPDLOG_ERROR("[Expedition QA] {}",error); }
}
std::vector<Actor*> Members() {
    std::vector<Actor*> members;
    for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next) {
        if(a->update && Expeditions::DamageMultiplier(a,DMG_SWORD)!=1.f) { members.push_back(a); }
    }
    return members;
}
int GauntletRoute() {
    for(int i=0;i<3;++i) { if(RouteObjective(gARPGSave.expedition,i)==Gauntlet) { return i; } }
    return -1;
}
void Tick() {
    if(phase==99) { return; }
    try {
        ++frames;
        if(phase==0 && frames>60 && gGameState) {
            CVarSetInteger(CVAR_DEVELOPER_TOOLS("DebugSaveFileMode"),2);
            gSaveContext.gameMode=GAMEMODE_NORMAL; gSaveContext.fileNum=0xFE; Sram_InitDebugSave();
            gSaveContext.gameMode=GAMEMODE_NORMAL; gSaveContext.fileNum=0xFF; gSaveContext.linkAge=0;
            gSaveContext.cutsceneIndex=0; gSaveContext.sceneLayer=0; gSaveContext.entranceIndex=ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;
            gSaveContext.respawnFlag=0; gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;
            gGameState->running=false; SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"),1); CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Affixes"),1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Items"),1); CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enemies"),1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enchanted"),1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"),0); CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Tactics"),1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Skills"),0); CVarSetInteger(CVAR_ENHANCEMENT("ARPG.PickupText"),0);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.InventoryOnPause"),1); CVarSetInteger(CVAR_WINDOW("ARPG"),0);
            ARPG::SeedRng(0xE770123); phase=1; frames=0; return;
        }
        if(phase>=2 && gPlayState && gPlayState->sceneNum==SCENE_HYRULE_FIELD && GameInteractor::IsPlayerInControl()) {
            auto* p=GET_PLAYER(gPlayState); gSaveContext.health=gSaveContext.healthCapacity;
            p->invincibilityTimer=20; p->actor.world.pos=fixture; p->actor.prevPos=fixture; p->actor.velocity={}; p->actor.speedXZ=0;
        }
        if(phase==1 && frames>100 && GameInteractor::IsPlayerInControl()) {
            fixture=GET_PLAYER(gPlayState)->actor.world.pos;
            Vec3f landmark{};
            Check(Enemies::FindWaystonePosition(landmark) && Math_Vec3f_DistXZ(&fixture,&landmark)>100 && std::abs(landmark.y-fixture.y)<45,
                  "Waystone placement finds reachable ground clear of Link");
            Check(Expeditions::StartRestriction().empty() && Expeditions::Start(0),"Native wilderness expedition starts from owned equipment");
            Check(!Expeditions::Start(0),"Starting twice cannot replace an active expedition");
            int route=0; while(RouteObjective(gARPGSave.expedition,route)!=Gauntlet) { ++route; }
            Check(Expeditions::Choose(route),"Route selection queues the first expedition wave");
            phase=2; frames=0; return;
        }
        if(phase==2 && gARPGSave.expedition.phase==Combat && frames>200 && GameInteractor::IsPlayerInControl()) {
            auto members=Members(); auto& r=gARPGSave.expedition;
            Check(members.size()==size_t(WaveSize(r)) && Expeditions::Remaining()==WaveSize(r),"Complete expedition wave survives with ambient packs disabled");
            auto landmark=Expeditions::GetWaystoneView();
            Check(landmark.visible && landmark.state.oath==r.mutator && landmark.state.cleared==r.wave &&
                  landmark.state.cache==Waystone::Locked && landmark.open==0,"World shrine shows this oath and completed wave count with a sealed cache");
            if(clearedWaves==0) {
                uint32_t attacks=0; for(auto* a:members) { attacks+=Enemies::AttackCount(a); } actions=attacks;
                Check(attacks>0,"Expedition enemies execute native combat tactics");
                auto* data=AffixSystem::GetAffixData(members[0]); Check(data && data->level==ItemLevel(r),"Expedition enemies use the depth item-level band");
                float value=10; auto* player=GET_PLAYER(gPlayState);
                auto* enemy=members[0]; auto expected=AffixSystem::ComputeDamageMultiplier(enemy,DMG_SLASH_KOKIRI)*Expeditions::DamageMultiplier(enemy,DMG_SLASH_KOKIRI)*10;
                GameInteractor_ExecuteOnDamageActor(enemy,&player->actor,DMG_SLASH_KOKIRI,AT_TYPE_PLAYER,&value);
                Check(std::abs(value-expected)<.01f,"Live damage hook applies expedition endurance and route counters");
                Actor unrelated{}; auto before=r; GameInteractor_ExecuteOnEnemyDefeat(&unrelated);
                Check(r.defeatedMask==before.defeatedMask && r.phase==before.phase,"Unrelated deaths do not advance the wave");
            }
            auto* first=members[0]; first->colChkInfo.health=0; GameInteractor_ExecuteOnEnemyDefeat(first);
            int remaining=Expeditions::Remaining(); GameInteractor_ExecuteOnEnemyDefeat(first);
            Check(Expeditions::Remaining()==remaining,"Duplicate death callbacks do not advance the expedition twice");
            Actor_Kill(first);
            for(size_t i=1;i<members.size();++i) { members[i]->colChkInfo.health=0; GameInteractor_ExecuteOnEnemyDefeat(members[i]); Actor_Kill(members[i]); }
            ++clearedWaves; frames=0;
            if(clearedWaves==3) {
                Check(r.phase==Reward && r.bestDepth==1 && r.clears==1,"Three native waves unlock one cache and record the clear");
                phase=10; frames=0;
            }
            return;
        }
        if(phase==10 && frames>40 && GameInteractor::IsPlayerInControl()) {
                auto& r=gARPGSave.expedition;auto shrine=Expeditions::GetWaystoneView();
                Check(shrine.visible && shrine.state.cleared==3 && shrine.state.cache==Waystone::Earned && shrine.open==1,
                      "Final clear animates the coffer fully open and lights all three wave markers");
                gSaveContext.health=0;GameInteractor_ExecuteOnPlayerUpdate();
                Check(r.phase==Reward,"Falling after a clear cannot revoke the earned unclaimed cache");
                gSaveContext.health=gSaveContext.healthCapacity;
                if(std::getenv("SOH_ARPG_EXPEDITIONS_UI")) { ARPG::Inventory::Open();phase=99;return; }
                gSaveContext.fileNum=0;
                const auto expected=SaveCodec::Encode(gARPGSave);
                SaveManager::Instance->SaveFile(0);
                gARPGSave.expedition={};
                SaveManager::Instance->ThreadPoolWait();
                SaveManager::Instance->LoadFile(0);
                Check(SaveCodec::Encode(gARPGSave)==expected,"Version-eight asynchronous SaveManager snapshot preserves the earned cache and records");
                auto roundtrip=SaveCodec::Decode(SaveCodec::Encode(gARPGSave),1);
                Check(roundtrip.expedition.phase==Reward && roundtrip.expedition.seed==r.seed,"Earned native cache survives save-codec reload");
                Expeditions::OnFileLoaded();
                Check(!Expeditions::GetWaystoneView().visible && r.phase==Reward,"Reload clears runtime placement while keeping the earned cache");
                phase=11;frames=0;return;
        }
        if(phase==11 && frames>80 && GameInteractor::IsPlayerInControl()) {
                auto& r=gARPGSave.expedition;auto shrine=Expeditions::GetWaystoneView();
                Check(shrine.visible && shrine.state.cache==Waystone::Earned && shrine.open==1 && Expeditions::Remaining()==0,
                      "Earned cache shrine returns on safe wilderness ground after reload without respawning combat");
                for(auto& slot:gARPGSave.backpack) { slot=RewardFor(r,0).item; }
                auto snapshot=SaveCodec::Encode(gARPGSave);
                Check(!Expeditions::Claim(0) && SaveCodec::Encode(gARPGSave)==snapshot,"Full backpack leaves the entire earned cache unclaimed");
                gARPGSave.backpack[0]={};
                Check(Expeditions::Claim(0) && r.phase==Rest,"Cache transfers chosen equipment and materials once");
                Check(!Expeditions::Claim(1),"Second cache choice is rejected after claiming");
                Check(Expeditions::GetWaystoneView().state.cache==Waystone::Claimed && Expeditions::GetWaystoneView().open==1,
                      "Claimed coffer stays open with no unclaimed treasure state");
                Check(Expeditions::Continue() && r.depth==2 && r.phase==Route,"Continue opens a harder depth with fresh route choices");
                Check(Expeditions::Choose(GauntletRoute()),"Second depth enters the actual encounter loop");
                phase=3;
                return;
        }
        if(phase==3 && gARPGSave.expedition.phase==Combat && frames>80) {
            auto members=Members(); Check(members.size()==3,"Second depth spawns its first wave");
            Actor_Kill(members[0]); phase=4; frames=0; return;
        }
        if(phase==4 && frames>150 && gARPGSave.expedition.phase==Combat) {
            Check(gARPGSave.expedition.clears==1 && Expeditions::Remaining()==3,"Despawned enemy retries its wave without granting a false clear");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Affixes"),0); phase=5; frames=0; return;
        }
        if(phase==5 && frames>15) {
            Check(gARPGSave.expedition.phase==Failed && Enemies::LiveCount()==0,"Disabling required gameplay ends combat and cleans expedition actors");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Affixes"),1);
            Check(Expeditions::Start(1) && gARPGSave.expedition.bestDepth==1 && gARPGSave.expedition.runs==2,"A new expedition preserves lifetime records and accepts another equipment target");
            Check(Expeditions::Choose(GauntletRoute()),"Restart enters an encounter after failure"); phase=6; frames=0; return;
        }
        if(phase==6 && gARPGSave.expedition.phase==Combat && frames>80) {
            gSaveContext.health=0; GameInteractor_ExecuteOnPlayerUpdate();
            Check(gARPGSave.expedition.phase==Failed && Enemies::LiveCount()==0,"Death ends the expedition while retaining previous clears");
            gSaveContext.health=gSaveContext.healthCapacity;
            Expeditions::Start(0); Expeditions::Choose(GauntletRoute()); phase=7; frames=0; return;
        }
        if(phase==7 && gARPGSave.expedition.phase==Combat && frames>80) {
            GET_PLAYER(gPlayState)->actor.world.pos.x+=2000; GameInteractor_ExecuteOnPlayerUpdate();
            Check(gARPGSave.expedition.phase==Failed,"Leaving the waystone radius ends combat");
            GET_PLAYER(gPlayState)->actor.world.pos=fixture;
            Expeditions::Start(0); Expeditions::Choose(GauntletRoute()); phase=8; frames=0; return;
        }
        if(phase==8 && gARPGSave.expedition.phase==Combat && frames>80) {
            gPlayState->nextEntranceIndex=ENTR_LON_LON_RANCH_ENTRANCE; gPlayState->transitionTrigger=TRANS_TRIGGER_START; gPlayState->transitionType=TRANS_TYPE_INSTANT;
            phase=9; frames=0; return;
        }
        if(phase==9 && frames>100 && GameInteractor::IsPlayerInControl()) {
            Check(gARPGSave.expedition.phase==Failed && Enemies::LiveCount()==0,"Scene transition ends unfinished combat and releases actor references");
            Check(!Expeditions::StartRestriction().empty() && !Expeditions::Start(0),"Protected settlement rejects expedition spawning");
            Result("passed"); return;
        }
        if(frames>1800) { throw std::runtime_error("Timed out in expedition phase "+std::to_string(phase)); }
    } catch(const std::exception& e) { Result("failed",e.what()); }
}
void Register() {
    const char* enabled=std::getenv("SOH_ARPG_EXPEDITIONS_QA"),*home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-")) { return; }
    output=std::filesystem::path(home)/"expedition-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
}
static RegisterShipInitFunc init(Register);
}
