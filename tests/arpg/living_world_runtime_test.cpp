#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGBridge.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/EnemySystem.h"
#include "soh/Enhancements/arpg/EnemyVisuals.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/ObjectExtension/ActorMaximumHealth.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <map>
#include <set>
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
int phase=0,frames=0; Vec3f fixture{}, previewPost{};
std::vector<Actor*> original;
struct Motion { Vec3f start; float maximum=0; };
std::map<Actor*,Motion> guardMotion, enemyMotion, wildlifeMotion;
std::map<Actor*,int> enemyHealth;bool sawEnemyDamage=false;
Actor* restingGuard=nullptr;
Actor* residentFixture=nullptr; Vec3f residentPoint{};
uint32_t assistBaseline=0;
int fieldTarget=0;double fieldArea=0;
int peakAC=0,peakAT=0,peakOC=0;
uint32_t originalCulling=0; float originalForward=0;
nlohmann::json checks=nlohmann::json::array(), observations=nlohmann::json::array();
std::filesystem::path output;
void Check(bool ok,const char* label) {
    if (!ok) { throw std::runtime_error(label); }
    checks.push_back(label); SPDLOG_INFO("[Living world QA] PASS: {}",label);
}
void Result(const char* status,const std::string& error="") {
    std::ofstream(output)<<nlohmann::json{{"status",status},{"checks",checks},{"observations",observations},{"error",error}}.dump(2);
    SPDLOG_INFO("[Living world QA] {} {}",status,error); phase=99;
}
std::vector<Actor*> Members() {
    std::vector<Actor*> out;
    for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next) {
        if(a->update && a->colChkInfo.health && Enemies::IsRegional(a)) { out.push_back(a); }
    }
    return out;
}
void Tick() {
    if(phase==99) { return; }
    try {
        ++frames;
        if (phase==98 && gPlayState) {
            auto* camera=GET_ACTIVE_CAM(gPlayState);
            Camera_RequestSetting(camera,CAM_SET_FREE0);
            camera->at={previewPost.x,previewPost.y+45,previewPost.z};
            camera->eye=camera->eyeNext={previewPost.x+340,previewPost.y+210,previewPost.z+390};
            auto* p=GET_PLAYER(gPlayState);p->actor.world.pos=fixture;p->actor.prevPos=fixture;p->actor.velocity={};p->actor.speedXZ=0;
            gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=20;
            return;
        }
        if(phase==0 && frames>60 && gGameState) {
            CVarSetInteger(CVAR_DEVELOPER_TOOLS("DebugSaveFileMode"),2);
            gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();
            gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFF;gSaveContext.linkAge=0;
            gSaveContext.cutsceneIndex=0;gSaveContext.nextCutsceneIndex=0xFFEF;gSaveContext.sceneLayer=0;gSaveContext.entranceIndex=ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;
            gSaveContext.respawnFlag=0;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;
            gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);
            for (const char* key:{CVAR_ENHANCEMENT("ARPG.Enabled"),CVAR_ENHANCEMENT("ARPG.Affixes"),CVAR_ENHANCEMENT("ARPG.Enemies"),CVAR_ENHANCEMENT("ARPG.Packs"),CVAR_ENHANCEMENT("ARPG.Tactics")}) { CVarSetInteger(key,1); }
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enchanted"),0);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Skills"),0);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.PopulationSafetyCap"),64);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.PackDensity"),100);
            CVarSetInteger(CVAR_WINDOW("ARPG"),0); SeedRng(0xA11FE);phase=1;frames=0;
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Wildlife"),1);
            if(std::getenv("SOH_ARPG_WATER_QA")) { gSaveContext.linkAge=LINK_AGE_CHILD;gSaveContext.entranceIndex=ENTR_ZORAS_DOMAIN_ENTRANCE;phase=12; }
            return;
        }
        if(phase>=2 && phase<5 && gPlayState) {
            auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=20;
            p->actor.world.pos=fixture;p->actor.prevPos=fixture;p->actor.velocity={};p->actor.speedXZ=0;
        }
        if(phase==2 && gPlayState) {
            for (auto& list:gPlayState->actorCtx.actorLists) for (auto* a=list.head;a;a=a->next) {
                auto it=wildlifeMotion.find(a);
                if(a->update && it!=wildlifeMotion.end()) { it->second.maximum=std::max(it->second.maximum,Math_Vec3f_DistXZ(&it->second.start,&a->world.pos)); }
            }
            peakAC=std::max(peakAC,int(gPlayState->colChkCtx.colACCount));
            peakAT=std::max(peakAT,int(gPlayState->colChkCtx.colATCount));
            peakOC=std::max(peakOC,int(gPlayState->colChkCtx.colOCCount));
            for(auto* a:Members()) {
                auto it=enemyHealth.find(a);
                if(it!=enemyHealth.end()) { sawEnemyDamage |= a->colChkInfo.health<it->second;it->second=a->colChkInfo.health; }
            }
        }
        if((phase==2 || phase==9) && gPlayState) {
            for(int category:{ACTORCAT_NPC,ACTORCAT_ENEMY}) for(auto* a=gPlayState->actorCtx.actorLists[category].head;a;a=a->next) {
                auto& samples=category==ACTORCAT_NPC ? guardMotion : enemyMotion;
                auto it=samples.find(a);if(it!=samples.end() && a->update) { it->second.maximum=std::max(it->second.maximum,Math_Vec3f_DistXZ(&it->second.start,&a->world.pos)); }
            }
        }
        if(phase==1 && frames>180 && GameInteractor::IsPlayerInControl()) {
            fixture=GET_PLAYER(gPlayState)->actor.world.pos;original=Members();
            int far=0,visible=0,cullable=0;
            std::set<int> occupiedSectors;
            auto* bounds=gPlayState->colCtx.colHeader;
            for(auto* a:original) {
                int column=std::clamp(int(3*(a->home.pos.x-bounds->minBounds.x)/std::max(1,int(bounds->maxBounds.x-bounds->minBounds.x))),0,2);
                int row=std::clamp(int(3*(a->home.pos.z-bounds->minBounds.z)/std::max(1,int(bounds->maxBounds.z-bounds->minBounds.z))),0,2);
                occupiedSectors.insert(row*3+column);
                far+=Math_Vec3f_DistXZ(&fixture,&a->world.pos)>2400;
                cullable+=!(a->flags & ACTOR_FLAG_DRAW_CULLING_DISABLED);
                visible+=a->scale.y>0 && a->draw && a->uncullZoneForward>=20000;
                observations.push_back({{"enemy",a->id},{"x",a->world.pos.x},{"y",a->world.pos.y},{"z",a->world.pos.z},{"distance",a->xzDistToPlayer},{"scaleY",a->scale.y}});
            }
            for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;a;a=a->next) if(Enemies::IsWatchman(a)) {
                observations.push_back({{"guard",a->id},{"x",a->world.pos.x},{"y",a->world.pos.y},{"z",a->world.pos.z},{"health",a->colChkInfo.health}});
            }
            int birds=0,butterflies=0;bool neutral=true;
            for (auto& list:gPlayState->actorCtx.actorLists) for (auto* a=list.head;a;a=a->next) if(a->update && Enemies::IsWildlife(a)) {
                birds+=a->id==ACTOR_EN_BIRD;butterflies+=a->id==ACTOR_EN_BUTTE;
                wildlifeMotion[a]={a->world.pos};
                neutral &= a->category!=ACTORCAT_ENEMY;
            }
            Check(neutral,"Wildlife uses neutral native actor categories");
            Check(birds>=2 && butterflies>=2 && Enemies::WildlifeCount()<=24,"Field receives bounded native bird flocks and butterflies");
            observations.push_back({{"wildlife",Enemies::WildlifeCount()},{"birds",birds},{"butterflies",butterflies}});
            fieldTarget=Enemies::PopulationTarget();fieldArea=Enemies::PopulationArea();
            observations.push_back({{"region","field"},{"area",fieldArea},{"target",fieldTarget},{"live",original.size()}});
            Check(original.size()>18 && original.size()<=size_t(fieldTarget),"Large field population exceeds the old fixed cap within its area budget");
            observations.push_back({{"populatedFieldSectors",occupiedSectors.size()}});
            Check(occupiedSectors.size()>=6,"Enemies occupy at least six broad field sectors, not one crowded corner");
            Check(far>=3,"Region includes enemies more than the old 2400-unit despawn distance away");
            Check(cullable>=3,"Distant noncombat patrols allow view culling while remaining active");
            Check(visible==int(original.size()),"Distant enemies complete emergence and retain extended draw distance");
            auto wolf=std::find_if(original.begin(),original.end(),[](Actor* a) { return a->id==ACTOR_EN_WF; });
            Check(wolf!=original.end(),"Field has a native Wolfos model-detail fixture");
            auto* camera=GET_ACTIVE_CAM(gPlayState);Vec3f savedEye=camera->eye;
            int remastered=CVarGetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1);
            Gfx nativeLimb[1]{};Gfx* limb=nativeLimb;
            camera->eye=(*wolf)->world.pos;camera->eye.z+=3000;
            ARPG_OverrideEnemyLimb(gPlayState,*wolf,17,&limb);
            bool nativeFar=limb==nativeLimb;
            camera->eye=(*wolf)->world.pos;
            ARPG_OverrideEnemyLimb(gPlayState,*wolf,17,&limb);
            bool detailedNear=limb!=nativeLimb;
            camera->eye=savedEye;CVarSetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),remastered);
            Check(nativeFar && detailedNear,"Distant enemies retain native geometry while nearby enemies retain remastered detail");
            Check(Enemies::WatchCount()==4,"Ranch and castle approaches each receive two native watch NPCs");
            Check(!Enemies::RewardEligible(original.front()),"Unassisted NPC combat cannot award player XP or loot");
            if (std::getenv("SOH_ARPG_WORLD_PREVIEW")) {
                float farthest=0;
                for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;a;a=a->next) if(Enemies::IsWatchman(a)) {
                    float distance=Math_Vec3f_DistXZ(&fixture,&a->home.pos);
                    if(distance>farthest) { farthest=distance;previewPost=a->home.pos; }
                }
                phase=98;return;
            }
            for(auto* a:original) { enemyHealth[a]=a->colChkInfo.health; }
            for(auto* a:original) if(Math_Vec3f_DistXZ(&fixture,&a->world.pos)>2400) { enemyMotion[a]={a->world.pos}; }
            for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;a;a=a->next) if(Enemies::IsWatchman(a)) { guardMotion[a]={a->world.pos}; }
            phase=2;frames=0;return;
        }
        if(phase==2 && frames>500 && GameInteractor::IsPlayerInControl()) {
            int movingWildlife=0,movingBirds=0,movingButterflies=0;
            for (auto& list:gPlayState->actorCtx.actorLists) for (auto* a=list.head;a;a=a->next) {
                auto it=wildlifeMotion.find(a);if(a->update && it!=wildlifeMotion.end() && it->second.maximum>15) { ++movingWildlife;movingBirds+=a->id==ACTOR_EN_BIRD;movingButterflies+=a->id==ACTOR_EN_BUTTE; }
            }
            Check(movingBirds>=2 && movingButterflies>=2,"Birds and butterflies both move while Link stays still");
            observations.push_back({{"movingWildlife",movingWildlife},{"movingBirds",movingBirds},{"movingButterflies",movingButterflies}});
            int movingGuards=0,movingDistantEnemies=0;
            for(auto& [actor,motion]:guardMotion) { movingGuards+=motion.maximum>60; observations.push_back({{"guardTravel",motion.maximum},{"speed",actor->speedXZ},{"velocityX",actor->velocity.x},{"velocityY",actor->velocity.y},{"gravity",actor->gravity},{"bgFlags",actor->bgCheckFlags},{"freeze",actor->freezeTimer},{"health",actor->colChkInfo.health}}); }
            for(auto& [actor,motion]:enemyMotion) { movingDistantEnemies+=motion.maximum>100; }
            observations.push_back({{"movingGuards",movingGuards},{"movingDistantEnemies",movingDistantEnemies},{"combatFrames",frames},{"observedDamage",sawEnemyDamage}});
            observations.push_back({{"peakAC",peakAC},{"peakAT",peakAT},{"peakOC",peakOC}});
            Check(peakAC>60 && peakAC<COLLISION_CHECK_AC_MAX && peakAT<COLLISION_CHECK_AT_MAX && peakOC<COLLISION_CHECK_OC_MAX,"Large population keeps native collision tables below capacity");
            Check(movingDistantEnemies>=3,"Multiple distant enemies travel at least 100 units while Link stays still");
            Check(Enemies::WatchAttacks()>0,"Watch patrols engage monsters without Link approaching");
            int hurt=0;for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;a;a=a->next) if(Enemies::IsWatchman(a)) { hurt+=a->colChkInfo.health<24; }
            observations.push_back({{"watchAttacks",Enemies::WatchAttacks()},{"hurtWatchmen",hurt},{"watchHits",Enemies::WatchHits()},{"remaining",Enemies::LiveCount()}});
            Check(Enemies::WatchHits()>0,"Monsters retaliate through native collision and damage the watch");
            auto current=Members();
            bool wounded=false;for(auto* a:current) { wounded |= a->colChkInfo.health<GetActorMaximumHealth(a); }
            Check(sawEnemyDamage || wounded || current.size()<original.size(),"Watch spear collisions damage living monsters without player attacks");
            int persisted=0;for(auto* a:current) { persisted+=std::find(original.begin(),original.end(),a)!=original.end(); }
            Check(persisted>0,"Distant regional actors persist beyond the old director cleanup interval");
            Check(current.size()<=original.size(),"No replacement waves spawn around an idle player");
            auto* enemy=current.back();float damage=2;
            GameInteractor_ExecuteOnDamageActor(enemy,&GET_PLAYER(gPlayState)->actor,DMG_SLASH_KOKIRI,AT_TYPE_PLAYER,&damage);
            Check(Enemies::RewardEligible(enemy),"Joining a monster fight enables normal player rewards");
            Enemies::ClearPacks();guardMotion.clear();
            for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;a;a=a->next) if(Enemies::IsWatchman(a)) { guardMotion[a]={a->world.pos}; }
            phase=9;frames=0;return;
        }
        if(phase==9 && frames>400) {
            Check(Enemies::WatchSearches()>0,"Watchmen check the fighting area after contact ends");
            Check(Enemies::WatchSecuringCount()==0,"Aftermath ends and does not trap guards in lookout duty");
            int moving=0;for(auto& [actor,motion]:guardMotion) { moving+=motion.maximum>100;observations.push_back({{"idleGuardTravel",motion.maximum}}); }
            Check(moving>=3,"Idle watchmen patrol more than 100 units after a battle clears");
            std::vector<Actor*> guards;
            for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;a;a=a->next) if(a->update && Enemies::IsWatchman(a)) { guards.push_back(a); }
            Check(guards.size()>=2,"Watch partners remain available after the skirmish");
            guards[0]->colChkInfo.health=8;
            guards[0]->world.pos=guards[0]->prevPos=guards[1]->world.pos;
            assistBaseline=Enemies::WatchAssists();phase=10;frames=0;return;
        }
        if(phase==10 && frames>40) {
            Check(Enemies::WatchAssists()>assistBaseline,"A watchman helps a nearby wounded partner");
            for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;a;a=a->next) if(a->update && Enemies::IsWatchman(a)) {
                a->colChkInfo.health=24;restingGuard=a;
            }
            Check(restingGuard!=nullptr,"A surviving watchman is available for quiet recovery");
            restingGuard->colChkInfo.health=20;phase=11;frames=0;return;
        }
        if(phase==11 && frames>130) {
            Check(restingGuard->colChkInfo.health>=22,"Watch wounds mend during a quiet stretch without monsters");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"),0);phase=3;frames=0;return;
        }
        if(phase==3 && frames>10) {
            Check(Enemies::WildlifeCount()==0,"Disabling populations also removes generated wildlife");
            Check(Enemies::LiveCount()==0 && Enemies::WatchCount()==0,"Disabling population cleans up monsters and watchmen");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"),1);phase=4;frames=0;return;
        }
        if(phase==4 && frames>180 && GameInteractor::IsPlayerInControl()) {
            Check(Enemies::LiveCount()>18 && Enemies::LiveCount()<=Enemies::PopulationTarget() && Enemies::WatchCount()==4,"Re-enabling populations rebuilds the region once");
            gPlayState->nextEntranceIndex=ENTR_DESERT_COLOSSUS_EAST_EXIT;gPlayState->transitionTrigger=TRANS_TRIGGER_START;gPlayState->transitionType=TRANS_TYPE_INSTANT;
            phase=5;frames=0;return;
        }
        if(phase==5 && frames>180 && gPlayState && gPlayState->sceneNum==SCENE_DESERT_COLOSSUS && GameInteractor::IsPlayerInControl()) {
            Check(Enemies::LiveCount()>=2 && Enemies::LiveCount()<=Enemies::PopulationTarget(),"A second wilderness region populates from its own collision");
            observations.push_back({{"region","colossus"},{"area",Enemies::PopulationArea()},{"target",Enemies::PopulationTarget()},{"live",Enemies::LiveCount()}});
            Check(Enemies::PopulationArea()<fieldArea && Enemies::PopulationTarget()<fieldTarget,"Smaller desert region receives a smaller population budget than Hyrule Field");
            Check(Enemies::WildlifeCount()==0,"Green-land wildlife does not populate the desert");
            Check(Enemies::WatchCount()==0,"Hylian watchmen do not follow Link into the desert");
            auto members=Members();Check(!members.empty(),"Destination has a safe resident fixture location");
            residentPoint=members.front()->world.pos;CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"),0);
            phase=6;frames=0;return;
        }
        if(phase==6 && frames>5 && GameInteractor::IsPlayerInControl()) {
            residentFixture=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_WF,residentPoint.x,residentPoint.y,residentPoint.z,0,0,0,int16_t(0xFF00));
            Check(residentFixture && residentFixture->update,"Native resident fixture initializes without pack membership");
            residentFixture->freezeTimer=120;
            originalCulling=residentFixture->flags & (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED);
            originalForward=residentFixture->uncullZoneForward;
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"),1);phase=7;frames=0;return;
        }
        if(phase==7 && frames>40 && GameInteractor::IsPlayerInControl()) {
            Check(residentFixture->update && Enemies::IsRegional(residentFixture) && residentFixture->uncullZoneForward==20000,"Existing native residents wake with the region population");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"),0);phase=8;frames=0;return;
        }
        if(phase==8 && frames>5 && GameInteractor::IsPlayerInControl()) {
            bool present=false;for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next) { if(a==residentFixture && a->update) { present=true; } }
            Check(present && !Enemies::IsRegional(residentFixture) && residentFixture->uncullZoneForward==originalForward &&
                  (residentFixture->flags & (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED))==originalCulling,
                  "Disabling living populations restores native residents without deleting them");
            Actor_Kill(residentFixture);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"),1);gPlayState->linkAgeOnLoad=LINK_AGE_CHILD;
            gPlayState->nextEntranceIndex=ENTR_ZORAS_DOMAIN_ENTRANCE;gPlayState->transitionTrigger=TRANS_TRIGGER_START;gPlayState->transitionType=TRANS_TYPE_INSTANT;
            phase=12;frames=0;return;
        }
        if(phase==12 && frames>180 && gPlayState && gPlayState->sceneNum==SCENE_ZORAS_DOMAIN && GameInteractor::IsPlayerInControl()) {
            int fish=0;bool submerged=true;
            for (auto& list:gPlayState->actorCtx.actorLists) for(auto* a=list.head;a;a=a->next) if(a->update && Enemies::IsWildlife(a)) {
                fish+=a->id==ACTOR_EN_FISH;float water=a->world.pos.y;WaterBox* box=nullptr;
                submerged &= WaterBox_GetSurface1(gPlayState,&gPlayState->colCtx,a->world.pos.x,a->world.pos.z,&water,&box) && a->world.pos.y<water;
            }
            observations.push_back({{"domainFish",fish},{"submerged",submerged},{"age",gSaveContext.linkAge}});
            Check(fish>=2 && fish==Enemies::WildlifeCount() && fish<=6 && submerged,"Child Zora's Domain receives only fish in validated water");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Wildlife"),0);phase=13;frames=0;return;
        }
        if(phase==13 && frames>10) {
            Check(Enemies::WildlifeCount()==0,"Wildlife can be disabled independently of regional enemies");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Wildlife"),1);phase=14;frames=0;return;
        }
        if(phase==14 && frames>50) {
            Check(Enemies::WildlifeCount()>=2,"Re-enabling wildlife repopulates its habitat");Result("passed");return;
        }
        if(frames>1800) { throw std::runtime_error("Timed out in phase "+std::to_string(phase)); }
    } catch(const std::exception& e) { Result("failed",e.what()); }
}
void Register() {
    const char* enabled=std::getenv("SOH_ARPG_WORLD_QA"),*home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-")) { return; }
    output=std::filesystem::path(home)/"living-world-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
}
static RegisterShipInitFunc init(Register);
}
