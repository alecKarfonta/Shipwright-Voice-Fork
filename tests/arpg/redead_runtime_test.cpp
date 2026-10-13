#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/RedeadVisuals.h"
#include "soh/Enhancements/arpg/RedeadGeometry.h"
#include "soh/Enhancements/arpg/CreatureSystem.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include <libultraship/bridge/consolevariablebridge.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <set>
#include <cstring>
#include <spdlog/spdlog.h>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "overlays/actors/ovl_En_Rd/z_en_rd.h"
void Sram_InitDebugSave(void);
void EnRd_SetupIdle(EnRd*);void EnRd_SetupWalkToPlayer(EnRd*,PlayState*);void EnRd_SetupCrouch(EnRd*);
void EnRd_UpdateDamage(EnRd*,PlayState*);void EnRd_SetupAttemptPlayerFreeze(EnRd*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0,kills=0,fireFrames=0,fadeFrames=0,grabFrames=0;bool visual=false,frozen=false,walking=false,rose=false,mourning=false;
EnRd* enemy=nullptr;EnRd* witness=nullptr;Vec3f fixture={-4,0,499},anchor={-4,0,309},firstPos{};
int health=0;std::array<Vec3s,26> firstJoints{};std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
void Write(const char* status,const std::string& error=""){std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"render_frames",drawFrames},{"fire_frames",fireFrames},{"fade_frames",fadeFrames},{"grab_frames",grabFrames},{"native_creature_defeats",kills},{"checks",checks}}.dump(2);}
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);checks.push_back(label);SPDLOG_INFO("[ReDead QA] PASS {}",label);Write("running");}
bool Alive(const Actor* actor){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a==actor)return a->update!=nullptr;return false;}
void Warp(){gSaveContext.entranceIndex=ENTR_DEKU_TREE_ENTRANCE;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);enemy=witness=nullptr;}
void Camera(){auto* p=GET_PLAYER(gPlayState);if(phase!=4)gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=0;
 p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};p->actor.shape.rot.y=0x8000;
 Vec3f at={anchor.x,35,anchor.z},eye={anchor.x+155,90,anchor.z+205};Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);}
void Fixture(){for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)Actor_Kill(a);Camera();}
EnRd* Spawn(int params,Vec3f p){auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_RD,p.x,p.y,p.z,0,0,0,params);Check(a && a->update,"Native undead spawns on its original rig");a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED;return reinterpret_cast<EnRd*>(a);}
Gfx* Native(int limb){return static_cast<StandardLimb*>(enemy->skelAnime.skeleton[limb-1])->dList;}
Gfx* Model(int limb){auto* dl=Native(limb);ARPG_OverrideRedeadLimb(gPlayState,&enemy->actor,limb,&dl);return dl;}
void Damage(int damage,int effect){enemy->collider.base.acFlags|=AC_HIT;enemy->actor.colChkInfo.damage=damage;enemy->actor.colChkInfo.damageEffect=effect;EnRd_UpdateDamage(enemy,gPlayState);}
void Models(){Check(enemy->skelAnime.limbCount==26 && enemy->skelAnime.dListCount==15 && std::abs(enemy->actor.scale.x-.01f)<.00001f && enemy->collider.dim.radius==20 && enemy->collider.dim.height==70,"Original rig, scale and cylinder remain authoritative");
 std::set<Gfx*> lists;for(int limb:RedeadGeometry::Limbs){auto* model=Model(limb);Check(model!=Native(limb) && model==Model(limb),"Visible native joint has a cached replacement");lists.insert(model);}Check(lists.size()==15,"All fifteen native flex matrices have distinct display lists");
 bool off=true;for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"}){CVarSetInteger(key,0);for(int limb:RedeadGeometry::Limbs)off&=Model(limb)==Native(limb);CVarSetInteger(key,1);}Check(off,"Both feature gates restore all original limbs");
 auto eye=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;Check(Model(24)==Native(24),"Distant actors use native geometry");GET_ACTIVE_CAM(gPlayState)->eye=eye;
 auto alpha=enemy->alpha;enemy->alpha=128;auto* fading=Model(24);Check(!lists.contains(fading)&&fading!=Native(24),"Native translucent death has a separate cached model");enemy->alpha=64;Check(Model(24)==fading,"Fade alpha does not allocate another model");enemy->alpha=alpha;
 Gfx* dl=nullptr;ARPG_OverrideRedeadLimb(gPlayState,&enemy->actor,23,&dl);Check(!dl,"Head rotation dummy joint stays hidden");
}
void Tick(){if(phase>=98)return;try{++frames;
 if(phase==0 && frames>60 && gGameState){CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_CHILD;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;gARPGSave={};
  for(auto k:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"})CVarSetInteger(k,1);
  for(auto k:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG"})CVarSetInteger(k,0);
  SeedRng(0xDEAD);Warp();phase=1;frames=0;return;}
 if(gPlayState && phase>=2 && phase<=14)Camera();
 if(phase==1 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()){
  Camera();auto* practice=Creatures::SpawnRedeadPractice(gPlayState,0);Check(practice!=nullptr,"Practice placement works alongside original room enemies");Actor_Kill(practice);Fixture();enemy=Spawn(0,anchor);phase=2;frames=0;return;}
 if(phase==2 && frames>25){Models();std::copy(std::begin(enemy->jointTable),std::end(enemy->jointTable),firstJoints.begin());firstPos=enemy->actor.world.pos;fixture.z=anchor.z+125;phase=3;frames=0;return;}
 if(phase==3){frozen|=GET_PLAYER(gPlayState)->actor.freezeTimer>0;walking|=enemy->action==4;
  if(frames>100){bool animated=false;for(int i=1;i<26;++i)animated|=std::memcmp(&enemy->jointTable[i],&firstJoints[i],sizeof(Vec3s))!=0;
   SPDLOG_INFO("[ReDead QA] motion frozen={} walking={} animated={} z={} start={} action={} distance={} flags1={} flags2={} params={}",frozen,walking,animated,enemy->actor.world.pos.z,firstPos.z,enemy->action,enemy->actor.xzDistToPlayer,GET_PLAYER(gPlayState)->stateFlags1,GET_PLAYER(gPlayState)->stateFlags2,enemy->actor.params);Check(frozen && walking && animated && enemy->actor.world.pos.z>firstPos.z+10,"Native gaze freezes Link, then advances an articulated walk");fixture=enemy->actor.world.pos;fixture.z+=30;health=gSaveContext.health;phase=4;frames=0;return;}}
 if(phase==4 && frames>65){Check(grabFrames>15 && enemy->grabState==2 && (GET_PLAYER(gPlayState)->stateFlags2&PLAYER_STATE2_GRABBED_BY_ENEMY),"Native proximity captures Link and enters the looping grab");Check(gSaveContext.health<health && enemy->actor.shape.yOffset<0,"Grab damages Link and adapts its height for child Link");GET_PLAYER(gPlayState)->stateFlags2&=~PLAYER_STATE2_GRABBED_BY_ENEMY;fixture.z+=180;phase=5;frames=0;return;}
 if(phase==5 && frames>40){Check(enemy->action!=8 && enemy->actor.shape.yOffset==0 && (enemy->actor.flags&ACTOR_FLAG_ATTENTION_ENABLED),"Escape animation restores targeting and normal body height");EnRd_SetupIdle(enemy);int hp=enemy->actor.colChkInfo.health;Damage(0,0);Check(enemy->actor.colChkInfo.health==hp&&enemy->action==0,"Native ineffective damage leaves undead armor rules unchanged");Damage(0,1);Check(enemy->action==1 && enemy->actor.colorFilterTimer==80,"Hookshot effect enters native eighty-tick stun");phase=6;frames=0;return;}
 if(phase==6 && frames>105){Check(enemy->action!=1 && enemy->actor.colChkInfo.health==8,"Hookshot stun recovers without damage");gSaveContext.sunsSongState=SUNSSONG_SPECIAL;EnRd_UpdateDamage(enemy,gPlayState);Check(enemy->stunnedBySunsSong && enemy->sunsSongStunTimer==600,"Sun's Song starts the native long stun");phase=7;frames=0;return;}
 if(phase==7 && frames>35){Check(enemy->stunnedBySunsSong && enemy->sunsSongStunTimer<600 && enemy->action==1,"Sun's Song holds the native pose while its timer advances");enemy->sunsSongStunTimer=1;enemy->actor.colorFilterTimer=1;phase=8;frames=0;return;}
 if(phase==8 && frames>30){Check(!enemy->stunnedBySunsSong&&gSaveContext.sunsSongState==SUNSSONG_INACTIVE&&enemy->action!=1,"Sun's Song releases through the native recovery path");EnRd_SetupIdle(enemy);Damage(2,14);Check(enemy->fireTimer==40&&enemy->action==9&&enemy->actor.colChkInfo.health==6,"Fire damage starts native recoil and attached flames");phase=9;frames=0;return;}
 if(phase==9 && frames==15){std::set<int> heights;for(auto p:enemy->firePos)heights.insert(p.y);Check(heights.size()>3,"Native post-limb callbacks update separate body fire anchors");}
 if(phase==9 && frames>85){Check(fireFrames>=35&&enemy->fireTimer==0,"Attached flames complete without blocking recovery");auto p=anchor;p.x+=80;witness=Spawn(2,p);Damage(20,15);Check(enemy->action==10&&kills==1&&witness->actor.parent==&enemy->actor,"Lethal damage emits one defeat and attracts nearby ReDeads");phase=10;frames=0;return;}
 if(phase==10){mourning|=witness->action==3||witness->isMourning;
  if(frames>410){Check(!Alive(reinterpret_cast<Actor*>(enemy))&&kills==1&&gARPGSave.kills==1&&fadeFrames>40,"Death animation, corpse timer and native fade complete with one reward");Check(mourning&&witness->actor.parent==nullptr,"Mourning uses the native walk and releases the corpse reference");Actor_Kill(&witness->actor);witness=nullptr;fixture={-4,0,499};enemy=Spawn(-2,anchor);phase=11;frames=0;return;}}
 if(phase==11 && frames>25){Models();auto* gibdo=Model(24);auto* saved=enemy;enemy=Spawn(3,{anchor.x+80,anchor.y,anchor.z});Check((enemy->actor.flags&ACTOR_FLAG_REACT_TO_LENS)&&Model(24)!=gibdo,"Lens-sensitive ReDead keeps its native flag and distinct mask");Actor_Kill(&enemy->actor);enemy=saved;Actor_Kill(&enemy->actor);enemy=Spawn(-3,anchor);Check(enemy->action==11&&enemy->actor.shape.rot.x==-0x4000,"Coffin variant begins in the original horizontal pose");phase=12;frames=0;return;}
 if(phase==12){rose|=enemy->actor.world.pos.y>anchor.y+40;
  if(frames>150){Check(rose&&enemy->action!=11&&enemy->actor.shape.rot.x==0&&enemy->actor.gravity==-3.5f,"Gibdo rises, rotates upright and restores gravity");Actor_Kill(&enemy->actor);enemy=nullptr;phase=13;frames=0;return;}}
 if(phase==13 && frames>10){Camera();std::vector<Actor*> practice;for(int variant=0;variant<4;++variant){auto* a=Creatures::SpawnRedeadPractice(gPlayState,variant);if(a)practice.push_back(a);if(a)Actor_Kill(a);}Check(practice.size()==4,"All four practice variants pass floor and headroom placement");Check(!Creatures::SpawnRedeadPractice(gPlayState,-1)&&!Creatures::SpawnRedeadPractice(gPlayState,4),"Unknown practice variants are rejected");Warp();phase=14;frames=0;return;}
 if(phase==14 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()){Check(kills==1,"Scene teardown adds no duplicate defeat rewards");if(!visual){phase=99;Write("passed");return;}Fixture();enemy=Spawn(0,{anchor.x-40,anchor.y,anchor.z});witness=Spawn(-2,{anchor.x+40,anchor.y,anchor.z});phase=15;frames=0;return;}
 if(phase==15){Camera();if(frames>20){gPlayState->frameAdvCtx.enabled=true;phase=98;Write("passed");}}
 if(frames>1200)throw std::runtime_error("ReDead integration timeout");
 }catch(const std::exception& e){phase=99;Write("failed",e.what());}}
void Register(){auto* enabled=std::getenv("SOH_ARPG_REDEAD_QA");auto* home=std::getenv("SHIP_HOME");if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
 output=std::filesystem::path(home)/"redead-results.json";visual=std::getenv("SOH_ARPG_REDEAD_UI")!=nullptr;
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(phase>=2 && phase<=12 && enemy && Alive(&enemy->actor)){++drawFrames;if(enemy->fireTimer>0)++fireFrames;if(enemy->alpha<255)++fadeFrames;if(enemy->action==8)++grabFrames;}});
 // [TIME] | [INFO] | RedeadQA.Footstep | Supply the native footstep stimulus after Link updates; the stationary camera fixture otherwise cannot wake an idle undead.
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorUpdate>([](void* ptr){if(phase==3 && static_cast<Actor*>(ptr)->id==ACTOR_PLAYER)GET_PLAYER(gPlayState)->stateFlags2|=PLAYER_STATE2_FOOTSTEP;});
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* actor){if(static_cast<Actor*>(actor)->id==ACTOR_EN_RD)++kills;});
}
static RegisterShipInitFunc init(Register);
}
