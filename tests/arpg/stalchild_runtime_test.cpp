#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/StalchildVisuals.h"
#include "soh/Enhancements/arpg/StalchildGeometry.h"
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
#include "objects/object_skb/object_skb.h"
#include "overlays/actors/ovl_En_Skb/z_en_skb.h"
#include "overlays/actors/ovl_En_Part/z_en_part.h"
void Sram_InitDebugSave(void);
void func_80AFD968(EnSkb*,PlayState*);void func_80AFD0A4(EnSkb*);void EnSkb_Advance(EnSkb*,PlayState*);void EnSkb_ReleaseDebris(EnSkb*);
s32 EnSkb_OverrideLimbDraw(PlayState*,s32,Gfx**,Vec3f*,Vec3s*,void*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0,kills=0,attackFrames=0,headlessFrames=0,debrisFrames=0,blueDrops=0,redDrops=0;bool visual=false,headDebris=false,jawDebris=false,night=true;
EnSkb* enemy=nullptr;Vec3f fixture{},anchor{};std::set<std::array<s16,9>> poses;std::set<s16> wander;Gfx* head=nullptr;Gfx* jaw=nullptr;ColliderInfo hit{};std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
void Write(const char* status,const std::string& error=""){std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"render_frames",drawFrames},{"attack_frames",attackFrames},{"headless_frames",headlessFrames},{"debris_frames",debrisFrames},{"distinct_poses",poses.size()},{"headless_yaw_values",wander.size()},{"native_defeats",kills},{"blue_drops",blueDrops},{"red_drops",redDrops},{"checks",checks}}.dump(2);}
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);checks.push_back(label);SPDLOG_INFO("[Stalchild QA] PASS {}",label);Write("running");}
bool Alive(const Actor* actor){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a==actor)return a->update!=nullptr;return false;}
void Next(int p){phase=p;frames=0;}
void Warp(){gSaveContext.entranceIndex=ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);enemy=nullptr;}
void Camera(){auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;gSaveContext.dayTime=gSaveContext.skyboxTime=night?0:0x8000;p->invincibilityTimer=20;p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};p->actor.shape.rot.y=Math_Vec3f_Yaw(&fixture,&anchor);Vec3f at={anchor.x,anchor.y+30,anchor.z},eye={anchor.x+110,anchor.y+85,anchor.z+170};Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);}
void Clear(){for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)Actor_Kill(a);enemy=nullptr;}
void Spawn(int params=0){auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_SKB,anchor.x,anchor.y,anchor.z,0,0,0,params);Check(a&&a->update,"Original Stalchild initializes");enemy=reinterpret_cast<EnSkb*>(a);a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;Check(enemy->actionState==0&&a->shape.yOffset==-8000&&!(a->flags&ACTOR_FLAG_ATTENTION_ENABLED),"Native underground emergence and untargetable opening remain intact");}
Gfx* Model(int limb,bool original=false){auto* dl=static_cast<StandardLimb*>(enemy->skelAnime.skeleton[limb-1])->dList;if(!original)ARPG_OverrideStalchildLimb(gPlayState,&enemy->actor,limb,&dl);return dl;}
void Models(){Check(enemy->skelAnime.limbCount==20,"Original twenty-entry skeleton is retained");bool replaced=true,cached=true,rigid=true;
 for(int l=1;l<=19;++l){auto* m=Model(l);if(!StalchildGeometry::Visible(l)){Check(!m,"Hidden native joint remains hidden");continue;}replaced&=m&&m!=Model(l,true);cached&=m==Model(l);for(int i=0;i<50000;++i){int op=m[i].words.w0>>24;if(op==G_ENDDL)break;if(op==G_MTX)rigid=false;}}
 Check(replaced&&cached,"All seventeen visible bones use persistent authored geometry");Check(rigid,"Detached anatomy needs no skeleton palette or frame allocation");head=Model(11);jaw=Model(12);
 for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"}){CVarSetInteger(key,0);bool off=true;for(int l=1;l<=19;++l)off&=Model(l)==Model(l,true);Check(off,"Feature gate restores every original Stalchild limb");CVarSetInteger(key,1);}
 auto eye=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;Check(Model(11)==Model(11,true)&&Model(15)==Model(15,true),"Distance fallback restores the whole original rig");GET_ACTIVE_CAM(gPlayState)->eye=eye;
}
void Damage(int amount,int effect){hit={};hit.toucher.dmgFlags=0x100;enemy->collider.elements[1].info.acHitInfo=&hit;enemy->collider.base.ac=&GET_PLAYER(gPlayState)->actor;enemy->collider.base.acFlags|=AC_HIT;enemy->actor.colChkInfo.damage=amount;enemy->actor.colChkInfo.damageEffect=effect;func_80AFD968(enemy,gPlayState);enemy->actor.colChkInfo.damageEffect=0;}
void Tick(){if(phase>=98)return;try{++frames;
 if(phase==0&&frames>60&&gGameState){CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_ADULT;gARPGSave={};for(auto k:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"})CVarSetInteger(k,1);for(auto k:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG","gEnhancements.RandomizedEnemies"})CVarSetInteger(k,0);SeedRng(0x5AB);Warp();Next(1);return;}
 if(gPlayState&&phase>=2&&phase<=22)Camera();
 if(phase==1&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){fixture=GET_PLAYER(gPlayState)->actor.world.pos;Actor* a=nullptr;for(int i=0;i<16&&!a;++i){GET_PLAYER(gPlayState)->actor.shape.rot.y=i*0x1000;a=Creatures::SpawnStalchildPractice(gPlayState,0);}Check(a&&a->params==0,"Practice finds safe ground for the original regular variant");anchor=a->world.pos;Clear();fixture={anchor.x,anchor.y,anchor.z+500};Camera();Spawn();Models();Check(enemy->actor.scale.x==.01f&&enemy->actor.colChkInfo.health==2&&enemy->collider.elements[0].dim.modelSphere.radius==10&&enemy->collider.elements[1].dim.modelSphere.radius==20,"Native health, regular scale and hand/body colliders are retained");Next(2);return;}
 if(phase==2&&frames>80){Check(enemy->actionState==4&&enemy->actor.shape.yOffset==0&&(enemy->actor.flags&ACTOR_FLAG_ATTENTION_ENABLED)&&poses.size()>20,"Emergence completes into the original articulated walk and targeting");fixture={enemy->actor.world.pos.x,enemy->actor.world.pos.y,enemy->actor.world.pos.z+40};Next(3);return;}
 if(phase==3&&enemy->setColliderAT){Check(enemy->actionState==3&&enemy->skelAnime.curFrame>=3&&enemy->skelAnime.curFrame<7,"Natural pursuit enters the original swipe and active hand window");enemy->collider.base.atFlags|=AT_BOUNCED;fixture.z+=350;Next(4);return;}
 if(phase==4&&frames==1){Check(enemy->actionState==5&&!enemy->setColliderAT&&enemy->skelAnime.playSpeed<0,"Shield bounce reverses the native swipe and disables its hitbox");}
 if(phase==4&&frames>35){Check(enemy->actionState==4,"Blocked swipe recovers into native pursuit");Damage(1,0xF);Check(enemy->actor.colChkInfo.health==1&&enemy->actionState==2,"Normal damage uses original nonlethal recoil");Next(5);return;}
 if(phase==5&&frames>45){Check(enemy->actionState==4,"Native recoil recovers");Damage(5,6);Check(enemy->actor.colChkInfo.health==1&&enemy->actionState==4,"Original ice-magic immunity is preserved");Damage(0,1);Check(enemy->actionState==6&&enemy->actor.colorFilterTimer==80&&!enemy->setColliderAT,"Nut or boomerang stun retains its eighty-frame window");Damage(1,1);Check(enemy->actor.colChkInfo.health==1,"Repeated stun cannot apply damage twice during the same freeze");Next(6);return;}
 if(phase==6&&frames>100){Check(enemy->actionState==4,"Stun expires into native movement");enemy->actor.colChkInfo.health=2;Damage(1,0xD);Check(enemy->breakFlags==1&&enemy->bodyBreak.val>0&&enemy->actor.colChkInfo.health==1,"Nonlethal spin reaction allocates the native two-part head break");Next(7);return;}
 if(phase==7&&frames>6){Check((enemy->breakFlags&2)&&headDebris&&jawDebris&&kills==0,"Skull and jaw detach using persistent upgraded geometry without a defeat reward");bool hidden=true;for(int l:{11,12}){auto* dl=Model(l,true);Vec3f p{};Vec3s r{};EnSkb_OverrideLimbDraw(gPlayState,l,&dl,&p,&r,enemy);hidden&=dl==nullptr;}Check(hidden,"Replacement callback respects native head and jaw hiding");Next(8);return;}
 if(phase==8&&frames>100){Check(enemy->actionState==4&&headlessFrames>60&&wander.size()>2,"Headless body retains its original wandering pursuit");Damage(2,0xF);Check(kills==1&&enemy->actionState==1,"Headless defeat grants exactly one native reward");Next(9);return;}
 if(phase==9&&frames>35){Check(!Alive(&enemy->actor)&&gARPGSave.kills==1&&debrisFrames>10,"Native body debris survives the parent with one ARPG defeat");fixture={anchor.x,anchor.y,anchor.z+500};Spawn(5);Check(enemy->actor.scale.x==.015f&&enemy->collider.elements[0].dim.modelSphere.radius==15&&enemy->collider.elements[1].dim.modelSphere.radius==30,"Large native variant scales its anatomy and original collider radii together");Next(10);return;}
 if(phase==10&&frames>55){Damage(0,7);Check(enemy->actionState==2&&enemy->actor.colorFilterTimer==25,"Fire reaction retains original attached flames and damage filter");Next(11);return;}
 if(phase==11&&frames>45){Damage(2,1);Check(enemy->actor.colChkInfo.health==0&&enemy->actionState==6&&kills==1,"Lethal stun defers defeat until the native freeze ends");Next(12);return;}
 if(phase==12&&frames>115){Check(!Alive(&enemy->actor)&&kills==2&&blueDrops>=1,"Large frozen defeat keeps its single reward and native blue-rupee drop");Spawn(10);Check(enemy->actor.scale.x==.02f&&enemy->collider.elements[0].dim.modelSphere.radius==20&&enemy->collider.elements[1].dim.modelSphere.radius==40,"Giant native variant uses doubled anatomy and original collider radii");Next(13);return;}
 if(phase==13&&frames>55){Damage(3,0xF);Next(14);return;}
 if(phase==14&&frames>35){Check(!Alive(&enemy->actor)&&kills==3&&redDrops>=3,"Giant defeat retains three red rupees and one defeat reward");Spawn();Next(15);return;}
 if(phase==15&&frames>45){night=false;Next(16);return;}
 if(phase==16&&frames>50){Check(!Alive(&enemy->actor)&&kills==3,"Daylight buries the original enemy without awarding a kill");night=true;fixture={anchor.x,anchor.y,anchor.z+950};Spawn();Next(17);return;}
 if(phase==17&&frames>100){Check(!Alive(&enemy->actor)&&kills==3,"Leaving the native home radius buries the enemy without a reward");fixture={anchor.x,anchor.y,anchor.z+500};Spawn();Next(18);return;}
 if(phase==18&&frames>45){enemy->actor.bgCheckFlags|=0x60;enemy->actor.yDistToWater=45;func_80AFD968(enemy,gPlayState);Check(enemy->actionState==1&&kills==4,"Native deep-water reaction begins a single defeat");Next(19);return;}
 if(phase==19&&frames>35){Check(!Alive(&enemy->actor)&&gARPGSave.kills==4,"Water defeat completes native body breakup and ARPG reward");Spawn();Next(20);return;}
 if(phase==20&&frames>45){Damage(0,0xD);Check(enemy->bodyBreak.val>0&&enemy->bodyBreak.matrices,"Pending head debris owns allocated native matrices");EnSkb_ReleaseDebris(enemy);EnSkb_ReleaseDebris(enemy);Check(!enemy->bodyBreak.matrices&&!enemy->bodyBreak.dLists&&!enemy->bodyBreak.objectIds&&enemy->bodyBreak.val==BODYBREAK_STATUS_FINISHED,"Interrupted debris cleanup is complete and idempotent");Actor_Kill(&enemy->actor);enemy=nullptr;Check(!Creatures::SpawnStalchildPractice(gPlayState,-1)&&!Creatures::SpawnStalchildPractice(gPlayState,3),"Invalid practice sizes are rejected");for(int i=0;i<6;++i)Spawn();Check(!Creatures::SpawnStalchildPractice(gPlayState,0),"Practice respects the six-Stalchild nearby cap");Clear();Spawn();Next(21);return;}
 if(phase==21&&frames>45){Damage(0,0xD);Check(enemy->bodyBreak.val>0,"Scene teardown fixture interrupts an active head break");Warp();Next(23);return;}
 if(phase==23&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){Check(kills==4&&gARPGSave.kills==4,"Scene teardown preserves exactly four earned defeats");if(!visual){phase=99;Write("passed");return;}fixture=GET_PLAYER(gPlayState)->actor.world.pos;anchor={fixture.x,fixture.y,fixture.z+200};fixture.x+=120;Clear();Camera();Spawn(5);Next(24);return;}
 if(phase==24){Camera();if(frames>45){gPlayState->frameAdvCtx.enabled=true;phase=98;Write("passed");}}
 if(frames>700)throw std::runtime_error("Stalchild integration timeout");
 }catch(const std::exception& e){phase=99;if(gPlayState)gPlayState->frameAdvCtx.enabled=true;Write("failed",e.what());}}
void Register(){auto* enabled=std::getenv("SOH_ARPG_STALCHILD_QA");auto* home=std::getenv("SHIP_HOME");if(!enabled||std::string(enabled)!="1"||!home||!std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;output=std::filesystem::path(home)/"stalchild-results.json";visual=std::getenv("SOH_ARPG_STALCHILD_UI")!=nullptr;
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* p){if(static_cast<Actor*>(p)->id==ACTOR_EN_SKB)++kills;});
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorInit>([](void* p){auto* a=static_cast<Actor*>(p);if(a->id==ACTOR_EN_ITEM00&&phase>=10&&phase<=14){blueDrops+=(a->params&0xff)==ITEM00_RUPEE_BLUE;redDrops+=(a->params&0xff)==ITEM00_RUPEE_RED;}});
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(!gPlayState)return;if(enemy&&Alive(&enemy->actor)){++drawFrames;std::array<s16,9> pose{};for(int j=0;j<3;++j){pose[j*3]=enemy->jointTable[11+j*2].x;pose[j*3+1]=enemy->jointTable[11+j*2].y;pose[j*3+2]=enemy->jointTable[11+j*2].z;}poses.insert(pose);attackFrames+=enemy->setColliderAT!=0;headlessFrames+=(enemy->breakFlags&2)!=0;if(enemy->breakFlags&2)wander.insert(enemy->headlessYawOffset);}
 for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;a;a=a->next)if(a->id==ACTOR_EN_PART&&a->update){auto* part=reinterpret_cast<EnPart*>(a);if(part->displayList==head)headDebris=true;if(part->displayList==jaw)jawDebris=true;++debrisFrames;}});
}
static RegisterShipInitFunc init(Register);
}
