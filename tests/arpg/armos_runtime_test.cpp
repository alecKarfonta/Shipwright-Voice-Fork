#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/ArmosVisuals.h"
#include "soh/Enhancements/arpg/ArmosGeometry.h"
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
#include "overlays/actors/ovl_En_Am/z_en_am.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
void Sram_InitDebugSave(void);
void EnAm_SetupSleep(EnAm*);void EnAm_SetupLunge(EnAm*);void EnAm_SetupCooldown(EnAm*);void EnAm_SetupRotateToHome(EnAm*);void EnAm_UpdateDamage(EnAm*,PlayState*);
void EnAm_Sleep(EnAm*,PlayState*);void EnAm_Statue(EnAm*,PlayState*);void EnAm_Lunge(EnAm*,PlayState*);void EnAm_Cooldown(EnAm*,PlayState*);void EnAm_Stunned(EnAm*,PlayState*);void EnAm_Ricochet(EnAm*,PlayState*);void EnAm_RecoilFromDamage(EnAm*,PlayState*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0,kills=0,hops=0,panicFrames=0,iceFrames=0,bombCount=0,statueBg=-1;bool visual=false,attacking=false,swordQuad=false,opaque=true;
EnAm* enemy=nullptr;EnAm* statue=nullptr;std::set<int> blends;std::set<Actor*> bombs;Vec3f fixture={-4,0,499},anchor={-4,0,269},statueStart{};
std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
void Write(const char* status,const std::string& error=""){std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"render_frames",drawFrames},{"native_hops",hops},{"awakening_blends",blends.size()},{"panic_frames",panicFrames},{"ice_frames",iceFrames},{"explosion_bombs",bombCount},{"native_defeats",kills},{"native_sword_quad",swordQuad},{"checks",checks}}.dump(2);}
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);checks.push_back(label);SPDLOG_INFO("[Armos QA] PASS {}",label);Write("running");}
bool Alive(const Actor* actor){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a==actor)return a->update!=nullptr;return false;}
void Warp(){gSaveContext.entranceIndex=ENTR_DEKU_TREE_ENTRANCE;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);enemy=statue=nullptr;}
void Camera(){auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=20;p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};p->actor.shape.rot.y=Math_Vec3f_Yaw(&fixture,&anchor);Vec3f at={anchor.x,anchor.y+38,anchor.z},eye={anchor.x+100,anchor.y+70,anchor.z+150};Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);}
void Fixture(){for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)Actor_Kill(a);Camera();}
EnAm* Spawn(bool passive=false){auto p=anchor;if(passive)p.x-=130;auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_AM,p.x,p.y,p.z,0,0,0,passive?0:1);Check(a&&a->update,"Original Armos actor initializes");a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;return reinterpret_cast<EnAm*>(a);}
Gfx* Model(EnAm* a,int limb,bool original=false){auto* dl=static_cast<StandardLimb*>(a->skelAnime.skeleton[limb-1])->dList;if(!original)ARPG_OverrideArmosLimb(gPlayState,&a->dyna.actor,limb,&dl);return dl;}
std::vector<uintptr_t> Geometry(Gfx* dl){std::vector<uintptr_t> addresses;for(int i=0;i<64;++i){int op=dl[i].words.w0>>24;if(op==G_ENDDL)break;if(op==G_DL)addresses.push_back(dl[i].words.w1);}return addresses;}
void Models(EnAm* a){Check(a->skelAnime.limbCount==14&&a->skelAnime.jointTable==a->jointTable&&a->skelAnime.morphTable==a->morphTable,"Original fourteen-joint rig retains embedded animation tables");bool replaced=true,cached=true;for(int limb:ArmosGeometry::Limbs){auto* first=Model(a,limb);replaced&=first&&first!=Model(a,limb,true);cached&=!Geometry(first).empty()&&Geometry(first)==Geometry(Model(a,limb));}Check(replaced,"All four original body/base/weapon limbs use the authored meshes");Check(cached,"Per-frame material wrappers reuse the same cached rigid geometry");
 int blend=a->textureBlend;bool colors=true;for(int value:{0,128,255}){a->textureBlend=value;auto* dl=Model(a,4);auto c=ArmosGeometry::Blend(0,value);colors&=(dl[1].words.w0>>24)==G_SETPRIMCOLOR&&dl[1].words.w1==((uint32_t(c.r)<<24)|(uint32_t(c.g)<<16)|(uint32_t(c.b)<<8)|255);}a->textureBlend=blend;Check(colors,"Native awakening selects continuous actor-local colors with fully opaque stone");
 for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"}){CVarSetInteger(key,0);bool off=true;for(int l:ArmosGeometry::Limbs)off&=Model(a,l)==Model(a,l,true);Check(off,"Feature gate restores original Armos geometry");CVarSetInteger(key,1);}
 auto eye=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;Check(Model(a,4)==Model(a,4,true),"Distance fallback restores the original meshes");GET_ACTIVE_CAM(gPlayState)->eye=eye;
 Check(Model(a,1)==nullptr&&Model(a,2)==nullptr,"Native root and hidden joints remain hidden");Check(a->dyna.actor.scale.x==.01f&&a->dyna.actor.gravity==-4&&a->hurtCollider.dim.radius==15&&a->hurtCollider.dim.height==70,"Original scale, gravity and collision dimensions remain intact");}
void Damage(int damage,int effect){enemy->hurtCollider.base.acFlags|=AC_HIT;enemy->dyna.actor.colChkInfo.damage=damage;enemy->dyna.actor.colChkInfo.damageEffect=effect;EnAm_UpdateDamage(enemy,gPlayState);enemy->dyna.actor.colChkInfo.damageEffect=0;}
void Tick(){if(phase>=98)return;try{++frames;
 if(phase==0&&frames>60&&gGameState){CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_CHILD;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;gARPGSave={};
  for(auto k:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"})CVarSetInteger(k,1);
  for(auto k:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG","gEnhancements.FasterBlockPush"})CVarSetInteger(k,0);
  SeedRng(0xA4A05);Warp();phase=1;frames=0;return;}
 if(gPlayState&&phase>=2&&phase<20)Camera();
 if(phase==1&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){Camera();auto* practice=Creatures::SpawnArmosPractice(gPlayState,0);Check(practice,"Guard practice finds safe ground beside original room enemies");Actor_Kill(practice);Fixture();enemy=Spawn();statue=Spawn(true);statueBg=statue->dyna.bgId;Models(enemy);Models(statue);Check(statue->dyna.actor.category==ACTORCAT_BG&&statue->actionFunc==EnAm_Statue&&!(statue->dyna.actor.flags&ACTOR_FLAG_ATTENTION_ENABLED),"Passive statue retains background category and ignores targeting");Check(DynaPoly_GetActor(&gPlayState->colCtx,statueBg)==&statue->dyna,"Passive statue retains native dynamic collision");phase=2;frames=0;return;}
 if(phase==2){if(frames==5){Check(enemy->actionFunc==EnAm_Sleep&&enemy->textureBlend==0,"Untouched guard stays in its native stone sleep");enemy->hurtCollider.base.acFlags|=AC_HIT;}blends.insert(enemy->textureBlend);bool hop=enemy->unk_264!=0;if(hop&&!attacking){++hops;Check(enemy->dyna.actor.speedXZ==6&&enemy->dyna.actor.velocity.y<=12,"Native hop preserves its launch speed and gravity");}attacking=hop;
  if(enemy->actionFunc==EnAm_Cooldown){Check(hops==3&&enemy->cooldownTimer>0&&enemy->textureBlend==255&&blends.size()>=8,"Native awakening completes and three attacking hops lead to cooldown");Check(swordQuad,"Original limb-one sword hitbox continues updating while the new meshes draw");Check(statue->textureBlend==0&&statue->dyna.actor.colChkInfo.health==1&&statue->actionFunc==EnAm_Statue,"Passive statue stays asleep through nearby combat");phase=3;frames=0;return;}}
 if(phase==3&&frames>50){Check(enemy->actionFunc!=EnAm_Cooldown,"Native recovery pause resumes the next hopping attack");enemy->attackTimer=0;phase=4;frames=0;return;}
 if(phase==4&&enemy->actionFunc==EnAm_Sleep&&enemy->textureBlend==0){Check(Math_Vec3f_DistXZ(&enemy->dyna.actor.world.pos,&enemy->dyna.actor.home.pos)<1&&enemy->dyna.actor.shape.rot.y==enemy->dyna.actor.home.rot.y,"Expired pursuit returns to its exact home pose and fades to stone");Check(!(enemy->dyna.actor.flags&ACTOR_FLAG_ATTENTION_ENABLED),"Sleeping guard releases targeting");enemy->hurtCollider.base.acFlags|=AC_HIT;phase=5;frames=0;return;}
 if(phase==5&&frames>20){Check(enemy->textureBlend==255&&enemy->behavior==10,"Native contact wakes a returned statue again");enemy->blockCollider.base.acFlags|=AC_BOUNCED|AC_HIT;EnAm_UpdateDamage(enemy,gPlayState);Check(enemy->actionFunc==EnAm_Ricochet&&!(enemy->hurtCollider.base.acFlags&AC_HIT)&&enemy->dyna.actor.colChkInfo.health==1,"Hard shell bounce clears hurt flags and starts native ricochet");phase=6;frames=0;return;}
 if(phase==6&&frames>15){Check(enemy->actionFunc==EnAm_Lunge,"Ricochet recovers into native hopping");Damage(2,0);Check(enemy->dyna.actor.colChkInfo.health==1&&enemy->actionFunc==EnAm_Ricochet,"Unstunned light weapons retain their native deflection rule");EnAm_SetupLunge(enemy);Damage(0,14);Check(enemy->dyna.actor.colChkInfo.health==1&&enemy->actionFunc==EnAm_Lunge,"Fire/light magic retains its native immunity");Damage(0,6);Check(enemy->actionFunc==EnAm_Stunned&&enemy->dyna.actor.colChkInfo.health==1&&enemy->dyna.actor.colorFilterTimer==100,"Boomerang/hookshot effect opens the native stun window");Damage(0,6);Check(enemy->dyna.actor.colChkInfo.health==1&&enemy->actionFunc==EnAm_Stunned,"Repeated stun does not kill the guard");phase=7;frames=0;return;}
 if(phase==7&&frames>18){Damage(1,0);Check(enemy->dyna.actor.colChkInfo.health==0&&enemy->actionFunc==EnAm_RecoilFromDamage&&kills==0,"Light weapon follow-up defeats a stunned Armos before its delayed explosion");phase=8;frames=0;return;}
 if(phase==8&&frames>140){Check(!Alive(&enemy->dyna.actor)&&kills==1&&gARPGSave.kills==1&&panicFrames>20&&bombCount>=1,"Native panic spin ends in one explosive defeat and reward");statueStart=statue->dyna.actor.world.pos;phase=9;frames=0;return;}
 if(phase==9){statue->dyna.unk_150=1;statue->dyna.unk_158=0x4000;if(frames>40){Check(Math_Vec3f_DistXZ(&statueStart,&statue->dyna.actor.world.pos)>3&&statue->actionFunc==EnAm_Statue,"Passive statue still moves through the original push solver");Actor_Kill(&statue->dyna.actor);enemy=Spawn();enemy->hurtCollider.base.acFlags|=AC_HIT;bombs.clear();phase=10;frames=0;return;}}
 if(phase==10&&frames>22){Check(DynaPoly_GetActor(&gPlayState->colCtx,statueBg)==nullptr,"Destroyed statue releases its dynamic collision slot");Damage(4,13);Check(enemy->actionFunc==EnAm_Stunned&&enemy->iceTimer==48&&enemy->dyna.actor.colChkInfo.health==0,"Ice preserves its native frozen defeat delay and attached shard timer");phase=11;frames=0;return;}
 if(phase==11&&frames>250){Check(!Alive(&enemy->dyna.actor)&&kills==2&&gARPGSave.kills==2&&iceFrames>=40&&bombCount>=2,"Frozen Armos releases its filter then explodes exactly once");enemy=Spawn();enemy->hurtCollider.base.acFlags|=AC_HIT;bombs.clear();phase=12;frames=0;return;}
 if(phase==12&&frames>22){Damage(0,1);Check(enemy->actionFunc==EnAm_Stunned&&enemy->dyna.actor.colChkInfo.health==1,"Deku nuts retain the native stun opening");enemy->dyna.actor.colorFilterTimer=0;phase=13;frames=0;return;}
 if(phase==13&&frames>3){Check(enemy->actionFunc==EnAm_Lunge&&enemy->dyna.actor.colChkInfo.health==1,"Nonlethal stun naturally returns to pursuit");Damage(2,15);Check(enemy->actionFunc==EnAm_RecoilFromDamage&&enemy->dyna.actor.colChkInfo.health==0,"Native heavy-weapon effect defeats an unstunned guard");phase=14;frames=0;return;}
 if(phase==14&&frames>140){Check(kills==3&&gARPGSave.kills==3&&bombCount>=3,"Heavy-weapon defeat grants one reward and one final blast");Check(!Creatures::SpawnArmosPractice(gPlayState,-1)&&!Creatures::SpawnArmosPractice(gPlayState,2),"Unknown practice variants are rejected");auto* a=Creatures::SpawnArmosPractice(gPlayState,1);Check(a&&a->category==ACTORCAT_BG,"Statue practice preserves its passive native actor type");Actor_Kill(a);phase=15;frames=0;return;}
 if(phase==15&&frames>15){std::vector<EnAm*> batch;for(int i=0;i<4;++i)batch.push_back(Spawn(i%2));Check(!Creatures::SpawnArmosPractice(gPlayState,0)&&!Creatures::SpawnArmosPractice(gPlayState,1),"Practice cap includes guards and passive statues across categories");for(auto* a:batch)Actor_Kill(&a->dyna.actor);phase=16;frames=0;return;}
 if(phase==16&&frames>15){Check(kills==3,"Interrupted removal never fabricates defeat rewards");Warp();phase=20;frames=0;return;}
 if(phase==20&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){Check(kills==3,"Scene teardown does not repeat Armos rewards");Fixture();if(!visual){phase=99;Write("passed");return;}enemy=Spawn();enemy->textureBlend=0;EnAm_SetupSleep(enemy);enemy->unk_258=0;phase=21;frames=0;return;}
 if(phase==21&&frames>4){Camera();gPlayState->frameAdvCtx.enabled=true;phase=98;Write("passed");}
 if(frames>1200)throw std::runtime_error("Armos integration timeout");
 }catch(const std::exception& e){phase=99;Write("failed",e.what());}}
void Register(){auto* enabled=std::getenv("SOH_ARPG_ARMOS_QA");auto* home=std::getenv("SHIP_HOME");if(!enabled||std::string(enabled)!="1"||!home||!std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;output=std::filesystem::path(home)/"armos-results.json";visual=std::getenv("SOH_ARPG_ARMOS_UI")!=nullptr;
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* p){if(static_cast<Actor*>(p)->id==ACTOR_EN_AM)++kills;});
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(!gPlayState)return;if(enemy&&Alive(&enemy->dyna.actor)){++drawFrames;if(enemy->deathTimer>0&&enemy->panicSpinRot>0)++panicFrames;if(enemy->iceTimer>0)++iceFrames;if(enemy->unk_264){auto* q=enemy->hitCollider.dim.quad;swordQuad|=std::abs(Math_Vec3f_DistXYZ(q,q+1)-std::sqrt(50.f*50+70.f*70))<.1&&std::abs(Math_Vec3f_DistXYZ(q,q+2)-40)<.1;}}
  for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_EXPLOSIVE].head;a;a=a->next)if(a->id==ACTOR_EN_BOM&&a->update&&!bombs.contains(a)){bombs.insert(a);++bombCount;}});
}
static RegisterShipInitFunc init(Register);
}
