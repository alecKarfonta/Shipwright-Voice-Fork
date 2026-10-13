#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/DodongoVisuals.h"
#include "soh/Enhancements/arpg/DodongoGeometry.h"
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
#include "overlays/actors/ovl_En_Dodongo/z_en_dodongo.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_En_Bom_Chu/z_en_bom_chu.h"
#include "overlays/actors/ovl_En_Bombf/z_en_bombf.h"
void Sram_InitDebugSave(void);
void EnDodongo_SetupIdle(EnDodongo*);void EnDodongo_SetupWalk(EnDodongo*);void EnDodongo_SetupSwallowBomb(EnDodongo*);
void EnDodongo_CollisionCheck(EnDodongo*,PlayState*);void EnDodongo_SwallowBomb(EnDodongo*,PlayState*);s32 EnDodongo_AteBomb(EnDodongo*,PlayState*);
void EnBomChu_Update(Actor*,PlayState*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0,kills=0,fireFrames=0,tailFrames=0,iceFrames=0,swellingFrames=0,bombType=0;
bool visual=false,moved=false,leftStep=false,rightStep=false,tailHitbox=false;EnDodongo* enemy=nullptr;Actor* bomb=nullptr;
Vec3f fixture={-4,0,499},anchor={-4,0,269},firstPos{};std::array<Vec3s,31> firstJoints{};
std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
void Write(const char* status,const std::string& error=""){std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"render_frames",drawFrames},{"fire_frames",fireFrames},{"tail_frames",tailFrames},{"ice_frames",iceFrames},{"swelling_frames",swellingFrames},{"native_creature_defeats",kills},{"checks",checks}}.dump(2);}
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);checks.push_back(label);SPDLOG_INFO("[Dodongo QA] PASS {}",label);Write("running");}
void Warp(){gSaveContext.entranceIndex=ENTR_DEKU_TREE_ENTRANCE;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);enemy=nullptr;}
bool Alive(const Actor* actor){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a==actor)return a->update!=nullptr;return false;}
void Camera(){auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=20;p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};p->actor.shape.rot.y=0x8000;
 Vec3f at={anchor.x,30,anchor.z},eye={anchor.x+180,100,anchor.z+220};Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);}
void Fixture(){for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)Actor_Kill(a);Camera();}
EnDodongo* Spawn(){auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_DODONGO,anchor.x,anchor.y,anchor.z,0,0,0,-1);Check(a && a->update,"Native Dodongo spawns on its original rig");a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED;return reinterpret_cast<EnDodongo*>(a);}
Gfx* Native(int limb){return static_cast<StandardLimb*>(enemy->skelAnime.skeleton[limb-1])->dList;}
Gfx* Model(int limb){auto* dl=Native(limb);ARPG_OverrideDodongoLimb(gPlayState,&enemy->actor,limb,&dl);return dl;}
void Damage(int damage,int effect){enemy->colliderBody.base.acFlags|=AC_HIT;enemy->actor.colChkInfo.damage=damage;enemy->actor.colChkInfo.damageEffect=effect;EnDodongo_CollisionCheck(enemy,gPlayState);}
void Models(){Check(enemy->skelAnime.limbCount==31 && std::abs(enemy->actor.scale.x-.01875f)<.00001f && enemy->colliderBody.count==6,"Native scale, rig and six body collision spheres remain authoritative");
 std::set<Gfx*> lists;for(int l:DodongoGeometry::Limbs){Check(Model(l)!=Native(l),"Visible native limb uses Blender geometry");Check(Model(l)==Model(l),"Limb geometry is cached without per-frame growth");lists.insert(Model(l));}Check(lists.size()==12,"All twelve visible joints have separate display lists");
 for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"}){CVarSetInteger(key,0);for(int l:DodongoGeometry::Limbs)Check(Model(l)==Native(l),"Disabling the upgrade restores original limbs");CVarSetInteger(key,1);}
 auto eye=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;Check(Model(7)==Native(7),"Distant actors retain native lightweight geometry");GET_ACTIVE_CAM(gPlayState)->eye=eye;
 Gfx* dl=nullptr;ARPG_OverrideDodongoLimb(gPlayState,&enemy->actor,1,&dl);Check(!dl,"Dummy joints remain hidden");
 Check(std::hypot(enemy->mouthPos.x-enemy->actor.world.pos.x,enemy->mouthPos.z-enemy->actor.world.pos.z)>20,"Native mouth position follows its animated upper-head joint");
 Check(enemy->colliderHard.elements[0].dim.vtx[0].y!=enemy->colliderHard.elements[0].dim.vtx[1].y,"Native armored-head triangles update after mesh replacement");
 std::copy(std::begin(enemy->jointTable),std::end(enemy->jointTable),firstJoints.begin());firstPos=enemy->actor.world.pos;
}
void BombchuMemory(){
 // [TIME] | [INFO] | DodongoQA.Bombchu | A padded real-layout object detects the old cross-type fuse write without touching allocator metadata.
 struct Guarded { EnBomChu actor{};std::array<unsigned char,sizeof(EnBombf)> guard{}; } test;
 test.guard.fill(0xA5);test.actor.actor.id=ACTOR_EN_BOM_CHU;test.actor.timer=100;enemy->actor.parent=&test.actor.actor;EnDodongo_SetupSwallowBomb(enemy);
 EnDodongo_SwallowBomb(enemy,gPlayState);Check(test.actor.timer==101 && std::all_of(test.guard.begin(),test.guard.end(),[](auto x){return x==0xA5;}),"Swallowed Bombchu updates its own fuse and leaves trailing memory intact");
 Check(test.actor.actor.freezeTimer==2,"Captured Bombchu uses the native freeze gate to suspend crawling and collision");enemy->actor.parent=nullptr;EnDodongo_SetupIdle(enemy);
}
void Feed(){int id=bombType==0?ACTOR_EN_BOM:bombType==1?ACTOR_EN_BOMBF:ACTOR_EN_BOM_CHU;auto p=enemy->mouthPos;
 bomb=Actor_Spawn(&gPlayState->actorCtx,gPlayState,id,p.x,p.y,p.z,0,0,0,0);Check(bomb && bomb->update,"Native explosive available for swallow test");bomb->parent=nullptr;bomb->world.pos=p;
 Check(EnDodongo_AteBomb(enemy,gPlayState) && bomb->parent==&enemy->actor,"Native mouth detects and captures the explosive");EnDodongo_SetupSwallowBomb(enemy);Check(enemy->actionState==1,"Capture starts the native reverse-mouth swallow animation");
}
void Tick(){if(phase>=98)return;try{++frames;
 if(phase==0 && frames>60 && gGameState){CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_CHILD;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;gARPGSave={};
  for(auto k:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"})CVarSetInteger(k,1);
  for(auto k:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG"})CVarSetInteger(k,0);
  SeedRng(0xD0D060);Warp();phase=1;frames=0;return;}
 if(gPlayState && phase>=2 && phase<=10)Camera();
 if(phase==1 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()){
  Camera();for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)SPDLOG_INFO("[Dodongo QA] Existing actor {} at {},{},{}",a->id,a->world.pos.x,a->world.pos.y,a->world.pos.z);
  auto* practice=Creatures::SpawnDodongoPractice(gPlayState);Check(practice!=nullptr,"Practice placement succeeds alongside the original room population");Actor_Kill(practice);
  Fixture();enemy=reinterpret_cast<EnDodongo*>(Creatures::SpawnDodongoPractice(gPlayState));Check(enemy!=nullptr,"Production practice placement finds room for the full Dodongo footprint");enemy->actor.flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED;phase=2;frames=0;return;}
 if(phase==2 && frames>35){Models();BombchuMemory();EnDodongo_SetupWalk(enemy);phase=3;frames=0;return;}
 if(phase==3){moved|=std::hypot(enemy->actor.world.pos.x-firstPos.x,enemy->actor.world.pos.z-firstPos.z)>50;leftStep|=!enemy->rightFootStep;rightStep|=enemy->rightFootStep;
  if(frames>230){bool joints=false;for(int i=1;i<31;++i)joints|=std::memcmp(&enemy->jointTable[i],&firstJoints[i],sizeof(Vec3s))!=0;
   Check(moved && leftStep && rightStep && joints,"Native walk advances the body, alternates footsteps and articulates the rig");Check(fireFrames>=10,"Approaching the player naturally reaches active fire breath");
   int hp=enemy->actor.colChkInfo.health;enemy->colliderHard.base.acFlags|=AC_BOUNCED;Damage(1,0);Check(enemy->actor.colChkInfo.health==hp,"Armored head deflects a weapon hit before body damage");
   EnDodongo_SetupIdle(enemy);Damage(1,0);Check(enemy->actor.colChkInfo.health==hp-1 && enemy->actionState==0,"Exposed body damage triggers native tail retaliation");
   fixture=enemy->actor.world.pos;fixture.x-=Math_SinS(enemy->actor.shape.rot.y)*160;fixture.z-=Math_CosS(enemy->actor.shape.rot.y)*160;phase=4;frames=0;return;}}
 if(phase==4){tailHitbox|=(enemy->colliderBody.base.atFlags&AT_ON) && enemy->sphElements[1].info.toucher.damage==8;
  if(frames>80){Check(tailFrames>3 && tailHitbox && enemy->blureIdx>=0,"Tail retaliation retains its spinning motion, active damage and blur trail");EnDodongo_SetupIdle(enemy);Damage(0,1);Check(enemy->actionState==7 && enemy->actor.colorFilterTimer==80,"Utility stun enters the native eighty-tick recovery");phase=5;frames=0;return;}}
 if(phase==5 && frames>95){Check(enemy->actionState!=7 && enemy->actor.colChkInfo.health==3,"Nonlethal utility stun recovers without extra damage");EnDodongo_SetupIdle(enemy);Damage(1,15);Check(enemy->actionState==7 && enemy->iceTimer==36,"Ice damage freezes the original pose and starts native attached shards");phase=6;frames=0;return;}
 if(phase==6 && frames>95){Check(iceFrames>=30 && enemy->actor.colChkInfo.health==2,"Native freeze renders and returns to combat");EnDodongo_SetupIdle(enemy);Damage(8,0);Check(enemy->actionState==2 && kills==1,"Lethal body damage enters native death exactly once");phase=7;frames=0;return;}
 if(phase==7 && frames>130){Check(!Alive(reinterpret_cast<Actor*>(enemy)) && kills==1 && gARPGSave.kills==1,"Native death explosion completes with one defeat reward");fixture={-4,0,499};enemy=Spawn();phase=8;frames=0;return;}
 if(phase==8 && frames>30){Feed();phase=9;frames=0;return;}
 if(phase==9){if(Alive(reinterpret_cast<Actor*>(enemy)) && enemy->bodyScale.y>1.05f)++swellingFrames;
  if(bombType==2 && frames==3)Check(Alive(bomb) && bomb->freezeTimer>0 && std::hypot(bomb->world.pos.x-enemy->mouthPos.x,bomb->world.pos.z-enemy->mouthPos.z)<3,"Live captured Bombchu remains held at the animated mouth");
  if(frames==14)Check(!Alive(bomb) && !enemy->actor.parent && !enemy->actor.child,"Swallowed explosive is consumed and ownership pointers clear");
  if(frames>165){Check(!Alive(reinterpret_cast<Actor*>(enemy)) && kills==bombType+2 && gARPGSave.kills==uint32_t(bombType+2),"Swallowed explosive finishes native swelling and death with one reward");
   if(++bombType<3){enemy=Spawn();phase=8;frames=0;return;}Check(swellingFrames>0,"Authored torso and tail use the native bomb-induced scale pulse");Warp();phase=10;frames=0;return;}}
 if(phase==10 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()){Check(kills==4,"Scene teardown adds no duplicate defeat rewards");if(!visual){phase=99;Write("passed");return;}Fixture();enemy=Spawn();phase=11;frames=0;return;}
 if(phase==11){Camera();if(frames>20){gPlayState->frameAdvCtx.enabled=true;phase=98;Write("passed");}}
 if(frames>1100)throw std::runtime_error("Dodongo integration timeout");
 }catch(const std::exception& e){phase=99;Write("failed",e.what());}}
void Register(){auto* enabled=std::getenv("SOH_ARPG_DODONGO_QA");auto* home=std::getenv("SHIP_HOME");if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
 output=std::filesystem::path(home)/"dodongo-results.json";visual=std::getenv("SOH_ARPG_DODONGO_UI")!=nullptr;
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(phase>=2 && phase<=9 && enemy && Alive(&enemy->actor)){++drawFrames;if(enemy->actionState==3 && enemy->skelAnime.curFrame>30 && enemy->skelAnime.curFrame<43)++fireFrames;if(enemy->actionState==0 && enemy->timer>1)++tailFrames;if(enemy->iceTimer>0)++iceFrames;}});
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* actor){if(static_cast<Actor*>(actor)->id==ACTOR_EN_DODONGO)++kills;});
}
static RegisterShipInitFunc init(Register);
}
