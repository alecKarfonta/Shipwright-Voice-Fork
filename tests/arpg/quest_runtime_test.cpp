#include "soh/Enhancements/arpg/QuestSystem.h"
#include "soh/Enhancements/arpg/QuestWorld.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/ARPGSaveCodec.h"
#include "soh/Enhancements/arpg/InventorySystem.h"
#include "soh/Enhancements/arpg/EnemySystem.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "soh/SaveManager.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>
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
using namespace ARPG;
using namespace ARPG::Quests;
int phase=0,frames=0,draws=0;
bool visual=false,interruptionChecked=false,roomRepairChecked=false;
bool plantReview=false;
std::array<Vec3f,5> plantSites{};
std::filesystem::path output;
nlohmann::json checks=nlohmann::json::array();
Vec3f npcPosition{};
void Report(const char* status,const std::string& error="") {
    std::ofstream(output)<<nlohmann::json{{"status",status},{"checks",checks},{"error",error},{"phase",phase},{"drawFrames",draws}}.dump(2);
}
void Check(bool ok,const char* text) {if(!ok)throw std::runtime_error(text);checks.push_back(text);SPDLOG_INFO("[Quest QA] PASS {}",text);Report("running");}
void Noop(Actor*,PlayState*){}
void Position(Vec3f p) {
    auto* player=GET_PLAYER(gPlayState);player->actor.world.pos=p;player->actor.prevPos=p;player->actor.velocity={};
    player->actor.speedXZ=0;player->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;
}
void ClearEnemies() {
    for(auto* actor=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;actor;actor=actor->next)Actor_Kill(actor);
}
Actor* ApproachContact() {
    for(int category=0;category<ACTORCAT_MAX;++category)for(auto* a=gPlayState->actorCtx.actorLists[category].head;a;a=a->next) {
        if(!HasQuests(a))continue;
        for(int offset:{60,-60,100,-100}) {
            Vec3f p=a->world.pos;p.x+=offset;Position(p);
            if(NearbyContact()>=0) {npcPosition=p;return a;}
        }
    }
    return nullptr;
}
void Defeat(int id) {
    auto p=GET_PLAYER(gPlayState)->actor.world.pos;p.z+=180;
    auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,id,p.x,p.y,p.z,0,0,0,0);
    Check(a && a->update,"Native quest enemy actor initializes");a->update=Noop;a->colChkInfo.health=0;
    GameInteractor_ExecuteOnEnemyDefeat(a);auto state=SaveCodec::Encode(gARPGSave);
    GameInteractor_ExecuteOnEnemyDefeat(a);
    Check(state==SaveCodec::Encode(gARPGSave),"Repeated native defeat callback cannot duplicate credit or rewards");Actor_Kill(a);
}
void Tick() {
    if(phase==99 || phase==98)return;
    if(phase==97) {
        Position({-405,0,70});
        gSaveContext.dayTime=gSaveContext.skyboxTime=std::getenv("SOH_ARPG_PLANTS_NIGHT")?0:0x8000;
        return;
    }
    try {
        ++frames;
        if(phase==0 && frames>60 && gGameState) {
            CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0xFF;
            gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.linkAge=LINK_AGE_CHILD;gSaveContext.inventory.questItems=0;
            gSaveContext.entranceIndex=ENTR_KOKIRI_FOREST_0;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;
            gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);
            for(const char* key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Quests"})CVarSetInteger(key,1);
            for(const char* key:{"gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Affixes","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG","gOpenWindows.ARPGInventory","gEnhancements.LocalNpc.Enabled"})CVarSetInteger(key,0);
            phase=1;frames=0;return;
        }
        if(!gPlayState || !GameInteractor::IsPlayerInControl()) {if(frames>1600)throw std::runtime_error("No player control");return;}
        GET_PLAYER(gPlayState)->invincibilityTimer=40;
        if(phase==1 && frames>100) {
            if(plantReview) {
                gARPGSave.campaign={};ClearEnemies();Position({-405,0,70});
                for(int i=0;i<5;++i) {
                    plantSites[i]={-360.f+i*53.f,0,50};
                    Check(ARPG::Enemies::QuestSiteSupported(plantSites[i]),"Plant preview has a supported native forest floor");
                }
                int camera=Play_CreateSubCamera(gPlayState);
                Check(camera>0,"Plant preview camera created");
                Play_ChangeCameraStatus(gPlayState,0,CAM_STAT_WAIT);
                Play_ChangeCameraStatus(gPlayState,camera,CAM_STAT_ACTIVE);
                Vec3f at{-260,32,50},eye{-245,115,335};
                if(std::getenv("SOH_ARPG_PLANTS_REVERSE"))eye={-230,150,-215};
                Play_CameraSetAtEye(gPlayState,camera,&at,&eye);
                phase=97;Report("visual_review");return;
            }
            gARPGSave.campaign={};for(auto& item:gARPGSave.backpack)item={};ClearEnemies();
            auto* contact=ApproachContact();Check(contact && NearbyContact()==int(Contact::Kokiri),"Original Kokiri NPC is a nearby quest giver");
            Check(DialogueContext(contact).find("Bite Before Breakfast")!=std::string::npos && DialogueContext(contact).find("A Dawn Worth")==std::string::npos,
                "NPC conversation context contains real local offers without unrelated future quest spoilers");
            Check(!AcceptNearby(2) && AcceptNearby(1),"NPC acceptance enforces chain gates");
            Defeat(ACTOR_EN_TITE);Check(!gARPGSave.campaign.quests[0].progress[0],"Wrong native enemy type does not count");
            for(int n=0;n<4;++n)Defeat(ACTOR_EN_DEKUBABA);
            Check(Status(gARPGSave.campaign,1)==State::Ready,"Native enemy deaths finish the opening hunt");
            Position({npcPosition.x+2000,npcPosition.y,npcPosition.z});Check(!ClaimNearby(1,0),"Rewards cannot be claimed away from a contact");Position(npcPosition);
            for(auto& item:gARPGSave.backpack)item=RewardItem(Catalog[0],0,false);
            auto full=SaveCodec::Encode(gARPGSave);Check(!ClaimNearby(1,0) && full==SaveCodec::Encode(gARPGSave),"Native full-backpack turn-in preserves every reward");
            gARPGSave.backpack[0]={};auto xp=gARPGSave.xp;Check(ClaimNearby(1,0) && gARPGSave.xp==xp+Catalog[0].xp,"NPC grants the selected gear and quest XP");
            auto claimed=SaveCodec::Encode(gARPGSave);Check(!ClaimNearby(1,0) && claimed==SaveCodec::Encode(gARPGSave),"Native turn-in is single-use");
            for(auto& item:gARPGSave.backpack)item={};Check(AcceptNearby(2),"Next gathering quest is offered by the same NPC");
            // The clearing is open and has a direct supported approach to the authored procedural sites.
            Position({0,0,0});phase=2;frames=0;return;
        }
        if(phase==2) {
            auto& progress=gARPGSave.campaign.quests[1];
            for(int node=0;node<3;++node) {
                auto& s=progress.sites[0][node];if(!s.placed || (progress.nodes[0]&(1<<node)))continue;
                if(!roomRepairChecked) {
                    s.room=127;Quests::Tick();
                    Check(!s.placed && !progress.nodes[0],"A malformed saved room is cleared so unfinished pickups can be replaced");
                    roomRepairChecked=true;return;
                }
                Position({s.x,s.y,s.z});
                if(!interruptionChecked) {
                    for(int n=0;n<5;++n)Quests::Tick();Check(ChannelProgress()>0,"Standing at a supported world pickup starts gathering");
                    auto paused=gPlayState->pauseCtx.state;gPlayState->pauseCtx.state=6;float channel=ChannelProgress();
                    for(int n=0;n<30;++n)Quests::Tick();Check(ChannelProgress()==channel,"Opening the journal freezes interaction progress");gPlayState->pauseCtx.state=paused;
                    auto health=gSaveContext.health;gSaveContext.health-=4;Quests::Tick();Check(ChannelProgress()==0,"Taking damage interrupts world interaction");gSaveContext.health=health;
                    interruptionChecked=true;
                }
                break;
            }
            if(Status(gARPGSave.campaign,2)==State::Ready) {
                Check(progress.nodes[0]==7 && progress.progress[0]==3,"Three distinct rendered pickups complete the collection");
                Position(npcPosition);Check(ClaimNearby(2,0),"Gathering quest turns in to the original NPC");
                gSaveContext.inventory.questItems|=1u<<QUEST_KOKIRI_EMERALD;
                Check(AcceptNearby(3) && Status(gARPGSave.campaign,3)==State::Ready,"Native story reward backfills a previously completed boss");
                Check(ClaimNearby(3,0) && AcceptNearby(4),"Boss contract unlocks the forest rebuilding quest");
                phase=3;frames=0;return;
            }
        }
        if(phase==3) {
            auto& p=gARPGSave.campaign.quests[3];
            for(int n=0;n<3;++n) {auto& s=p.sites[0][n];if(s.placed && !(p.nodes[0]&(1<<n))) {Position({s.x,s.y,s.z});break;}}
            if(Status(gARPGSave.campaign,4)==State::Ready) {
                Check(p.nodes[0]==7 && VisibleSites()>=3,"Rebuilding leaves three restored saplings in the live scene");
                auto s=p.sites[0][0];Position({s.x,s.y,s.z});gSaveContext.health=gSaveContext.healthCapacity-32;
                for(int n=0;n<45;++n)Quests::Tick();Check(gSaveContext.health>gSaveContext.healthCapacity-32,"Resting beside a restored site recovers health");
                auto saved=SaveCodec::Encode(gARPGSave);gSaveContext.fileNum=0;
                SaveManager::Instance->SaveFile(0);SaveManager::Instance->ThreadPoolWait();gARPGSave.campaign={};SaveManager::Instance->LoadFile(0);
                Check(SaveCodec::Encode(gARPGSave)==saved,"Version 14 native SaveManager reload preserves quests, rewards, and world changes");gSaveContext.fileNum=0xFF;
                Quests::Tick();Check(VisibleSites()>=3,"Restored world objects return after reloading a save");
                CVarSetInteger("gEnhancements.ARPG.Quests",0);Quests::Tick();Check(!Enabled() && !VisibleSites(),"Quest toggle removes transient visuals without altering saved progress");
                CVarSetInteger("gEnhancements.ARPG.Quests",1);Quests::Tick();Check(VisibleSites()>=3,"Re-enabling restores persistent world state");
                Check(draws>30,"Quest props render through native frames");
                Position(npcPosition);if(visual)gSaveContext.fileNum=0;OpenFor(ApproachContact());phase=4;frames=0;return;
            }
        }
        if(frames>1800)throw std::runtime_error("Timeout phase "+std::to_string(phase));
    }catch(const std::exception& e){Report("failed",e.what());phase=99;SPDLOG_ERROR("[Quest QA] {}",e.what());}
}
void CheckJournal() {
    if(phase!=4 || !gPlayState || gPlayState->pauseCtx.state!=6)return;
    try {Check(ARPG::Inventory::IsOpen(),"NPC quest action opens the unified adventure journal");Report("passed");phase=visual?98:99;}
    catch(const std::exception& e){Report("failed",e.what());phase=99;}
}
void Register() {
    const char* enabled=std::getenv("SOH_ARPG_QUEST_QA"),*home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
    plantReview=std::getenv("SOH_ARPG_PLANTS_REVIEW")!=nullptr;
    visual=std::getenv("SOH_ARPG_QUEST_UI")!=nullptr;
    output=std::filesystem::path(home)/(plantReview?"plant-preview-results.json":"quest-results.json");
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(CheckJournal);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){
        if(VisibleSites())++draws;
        if(phase==97) {
            for(int i=0;i<5;++i) {
                auto p=plantSites[i];DrawProp(gPlayState,i==0?Prop::Herbs:Prop::Sapling,p.x,p.y,p.z,i>1,false,0);
            }
            ++draws;
            if(draws==120)Report("visual_review");
        }
    });
}
static RegisterShipInitFunc init(Register);
}
