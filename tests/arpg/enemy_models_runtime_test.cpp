#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/EnemyGeometry.h"
#include "soh/Enhancements/arpg/EnemyVisuals.h"
#include "soh/Enhancements/arpg/AffixSystem.h"
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
#include "overlays/actors/ovl_En_Test/z_en_test.h"
#include "overlays/actors/ovl_En_Part/z_en_part.h"
void Sram_InitDebugSave(void);
s32 EnTest_OverrideLimbDraw(PlayState*,s32,Gfx**,Vec3f*,Vec3s*,void*);
void EnTest_SetupStopAndBlock(EnTest*);
extern PlayState* gPlayState;
extern GameState* gGameState;
}
namespace {
int phase=0,frames=0,kills=0;bool swung=false,animated=false,shieldMoved=false,parts=false;
std::array<EnTest*,3> enemies{};std::set<Gfx*> replacements;
Vec3f fixture{};float firstFrame=0;Vec3s firstShield{};
nlohmann::json checks=nlohmann::json::array();std::filesystem::path output;
void Check(bool ok,const char* label) {
    if(!ok) { throw std::runtime_error(label); }
    checks.push_back(label);SPDLOG_INFO("[Enemy model QA] PASS: {}",label);
}
void Result(const char* status,const std::string& error="") {
    std::ofstream(output)<<nlohmann::json{{"status",status},{"checks",checks},{"error",error}}.dump(2);phase=99;
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
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"),1);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Affixes"),1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Items"),1);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enemies"),1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"),0);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Tactics"),1);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enchanted"),0);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.InventoryOnPause"),1);CVarSetInteger(CVAR_WINDOW("ARPG"),0);
            ARPG::SeedRng(0xE11E);phase=1;frames=0;return;
        }
        if(phase>=2 && gPlayState && GameInteractor::IsPlayerInControl()) {
            auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=20;
            if(!std::getenv("SOH_ARPG_ENEMY_MODELS_UI")) { p->actor.world.pos=fixture;p->actor.prevPos=fixture;p->actor.velocity={};p->actor.speedXZ=0; }
        }
        if(phase==1 && frames>100 && GameInteractor::IsPlayerInControl()) {
            fixture=GET_PLAYER(gPlayState)->actor.world.pos;
            for(int rank=0;rank<3;++rank) {
                auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_TEST,fixture.x+(rank-1)*110,fixture.y,fixture.z+180,0,0x8000,0,STALFOS_TYPE_2);
                Check(a && a->update,"Native Stalfos initializes for rank replacement");
                enemies[rank]=reinterpret_cast<EnTest*>(a);AffixSystem::Configure(a,rank);
                for(int limb:ARPG::EnemyGeometry::StalfosLimbs) {
                    auto* native=static_cast<StandardLimb*>(enemies[rank]->skelAnime.skeleton[limb-1])->dList;
                    Gfx* replacement=native;ARPG_OverrideEnemyLimb(gPlayState,a,limb,&replacement);
                    if(!native || native==replacement || !replacement) { throw std::runtime_error("Missing native replacement at limb "+std::to_string(limb)); }
                    replacements.insert(replacement);
                    Gfx* again=native;ARPG_OverrideEnemyLimb(gPlayState,a,limb,&again);
                    if(again!=replacement) { throw std::runtime_error("Unstable cached display list"); }
                }
            }
            Check(replacements.size()==72,"All 24 native visible limbs have stable distinct caches for three ranks");
            auto* actor=&enemies[0]->actor;
            Gfx* native=static_cast<StandardLimb*>(enemies[0]->skelAnime.skeleton[STALFOS_LIMB_SWORD-1])->dList;
            Gfx* dl=native;CVarSetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),0);
            ARPG_OverrideEnemyLimb(gPlayState,actor,STALFOS_LIMB_SWORD,&dl);Check(dl==native,"Model setting immediately restores original display lists");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"),0);
            ARPG_OverrideEnemyLimb(gPlayState,actor,STALFOS_LIMB_SWORD,&dl);Check(dl==native,"Master ARPG gate preserves vanilla rendering");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"),1);dl=nullptr;
            ARPG_OverrideEnemyLimb(gPlayState,actor,STALFOS_LIMB_SWORD,&dl);Check(dl==nullptr,"Hidden and empty limb display lists stay hidden");
            Vec3f pos{};Vec3s rot{};actor->params=STALFOS_TYPE_INVISIBLE;actor->flags&=~ACTOR_FLAG_REACT_TO_LENS;dl=native;
            EnTest_OverrideLimbDraw(gPlayState,STALFOS_LIMB_SWORD,&dl,&pos,&rot,enemies[0]);Check(dl==nullptr,"Invisible Stalfos remains hidden without Lens of Truth");
            actor->flags|=ACTOR_FLAG_REACT_TO_LENS;dl=native;
            EnTest_OverrideLimbDraw(gPlayState,STALFOS_LIMB_SWORD,&dl,&pos,&rot,enemies[0]);Check(replacements.contains(dl),"Lens-revealed Stalfos uses the replacement");
            actor->params=STALFOS_TYPE_2;actor->flags&=~ACTOR_FLAG_REACT_TO_LENS;
            phase=2;frames=0;
            if(std::getenv("SOH_ARPG_ENEMY_MODELS_UI")) { std::ofstream(output)<<nlohmann::json{{"status","visual_review"},{"checks",checks}}.dump(2); }
            return;
        }
        if(phase==2) {
            if(std::getenv("SOH_ARPG_ENEMY_MODELS_UI")) { return; }
            if(frames==100) {
                firstFrame=enemies[0]->skelAnime.curFrame;firstShield=enemies[0]->shieldCollider.dim.pos;
                EnTest_SetupStopAndBlock(enemies[0]);
            }
            if(frames>100) { for(auto* e:enemies) {
                if(!e) { throw std::runtime_error("Fixture enemy disappeared before combat checks"); }
                swung|=e->meleeWeaponState>0;animated|=e->skelAnime.curFrame!=firstFrame;
                shieldMoved|=e->shieldCollider.dim.pos.x!=firstShield.x || e->shieldCollider.dim.pos.z!=firstShield.z;
            } }
            if(frames>700) {
                Check(animated,"Native animation advances with all replacement limbs");
                Check(swung,"Native sword attacks activate the original weapon collider");
                Check(shieldMoved,"Native shield collider continues following animated limb matrices");
                auto* e=enemies[0];e->bodyCollider.base.acFlags|=AC_HIT;e->actor.colChkInfo.damage=1;e->actor.colChkInfo.damageEffect=0;
                phase=3;frames=0;
            }
            return;
        }
        if(phase==3 && frames>2) {
            Check(enemies[0] && enemies[0]->actor.colorFilterTimer>0,"Native damage hit enters its flash and recoil state");
            auto* e=enemies[1];e->bodyCollider.base.acFlags|=AC_HIT;e->actor.colChkInfo.damage=0;e->actor.colChkInfo.damageEffect=15;
            phase=4;frames=0;return;
        }
        if(phase==4 && frames>3) {
            Check(enemies[1] && enemies[1]->iceTimer>0,"Freeze attaches native ice effects to the replacement's joint positions");
            auto* e=enemies[2];e->bodyCollider.base.acFlags|=AC_HIT;e->actor.colChkInfo.damage=255;e->actor.colChkInfo.damageEffect=0;
            phase=5;frames=0;return;
        }
        if(phase==5) {
            for(int cat=0;cat<ACTORCAT_MAX;++cat) { for(auto* a=gPlayState->actorCtx.actorLists[cat].head;a;a=a->next) {
                if(a->id==ACTOR_EN_PART && a->update) { parts|=replacements.contains(reinterpret_cast<EnPart*>(a)->displayList); }
            } }
            if(frames>170) {
                Check(kills==1,"Native fatal hit completes a single defeat event");
                Check(parts,"Native break-apart death renders the new cached limb geometry");
                Check(enemies[2]==nullptr,"Dead actor is destroyed without retaining a renderer actor reference");
                gPlayState->nextEntranceIndex=ENTR_LON_LON_RANCH_ENTRANCE;gPlayState->transitionTrigger=TRANS_TRIGGER_START;gPlayState->transitionType=TRANS_TYPE_INSTANT;
                phase=6;frames=0;
            }
            return;
        }
        if(phase==6 && frames>100 && GameInteractor::IsPlayerInControl()) {
            Check(!enemies[0] && !enemies[1],"Scene teardown safely releases surviving animated enemies");Result("passed");return;
        }
        if(frames>1800) { throw std::runtime_error("Timed out in phase "+std::to_string(phase)); }
    } catch(const std::exception& e) { Result("failed",e.what());SPDLOG_ERROR("[Enemy model QA] {}",e.what()); }
}
void Register() {
    const char* enabled=std::getenv("SOH_ARPG_ENEMY_MODELS_QA"),*home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-")) { return; }
    output=std::filesystem::path(home)/"enemy-model-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorDestroy>([](void* ref) { for(auto*& e:enemies) { if(e==ref) { e=nullptr; } } });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* ref) { for(auto* e:enemies) { if(e==ref) { ++kills; } } });
}
static RegisterShipInitFunc init(Register);
}
