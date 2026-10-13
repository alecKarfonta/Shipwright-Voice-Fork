// Opt-in fixed-camera native benchmark; never runs in a normal game profile.
#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/EnemySystem.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <numeric>
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
using Clock=std::chrono::steady_clock;
Clock::time_point start{},previous{};
int stage=-1,frames=0;
Vec3f fixture{};
std::vector<double> work,intervals;
double entryPeak=0,entryTickPeak=0;
int minPopulation=999,maxPopulation=0;
std::filesystem::path output;
nlohmann::json results=nlohmann::json::array();
double Millis(Clock::duration duration) { return std::chrono::duration<double,std::milli>(duration).count(); }
nlohmann::json Stats(std::vector<double> samples) {
    std::sort(samples.begin(),samples.end());
    return {{"samples",samples.size()},{"meanMs",std::accumulate(samples.begin(),samples.end(),0.0)/samples.size()},
            {"p95Ms",samples[size_t((samples.size()-1)*.95)]},{"p99Ms",samples[size_t((samples.size()-1)*.99)]},
            {"maxMs",samples.back()}};
}
void Begin() { start=Clock::now(); }
void Tick() {
    auto end=Clock::now();double elapsed=Millis(end-start);
    double interval=previous==Clock::time_point{} ? 0 : Millis(start-previous);previous=start;
    if(stage==99) { return; }
    ++frames;
    if(stage==-1 && frames>60 && gGameState) {
        CVarSetInteger(CVAR_DEVELOPER_TOOLS("DebugSaveFileMode"),2);
        gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();
        gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFF;gSaveContext.linkAge=0;
        gSaveContext.cutsceneIndex=0;gSaveContext.nextCutsceneIndex=0xFFEF;gSaveContext.sceneLayer=0;
        gSaveContext.entranceIndex=ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;
        gSaveContext.respawnFlag=0;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;
        gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);
        for(const char* key:{CVAR_ENHANCEMENT("ARPG.Enabled"),CVAR_ENHANCEMENT("ARPG.Affixes"),CVAR_ENHANCEMENT("ARPG.Enemies"),CVAR_ENHANCEMENT("ARPG.Tactics")}) { CVarSetInteger(key,1); }
        CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"),0);
        CVarSetInteger(CVAR_ENHANCEMENT("ARPG.PackDensity"),100);
        CVarSetInteger(CVAR_SETTING("InterpolationFPS"),60);
        CVarSetInteger(CVAR_WINDOW("ARPG"),0);
        stage=0;frames=0;return;
    }
    if(stage<0 || !gPlayState) { return; }
    auto* p=GET_PLAYER(gPlayState);
    if(stage==0 && frames<100) { fixture=p->actor.world.pos; }
    if(frames>=100 || stage>0) {
        p->actor.world.pos=p->actor.prevPos=fixture;p->actor.velocity={};p->actor.speedXZ=0;
        gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=20;
        gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;
        auto* camera=GET_ACTIVE_CAM(gPlayState);Camera_RequestSetting(camera,CAM_SET_FREE0);
        auto* bounds=gPlayState->colCtx.colHeader;
        camera->at={(bounds->minBounds.x+bounds->maxBounds.x)*.5f,fixture.y,
                    (bounds->minBounds.z+bounds->maxBounds.z)*.5f};
        camera->eye=camera->eyeNext={fixture.x+600,fixture.y+1200,fixture.z+600};
    }
    if(frames<=180) { entryPeak=std::max(entryPeak,elapsed);entryTickPeak=std::max(entryTickPeak,interval);return; }
    work.push_back(elapsed);intervals.push_back(interval);
    int population=ARPG::Enemies::LiveCount();minPopulation=std::min(minPopulation,population);maxPopulation=std::max(maxPopulation,population);
    if(work.size()<300) { return; }
    results.push_back({{"mode",stage==0?"off":stage==1?"default64":"stress96"},
        {"stateMainCpuWallTime",Stats(work)},{"simulationTickInterval",Stats(intervals)},
        {"warmupPeakMs",entryPeak},{"warmupTickPeakMs",entryTickPeak},{"minPopulation",minPopulation},{"maxPopulation",maxPopulation},
        {"target",ARPG::Enemies::PopulationTarget()}});
    SPDLOG_INFO("[World performance] {}",results.back().dump());
    if(stage==2) {
        bool valid=results[0]["maxPopulation"]==0 && int(results[1]["minPopulation"])>18 && int(results[2]["minPopulation"])>64;
        std::ofstream(output)<<nlohmann::json{{"status",valid?"completed":"invalid_population"},
            {"note","Release build, fixed Hyrule Field camera, 60 FPS interpolation. CPU wall time covers GameState main through display-list generation, excluding renderer/GPU and frame pacing; tick interval includes those and scheduling. 180 warmup and 300 sampled ticks per mode. Warmup peak includes scene setup or population creation."},
            {"results",results}}.dump(2);stage=99;return;
    }
    ++stage;frames=0;work.clear();intervals.clear();entryPeak=0;entryTickPeak=0;minPopulation=999;maxPopulation=0;
    ARPG::Enemies::ClearPacks();
    // Give the director a full frame with populations disabled to reset its
    // visit flag, then enable through the next frame-start hook.
    CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"),0);
    CVarSetInteger(CVAR_ENHANCEMENT("ARPG.PopulationSafetyCap"),stage==1?64:96);
    ARPG::SeedRng(0xA11FE);
}
void Register() {
    const char* enabled=std::getenv("SOH_ARPG_WORLD_PERF"),*home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-")) { return; }
    output=std::filesystem::path(home)/"world-performance-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>([] {
        if(stage>0 && stage<99 && frames==2) { CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"),1); }
        Begin();
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
}
static RegisterShipInitFunc init(Register);
}
