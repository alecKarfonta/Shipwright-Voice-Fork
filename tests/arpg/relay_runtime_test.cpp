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
std::array<Relay::LanternView,3> sites{};
std::vector<Actor*> guards;
int carry=-1;
uint32_t renderFrames=0;
nlohmann::json checks=nlohmann::json::array();
std::filesystem::path output;
void Check(bool condition,const char* message) {
    if(!condition) { throw std::runtime_error(message); }
    checks.push_back(message); SPDLOG_INFO("[Relay QA] PASS: {}",message);
}
void Result(const char* status,const std::string& error="") {
    std::ofstream(output)<<nlohmann::json{{"status",status},{"checks",checks},{"error",error},{"relay_render_frames",renderFrames}}.dump(2);
    phase=99; if(status==std::string("failed")) { SPDLOG_ERROR("[Relay QA] {}",error); }
}
int RouteForRelay() {
    for(int i=0;i<3;++i) if(RouteObjective(gARPGSave.expedition,i)==ExpeditionRules::SpiritRelay) return i;
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
void Next(int next) { phase=next; frames=0; }
Vec3f Vec(Relay::Point p) { return {p.x,p.y,p.z}; }
void Kill(Actor* actor) { actor->colChkInfo.health=0; GameInteractor_ExecuteOnEnemyDefeat(actor); GameInteractor_ExecuteOnEnemyDefeat(actor); Actor_Kill(actor); }
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
            Check(Expeditions::Start(0) && RouteForRelay()>=0 && Expeditions::Choose(RouteForRelay()),"All three route objectives include a selectable spirit relay");
            Next(2);return;
        }
        if(phase==2 && r.phase==Combat && frames>160) {
            auto members=Members();auto view=Expeditions::GetRelayView();sites=view.lanterns;
            Check(members.size()==4 && view.visible && !Expeditions::GetWaystoneView().wards,"Relay has three native lanterns, one warden and three bearers");
            for(int i=0;i<3;++i)for(int j=i+1;j<3;++j) Check(std::hypot(sites[i].position.x-sites[j].position.x,sites[i].position.z-sites[j].position.z)>350,"Lantern sites are separated on supported approaches");
            for(auto* a:members) { if(Expeditions::DamageMultiplier(a,DMG_SLASH_KOKIRI)==0)warden=a;else guards.push_back(a); }
            Check(warden && guards.size()==3 && Hit(warden)==0,"Native collision cannot damage the sealed relay warden");
            GameInteractor_ExecuteOnEnemyDefeat(warden);Check(!r.defeatedMask,"Protected-warden defeat cannot grant reward progress");
            guards[0]->world.pos={fixture.x+100,fixture.y,fixture.z};Kill(guards[0]);
            view=Expeditions::GetRelayView();int count=0;
            for(int i=0;i<3;++i)if(view.spirits[i].visible){++count;carry=i;}
            Check(count==1 && carry>=0 && Expeditions::Remaining()==3,"A registered bearer releases one spirit despite duplicate defeat hooks");
            target=Vec(view.spirits[carry].position);
            auto* p=GET_PLAYER(gPlayState);p->actor.world.pos=target;
            auto pause=gPlayState->pauseCtx.state;gPlayState->pauseCtx.state=6;
            for(int i=0;i<40;++i)GameInteractor_ExecuteOnPlayerUpdate();
            Check(Expeditions::GetRelayView().carried==-1,"Pause prevents native spirit pickup and delivery");
            gPlayState->pauseCtx.state=pause;
            Next(3);return;
        }
        if(phase==3 && frames>15) {
            auto view=Expeditions::GetRelayView();Check(view.carried==carry && view.spirits[carry].carried,"Walking into a released spirit attaches it to Link");
            gSaveContext.health-=8;GameInteractor_ExecuteOnPlayerUpdate();view=Expeditions::GetRelayView();
            Check(view.carried==-1 && view.spirits[carry].visible && !view.spirits[carry].carried,"Actual health loss drops the carried spirit to reachable ground");
            Check(r.sealMask==0,"A damage drop cannot light a lantern");
            target=Vec(view.spirits[carry].position);GET_PLAYER(gPlayState)->actor.world.pos=target;
            for(int i=0;i<10;++i)GameInteractor_ExecuteOnPlayerUpdate();
            Check(Expeditions::GetRelayView().carried==-1,"Drop recovery delay prevents immediate reacquisition");Next(4);return;
        }
        if(phase==4 && frames>120) {
            Check(Expeditions::GetRelayView().carried==carry,"Dropped spirit can be recovered after regaining footing");
            target=Vec(sites[2].position);auto* p=GET_PLAYER(gPlayState);p->actor.world.pos=target;p->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;
            auto melee=p->meleeWeaponState;p->meleeWeaponState=1;p->actor.speedXZ=4;GameInteractor_ExecuteOnPlayerUpdate();p->meleeWeaponState=melee;
            Check(r.sealMask==4 && Expeditions::GetRelayView().carried==-1,"Moving and attacking can deliver to any unlit lantern without channeling");
            Check(Expeditions::GetRelayView().lanterns[2].lit && Waystone::LampTone(Expeditions::GetWaystoneView().state,2)==5,"Delivered spirit lights the corresponding lantern and shrine lens");
            Check(Hit(warden)==0 && r.phase==Combat && r.clears==0,"One delivery leaves the warden sealed and grants no cache");
            auto safe=Enemies::SafeSpiritPosition({fixture.x,fixture.y-5000,fixture.z},fixture);
            Check(std::abs(safe.y-fixture.y)<.1f,"Unsafe spirit drop returns to supported refuge");
            for(int i=1;i<3;++i){guards[i]->world.pos={fixture.x+100+i*70,fixture.y,fixture.z};Kill(guards[i]);}
            auto view=Expeditions::GetRelayView();int loose=0;for(const auto& spirit:view.spirits)loose+=spirit.visible;
            Check(loose==2 && Expeditions::Remaining()==1,"Remaining bearers each release one recoverable spirit");
            for(int i=0;i<3;++i)if(view.spirits[i].visible){carry=i;target=Vec(view.spirits[i].position);break;}
            Next(5);return;
        }
        if(phase==5 && frames>15) {
            Check(Expeditions::GetRelayView().carried==carry,"Second spirit is gathered independently");target=Vec(sites[0].position);Next(6);return;
        }
        if(phase==6 && frames>15) {
            Check(r.sealMask==5 && Hit(warden)==0,"Two deliveries retain protection until the last lantern");
            auto view=Expeditions::GetRelayView();for(int i=0;i<3;++i)if(view.spirits[i].visible){carry=i;target=Vec(view.spirits[i].position);break;}
            Next(7);return;
        }
        if(phase==7 && frames>15) { Check(Expeditions::GetRelayView().carried==carry,"Third spirit remains recoverable");target=Vec(sites[1].position);Next(8);return; }
        if(phase==8 && frames>15) {
            Check(r.sealMask==7 && Hit(warden)>0,"All lanterns expose the warden to real native collision damage");
            Check(r.phase==Combat && r.clears==0,"Lighting every lantern alone does not grant the cache");Kill(warden);
            Check(r.phase==Reward && r.clears==1 && r.bestDepth==1,"Defeating the exposed warden unlocks exactly one relay cache");target=fixture;Next(9);return;
        }
        if(phase==9 && frames>40) {
            Check(Expeditions::GetWaystoneView().open==1,"Relay clear opens the physical reward coffer");
            gSaveContext.fileNum=0;auto snapshot=SaveCodec::Encode(gARPGSave);
            SaveManager::Instance->SaveFile(0);SaveManager::Instance->ThreadPoolWait();gARPGSave.expedition={};SaveManager::Instance->LoadFile(0);
            Check(SaveCodec::Encode(gARPGSave)==snapshot && r.objective==SpiritRelay,"Version-nine asynchronous save restores the earned relay cache");
            Expeditions::OnFileLoaded();Next(10);return;
        }
        if(phase==10 && frames>80) {
            Check(Expeditions::GetWaystoneView().visible && !Expeditions::GetRelayView().visible,"Earned cache reload creates no stale spirits or combat lanterns");
            Check(Expeditions::Claim(1) && !Expeditions::Claim(1),"Relay cache transfers only once");
            Check(Expeditions::Continue() && Expeditions::Choose(RouteForRelay()),"Next depth offers another deterministic relay");Next(11);return;
        }
        if(phase==11 && r.phase==Combat && frames>80) {
            auto members=Members();Check(members.size()==4 && r.sealMask==0,"Next relay starts with fresh bearers and unlit lanterns");
            Actor_Kill(members[0]);Next(12);return;
        }
        if(phase==12 && frames>180 && r.phase==Combat) {
            auto view=Expeditions::GetRelayView();int spirits=0;for(auto& spirit:view.spirits)spirits+=spirit.visible;
            Check(Expeditions::Remaining()==4 && !r.sealMask && !spirits && r.clears==1,"Unexpected despawn retries the relay without duplicate souls, stale deliveries or false clear");
            Expeditions::End();Check(!Expeditions::GetRelayView().visible && !Expeditions::GetWaystoneView().visible && Enemies::LiveCount()==0,"Ending cleans up encounter and relay presentation");
            Check(renderFrames>300,"Native production renderer drew lanterns and spirit states over hundreds of frames");Result("passed");return;
        }
        if(frames>1800)throw std::runtime_error("Timed out in relay phase "+std::to_string(phase));
    }catch(const std::exception& e){Result("failed",e.what());}
}
void Register() {
    const char* enabled=std::getenv("SOH_ARPG_RELAY_QA"),*home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
    output=std::filesystem::path(home)/"relay-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(Expeditions::GetRelayView().visible)++renderFrames;});
}
static RegisterShipInitFunc init(Register);
}
