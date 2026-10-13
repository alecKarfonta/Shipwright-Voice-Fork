#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/GearVisuals.h"
#include "soh/Enhancements/arpg/QuestWorld.h"
#include "soh/Enhancements/arpg/SkillVfx.h"
#include "soh/Enhancements/arpg/SkillAuras.h"
#include "soh/Enhancements/arpg/AuraGeometry.h"
#include "soh/Enhancements/arpg/InventorySystem.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include <libultraship/bridge/consolevariablebridge.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
extern PlayState* gPlayState;extern GameState* gGameState;
void Sram_InitDebugSave(void);
}
namespace {
int phase=0,frames=0,draws=0,mode=0,movingFrames=0,swimmingFrames=0,attackFrames=0,rollingFrames=0,captureFrames=0;
std::string captureCommand,animationName;
Vec3f center{};std::array<Actor,ARPG_SKILL_COUNT_> props{};
std::filesystem::path output;
void Report(const char* status) {
    std::ofstream(output)<<nlohmann::json{{"status",status},{"drawFrames",draws},{"mode",mode},
        {"movingFrames",movingFrames},{"swimmingFrames",swimmingFrames},{"attackFrames",attackFrames},
        {"rollingFrames",rollingFrames},{"animation",animationName},{"scene",gPlayState?gPlayState->sceneNum:-1}}.dump(2);
}
Vec3f Ground(Vec3f p) {
    Vec3f ray=p;ray.y+=std::getenv("SOH_ARPG_ART_INTERIOR")?80:1000;CollisionPoly* poly=nullptr;
    float y=BgCheck_EntityRaycastFloor1(&gPlayState->colCtx,&poly,&ray);
    if(y>BGCHECK_Y_MIN)p.y=y;return p;
}
void Update() {
    if(++frames>60 && phase==0 && gGameState) {
        CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0xFF;
        gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.linkAge=LINK_AGE_ADULT;
        gSaveContext.entranceIndex=(mode==7 || std::getenv("SOH_ARPG_ART_INTERIOR"))?ENTR_DEKU_TREE_ENTRANCE:mode==8?ENTR_LAKE_HYLIA_OUTSIDE_TEMPLE:ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;
        Flags_SetEventChkInf(EVENTCHKINF_USED_FOREST_TEMPLE_BLUE_WARP);Flags_SetEventChkInf(EVENTCHKINF_EPONA_OBTAINED);
        if(mode==8)Flags_SetEventChkInf(EVENTCHKINF_RAISED_LAKE_HYLIA_WATER);
        gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);
        for(const char* key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills","gEnhancements.ARPG.RemasteredGear"})CVarSetInteger(key,1);
        for(const char* key:{"gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Packs","gEnhancements.ARPG.RegionalWorld","gEnhancements.ARPG.Quests","gOpenWindows.ARPG","gOpenWindows.ARPGInventory","gEnhancements.LocalNpc.Enabled"})CVarSetInteger(key,0);
        frames=0;phase=1;return;
    }
    if(!gPlayState || !GameInteractor::IsPlayerInControl())return;
    auto* player=GET_PLAYER(gPlayState);
    if(phase==1 && frames>100) {
        center=player->actor.world.pos;if(mode!=7 && mode!=8 && !std::getenv("SOH_ARPG_ART_INTERIOR")){center.z+=3500;center.x-=800;}
        if(std::getenv("SOH_ARPG_ART_INTERIOR"))center={-4,0,260};
        if(mode!=8)center=Ground(center);
        for(auto& item:gARPGSave.equipment)item={};
        ARPGItemInstance sword{};sword.slot=ARPG_SLOT_SWORD;sword.baseEquip=2;sword.rarity=ARPG_RARITY_UNIQUE;sword.uniqueId=1;sword.itemLevel=10;
        gARPGSave.equipment[ARPG_SLOT_SWORD]=sword;
        if(mode>=5)for(int slot=0;slot<4;++slot) {
            auto& item=gARPGSave.equipment[slot];item={};item.slot=slot;item.baseEquip=CUR_EQUIP_VALUE(slot);
            item.rarity=ARPG_RARITY_UNIQUE;item.uniqueId=slot*2+1;item.itemLevel=10;
        }
        for(int i=0;i<12;++i)props[i].world.pos=Ground({center.x+(i%4-1.5f)*75,center.y,center.z-(i/4)*95});
        if(mode>=6) {
            Vec3f p=mode==7?Vec3f{-4,0,499}:center;
            // The native Water Temple exit initializes swimming, collision and camera
            // together; arbitrary water-box teleports can hit an exit or stale floor.
            player->actor.world.pos=player->actor.prevPos=p;player->actor.velocity={};
            player->actor.shape.rot.y=0x8000;center=p;phase=2;Report("running");return;
        }
        int camera=Play_CreateSubCamera(gPlayState);Play_ChangeCameraStatus(gPlayState,0,CAM_STAT_WAIT);Play_ChangeCameraStatus(gPlayState,camera,CAM_STAT_ACTIVE);
        Vec3f at{center.x,center.y+34,center.z-65},eye{center.x+65,center.y+170,center.z+280};
        if(std::getenv("SOH_ARPG_ART_INTERIOR")){at={center.x,center.y+25,center.z-75};eye={center.x+35,center.y+150,center.z+170};}
        if(mode==5){at={center.x-155,center.y+38,center.z+25};eye={center.x-130,center.y+53,center.z+160};}
        Play_CameraSetAtEye(gPlayState,camera,&at,&eye);phase=2;Report("running");
    }
    if(phase==2 && mode>=6) {
        // Native Link animations retain their original __OTR__ resource names.
        const auto* animation=static_cast<const char*>(player->skelAnime.animation);
        if(animation && std::string_view(animation,7)=="__OTR__")animationName=animation;
        const bool rolling=animationName.find("landing_roll")!=std::string::npos;
        if(!gPlayState->frameAdvCtx.enabled) {
            movingFrames+=player->actor.speedXZ>3.f;
            swimmingFrames+=(player->stateFlags1&PLAYER_STATE1_IN_WATER)!=0;
            attackFrames+=player->meleeWeaponState!=0;rollingFrames+=rolling;
        }
        gSaveContext.health=gSaveContext.healthCapacity;
        // Optional QA capture command freezes a real native action mid-animation.
        // Change the command (or write 'resume') to resume normal controls.
        std::string command;std::ifstream(output.parent_path()/"native-art-capture.txt")>>command;
        if(command!=captureCommand){captureCommand=command;captureFrames=0;gPlayState->frameAdvCtx.enabled=false;}
        bool matched=(command=="run" && player->actor.speedXZ>3.f && !rolling) ||
                     (command=="roll" && rolling) || (command=="attack" && player->meleeWeaponState!=0) ||
                     (command=="swim" && (player->stateFlags1&PLAYER_STATE1_IN_WATER));
        if(matched && !gPlayState->frameAdvCtx.enabled && ++captureFrames>=3) {
            gPlayState->frameAdvCtx.enabled=true;Report(("captured-"+command).c_str());
        } else if(!matched)captureFrames=0;
        if(draws%20==0 && !gPlayState->frameAdvCtx.enabled)Report("running");
    }
    if(phase==2 && mode<6) {
        Vec3f p=Ground({center.x-155,center.y,center.z+25});player->actor.world.pos=p;player->actor.prevPos=p;player->actor.velocity={};player->actor.speedXZ=0;player->actor.floorHeight=p.y;player->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;if(mode==5)player->actor.shape.rot.y=0;
        gSaveContext.dayTime=gSaveContext.skyboxTime=std::getenv("SOH_ARPG_ART_NIGHT")?0:0x8000;
    }
}
void Draw() {
    if(phase!=2)return;
    using namespace ARPG;
    if(mode==0) {
        for(int i=0;i<12;++i) {
            ARPGItemInstance item{};item.slot=i/3;item.baseEquip=i%3+1;item.itemLevel=10;
            if(const char* page=std::getenv("SOH_ARPG_ART_RELICS")) {
                int id=(std::atoi(page)-1)*12+i;if(id>=20)break;item.baseEquip=2;
                if(id<8){item.slot=id/2;item.rarity=ARPG_RARITY_UNIQUE;item.uniqueId=id+1;}
                else{item.slot=(id-8)%4;item.rarity=ARPG_RARITY_UNIQUE;item.setId=(id-8)/4+1;}
            }
            GearVisuals::DrawLoot(gPlayState,&props[i],item);
        }
    } else if(mode==1) {
        for(int i=0;i<9;++i){auto p=props[i].world.pos;Quests::DrawProp(gPlayState,Quests::Prop(i),p.x,p.y,p.z,true,false,0);}
    } else if(mode==2) {
        constexpr int skills[]={ARPG_SKILL_SWORD_CLEAVE,ARPG_SKILL_SWORD_CYCLONE,ARPG_SKILL_MAGIC_FLAME_BURST,ARPG_SKILL_MAGIC_FROST_NOVA};
        for(int i=0;i<4;++i) {
            Vec3f p=Ground({center.x+(i%2?110.f:-110.f),center.y,center.z-(i/2)*200.f});
            int age=std::getenv("SOH_ARPG_ART_STILL")?5:draws%50;
            SkillVfx::Draw(&props[i],gPlayState,skills[i],0,age,SkillPatterns::Duration(skills[i],0),p,0,SkillPatterns::Sample(skills[i],0,age),nullptr,0);
        }
    } else if(mode==3) {
        // Sweep every attack/rune through its full lifetime using the production draw path.
        for(int id=0;id<ARPG_SKILL_COUNT_;++id)if(SkillRules::Castable(id) && !AuraGeometry::Supported(id)) {
            int rune=(draws/100)%3,age=draws%100;
            Vec3f targets[]={{center.x+90,center.y,center.z},{center.x+160,center.y,center.z-70}};
            SkillVfx::Draw(&props[id],gPlayState,id,rune,age,SkillPatterns::Duration(id,rune),center,0,SkillPatterns::Sample(id,rune,age),targets,2);
        }
    } else if(mode==4) {
        Waystone::View view;view.visible=true;view.x=center.x;view.y=center.y;view.z=center.z;view.state.cache=Waystone::Earned;view.open=1;
        GearVisuals::DrawWaystone(gPlayState,view);
    }
    ++draws;if(draws==320 && mode<6)Report("passed");
}
void Register() {
    const char* enabled=std::getenv("SOH_ARPG_ART_QA"),*home=std::getenv("SHIP_HOME");
    if(!enabled || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
    mode=std::atoi(enabled);output=std::filesystem::path(home)/"native-art-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Update);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>(Draw);
}
static RegisterShipInitFunc init(Register);
}
