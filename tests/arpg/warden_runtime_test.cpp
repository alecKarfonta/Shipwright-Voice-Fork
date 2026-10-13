#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/ARPGSaveCodec.h"
#include "soh/Enhancements/arpg/ExpeditionSystem.h"
#include "soh/Enhancements/arpg/EnemySystem.h"
#include "soh/Enhancements/arpg/WardenCombat.h"
#include "soh/Enhancements/arpg/InventorySystem.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
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
extern PlayState* gPlayState;
extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,oath=0;
Vec3f fixture{};
Actor* warden=nullptr;
nlohmann::json checks=nlohmann::json::array();
std::filesystem::path output;
void Check(bool condition,const char* message) {
    if(!condition) throw std::runtime_error(std::string(message)+" / oath "+std::to_string(oath));
    checks.push_back({{"oath",oath},{"check",message}}); SPDLOG_INFO("[Warden QA] PASS {}: {}",oath,message);
}
void Result(const char* status,const std::string& error="") {
    std::ofstream(output)<<nlohmann::json{{"status",status},{"checks",checks},{"error",error}}.dump(2);
    phase=99; if(!error.empty()) SPDLOG_ERROR("[Warden QA] {}",error);
}
void Still(Actor*,PlayState*) {}
void Step(bool control=true) { ++gPlayState->gameplayFrames; Warden::Tick(gPlayState,warden,oath,control); }
void Place(Warden::Point point,float height=0) {
    auto v=Warden::GetView(); float sn=std::sin(v.yaw),cs=std::cos(v.yaw);
    auto* player=GET_PLAYER(gPlayState);
    player->actor.world.pos={v.x+cs*point.x+sn*point.z,v.y+height,v.z-sn*point.x+cs*point.z};
    player->actor.prevPos=player->actor.world.pos; player->actor.velocity={}; player->actor.speedXZ=0;
}
void Prime() {
    auto* p=GET_PLAYER(gPlayState); p->actor.world.pos=fixture; p->invincibilityTimer=0; p->knockbackType=0;
    Warden::Reset(); for(int i=0;i<150 && Warden::GetView().phase==Warden::Phase::Waiting;++i) Step();
    Check(Warden::GetView().phase==Warden::Phase::Warning && Warden::GetView().triangles>0,"Live terrain generates a locked warning footprint");
}
void Until(Warden::Phase target) {
    for(int i=0;i<250 && Warden::GetView().phase!=target;++i) Step();
    Check(Warden::GetView().phase==target,"Native cast reaches its expected phase");
}
void Probe() {
    auto* player=GET_PLAYER(gPlayState); auto& run=gARPGSave.expedition;
    auto snapshot=SaveCodec::Encode(gARPGSave)["expedition"];
    gSaveContext.isDoubleDefenseAcquired=0;
    const auto& d=Warden::Attacks[oath];
    const std::array<Warden::Point,5> danger{{{0,170},{0,170},{0,180},{0,170},{0,170}}};
    const std::array<Warden::Point,5> safe{{{0,-100},{100,170},{-220,50},{0,0},{150,150}}};
    Prime(); auto start=Warden::GetView();
    for(int i=0;i<10;++i) Step(false);
    Check(Warden::GetView().age==start.age,"Loss of player control pauses the warning clock");
    Warden::Tick(gPlayState,warden,oath,true);
    Check(Warden::GetView().age==start.age,"Duplicate hooks in one gameplay frame cannot advance attacks");
    Place(danger[oath]); auto aim=Warden::GetView().yaw;
    player->invincibilityTimer=-20; gSaveContext.health=gSaveContext.healthCapacity; int health=gSaveContext.health;
    Until(Warden::Phase::Opening);
    Check(gSaveContext.health==health && !Warden::GetView().hit,"Native invulnerability prevents warden damage throughout the strike");
    Check(Warden::GetView().yaw==aim,"Warning aim remains locked after player movement");
    Check(Expeditions::DamageMultiplier(warden,DMG_SLASH_KOKIRI)==0,"Ritual protection still overrides the retaliatory opening");
    run.sealMask=7;
    float base=ExpeditionRules::IncomingScale(run)*(oath==0?.65f:1.f)*1.5f;
    Check(std::abs(Expeditions::DamageMultiplier(warden,DMG_SLASH_KOKIRI)-base*1.5f)<.0001f,"Exposed ritual warden composes its opening with oath and seal damage multipliers");
    Until(Warden::Phase::Waiting);
    Check(std::abs(Expeditions::DamageMultiplier(warden,DMG_SLASH_KOKIRI)-base)<.0001f,"Retaliatory bonus ends with the opening"); run.sealMask=0;
    Prime(); Place(safe[oath]); player->invincibilityTimer=0; health=gSaveContext.health;
    Until(Warden::Phase::Opening);
    Check(gSaveContext.health==health && !Warden::GetView().hit,"The oath's advertised positional counter avoids all pulses");
    Prime(); Place(danger[oath],100); player->invincibilityTimer=0;
    Until(Warden::Phase::Opening);
    Check(gSaveContext.health==health,"Ground attacks cannot hit a player above their visible footprint");
    Prime(); Place(danger[oath]); gSaveContext.health=gSaveContext.healthCapacity; player->invincibilityTimer=0;
    while(Warden::GetView().age<d.warning-1) Step();
    Check(gSaveContext.health==gSaveContext.healthCapacity,"Full warning duration is harmless");
    Step(); int first=gSaveContext.health;
    Check(first<gSaveContext.healthCapacity && Warden::GetView().hit,"A visible strike reaches the native player health path");
    while(Warden::GetView().phase==Warden::Phase::Strike) { player->invincibilityTimer=0; Step(); }
    Check(gSaveContext.health==first,"A cast hits only once even when damage invulnerability is cleared");
    int raw=gSaveContext.healthCapacity-first;
    Prime(); Place(danger[oath]); gSaveContext.health=gSaveContext.healthCapacity;
    gSaveContext.isDoubleDefenseAcquired=1; player->invincibilityTimer=0; Until(Warden::Phase::Opening);
    int reduced=gSaveContext.healthCapacity-gSaveContext.health; gSaveContext.isDoubleDefenseAcquired=0;
    Check(reduced>0 && reduced<raw,"Native double defense reduces the same hazard damage");
    Check(SaveCodec::Encode(gARPGSave)["expedition"]==snapshot,"Warnings and strike state never leak into the saved expedition");
    Prime(); warden->colChkInfo.health=0; Step();
    Check(Warden::GetView().phase==Warden::Phase::Waiting && Warden::GetView().triangles==0,"Warden death cancels its pending strike and world marks");
    warden->colChkInfo.health=10; Prime();
}
void Begin() {
    using namespace ExpeditionRules;
    Check(Expeditions::Start(0),"A fresh native expedition starts for the oath fixture");
    auto& run=gARPGSave.expedition; int choice=-1;
    for(int attempt=0;attempt<200 && choice<0;++attempt) {
        run.seed=0x8210+attempt; auto routes=Routes(run);
        for(int i=0;i<3;++i) if(routes[i]==oath && RouteObjective(run,i)==ExpeditionRules::Ritual) choice=i;
    }
    Check(choice>=0 && Expeditions::Choose(choice),"A deterministic ritual route selects the requested oath");
    phase=2; frames=0;
}
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
            for(const char* key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Enchanted"}) CVarSetInteger(key,1);
            for(const char* key:{"gEnhancements.ARPG.Packs","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Tactics","gEnhancements.ARPG.PickupText","gWindows.ARPG"}) CVarSetInteger(key,0);
            gARPGSave={}; SeedRng(0xA75); phase=1; frames=0; return;
        }
        if(phase>=2 && gPlayState) {
            auto* p=GET_PLAYER(gPlayState); gSaveContext.health=gSaveContext.healthCapacity; p->invincibilityTimer=20;
            p->actor.world.pos=fixture; p->actor.prevPos=fixture; p->actor.velocity={}; p->actor.speedXZ=0; p->knockbackType=0;
        }
        if(phase==1 && frames>100 && GameInteractor::IsPlayerInControl()) { fixture={-1840,72,5497}; GET_PLAYER(gPlayState)->actor.world.pos=fixture; Begin(); return; }
        if(phase==2 && gARPGSave.expedition.phase==ExpeditionRules::Combat) {
            warden=nullptr;
            for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next) {
                if(Expeditions::DamageMultiplier(a,DMG_SLASH_KOKIRI)==1.f) continue;
                if(Expeditions::DamageMultiplier(a,DMG_SLASH_KOKIRI)==0) warden=a;
                a->update=Still;
            }
            Check(warden!=nullptr,"Spawned ritual includes a live protected warden");
            warden->world.pos={fixture.x,fixture.y,fixture.z+180};
            auto frame=gPlayState->gameplayFrames; Probe(); gPlayState->gameplayFrames=frame;
            phase=3; frames=0; return;
        }
        if(phase==3 && frames>160) {
            Check(Warden::GetView().phase!=Warden::Phase::Warning,"Native frame hooks advance the visible cast after the test probes");
            Expeditions::End();
            Check(Warden::GetView().triangles==0 && Warden::OpeningMultiplier(warden)==1,"Ending the expedition clears marks and vulnerability");
            if(++oath==5) { Result("passed"); return; }
            phase=4; frames=0; return;
        }
        if(phase==4 && frames>30 && GameInteractor::IsPlayerInControl()) { Begin(); return; }
        if(frames>1400) throw std::runtime_error("Timed out in warden phase "+std::to_string(phase));
    } catch(const std::exception& e) { Result("failed",e.what()); }
}
void Register() {
    const char* enabled=std::getenv("SOH_ARPG_WARDEN_QA"),*home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-")) return;
    output=std::filesystem::path(home)/"warden-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
}
static RegisterShipInitFunc init(Register);
}
