#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/EnemyVisuals.h"
#include "soh/Enhancements/arpg/SkulltulaGeometry.h"
#include "soh/Enhancements/arpg/CreatureSystem.h"
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
#define this thisx
#include "overlays/actors/ovl_En_St/z_en_st.h"
#undef this
void Sram_InitDebugSave(void);
void EnSt_WaitOnCeiling(EnSt*,PlayState*);void EnSt_MoveToGround(EnSt*,PlayState*);
void EnSt_WaitOnGround(EnSt*,PlayState*);void EnSt_ReturnToCeiling(EnSt*,PlayState*);
void EnSt_BounceAround(EnSt*,PlayState*);void EnSt_Die(EnSt*,PlayState*);
s32 EnSt_CheckHitFrontside(EnSt*);s32 EnSt_CheckHitBackside(EnSt*,PlayState*);
s32 EnSt_OverrideLimbDraw(PlayState*,s32,Gfx**,Vec3f*,Vec3s*,void*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0,kills=0,stunnedFrames=0;
bool visual=false,dropped=false,landed=false,jointsMoved=false,pulsed=false;
EnSt* enemy=nullptr;Vec3f fixture{},perch{},start{};std::array<Vec3s,30> firstJoints{};
std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
void Write(const char* status,const std::string& error="") {
    std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"render_frames",drawFrames},{"stunned_frames",stunnedFrames},{"fixture",{fixture.x,fixture.y,fixture.z}},{"perch",{perch.x,perch.y,perch.z}},{"checks",checks}}.dump(2);
}
void Check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);checks.push_back(name);SPDLOG_INFO("[Skulltula QA] PASS {}",name);Write("running");}
void Warp(){gSaveContext.entranceIndex=ENTR_DEKU_TREE_ENTRANCE;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;
    gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);enemy=nullptr;}
bool Alive(EnSt* e){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a==reinterpret_cast<Actor*>(e))return a->update!=nullptr;return false;}
EnSt* Spawn(int param=0){auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_ST,perch.x,perch.y,perch.z,0,Math_Vec3f_Yaw(&perch,&fixture),0,param);
    Check(a && a->update,"Native Skulltula initializes beneath the real ceiling");return reinterpret_cast<EnSt*>(a);}
bool Practice(){auto* p=GET_PLAYER(gPlayState);start=p->actor.world.pos;
    for(int i=0;i<16;++i){p->actor.shape.rot.y=i*0x1000;auto* a=Creatures::SpawnSkulltulaPractice(gPlayState,0);if(!a)continue;
        enemy=reinterpret_cast<EnSt*>(a);perch=a->world.pos;fixture=start;float dx=perch.x-start.x,dz=perch.z-start.z,len=std::hypot(dx,dz);fixture.x+=dx*(len-125)/len;fixture.z+=dz*(len-125)/len;
        auto ray=fixture;ray.y+=80;CollisionPoly* poly=nullptr;fixture.y=BgCheck_EntityRaycastFloor1(&gPlayState->colCtx,&poly,&ray);return poly && fixture.y>BGCHECK_Y_MIN;}
    return false;
}
Gfx* Native(int limb){return static_cast<StandardLimb*>(enemy->skelAnime.skeleton[limb-1])->dList;}
Gfx* Model(int limb){auto* p=Native(limb);ARPG_OverrideEnemyLimb(gPlayState,&enemy->actor,limb,&p);return p;}
void Damage(int damage,int effect,uint32_t flags=DMG_SLASH_KOKIRI){static ColliderInfo source{};source.toucher.dmgFlags=flags;enemy->colCylinder[0].base.acFlags|=AC_HIT;enemy->colCylinder[0].info.acHitInfo=&source;enemy->actor.colChkInfo.damage=damage;enemy->actor.colChkInfo.damageEffect=effect;EnSt_CheckHitBackside(enemy,gPlayState);}
void Models(){
    Check(enemy->actor.params==0 && enemy->skelAnime.limbCount==30 && std::abs(enemy->actor.scale.x-.04f)<.0001f && enemy->colSph.elements[0].dim.modelSphere.radius==28,"Original skeleton, scale and collision remain authoritative");
    std::set<Gfx*> lists;
    for(int limb:SkulltulaGeometry::Limbs){auto* p=Model(limb);Check(p && p!=Native(limb),"Each visible native joint uses authored geometry");Check(p==Model(limb),"Each joint has a stable cached display list");lists.insert(p);}
    Check(lists.size()==10,"Jaw, shield and eight legs retain separate native animation transforms");
    for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"}){CVarSetInteger(key,0);for(int limb:SkulltulaGeometry::Limbs)Check(Model(limb)==Native(limb),"Disabling the upgrade restores each native limb");CVarSetInteger(key,1);}
    Gfx* p=nullptr;ARPG_OverrideEnemyLimb(gPlayState,&enemy->actor,7,&p);Check(!p,"Hidden joints remain invisible");
    auto eye=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;Check(Model(5)==Native(5),"Distant enemies use lightweight native geometry");GET_ACTIVE_CAM(gPlayState)->eye=eye;
    std::copy(std::begin(enemy->jointTable),std::end(enemy->jointTable),firstJoints.begin());
    auto* big=Spawn(1);Check(std::abs(big->actor.scale.x-.056f)<.0001f && big->colliderScale==1.4f && big->floorHeightOffset>44 && big->colSph.elements[0].dim.modelSphere.radius==39,"Big Skulltula retains the native 1.4x size and collider scaling");Actor_Kill(&big->actor);
    auto* invisible=Spawn(2);Check(invisible->actor.flags&ACTOR_FLAG_REACT_TO_LENS,"Invisible Skulltula retains the original Lens visibility flag");auto* saved=enemy;enemy=invisible;Check(lists.count(Model(5))==1,"Invisible variant shares the bounded native geometry cache");enemy=saved;Actor_Kill(&invisible->actor);
}
void Camera(){auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=20;p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};p->actor.shape.rot.y=Math_Vec3f_Yaw(&fixture,&perch);
    Vec3f at={perch.x,fixture.y+65,perch.z};float dx=fixture.x-perch.x,dz=fixture.z-perch.z;float len=std::hypot(dx,dz);Vec3f eye={perch.x+dx/len*245,fixture.y+75,perch.z+dz/len*245};Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);}
void Tick(){if(phase>=98)return;try{++frames;
    if(phase==0 && frames>60 && gGameState){
        CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();
        gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_CHILD;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;gARPGSave={};
        for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"})CVarSetInteger(key,1);
        for(auto key:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG"})CVarSetInteger(key,0);
        SeedRng(0x5C011);Warp();phase=1;frames=0;return;
    }
    if(gPlayState && phase>=2 && phase<=8)Camera();
    if(phase==1 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()){
        for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)Actor_Kill(a);
        Check(Practice(),"Practice summon finds an actual room ceiling, clear approach and walkable floor");
        Check(enemy->ceilingPos.y>perch.y && std::abs(enemy->ceilingPos.x-perch.x)<1,"Native ceiling ray anchors the hanging actor to real room geometry");phase=2;frames=0;return;
    }
    if(phase==2){dropped|=enemy->actionFunc==EnSt_MoveToGround;landed|=enemy->actionFunc==EnSt_WaitOnGround;
        if(frames>100){Models();Check(dropped && landed,"Approaching naturally triggers native drop, landing and ground-wait states");phase=3;frames=0;return;}}
    if(phase==3){pulsed|=enemy->teethR>150;for(int i=1;i<30;++i){auto a=enemy->jointTable[i],b=firstJoints[i];jointsMoved|=a.x!=b.x || a.y!=b.y || a.z!=b.z;}
        if(frames>160){Check(jointsMoved && pulsed && drawFrames>100,"Live rendered legs, mandible and native warning pulse animate");Check(enemy->colSph.elements[0].dim.worldSphere.radius>0,"Native post-limb callback continues updating the attack sphere");
            enemy->colCylinder[2].base.acFlags|=AC_HIT;int hp=enemy->actor.colChkInfo.health;Check(EnSt_CheckHitFrontside(enemy) && enemy->swayTimer==60 && enemy->actor.colChkInfo.health==hp,"Armored front deflects damage into native sixty-tick swaying");phase=4;frames=0;return;}}
    if(phase==4 && frames>70){Check(enemy->swayTimer==0 && std::isfinite(enemy->actor.world.pos.x) && std::hypot(enemy->actor.world.pos.x-perch.x,enemy->actor.world.pos.z-perch.z)<2,"Sway completes at its real ceiling anchor without positional drift");Damage(0,1);Check(enemy->stunTimer==120 && enemy->actor.colChkInfo.health==2,"Utility stun keeps health and starts the native 120-tick pause");phase=5;frames=0;return;}
    if(phase==5){if(enemy->stunTimer)++stunnedFrames;
        if(frames>130){Check(stunnedFrames>=115 && !enemy->stunTimer,"Native stun expires and movement resumes");Damage(1,0);Check(enemy->actor.colChkInfo.health==1 && enemy->takeDamageSpinTimer>0,"Exposed rear takes damage and starts the native spinning recoil");phase=6;frames=0;return;}}
    if(phase==6 && frames>50){fixture=start;phase=7;frames=0;return;}
    if(phase==7 && frames%200==0)SPDLOG_INFO("[Skulltula QA] RETURN wait={} return={} y={} home={} xz={} player={},{}",enemy->actionFunc==EnSt_WaitOnCeiling,enemy->actionFunc==EnSt_ReturnToCeiling,enemy->actor.world.pos.y,enemy->actor.home.pos.y,enemy->actor.xzDistToPlayer,GET_PLAYER(gPlayState)->actor.world.pos.x,GET_PLAYER(gPlayState)->actor.world.pos.z);
    if(phase==7 && enemy->actionFunc==EnSt_WaitOnCeiling){Check(std::abs(enemy->actor.world.pos.y-perch.y)<8,"Leaving range returns the actor to its native hanging position");Damage(8,0);Check(enemy->actionFunc==EnSt_BounceAround && kills==1 && !(enemy->actor.flags&ACTOR_FLAG_ATTENTION_ENABLED),"Lethal rear strike enters native bouncing death and rewards once");phase=8;frames=0;return;}
    if(phase==8 && frames>180){Check(!Alive(enemy) && kills==1,"Native bouncing death removes the actor without duplicate reward");enemy=Spawn();Damage(8,0,DMG_ARROW_NORMAL);Check(enemy->actionFunc==EnSt_Die && enemy->finishDeathTimer==8 && kills==2,"Arrow damage retains the separate native death route");phase=9;frames=0;return;}
    if(phase==9 && frames>30){Check(!Alive(enemy) && kills==2,"Arrow death finishes and rewards exactly once");Warp();phase=10;frames=0;return;}
    if(phase==10 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()){
        Check(kills==2,"Scene teardown does not create duplicate defeats");if(!visual){phase=99;Write("passed");return;}
        Check(Practice(),"Fresh visual fixture uses the production practice-summon path");phase=11;frames=0;return;}
    if(phase==11){Camera();if(frames>100){gPlayState->frameAdvCtx.enabled=true;phase=98;Write("passed");}}
    if(frames>1000)throw std::runtime_error("Skulltula integration timeout");
}catch(const std::exception& e){phase=99;Write("failed",e.what());}}
void Register(){auto* enabled=std::getenv("SOH_ARPG_SKULLTULA_QA");auto* home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
    output=std::filesystem::path(home)/"skulltula-results.json";visual=std::getenv("SOH_ARPG_SKULLTULA_UI")!=nullptr;
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(phase==3)++drawFrames;});
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* a){if(a==enemy)++kills;});
}
static RegisterShipInitFunc init(Register);
}
