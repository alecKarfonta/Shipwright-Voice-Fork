#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/ARPGTheme.h"
#include "soh/Enhancements/arpg/GearVisuals.h"
#include "soh/Enhancements/arpg/RelicGeometry.h"
#include "soh/Enhancements/arpg/InventorySystem.h"
#include "soh/Enhancements/arpg/ItemModifierSystem.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <set>
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
int phase=0,frames=0,identity=0,age=0;
std::filesystem::path output;
nlohmann::json checks=nlohmann::json::array();
std::set<ImTextureID> icons;
ARPGItemInstance shown{};Actor fixture{};bool draw=false;int rendered=0;
void Check(bool ok,const std::string& label) {
    if(!ok) { throw std::runtime_error(label); }
    checks.push_back(label);SPDLOG_INFO("[Relic QA] PASS: {}",label);
}
void Result(const char* status,const std::string& error="") {
    std::ofstream(output)<<nlohmann::json{{"status",status},{"checks",checks},{"error",error},{"rendered_pickup_frames",rendered}}.dump(2);phase=99;draw=false;
}
void Tick() {
    if(phase==99) { return; }
    try {
        ++frames;
        if(phase==0 && frames>60 && gGameState) {
            CVarSetInteger(CVAR_DEVELOPER_TOOLS("DebugSaveFileMode"),2);
            gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();
            gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFF;gSaveContext.linkAge=0;
            gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.entranceIndex=ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;
            gSaveContext.respawnFlag=0;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;
            gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"),1);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Items"),1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredGear"),1);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"),0);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enchanted"),0);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.InventoryOnPause"),1);
            CVarSetInteger(CVAR_WINDOW("ARPG"),0);phase=1;frames=0;return;
        }
        if(phase==1 && frames>100 && GameInteractor::IsPlayerInControl()) {
            fixture.world.pos=GET_PLAYER(gPlayState)->actor.world.pos;fixture.world.pos.z+=130;
            phase=2;frames=0;return;
        }
        if(phase==2 && GameInteractor::IsPlayerInControl() && frames>12) {
            using namespace ARPG;auto style=ItemAppearance::relics[identity];int slot=ItemAppearance::RelicSlot(style.kind);
            shown={};shown.slot=slot;shown.baseEquip=slot==0?(age?1:2):slot==1?(age?1:2):1;shown.itemLevel=35;
            if(identity<8) { Loot::IdentifyUnique(shown,identity+1); } else { Loot::IdentifySet(shown,(identity-8)/4+1); }
            gSaveContext.inventory.equipment|=1<<(slot*4+shown.baseEquip-1);
            gARPGSave.backpack[0]=shown;
            Check(ARPG::Inventory::Equip(0) && ItemAppearance::WorldKind(gARPGSave.equipment[slot])==style.kind,
                "Native equip identity "+std::to_string(identity)+" age "+std::to_string(age));
            auto icon=Theme::Artwork(style.artwork);Check(icon!=nullptr,"Packed painted icon "+std::string(style.artwork));icons.insert(icon);
            auto* player=GET_PLAYER(gPlayState);static Gfx native[]={gsSPEndDisplayList()};Gfx* dl=native;
            int limb=slot==0?PLAYER_LIMB_L_HAND:slot==1?PLAYER_LIMB_R_HAND:slot==2?PLAYER_LIMB_TORSO:PLAYER_LIMB_L_FOOT;
            ARPG_OverrideEquipmentLimb(gPlayState,limb,&dl,player,1);
            Check(dl && dl!=native,"Native mounted display list "+std::to_string(identity)+" age "+std::to_string(age));
            dl=native;CVarSetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredGear"),0);
            ARPG_OverrideEquipmentLimb(gPlayState,limb,&dl,player,1);
            Check(dl==native,"Disabled model gate preserves equipment "+std::to_string(identity));
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredGear"),1);
            if(slot==2) {
                Color_RGB8 color{};ARPG_EquipmentTunicColor(gPlayState,player,&color,1);auto cloth=RelicGeometry::Cloth(style.kind);
                Check(color.r==cloth.r && color.g==cloth.g && color.b==cloth.b,"Native tunic palette "+std::to_string(identity));
            }
            draw=true;phase=3;frames=0;return;
        }
        if(phase==3 && frames>12) {
            draw=false;++identity;frames=0;
            if(identity<20) { phase=2;return; }
            if(age==0) {
                age=1;identity=0;gPlayState->linkAgeOnLoad=LINK_AGE_CHILD;gPlayState->nextEntranceIndex=gSaveContext.entranceIndex;
                gPlayState->transitionTrigger=TRANS_TRIGGER_START;gPlayState->transitionType=TRANS_TYPE_INSTANT;phase=1;return;
            }
            Check(icons.size()==20,"Twenty distinct painted icons loaded through the production resource path");
            Check(rendered>=400,"Every named pickup rendered through native geometry in both ages");
            Check(gSaveContext.fileNum==0xFF,"All checks stayed inside the unsaved debug adventure");Result("passed");return;
        }
        if(frames>1800) { throw std::runtime_error("Timeout in phase "+std::to_string(phase)); }
    } catch(const std::exception& e) { Result("failed",e.what());SPDLOG_ERROR("[Relic QA] {}",e.what()); }
}
void Register() {
    const char* enabled=std::getenv("SOH_ARPG_RELIC_QA"),*home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-")) { return; }
    output=std::filesystem::path(home)/"relic-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([]() {
        if(draw && gPlayState && ARPG::GearVisuals::DrawLoot(gPlayState,&fixture,shown)) { ++rendered; }
    });
}
static RegisterShipInitFunc init(Register);
}
