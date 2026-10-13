#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/KeeseVisuals.h"
#include "soh/Enhancements/arpg/KeeseGeometry.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include <libultraship/bridge/consolevariablebridge.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <set>
#include <spdlog/spdlog.h>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "overlays/actors/ovl_En_Firefly/z_en_firefly.h"
void Sram_InitDebugSave(void);
void EnFirefly_Ignite(EnFirefly*);void EnFirefly_Extinguish(EnFirefly*);
void EnFirefly_SetupFlyIdle(EnFirefly*);void EnFirefly_SetupPerch(EnFirefly*);
void EnFirefly_Perch(EnFirefly*,PlayState*);void EnFirefly_DisturbDiveAttack(EnFirefly*,PlayState*);
void EnFirefly_DiveAttack(EnFirefly*,PlayState*);void EnFirefly_Rebound(EnFirefly*,PlayState*);
void EnFirefly_Stunned(EnFirefly*,PlayState*);void EnFirefly_FrozenFall(EnFirefly*,PlayState*);
void EnFirefly_UpdateDamage(EnFirefly*,PlayState*);
s32 EnFirefly_OverrideLimbDraw(PlayState*,s32,Gfx**,Vec3f*,Vec3s*,void*,Gfx**);
void EnFirefly_PostLimbDraw(PlayState*,s32,Gfx**,Vec3s*,void*,Gfx**);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
// EnFirefly keeps its aura enum private to the native C translation unit.
constexpr int KEESE_AURA_NONE=0,KEESE_AURA_FIRE=1,KEESE_AURA_ICE=2;
int phase=0,frames=0,drawFrames=0;bool visual=false,dived=false,rebounded=false,moved=false,flapped=false;
std::array<EnFirefly*,3> bats{};Vec3f fixture{},firstPos{};Vec3s firstWing{};
std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
void Write(const char* status,const std::string& error=""){
    std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"render_frames",drawFrames},{"checks",checks}}.dump(2);
}
void Check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);checks.push_back(name);SPDLOG_INFO("[Keese QA] PASS {}",name);Write("running");}
void Warp(){gSaveContext.entranceIndex=ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;
    gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);bats={};}
EnFirefly* Spawn(int params,float x=0,float z=155){
    auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_FIREFLY,fixture.x+x,fixture.y+65,fixture.z+z,0,0x8000,0,params);
    Check(a && a->update,"An original EnFirefly actor initializes with its native skeleton");
    a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    return reinterpret_cast<EnFirefly*>(a);
}
Gfx* Native(EnFirefly* e,int limb){return static_cast<StandardLimb*>(e->skelAnime.skeleton[limb-1])->dList;}
Gfx* Head(EnFirefly* e){auto* p=Native(e,27);ARPG_OverrideKeeseLimb(gPlayState,&e->actor,27,&p,0);return p;}
void Damage(EnFirefly* e,int damage,int effect){e->collider.base.acFlags|=AC_HIT;e->actor.colChkInfo.damage=damage;e->actor.colChkInfo.damageEffect=effect;EnFirefly_UpdateDamage(e,gPlayState);}
void Mechanics(){
    std::set<Gfx*> lists;
    for(auto* e:bats){
        Check(e->actor.scale.x==.005f && e->collider.elements[0].dim.worldSphere.radius==15,"Native scale and collision radius are unchanged");
        for(int limb:KeeseGeometry::Limbs)for(int xlu=0;xlu<2;++xlu){
            auto* original=Native(e,limb);auto* p=original;ARPG_OverrideKeeseLimb(gPlayState,&e->actor,limb,&p,xlu);
            Check(original && p && p!=original,"A native visible limb receives its replacement");
            auto* again=original;ARPG_OverrideKeeseLimb(gPlayState,&e->actor,limb,&again,xlu);
            Check(p==again,"Rendering reuses stable cached display lists");lists.insert(p);
        }
    }
    Check(lists.size()==60,"All three variants have distinct opaque and Lens render lists");
    auto* e=bats[0];auto* native=Native(e,27);Gfx* p=nullptr;
    ARPG_OverrideKeeseLimb(gPlayState,&e->actor,27,&p,0);Check(!p,"Invisible native limbs are never resurrected");
    p=native;ARPG_OverrideKeeseLimb(gPlayState,&e->actor,14,&p,0);Check(p==native,"Unsupported rig joints remain unchanged");
    for(auto key:{"gEnhancements.ARPG.RemasteredEnemies","gEnhancements.ARPG.Enabled"}){
        CVarSetInteger(key,0);Check(Head(e)==native,"Disabling the renderer or ARPG restores native geometry and eyes");CVarSetInteger(key,1);
    }
    auto camera=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;
    Check(Head(e)==native,"Distant Keese use native lightweight geometry");GET_ACTIVE_CAM(gPlayState)->eye=camera;
    auto* normal=Head(e);auto* fire=Head(bats[1]);auto* ice=Head(bats[2]);
    EnFirefly_Ignite(e);Check(Head(e)==fire && e->onFire && e->auraType==KEESE_AURA_FIRE,"Ignition swaps to the fire model while retaining the native flame aura");
    EnFirefly_Extinguish(e);Check(Head(e)==normal && !e->onFire && e->auraType==KEESE_AURA_NONE,"Extinguishing immediately restores the normal model");
    auto* invis=Spawn(0x8002,230);Vec3f pos{};Vec3s rot{};Gfx cmds[12]{};Gfx* cursor=cmds;
    auto lens=gPlayState->actorCtx.lensActive;gPlayState->actorCtx.lensActive=false;p=Native(invis,27);
    EnFirefly_OverrideLimbDraw(gPlayState,27,&p,&pos,&rot,invis,&cursor);
    Check(!p && !ARPG_IsKeeseHead(p),"Lens-off rendering preserves native hidden-body behavior");
    gPlayState->actorCtx.lensActive=true;p=Native(invis,27);
    EnFirefly_OverrideLimbDraw(gPlayState,27,&p,&pos,&rot,invis,&cursor);
    Check(p && ARPG_IsKeeseHead(p) && p!=normal,"Lens-on rendering selects the translucent-path replacement");
    EnFirefly_PostLimbDraw(gPlayState,27,&p,&rot,invis,&cursor);
    Check(cursor==cmds,"Replacement heads suppress only the old extra eye display list");
    p=Native(invis,27);EnFirefly_PostLimbDraw(gPlayState,27,&p,&rot,invis,&cursor);
    Check(cursor>cmds,"Native fallback heads still draw their original eyes");
    p=nullptr;EnFirefly_OverrideLimbDraw(gPlayState,1,&p,&pos,&rot,invis,&cursor);
    Check(pos.y==2300,"The native root offset is preserved");gPlayState->actorCtx.lensActive=lens;Actor_Kill(&invis->actor);
    auto* perched=Spawn(3,280);perched->actor.xzDistToPlayer=300;EnFirefly_SetupPerch(perched);EnFirefly_Perch(perched,gPlayState);
    Check(perched->actionFunc==EnFirefly_Perch && perched->actor.speedXZ==0,"Native perching remains stationary while the wing rig idles");
    perched->actor.xzDistToPlayer=60;EnFirefly_Perch(perched,gPlayState);
    Check(perched->actionFunc==EnFirefly_DisturbDiveAttack && perched->skelAnime.playSpeed==3,"Approaching a perch starts the original fast disturbance dive");Actor_Kill(&perched->actor);
    auto* hurt=Spawn(2,-260);hurt->actor.colChkInfo.health=5;Damage(hurt,0,1);
    Check(hurt->actionFunc==EnFirefly_Stunned && hurt->actor.colorFilterTimer==80 && Head(hurt)==normal,"Deku-nut stun keeps native animation and color filtering");
    Damage(hurt,1,3);Check(hurt->actionFunc==EnFirefly_FrozenFall && Head(hurt)==normal,"Ice damage uses the native frozen-fall state with the upgraded model");Actor_Kill(&hurt->actor);
    Check(Head(bats[2])==ice && bats[2]->auraType==KEESE_AURA_ICE,"Ice Keese retain their distinct frost aura");
    for(auto* bat:bats){EnFirefly_SetupFlyIdle(bat);bat->timer=0;}
    firstWing=bats[0]->jointTable[14];firstPos=bats[0]->actor.world.pos;
}
void Tick(){
    if(phase>=98)return;
    try{
        ++frames;
        if(phase==0 && frames>60 && gGameState){
            CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();
            gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_CHILD;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;gARPGSave={};
            for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"})CVarSetInteger(key,1);
            for(auto key:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG"})CVarSetInteger(key,0);
            SeedRng(0xB475);Warp();phase=1;frames=0;return;
        }
        if(phase>1 && gPlayState){auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=20;p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};}
        if(phase==1 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()){
            fixture=GET_PLAYER(gPlayState)->actor.world.pos;bats={Spawn(2,-75),Spawn(0,0),Spawn(4,75)};phase=2;frames=0;return;
        }
        if(phase==2 && frames>5){Mechanics();phase=3;frames=0;return;}
        if(phase==3){for(auto* e:bats){
            dived|=e->actionFunc==EnFirefly_DiveAttack;rebounded|=e->actionFunc==EnFirefly_Rebound;
            moved|=std::abs(e->actor.world.pos.y-firstPos.y)>5;
            auto joint=e->jointTable[14];flapped|=joint.x!=firstWing.x || joint.y!=firstWing.y || joint.z!=firstWing.z;
        }}
        if(phase==3 && frames>300){
            Check(dived,"Original Keese AI enters a dive attack during a live encounter");
            Check(moved,"Native flight changes altitude during a live encounter");
            Check(flapped,"Native flight articulates the wing pivot driving the replacement membrane");
            Check(drawFrames>100,"Upgraded Keese render for more than one hundred live flight frames");
            for(auto* e:bats)Check(std::isfinite(e->bodyPartsPos[0].x) && std::abs(e->bodyPartsPos[0].x-e->actor.world.pos.x)<100,"Native wing effect anchors remain attached to the flying actor");
            Warp();phase=4;frames=0;return;
        }
        if(phase==4 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()){
            int count=0;for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->id==ACTOR_EN_FIREFLY)++count;
            Check(count==0,"Native scene teardown cleans up all practice Keese");
            if(!visual){phase=99;Write("passed");return;}
            fixture=GET_PLAYER(gPlayState)->actor.world.pos;
            bats={Spawn(2,-50,110),Spawn(0,0,110),Spawn(4,50,110)};phase=5;frames=0;return;
        }
        if(phase==5 && frames>2){
            gPlayState->frameAdvCtx.enabled=true;
            Vec3f at={fixture.x,fixture.y+70,fixture.z+110},eye={fixture.x,fixture.y+95,fixture.z-80};
            Play_CameraSetAtEye(gPlayState,Play_GetActiveCamId(gPlayState),&at,&eye);phase=98;Write("passed");return;
        }
        if(frames>1000)throw std::runtime_error("Keese QA timeout");
    }catch(const std::exception& e){phase=99;Write("failed",e.what());}
}
void Register(){
    auto* enabled=std::getenv("SOH_ARPG_KEESE_QA");auto* home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
    output=std::filesystem::path(home)/"keese-results.json";visual=std::getenv("SOH_ARPG_KEESE_UI")!=nullptr;
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(phase==3)++drawFrames;});
}
static RegisterShipInitFunc init(Register);
}
