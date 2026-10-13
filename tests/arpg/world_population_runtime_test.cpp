#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGBridge.h"
#include "soh/Enhancements/arpg/EnemySystem.h"
#include "soh/Enhancements/arpg/WorldPopulationRules.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>
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
struct Visit { int scene; bool adult; int minimumEnemies,minimumWatch; };
const Visit visits[]={
    {0x51,true,6,4},{0x60,true,3,0},{0x61,true,0,0},{0x54,false,3,0},
    {0x59,false,3,0},{0x57,true,3,0},{0x5A,true,3,0},{0x5E,true,3,0},
    {0x5C,true,3,0},{0x5B,false,0,0},{0x56,true,0,0},{0x64,true,0,0},
    {0x52,true,0,0},{0x55,false,0,0},{0x55,true,0,0},{0x58,false,0,0},
    {0x58,true,0,0},{0x62,true,0,0},{0x63,false,2,2},{0x63,true,2,2},
    {0x5F,false,2,2},{0x5D,true,0,0},{0x53,true,0,0},{0x20,false,0,1},
    {0x36,false,0,0},{0x39,true,0,0},{0x4D,false,0,0},
    {0x00,false,2,0},{0x01,false,0,0},{0x03,true,0,0},
    {0x2C,false,0,0},{0x34,false,0,0},{0x43,true,0,0}
};
int phase=0,frames=0,visit=0;bool captured=false;Vec3f entry{},previewPost{};
std::filesystem::path output;
nlohmann::json results=nlohmann::json::array();
int Entrance(int scene) {
    for (int i=0;i<ENTR_MAX;++i) { if (gEntranceTable[i].scene==scene && gEntranceTable[i].spawn==0) { return i; } }
    throw std::runtime_error("No entrance for scene "+std::to_string(scene));
}
void Report(const char* status,const std::string& error="") {
    std::ofstream(output)<<nlohmann::json{{"status",status},{"visits",results},{"error",error}}.dump(2);
    SPDLOG_INFO("[World sweep] {} {}",status,error);
    if(std::string(status)!="running") { phase=99; }
}
void ConfigureVisit() {
    int age=visits[visit].adult ? LINK_AGE_ADULT : LINK_AGE_CHILD;
    if (gPlayState && phase>0) { gPlayState->linkAgeOnLoad=age; }
    else { gSaveContext.linkAge=age; }
    gSaveContext.cutsceneIndex=0;gSaveContext.nextCutsceneIndex=0xFFEF;gSaveContext.sceneLayer=0;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;
    captured=false;frames=0;
}
void Tick() {
    if(phase==99) { return; }
    try {
        ++frames;
        if(phase==98 && gPlayState) {
            auto* player=GET_PLAYER(gPlayState);player->actor.world.pos=entry;player->actor.prevPos=entry;player->actor.velocity={};player->actor.speedXZ=0;
            player->invincibilityTimer=20;gSaveContext.health=gSaveContext.healthCapacity;
            auto* camera=GET_ACTIVE_CAM(gPlayState);Camera_RequestSetting(camera,CAM_SET_FREE0);
            camera->at={previewPost.x,previewPost.y+40,previewPost.z};
            camera->eye=camera->eyeNext={previewPost.x+340,previewPost.y+230,previewPost.z+390};return;
        }
        if(phase==0 && frames>60 && gGameState) {
            CVarSetInteger(CVAR_DEVELOPER_TOOLS("DebugSaveFileMode"),2);
            gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();
            gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFF;ConfigureVisit();
            gSaveContext.entranceIndex=Entrance(visits[visit].scene);gSaveContext.respawnFlag=0;
            gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);
            for(const char* key:{CVAR_ENHANCEMENT("ARPG.Enabled"),CVAR_ENHANCEMENT("ARPG.Affixes"),CVAR_ENHANCEMENT("ARPG.Enemies"),CVAR_ENHANCEMENT("ARPG.Packs"),CVAR_ENHANCEMENT("ARPG.Tactics")}) { CVarSetInteger(key,1); }
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enchanted"),0);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Skills"),0);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.EnemyCap"),12);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.PackDensity"),100);
            CVarSetInteger(CVAR_WINDOW("ARPG"),0);CVarSetInteger(CVAR_ENHANCEMENT("SkipIntro"),1);
            ARPG::SeedRng(0xB10F);phase=1;return;
        }
        if(phase==1 && gPlayState && gPlayState->sceneNum==visits[visit].scene && GameInteractor::IsPlayerInControl()) {
            auto* player=GET_PLAYER(gPlayState);
            if(!captured) { entry=player->actor.world.pos;captured=true;frames=0; }
            player->actor.world.pos=entry;player->actor.prevPos=entry;player->actor.velocity={};player->actor.speedXZ=0;
            player->invincibilityTimer=20;gSaveContext.health=gSaveContext.healthCapacity;
            if(frames<120) { return; }
            const auto& v=visits[visit];
            int enemies=Enemies::LiveCount(),watch=Enemies::WatchCount();
            auto region=Enemies::CurrentRegion();auto plan=Enemies::ActivityFor(v.scene,v.adult);
            int activeResidents=0;float farthest=0;
            for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next) if(a->update && Enemies::IsRegional(a)) {
                ++activeResidents;farthest=std::max(farthest,Math_Vec3f_DistXZ(&entry,&a->world.pos));
            }
            bool protectedScene=region.population==Enemies::Population::None && plan.watchLimit==0;
            bool ok=(bool(LINK_IS_ADULT)==v.adult) && enemies>=v.minimumEnemies && watch>=v.minimumWatch && enemies<=64 && watch<=plan.watchLimit && (!protectedScene || (!enemies && !watch));
            results.push_back({{"scene",v.scene},{"adult",v.adult},{"actualAdult",bool(LINK_IS_ADULT)},{"name",region.name},{"room",gPlayState->roomCtx.curRoom.num},
                {"enemies",enemies},{"watch",watch},{"residents",activeResidents},{"farthest",farthest},{"passed",ok}});
            SPDLOG_INFO("[World sweep] scene {} age {} enemies {} watch {} {}",v.scene,v.adult,enemies,watch,ok ? "PASS" : "FAIL");
            Report("running");
            if(std::getenv("SOH_ARPG_WORLD_HOLD")) {
                previewPost=entry;
                for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;a;a=a->next) if(a->update && Enemies::IsWatchman(a)) { previewPost=a->home.pos;break; }
                phase=98;return;
            }
            ++visit;
            if(visit==int(std::size(visits))) {
                bool passed=true;for(auto& result:results) { passed &= result["passed"].get<bool>(); }
                Report(passed ? "passed" : "failed",passed ? "" : "See failed scene placements");return;
            }
            ConfigureVisit();gPlayState->nextEntranceIndex=Entrance(visits[visit].scene);
            gPlayState->transitionTrigger=TRANS_TRIGGER_START;gPlayState->transitionType=TRANS_TYPE_INSTANT;
        }
        if(frames>1200) { throw std::runtime_error("Timeout visiting scene "+std::to_string(visits[visit].scene)); }
    } catch(const std::exception& e) { Report("failed",e.what()); }
}
void Register() {
    const char* enabled=std::getenv("SOH_ARPG_WORLD_SWEEP"),*home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-")) { return; }
    if(const char* start=std::getenv("SOH_ARPG_WORLD_START")) { visit=std::clamp(std::atoi(start),0,int(std::size(visits))-1); }
    output=std::filesystem::path(home)/"world-sweep-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
}
static RegisterShipInitFunc init(Register);
}
