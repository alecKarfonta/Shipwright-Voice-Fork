#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/CreatureSystem.h"
#include "soh/Enhancements/arpg/EnemySystem.h"
#include "soh/Enhancements/arpg/AffixSystem.h"
#include "soh/Enhancements/arpg/InventorySystem.h"
#include "soh/ActorDB.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/ObjectExtension/ActorMaximumHealth.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
void Sram_InitDebugSave(void);
void CollisionCheck_ApplyDamage(PlayState*,CollisionCheckContext*,Collider*,ColliderInfo*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;using namespace ARPG::Creatures;
int phase=0,frames=0,renders=0,defeats=0;bool visual=false;bool bolts[3]{};
Actor* sentinel=nullptr;Actor* wraith=nullptr;Actor* stalfos=nullptr;Actor* wolf=nullptr;Actor* ally=nullptr;
std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
void Write(const char* status,const std::string& error=""){std::ofstream(output)<<nlohmann::json{{"status",status},{"checks",checks},{"error",error},{"render_frames",renders},{"projectile_styles",bolts},{"phase",phase}}.dump(2);}
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);checks.push_back(message);SPDLOG_INFO("[Creature QA] PASS {}",message);Write("running");}
void Noop(Actor*,PlayState*){}
void Metrics(Actor* a){auto* player=GET_PLAYER(gPlayState);a->xzDistToPlayer=Actor_WorldDistXZToActor(a,&player->actor);a->yawTowardsPlayer=Actor_WorldYawTowardActor(a,&player->actor);a->yDistToPlayer=player->actor.world.pos.y-a->world.pos.y;}
ColliderCylinder* Body(Actor* a) {
    for(int i=0;i<gPlayState->colChkCtx.colACCount;++i){auto* c=gPlayState->colChkCtx.colAC[i];if(c->actor==a && c->shape==COLSHAPE_CYLINDER)return reinterpret_cast<ColliderCylinder*>(c);}return nullptr;
}
void Step(Actor* a,int n=1) {
    auto saved=gPlayState->colChkCtx;
    for(int i=0;i<n;++i){gPlayState->colChkCtx={};++gPlayState->gameplayFrames;Metrics(a);a->prevPos=a->world.pos;if(a->update)a->update(a,gPlayState);}
    gPlayState->colChkCtx=saved;
}
ColliderCylinder* RegisterBody(Actor* a){gPlayState->colChkCtx={};Metrics(a);a->update(a,gPlayState);auto* body=Body(a);Check(body!=nullptr,"New enemy registers a native receiving cylinder");return body;}
void Normalize(Actor* a){if(auto* data=AffixSystem::GetAffixData(a)){for(auto& r:data->elemMult)r=256;data->affixCount=0;data->dmgMult=256;}}
int Hit(Actor* target,ColliderCylinder* body,uint32_t flags,bool hard=false,uint8_t alignment=AT_TYPE_PLAYER,bool apply=false) {
    auto info=target->colChkInfo;auto base=body->base;auto contact=body->info;Collider source{};ColliderInfo strike{};
    source.actor=&GET_PLAYER(gPlayState)->actor;source.atFlags=alignment;strike.toucher.dmgFlags=flags;strike.toucher.damage=2;
    body->base.acFlags=AC_HIT|(hard?AC_HARD:0);body->info.bumperFlags=BUMP_HIT;body->info.acHit=&source;body->info.acHitInfo=&strike;target->colChkInfo.damage=0;
    ++gPlayState->gameplayFrames;CollisionCheck_ApplyDamage(gPlayState,&gPlayState->colChkCtx,&body->base,&body->info);int result=target->colChkInfo.damage;
    if(apply)target->update(target,gPlayState);
    else target->colChkInfo=info;
    body->base=base;body->info=contact;return result;
}
Actor* New(Kind kind,float x,float z) {
    auto pos=GET_PLAYER(gPlayState)->actor.world.pos;pos.x+=x;pos.z+=z;auto* a=Spawn(gPlayState,kind,pos);Check(a && a->update,"Original creature spawns through ActorDB on safe native ground");Normalize(a);return a;
}
void TechniqueTicks(Actor* a,int count) {
    for(int i=0;i<count;++i){++gPlayState->gameplayFrames;GameInteractor_ExecuteOnPlayerUpdate();Metrics(a);Creatures::NeutralTechnique(a,gPlayState);}
}
void MendChecks() {
    auto* patient=New(Kind::Sentinel,50,220);
    patient->colChkInfo.health=5;SetActorMaximumHealth(patient,20);
    auto* healer=New(Kind::Wraith,-50,220);
    Step(healer,40);
    Check(MendTarget(healer,gPlayState)==patient && MendStatus(healer).spent==1 && !Status(healer).timer,
          "A Wraith prioritizes a wounded ally when its natural attack cooldown expires");
    auto* body=RegisterBody(healer);auto saved=Status(healer);auto pause=gPlayState->pauseCtx.state;
    gPlayState->pauseCtx.state=6;Step(healer,90);gPlayState->pauseCtx.state=pause;
    Check(Status(healer).timer==saved.timer && patient->colChkInfo.health==5,"Pause preserves the mend warning without healing");
    Hit(healer,body,DMG_BOOMERANG,true);
    Check(MendStatus(healer).target!=0,"A rejected hard collision cannot interrupt the healing channel");
    Hit(healer,body,DMG_BOOMERANG);
    Check(!MendStatus(healer).target && MendStatus(healer).spent==1 && patient->colChkInfo.health==5 && Status(healer).timer==60,
          "An accepted zero-damage stun spends the attempt and breaks the tether without healing");
    for(int i=0;i<250 && !MendStatus(healer).target;++i)Step(healer);
    Check(MendTarget(healer,gPlayState)==patient && MendStatus(healer).spent==2,"An interrupted Wraith can use its second finite charge after recovery");
    Step(healer,MendWindup-1);Check(patient->colChkInfo.health==5,"No healing occurs during the forty-four-tick warning");
    Step(healer);Check(patient->colChkInfo.health==9 && MendStatus(healer).completed==1 && !MendStatus(healer).target,
                       "A completed channel restores twenty percent of the ally's maximum health exactly once");
    Step(healer,350);Check(MendStatus(healer).spent==2 && MendStatus(healer).completed==1 && Status(healer).attacks>0,
                           "Exhausted Wraiths resume attacks instead of healing indefinitely");
    Actor_Kill(healer);
    // The recipient budget spans different casters; simultaneous healers reserve
    // separate patients, and no cast can revive a dead or removed actor.
    healer=New(Kind::Wraith,-50,220);auto* rival=New(Kind::Wraith,-100,220);Step(healer,40);Step(rival,40);
    Check(MendTarget(healer,gPlayState)==patient && !MendStatus(rival).target,"Two Wraiths cannot channel into the same ally at once");
    Step(healer,MendWindup);Check(patient->colChkInfo.health==13,"A second caster can use the recipient's remaining healing allowance");
    patient->colChkInfo.health=1;Step(rival,350);Step(healer,350);
    Check(!MendStatus(rival).spent && MendStatus(healer).spent==1 && patient->colChkInfo.health==1,
          "Each enemy accepts at most two heals across all Wraiths, even after taking more damage");
    Actor_Kill(healer);Actor_Kill(rival);Actor_Kill(patient);
    patient=New(Kind::Sentinel,50,220);patient->colChkInfo.health=5;SetActorMaximumHealth(patient,20);
    healer=New(Kind::Wraith,-50,220);Step(healer,40);patient->colChkInfo.health=0;Step(healer);
    Check(!MendStatus(healer).target && !MendStatus(healer).completed && !patient->colChkInfo.health,"A dying target cancels mending and is never resurrected");
    Actor_Kill(patient);Actor_Kill(healer);
    patient=New(Kind::Sentinel,50,220);patient->colChkInfo.health=5;SetActorMaximumHealth(patient,20);
    healer=New(Kind::Wraith,-50,220);Step(healer,40);auto old=patient->world.pos;patient->world.pos.x+=600;Step(healer);
    Check(!MendStatus(healer).target && !MendStatus(healer).completed,"Separating the linked enemies beyond the tether range cancels healing");
    patient->world.pos=old;
    for(int i=0;i<200 && !MendStatus(healer).target;++i)Step(healer);
    Check(MendTarget(healer,gPlayState)==patient,"A cancelled channel releases its recipient reservation");
    Actor_Kill(patient);Step(healer);Check(!MendStatus(healer).target,"Actor retirement safely cancels a pending channel");Actor_Kill(healer);
    patient=New(Kind::Wraith,50,220);patient->colChkInfo.health=1;SetActorMaximumHealth(patient,20);
    healer=New(Kind::Wraith,-50,220);Step(healer,250);
    Check(!MendStatus(healer).spent && patient->colChkInfo.health==1,"Wraiths cannot form mutual healing loops");Actor_Kill(patient);Actor_Kill(healer);
    auto p=GET_PLAYER(gPlayState)->actor.world.pos;
    patient=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_WF,p.x+50,p.y,p.z+220,0,0x8000,0,0xFF00);
    Check(patient && patient->update,"A native Wolfos initializes for the mixed-species support encounter");
    patient->update=Noop;patient->colChkInfo.health=3;SetActorMaximumHealth(patient,20);
    healer=New(Kind::Wraith,-50,220);Step(healer,40);
    Check(MendTarget(healer,gPlayState)==patient,"Wraith support recognizes a wounded native enemy, not only custom actors");
    Step(healer,MendWindup);Check(patient->colChkInfo.health==7,"A native Wolfos receives healing through its original health field");
    Actor_Kill(patient);Actor_Kill(healer);
    patient=New(Kind::Sentinel,50,220);patient->colChkInfo.health=5;SetActorMaximumHealth(patient,20);
    healer=New(Kind::Wraith,-50,220);Step(healer,40);
    Check(MendTarget(healer,gPlayState)==patient,"The terrain-occlusion fixture starts with a clear healing connection");
    // Place the endpoint below the real scene floor while keeping it inside
    // both range limits. This exercises native collision geometry, not a
    // fabricated visibility result or the vertical-distance rejection.
    patient->world.pos.y=healer->world.pos.y-80;
    Vec3f from=healer->world.pos,to=patient->world.pos,hit{};from.y+=40;to.y+=35;
    CollisionPoly* poly=nullptr;int bg=BGCHECK_SCENE;
    Check(BgCheck_EntityLineTest1(&gPlayState->colCtx,&from,&to,&hit,&poly,true,true,true,true,&bg),
          "Real static terrain intersects the tether inside its allowed horizontal and vertical range");
    Step(healer);Check(!MendStatus(healer).target && !MendStatus(healer).completed && patient->colChkInfo.health==5,
                       "Static collision geometry breaks the healing connection without restoring health");
    Actor_Kill(patient);Actor_Kill(healer);
}
void Tick() {
    if(phase==99 || phase==98)return;
    try {
        ++frames;
        if(phase==0 && frames>60 && gGameState) {
            CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();
            gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_ADULT;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;
            for(const char* k:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures"})CVarSetInteger(k,1);
            for(const char* k:{"gEnhancements.ARPG.Packs","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.Tactics","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG"})CVarSetInteger(k,0);
            gSaveContext.entranceIndex=ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;
            gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);gARPGSave={};ARPG::SeedRng(0xA72);phase=1;frames=0;return;
        }
        if(gPlayState && phase>0){gSaveContext.health=gSaveContext.healthCapacity;GET_PLAYER(gPlayState)->invincibilityTimer=40;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;}
        if(phase==1 && frames>100 && GameInteractor::IsPlayerInControl()) {
            sentinel=New(Kind::Sentinel,-60,160);wraith=New(Kind::Wraith,100,220);
            Check(sentinel->id!=wraith->id && sentinel->id!=ACTOR_EN_TEST && sentinel->category==ACTORCAT_ENEMY && wraith->category==ACTORCAT_ENEMY,"Two separate enemy actor types participate in the native enemy category");
            auto saved=gPlayState->colChkCtx;auto* shell=RegisterBody(sentinel);auto* spirit=RegisterBody(wraith);
            Check(!(shell->base.acFlags&AC_TYPE_ENEMY) && !(spirit->base.acFlags&AC_TYPE_ENEMY),"Enemy projectiles cannot strike their caster or allied creatures");
            Check((sentinel->flags&ACTOR_FLAG_ATTENTION_ENABLED) && (wraith->flags&ACTOR_FLAG_ATTENTION_ENABLED),"Both creatures support native targeting");
            Check(Hit(sentinel,shell,DMG_HAMMER_SWING,true)==0 && !Status(sentinel).exposed,"Hard collisions do not break the Sentinel shell");
            Hit(sentinel,shell,DMG_HAMMER_SWING,false,AT_TYPE_ENEMY);Check(!Status(sentinel).exposed,"Enemy-aligned contacts cannot trigger player weapon counters");
            auto pause=gPlayState->pauseCtx.state;gPlayState->pauseCtx.state=6;auto s=Status(sentinel);auto pos=sentinel->world.pos;Step(sentinel,80);
            Check(Status(sentinel).timer==s.timer && Status(sentinel).cooldown==s.cooldown && sentinel->world.pos.x==pos.x && sentinel->world.pos.z==pos.z,"Paused native updates preserve warning timers and positions");
            Hit(sentinel,shell,DMG_HAMMER_SWING);Check(!Status(sentinel).exposed,"Paused collision hooks cannot open a shell");gPlayState->pauseCtx.state=pause;
            int armored=Hit(sentinel,shell,DMG_ARROW_NORMAL);Check(armored==1,"Armored Sentinel reduces native arrow-table damage");
            Check(Hit(sentinel,shell,DMG_HAMMER_SWING)>=4 && Status(sentinel).exposed==100 && Status(sentinel).phase==Phase::Stagger,"A native hammer collision opens and staggers the Sentinel");
            Check(Hit(sentinel,shell,DMG_ARROW_NORMAL)>armored,"An open shell takes increased native arrow damage");
            Check(Hit(wraith,spirit,DMG_BOOMERANG)==0 && Status(wraith).exposed==100 && Status(wraith).timer==60,"A zero-damage boomerang collision stuns and exposes the Wraith");
            Check(Hit(wraith,spirit,DMG_ARROW_LIGHT)==9,"Light arrows exploit the exposed core through the native damage table");
            CVarSetInteger("gEnhancements.ARPG.Creatures",0);Check(!Spawn(gPlayState,Kind::Wraith,GET_PLAYER(gPlayState)->actor.world.pos),"The creatures toggle prevents new spawns");CVarSetInteger("gEnhancements.ARPG.Creatures",1);
            Vec3f invalid{1.e7f,1.e7f,1.e7f};Check(!Spawn(gPlayState,Kind::Sentinel,invalid),"Spawns reject unsupported out-of-scene positions");
            // The real actor update commits death once and then leaves a bounded visual dissolve.
            sentinel->colChkInfo.health=1;int before=defeats;Hit(sentinel,shell,DMG_HAMMER_SWING,false,AT_TYPE_PLAYER,true);
            Check(Status(sentinel).phase==Phase::Dead && defeats==before+1,"A lethal native collision reaches the enemy-defeat reward hook once");
            Step(sentinel,23);Check(defeats==before+1,"Death animation cannot duplicate defeat rewards");Step(sentinel,2);Check(!sentinel->update,"The death visual retires its actor within twenty-four ticks");
            sentinel=New(Kind::Sentinel,-50,160);
            auto p=GET_PLAYER(gPlayState)->actor.world.pos;
            stalfos=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_TEST,p.x+10,p.y,p.z+320,0,0x8000,0,2);
            wolf=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_WF,p.x-200,p.y,p.z+170,0,0x8000,0,0xFF00);
            ally=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_WF,p.x-260,p.y,p.z+210,0,0x8000,0,0xFF00);
            Check(stalfos && wolf && ally && stalfos->update && wolf->update && ally->update,"Native Stalfos and Wolfos fixtures initialize");
            stalfos->update=wolf->update=ally->update=Noop;
            TechniqueTicks(stalfos,102);Check(Status(stalfos).phase==Phase::Windup,"Stalfos begins a telegraphed ranged technique from neutral");
            Creatures::OnContact(stalfos,&GET_PLAYER(gPlayState)->actor,DMG_SLASH_MASTER,AT_TYPE_PLAYER,2);
            Check(Status(stalfos).phase==Phase::Stagger && Status(stalfos).interruptions==1,"A damaging contact interrupts a Stalfos volley before release");
            TechniqueTicks(stalfos,160);Check(Status(stalfos).attacks==1,"An uninterrupted Stalfos charge releases one bone-dart volley");
            TechniqueTicks(wolf,102);Check(Status(wolf).phase==Phase::Windup,"A Wolfos with a nearby ally begins its rally warning");
            Creatures::OnContact(wolf,&GET_PLAYER(gPlayState)->actor,DMG_DEKU_NUT,AT_TYPE_PLAYER,0);
            Check(Status(wolf).interruptions==1 && RallySpeed(ally)==1,"An accepted stun cancels a howl before any pack buff");
            TechniqueTicks(wolf,164);Check(Status(wolf).attacks==1 && RallySpeed(wolf)>1 && RallySpeed(ally)>1,"A completed howl buffs the caller and a nearby Wolfos");
            TechniqueTicks(wolf,121);Check(RallySpeed(ally)==1,"Rallied movement expires after six native seconds");
            Actor_Kill(ally);TechniqueTicks(wolf,400);Check(Status(wolf).attacks==1,"A lone Wolfos cannot cast another pack rally");
            Actor_Kill(stalfos);Actor_Kill(wolf);MendChecks();gPlayState->colChkCtx=saved;
            phase=2;frames=0;return;
        }
        if(phase==2 && frames>400) {
            Check(Status(sentinel).attacks>0 && Status(wraith).attacks>0,"Both custom enemies complete live attack cycles");
            Check(bolts[0] && bolts[1] && bolts[2],"Bone darts, spirit bolts and ground spikes enter native collision checks");
            Check(renders>=120,"Articulated original enemies and attack warnings render in the native scene");
            auto pack=Enemies::SpawnExpeditionWave(Enemies::PackKind::Champions,3);int specialists=0;for(auto* a:pack)specialists+=IsCreature(a);
            Check(pack.size()==3 && specialists==1,"A regional champion wave mixes one original specialist with native enemies");Enemies::ClearPacks();
            auto* patient=New(Kind::Sentinel,50,220);patient->colChkInfo.health=5;SetActorMaximumHealth(patient,20);
            auto* healer=New(Kind::Wraith,-50,220);Step(healer,40);
            Check(MendTarget(healer,gPlayState)==patient,"A healing reservation is live when the native scene teardown begins");
            gSaveContext.entranceIndex=ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;
            gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);phase=3;frames=0;return;
        }
        if(phase==3 && frames>100 && GameInteractor::IsPlayerInControl()) {
            int remaining=0;for(int cat:{ACTORCAT_ENEMY,ACTORCAT_ITEMACTION})for(auto* a=gPlayState->actorCtx.actorLists[cat].head;a;a=a->next)remaining+=IsCreature(a) || a->id==ActorDB::Instance->RetrieveId("Arpg_CreatureProjectile");
            Check(!remaining,"Scene teardown removes custom enemies and every pending projectile");
            if(visual){
                auto* p=GET_PLAYER(gPlayState);p->actor.world.pos.z+=700;p->actor.prevPos=p->actor.world.pos;
                sentinel=New(Kind::Sentinel,-65,-150);wraith=New(Kind::Wraith,90,-240);
                sentinel->colChkInfo.health=std::max(1,GetActorMaximumHealth(sentinel)/2);
                CVarSetInteger("gOpenWindows.ARPG",0);phase=97;frames=0;return;
            }
            phase=99;Write("passed");return;
        }
        if(phase==97 && MendTarget(wraith,gPlayState)==sentinel && Status(wraith).timer>=MendWindup/2) {
            Check(true,"The visual fixture reaches a live healing channel after the camera settles");
            // Freeze a test-only frame through the game's frame-advance mode.
            gPlayState->frameAdvCtx.enabled=true;GET_PLAYER(gPlayState)->invincibilityTimer=0;
            phase=98;Write("passed");return;
        }
        if(frames>1600)throw std::runtime_error("Creature QA timed out");
    } catch(const std::exception& e){Write("failed",e.what());phase=99;}
}
void Probe(void* ref) {
    auto* source=static_cast<Actor*>(ref);if((phase!=1 && phase!=2) || source->id!=ActorDB::Instance->RetrieveId("Arpg_CreatureProjectile"))return;
    int style=source->params;if(style<0 || style>2 || bolts[style])return;
    try {
        auto& ctx=gPlayState->colChkCtx;
        for(int i=0;i<ctx.colATCount;++i) {
            auto* base=ctx.colAT[i];if(base->actor!=source || base->shape!=COLSHAPE_CYLINDER)continue;auto* c=reinterpret_cast<ColliderCylinder*>(base);
            Actor probe{};probe.id=ACTOR_PLAYER;probe.category=ACTORCAT_PLAYER;probe.update=Noop;probe.colChkInfo.health=200;probe.world.pos={float(c->dim.pos.x),float(c->dim.pos.y)-10,float(c->dim.pos.z)};
            ColliderCylinder receiver{};ColliderCylinderInit init={{COLTYPE_HIT0,AT_NONE,AC_ON|AC_TYPE_ENEMY,OC1_NONE,OC2_TYPE_1,COLSHAPE_CYLINDER},
                {ELEMTYPE_UNK0,{0,0,0},{0xFFFFFFFF,0,0},TOUCH_NONE,BUMP_ON,OCELEM_NONE},{20,60,0,{0,0,0}}};
            Collider_InitCylinder(gPlayState,&receiver);Collider_SetCylinder(gPlayState,&receiver,&probe,&init);Collider_UpdateCylinder(&probe,&receiver);
            auto saved=ctx;ctx={};ctx.colATCount=1;ctx.colAT[0]=base;ctx.colACCount=1;ctx.colAC[0]=&receiver.base;
            CollisionCheck_AT(gPlayState,&ctx);CollisionCheck_Damage(gPlayState,&ctx);
            Check((receiver.base.acFlags&AC_HIT) && probe.colChkInfo.damage>0,"A new enemy projectile intersects the native narrow phase and damage pipeline");
            base->atFlags&=~(AT_HIT|AT_BOUNCED);c->info.toucherFlags&=~TOUCH_HIT;receiver.base.acFlags=AC_ON|AC_TYPE_ENEMY|AC_HARD;receiver.info.bumperFlags=BUMP_ON;receiver.base.colType=COLTYPE_METAL;probe.colChkInfo.damage=0;
            CollisionCheck_AT(gPlayState,&ctx);CollisionCheck_Damage(gPlayState,&ctx);
            Check((base->atFlags&AT_BOUNCED) && !probe.colChkInfo.damage,"A hard native shield volume blocks the projectile without damage");
            ctx=saved;base->at=nullptr;c->info.atHit=nullptr;c->info.atHitInfo=nullptr;
            source->update(source,gPlayState);Check(!source->update,"Blocked projectiles retire after one impact");
            Collider_DestroyCylinder(gPlayState,&receiver);ObjectExtension::GetInstance().Free(&probe);bolts[style]=true;break;
        }
    } catch(const std::exception& e){Write("failed",e.what());phase=99;}
}
void Register() {
    const char* enabled=std::getenv("SOH_ARPG_CREATURE_QA"),*home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
    visual=std::getenv("SOH_ARPG_CREATURE_UI")!=nullptr;output=std::filesystem::path(home)/"creature-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorUpdate>(Probe);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void*){++defeats;});
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(phase==2)++renders;});
}
static RegisterShipInitFunc init(Register);
}
