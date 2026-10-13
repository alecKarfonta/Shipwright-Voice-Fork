#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/BroodSystem.h"
#include "soh/Enhancements/arpg/SkillCombat.h"
#include "soh/Enhancements/arpg/ArsenalSystem.h"
#include "soh/Enhancements/arpg/CreatureSystem.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/ShipInit.hpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "overlays/actors/ovl_Boss_Goma/z_boss_goma.h"
#include "overlays/actors/ovl_En_Goma/z_en_goma.h"
void Sram_InitDebugSave(void);
void BossGoma_SetupFloorIdle(BossGoma*);
void BossGoma_SetupFloorMain(BossGoma*);
void BossGoma_FloorStunned(BossGoma*,PlayState*);
void BossGoma_Update(Actor*,PlayState*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;using namespace ARPG::Brood;
int phase=0,frames=0,defeats=0,renderFrames=0;BossGoma* boss=nullptr;
std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();bool visual=false;
void Write(const char* status,const std::string& error=""){std::ofstream(output)<<nlohmann::json{{"status",status},{"checks",checks},{"error",error},{"phase",phase},{"render_frames",renderFrames}}.dump(2);}
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);checks.push_back(label);SPDLOG_INFO("[Brood QA] PASS {}",label);Write("running");}
void Noop(Actor*,PlayState*){}
void Warp(){gSaveContext.entranceIndex=ENTR_DEKU_TREE_BOSS_ENTRANCE;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;gSaveContext.sceneFlags[SCENE_DEKU_TREE_BOSS].clear=0;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);boss=nullptr;}
void PrepareBoss(bool native=false){
    for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_BOSS].head;a;a=a->next)if(a->id==ACTOR_BOSS_GOMA)boss=reinterpret_cast<BossGoma*>(a);
    Check(boss && boss->actor.update,"Gohma initializes in her actual native arena");
    if(boss->subCameraId){func_800C08AC(gPlayState,boss->subCameraId,0);boss->subCameraId=0;}
    // This fixture skips Encounter before it starts a cutscene; its command pointer is not initialized.
    gPlayState->csCtx.state=CS_STATE_IDLE;gPlayState->csCtx.linkAction=nullptr;
    Player_SetCsActionWithHaltedActors(gPlayState,&boss->actor,7);
    boss->disableGameplayLogic=false;boss->actor.world.pos={0,-640,-80};boss->actor.shape.rot.x=0;boss->actor.gravity=-1;
    boss->actor.colChkInfo.health=100;boss->patienceTimer=250;
    if(native){BossGoma_SetupFloorMain(boss);boss->actor.update=BossGoma_Update;}
    else{BossGoma_SetupFloorIdle(boss);boss->framesUntilNextAction=20000;boss->actor.update=Noop;}
    auto* p=GET_PLAYER(gPlayState);p->actor.world.pos=p->actor.prevPos={0,-640,180};p->actor.shape.rot.y=p->actor.world.rot.y=0x8000;
}
void Step(Actor* a,int ticks=1){auto ctx=gPlayState->colChkCtx;for(int i=0;i<ticks && a->update;++i){gPlayState->colChkCtx={};++gPlayState->gameplayFrames;a->update(a,gPlayState);}gPlayState->colChkCtx=ctx;}
Actor* PodAt(float x=-90,float z=100){auto* p=Spawn(gPlayState,&boss->actor,{x,-640,z});Check(p && p->update,"A distinct brood-pod actor spawns on supported arena ground");return p;}
ColliderCylinder* Body(Actor* actor){
    gPlayState->colChkCtx={};actor->update(actor,gPlayState);
    for(int i=0;i<gPlayState->colChkCtx.colACCount;++i)if(gPlayState->colChkCtx.colAC[i]->actor==actor)return reinterpret_cast<ColliderCylinder*>(gPlayState->colChkCtx.colAC[i]);
    return nullptr;
}
int Hit(Actor* actor,ColliderCylinder* body,uint32_t flags,uint8_t alignment=AT_TYPE_PLAYER,bool hard=false){
    Actor source{};source.id=ACTOR_PLAYER;source.category=ACTORCAT_PLAYER;source.update=Noop;source.world.pos=actor->world.pos;
    ColliderCylinder attack{};ColliderCylinderInit init={{COLTYPE_NONE,uint8_t(AT_ON|alignment),AC_NONE,OC1_NONE,OC2_TYPE_1,COLSHAPE_CYLINDER},
        {ELEMTYPE_UNK0,{flags,0,2},{0,0,0},TOUCH_ON,BUMP_NONE,OCELEM_NONE},{25,50,0,{0,0,0}}};
    Collider_InitCylinder(gPlayState,&attack);Collider_SetCylinder(gPlayState,&attack,&source,&init);Collider_UpdateCylinder(&source,&attack);
    auto ctx=gPlayState->colChkCtx;auto originalFlags=body->base.acFlags;
    body->base.acFlags=AC_ON|AC_TYPE_PLAYER|(hard?AC_HARD:0);body->info.bumperFlags&=~BUMP_HIT;actor->colChkInfo.damage=actor->colChkInfo.damageEffect=0;
    gPlayState->colChkCtx={};gPlayState->colChkCtx.colATCount=1;gPlayState->colChkCtx.colAT[0]=&attack.base;gPlayState->colChkCtx.colACCount=1;gPlayState->colChkCtx.colAC[0]=&body->base;
    CollisionCheck_AT(gPlayState,&gPlayState->colChkCtx);CollisionCheck_Damage(gPlayState,&gPlayState->colChkCtx);
    int damage=actor->colChkInfo.damage;actor->update(actor,gPlayState);
    body->base.acFlags=originalFlags&~AC_HIT;body->info.acHit=nullptr;body->info.acHitInfo=nullptr;body->info.bumperFlags&=~BUMP_HIT;
    gPlayState->colChkCtx=ctx;Collider_DestroyCylinder(gPlayState,&attack);ObjectExtension::GetInstance().Free(&source);return damage;
}
void Mechanics(){
    Clear(gPlayState);auto* a=PodAt();auto* b=PodAt(90);
    Check(LivePods(gPlayState)==2 && !Spawn(gPlayState,&boss->actor,{0,-640,300}),"No more than two additional pods are allocated");
    Check(a->category==ACTORCAT_PROP && !(a->flags&ACTOR_FLAG_ATTENTION_ENABLED) && !Targetable(a),"Falling pods are harmless props, not rewarding enemy defeats or premature targets");
    auto pause=gPlayState->pauseCtx.state;gPlayState->pauseCtx.state=6;Step(a,100);gPlayState->pauseCtx.state=pause;
    Check(Status(a).age==0,"Pausing preserves the complete fall warning");
    Step(a,FallTicks-2);Check(Status(a).phase==Phase::Falling && !Body(a),"The receiving collider is absent during the warning");
    auto* body=Body(a);Check(body && Targetable(a) && Status(a).phase==Phase::Incubating && (a->flags&ACTOR_FLAG_ATTENTION_ENABLED),"Landed pods become native lock-on targets with receiving cylinders");
    Check(!(body->base.acFlags&AC_TYPE_ENEMY),"Enemy attacks cannot damage their own brood pods");
    Check(Hit(a,body,DMG_EXPLOSIVE,AT_TYPE_ENEMY)==0 && a->colChkInfo.health==3,"The native narrow phase rejects an enemy-aligned explosive");
    Check(Hit(a,body,DMG_EXPLOSIVE,AT_TYPE_PLAYER,true)==0 && a->colChkInfo.health==3,"Hard native collisions cannot destroy a pod");
    Check(Hit(a,body,DMG_SLINGSHOT)==1 && a->colChkInfo.health==2,"A native slingshot hit damages the landed shell");
    Step(a,6);body=Body(a);Check(body && Hit(a,body,DMG_BOOMERANG)==0 && Status(a).delayUsed && Status(a).delayed>0,"A zero-damage boomerang contact delays hatching through the native collision path");
    int delayed=Status(a).delayed;Hit(a,body,DMG_DEKU_NUT);Check(Status(a).delayed<delayed,"Repeated utility contacts cannot reset the finite delay");
    pause=gPlayState->pauseCtx.state;gPlayState->pauseCtx.state=6;auto state=Status(a);Step(a,100);gPlayState->pauseCtx.state=pause;
    Check(Status(a).age==state.age && Status(a).delayed==state.delayed,"Pause preserves both incubation and its utility delay");
    int kills=defeats;Hit(a,body,DMG_EXPLOSIVE);Check(Status(a).phase==Phase::Broken && defeats==kills,"A bomb breaks the shell without producing enemy-defeat loot or XP");
    Step(a,100);Check(!a->update && AddedLarvae(gPlayState)==0,"A shattered pod retires without hatching");
    Step(b,FallTicks+HatchTicks-1);Check(AddedLarvae(gPlayState)==0,"An intact pod cannot hatch before both timers expire");
    Step(b);Check(Status(b).phase==Phase::Hatched && AddedLarvae(gPlayState)==1,"An ignored pod hatches exactly one tracked native larva");
    Actor* child=nullptr;for(auto* e=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;e;e=e->next)if(e->update && e->id==ACTOR_EN_GOMA && e->params==7)child=e;
    Check(child && !child->parent && reinterpret_cast<EnGoma*>(child)->gomaType==ENGOMA_NORMAL,"The hatchling has native combat state without a boss-array index or parent pointer");
    Step(b,100);Check(AddedLarvae(gPlayState)==1,"A hatched shell cannot create duplicate larvae");
    a=PodAt();Step(a,FallTicks+HatchTicks);Check(AddedLarvae(gPlayState)==2 && !Spawn(gPlayState,&boss->actor,{0,-640,300}),"Living hatchlings share the two-slot additional-brood budget");
    auto* original=Actor_SpawnAsChild(&gPlayState->actorCtx,&boss->actor,gPlayState,ACTOR_EN_GOMA,0,-640,300,0,0,0,0);
    Check(original && original->update,"Gohma's original indexed brood still spawns through its native path");Clear(gPlayState);
    Check(original->update && AddedLarvae(gPlayState)==0 && Larvae(gPlayState)==1,"Clearing added brood preserves the boss's original child");Actor_Kill(original);
    a=PodAt();boss->actionFunc=BossGoma_FloorStunned;Step(a);Check(Status(a).phase==Phase::Broken,"Stunning Gohma cancels a falling pod before it lands");Step(a,100);Check(!AddedLarvae(gPlayState),"A cancelled broodfall never hatches later");BossGoma_SetupFloorIdle(boss);boss->framesUntilNextAction=20000;
    a=PodAt();Step(a,FallTicks);body=Body(a);Check(Hit(a,body,DMG_ARROW_FIRE)>=3 && Status(a).phase==Phase::Broken,"Fire destroys a landed pod through its native damage table");Clear(gPlayState);
    a=PodAt();Step(a,FallTicks);body=Body(a);Check(Hit(a,body,DMG_ARROW_ICE)==2 && Status(a).delayed>0 && a->colChkInfo.health==1,"Ice combines shell damage with the finite incubation delay");Clear(gPlayState);
    a=PodAt(0,100);Step(a,FallTicks);CVarSetInteger("gEnhancements.ARPG.Skills",1);
    GET_PLAYER(gPlayState)->actor.world.pos={0,-640,180};
    Check(SkillCombat::Cast(gPlayState,ARPG_SKILL_MAGIC_FLAME_BURST,0,1),"The real Flame Burst skill initializes beside a brood pod");
    auto collision=gPlayState->colChkCtx;
    for(int i=0;i<12 && Status(a).phase==Phase::Incubating;++i){
        gPlayState->colChkCtx={};++gPlayState->gameplayFrames;a->update(a,gPlayState);
        for(auto* e=gPlayState->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;e;e=e->next)if(e->update && e->id==ACTOR_EN_M_THUNDER)e->update(e,gPlayState);
        CollisionCheck_AT(gPlayState,&gPlayState->colChkCtx);CollisionCheck_Damage(gPlayState,&gPlayState->colChkCtx);
    }
    Check(Status(a).phase==Phase::Broken,"Flame Burst reaches the pod through real skill geometry and the native damage pipeline");
    gPlayState->colChkCtx=collision;SkillCombat::CancelAll(gPlayState);CVarSetInteger("gEnhancements.ARPG.Skills",0);Clear(gPlayState);
    a=PodAt(0,100);Step(a,FallTicks);CVarSetInteger("gEnhancements.ARPG.Items",1);
    auto* player=GET_PLAYER(gPlayState);player->actor.world.pos={0,-640,180};player->actor.shape.rot.y=0x8000;player->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;
    CVarSetInteger("gEnhancements.ARPG.Creatures",1);
    auto* primary=Creatures::Spawn(gPlayState,Creatures::Kind::Sentinel,{-100,-640,100});
    Check(primary && primary->update,"An actual Sentinel provides a valid primary hammer target");
    Arsenal::OnContact(primary,&player->actor,DMG_HAMMER_SWING,AT_TYPE_PLAYER,3);
    Check(Arsenal::LiveFaults(gPlayState)>0,"A hammer contact creates the real terrain-bound fault-line technique");
    collision=gPlayState->colChkCtx;
    for(int i=0;i<16 && Status(a).phase==Phase::Incubating;++i){
        gPlayState->colChkCtx={};++gPlayState->gameplayFrames;a->update(a,gPlayState);
        for(auto* e=gPlayState->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;e;e=e->next)if(e->update && Arsenal::IsFault(e))e->update(e,gPlayState);
        CollisionCheck_AT(gPlayState,&gPlayState->colChkCtx);CollisionCheck_Damage(gPlayState,&gPlayState->colChkCtx);
    }
    Check(Status(a).phase==Phase::Broken,"Hammer fault-line geometry shatters a landed pod through native collision");
    gPlayState->colChkCtx=collision;Arsenal::Reset(gPlayState);Actor_Kill(primary);CVarSetInteger("gEnhancements.ARPG.Creatures",0);CVarSetInteger("gEnhancements.ARPG.Items",0);Clear(gPlayState);
    a=PodAt();Step(a,FallTicks+HatchTicks+FadeTicks);b=PodAt(90);boss->actor.colChkInfo.health=0;Step(b);GameInteractor_ExecuteOnPlayerUpdate();
    Check(!b->update && AddedLarvae(gPlayState)==0,"Boss defeat removes pending pods and surviving added larvae");boss->actor.colChkInfo.health=100;
    a=PodAt();CVarSetInteger("gEnhancements.ARPG.BossBroods",0);Step(a);Check(!a->update && !Spawn(gPlayState,&boss->actor,{90,-640,100}),"The feature toggle clears live pods and prevents new summons");CVarSetInteger("gEnhancements.ARPG.BossBroods",1);
    Check(!Spawn(gPlayState,&boss->actor,{1.e7f,1.e7f,1.e7f}),"Unsupported positions cannot create invisible ground warnings");
    int total=gPlayState->actorCtx.total;gPlayState->actorCtx.total=180;Check(!Spawn(gPlayState,&boss->actor,{-90,-640,100}),"Actor allocation respects the global safety ceiling");gPlayState->actorCtx.total=total;
    int waves=Waves(&boss->actor);for(int i=0;i<500 && Waves(&boss->actor)==waves;++i){++gPlayState->gameplayFrames;TickBoss(&boss->actor,gPlayState);}
    Check(Waves(&boss->actor)==waves+1 && LivePods(gPlayState)>0,"The boss director naturally emits a marked broodfall wave");
    for(int i=0;i<1000;++i)TickBoss(&boss->actor,gPlayState);Check(Waves(&boss->actor)==waves+1,"Repeated hooks in the same gameplay frame cannot advance or duplicate a wave");
    Clear(gPlayState);a=PodAt();Check(a->update,"A pending brood pod is present before actual scene teardown");
}
void Tick(){
    if(phase==99 || phase==98)return;
    try{
        ++frames;
        if(phase==0 && frames>60 && gGameState){
            CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_CHILD;
            for(auto k:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.BossBroods"})CVarSetInteger(k,1);
            for(auto k:{"gEnhancements.ARPG.Packs","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG"})CVarSetInteger(k,0);
            gARPGSave={};SeedRng(0xBA00D);Warp();phase=1;frames=0;return;
        }
        if(gPlayState && phase>0){gSaveContext.health=gSaveContext.healthCapacity;GET_PLAYER(gPlayState)->invincibilityTimer=40;}
        if(phase==1 && frames>80 && gPlayState && gPlayState->sceneNum==SCENE_DEKU_TREE_BOSS){PrepareBoss();phase=2;frames=0;return;}
        if(phase==2 && frames>30 && GameInteractor::IsPlayerInControl()){Mechanics();Warp();phase=3;frames=0;return;}
        if(phase==3 && frames>80 && gPlayState && gPlayState->sceneNum==SCENE_DEKU_TREE_BOSS){Check(!LivePods(gPlayState) && !AddedLarvae(gPlayState),"Native scene teardown removes brood actors and tracking state");PrepareBoss(true);phase=4;frames=0;return;}
        if(phase==4){
            // Keep the unattended fixture away from arena walls while Gohma runs her original AI.
            auto* p=GET_PLAYER(gPlayState);p->actor.world.pos={0,-640,200};p->actor.velocity={};p->actor.speedXZ=0;
            if(frames%100==0)SPDLOG_INFO("[Brood QA] live frame {} waves {} pods {} larvae {} control {} enabled {} boss disabled {}",frames,Waves(&boss->actor),LivePods(gPlayState),Larvae(gPlayState),GameInteractor::IsPlayerInControl(),Brood::Enabled(),boss->disableGameplayLogic);
        }
        if(phase==4 && frames>500){
            Check(Waves(&boss->actor)>0,"The original Gohma AI produces broodfall during a live fight");
            Check(renderFrames>100,"Original cocoon geometry and spiderweb warnings render in the native arena");
            if(visual){Clear(gPlayState);boss->actor.update=Noop;BossGoma_SetupFloorIdle(boss);boss->framesUntilNextAction=20000;
                auto* p=GET_PLAYER(gPlayState);p->actor.world.pos=p->actor.prevPos={0,-640,260};p->actor.shape.rot.y=0x8000;
                PodAt(-80,140);PodAt(80,140);phase=5;frames=0;return;}
            phase=99;Write("passed");return;
        }
        if(phase==5){GET_PLAYER(gPlayState)->actor.world.pos={0,-640,300};}
        if(phase==5 && frames>45){
            gPlayState->frameAdvCtx.enabled=true;GET_PLAYER(gPlayState)->invincibilityTimer=0;
            Vec3f at={0,-612,130},eye={0,-490,410};Play_CameraSetAtEye(gPlayState,Play_GetActiveCamId(gPlayState),&at,&eye);
            phase=98;Write("passed");return;
        }
        if(frames>1600)throw std::runtime_error("Brood QA timeout at phase "+std::to_string(phase));
    }catch(const std::exception& e){phase=99;Write("failed",e.what());}
}
static void Register(){
    const char* enabled=std::getenv("SOH_ARPG_BROOD_QA"),*home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
    visual=std::getenv("SOH_ARPG_BROOD_UI")!=nullptr;output=std::filesystem::path(home)/"brood-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void*){++defeats;});
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(phase==4 && LivePods(gPlayState))++renderFrames;});
}
static RegisterShipInitFunc init(Register);
}
