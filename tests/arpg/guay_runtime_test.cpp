#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/GuayVisuals.h"
#include "soh/Enhancements/arpg/GuayGeometry.h"
#include "soh/Enhancements/arpg/CreatureSystem.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"
#include "soh/ObjectExtension/ActorMaximumHealth.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include <libultraship/bridge/consolevariablebridge.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <set>
#include <chrono>
#include <spdlog/spdlog.h>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "overlays/actors/ovl_En_Crow/z_en_crow.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
void Sram_InitDebugSave(void);
void EnCrow_FlyIdle(EnCrow*,PlayState*);void EnCrow_DiveAttack(EnCrow*,PlayState*);void EnCrow_Damaged(EnCrow*,PlayState*);void EnCrow_Die(EnCrow*,PlayState*);void EnCrow_Respawn(EnCrow*,PlayState*);void EnCrow_TurnAway(EnCrow*,PlayState*);
s32 EnCrow_OverrideLimbDraw(PlayState*,s32,Gfx**,Vec3f*,Vec3s*,void*);
void EnArrow_Shoot(EnArrow*,PlayState*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0,notifications=0,duplicates=0,diveFrames=0,contactFrames=0,scatterFrames=0,fallFrames=0,hiddenFrames=0,redRupees=0,redBeforeGiant=0,cycle=0,fireFrames=0,iceFrames=0,lastDeathEffect=0,fireStarts=0,iceStarts=0;
uint32_t rewardRng=0;bool visual=false,interior=false;EnCrow* enemy=nullptr;Vec3f fixture{},anchor{};
std::set<std::array<s16,6>> poses;std::vector<double> nativeTimes,upgradedTimes;std::chrono::steady_clock::time_point lastDraw;
std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
nlohmann::json Timing(std::vector<double> v){if(v.empty())return {};std::sort(v.begin(),v.end());return {{"samples",v.size()},{"median_ms",v[v.size()/2]},{"p95_ms",v[size_t(v.size()*.95)]}};}
bool Alive(const Actor* actor){if(!gPlayState||!actor)return false;for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a==actor)return a->update!=nullptr;return false;}
void Write(const char* status,const std::string& error=""){std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"frames",frames},{"cycle",cycle},{"render_frames",drawFrames},{"dive_frames",diveFrames},{"contact_frames",contactFrames},{"scatter_frames",scatterFrames},{"fall_frames",fallFrames},{"hidden_respawn_frames",hiddenFrames},{"fire_death_frames",fireFrames},{"ice_death_frames",iceFrames},{"native_fire_death_starts",fireStarts},{"native_ice_death_starts",iceStarts},{"distinct_poses",poses.size()},{"native_defeat_notifications",notifications},{"injected_duplicate_notifications",duplicates},{"earned_defeats",gARPGSave.kills},{"red_rupees",redRupees},{"four_actor_paced_draw_interval_native",Timing(nativeTimes)},{"four_actor_paced_draw_interval_upgraded",Timing(upgradedTimes)},{"checks",checks}}.dump(2);}
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);checks.push_back(label);SPDLOG_INFO("[Guay QA] PASS {}",label);Write("running");}
void Next(int p){phase=p;frames=0;Write("running");}
void Warp(){gSaveContext.entranceIndex=interior?ENTR_DEKU_TREE_ENTRANCE:ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);enemy=nullptr;}
void Camera(){auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;p->invincibilityTimer=0;p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};
 Vec3f target=anchor;if(enemy&&Alive(&enemy->actor))target=enemy->actor.world.pos;p->actor.shape.rot.y=Math_Vec3f_Yaw(&fixture,&target);
 Vec3f at={target.x,target.y+20,target.z},eye={target.x+90,target.y+65,target.z+115};if(phase==20||phase==21){at={anchor.x,anchor.y,anchor.z};eye={anchor.x+300,anchor.y+180,anchor.z+400};}Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);}
void Clear(){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a->category==ACTORCAT_ENEMY||a->id==ACTOR_EN_ARROW)Actor_Kill(a);enemy=nullptr;}
EnCrow* Spawn(float x=0,float z=0){auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_CROW,anchor.x+x,anchor.y,anchor.z+z,0,0,0,0);Check(a&&a->update,"Original Guay initializes");a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;enemy=reinterpret_cast<EnCrow*>(a);return enemy;}
Gfx* Model(int limb,bool original=false){auto* dl=static_cast<StandardLimb*>(enemy->skelAnime.skeleton[limb-1])->dList;if(!original)ARPG_OverrideGuayLimb(gPlayState,&enemy->actor,limb,&dl);return dl;}
void Models(){Check(enemy->skelAnime.limbCount==9&&enemy->skelAnime.jointTable==enemy->jointTable,"Original nine-entry flex rig retains embedded joint tables");bool replaced=true,cached=true;int matrices=0,textures=0;
 for(int l=1;l<=8;++l){auto* m=Model(l);if(!GuayGeometry::Visible(l)){Check(!m,"Native root pivot remains invisible");continue;}replaced&=m&&m!=Model(l,true);cached&=m==Model(l);for(int i=0;i<50000;++i){int op=m[i].words.w0>>24;if(op==G_ENDDL)break;matrices+=op==G_MTX;textures+=op==G_SETTIMG;}}
 Check(replaced&&cached,"All seven body, wing and tail meshes use stable cached geometry");Check(matrices>12&&textures>25,"Mixed-joint display lists bind original flex matrices and native painted textures");
 for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"}){CVarSetInteger(key,0);bool native=true;for(int l=1;l<=8;++l)native&=Model(l)==Model(l,true);Check(native,"Feature gate restores every original mesh");CVarSetInteger(key,1);}
 auto eye=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;Check(Model(2)==Model(2,true),"Camera-distance fallback restores native Guay art");GET_ACTIVE_CAM(gPlayState)->eye=eye;
 auto frame=enemy->skelAnime.curFrame;enemy->skelAnime.curFrame=2;Vec3f p{};Vec3s rot{};auto* dl=Model(7,true);EnCrow_OverrideLimbDraw(gPlayState,7,&dl,&p,&rot,enemy);Check(rot.y==0xC00&&dl==Model(7),"Native tail oscillation precedes the authored limb override");enemy->skelAnime.curFrame=frame;
 Check(enemy->actor.scale.x==.01f&&enemy->actor.shape.yOffset==2000&&enemy->colliderItems[0].dim.worldSphere.radius==20,"Original regular scale, flying offset and body collision are unchanged");
 bool finite=true;for(auto& p:enemy->bodyPartsPos)finite&=std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);Check(finite,"Original head, wing and tail effect anchors remain finite");
}
void Arrow(int type){lastDeathEffect=0;auto& s=enemy->colliderItems[0].dim.worldSphere.center;Vec3f target={float(s.x),float(s.y),float(s.z)},start=target;start.x+=230;auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_ARROW,start.x,start.y,start.z,Math_Vec3f_Pitch(&start,&target),Math_Vec3f_Yaw(&start,&target),0,type);Check(a&&a->update,"Real native arrow launches toward the flying collider");a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;auto* p=GET_PLAYER(gPlayState);auto old=p->unk_A73;p->unk_A73=1;EnArrow_Shoot(reinterpret_cast<EnArrow*>(a),gPlayState);p->unk_A73=old;}
void Nut(){auto p=enemy->actor.world.pos;auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_M_FIRE1,p.x,p.y-20,p.z,0,0,0,0);Check(a&&a->update,"Real Deku nut shockwave initializes at the flying bird");}
void Duplicate(){auto earned=gARPGSave.kills;auto rng=gARPGSave.rngState;GameInteractor_ExecuteOnEnemyDefeat(&enemy->actor);++duplicates;Check(gARPGSave.kills==earned&&gARPGSave.rngState==rng,"Duplicate defeat notification grants neither XP credit nor a second loot roll");}
void Tick(){if(phase>=98)return;try{++frames;if(frames%60==0)Write("running");
 if(phase==0&&frames>60&&gGameState){CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_ADULT;gARPGSave={};for(auto k:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies","gEnhancements.ARPG.Items"})CVarSetInteger(k,1);for(auto k:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG","gEnhancements.RandomizedEnemies"})CVarSetInteger(k,0);SeedRng(0x61A7);Warp();Next(1);return;}
 if(gPlayState&&phase>=2&&phase<=24)Camera();
 if(phase==1&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){GET_PLAYER(gPlayState)->actor.world.pos.z+=1200;Actor* a=nullptr;for(int i=0;i<16&&!a;++i){GET_PLAYER(gPlayState)->actor.shape.rot.y=i*0x1000;a=Creatures::SpawnGuayPractice(gPlayState);}Check(a&&a->id==ACTOR_EN_CROW&&a->params==0,"Practice places the original Guay in a checked open-air orbit");enemy=reinterpret_cast<EnCrow*>(a);a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;anchor=a->home.pos;fixture={anchor.x+80,anchor.y-100,anchor.z+80};Next(2);return;}
 if(phase==2&&frames>20){Models();EnemyDifficulty::ApplyHealthMultiplier(&enemy->actor,4);Check(enemy->actor.colChkInfo.health==4&&GetActorMaximumHealth(&enemy->actor)==4,"Native health multiplier creates a four-health survival fixture");Next(3);return;}
 if(phase==3&&enemy->actionFunc==EnCrow_DiveAttack){Check(enemy->actor.speedXZ==4&&enemy->skelAnime.playSpeed==2,"Original circling timer starts a faster flapping dive");Next(4);return;}
 if(phase==4&&frames>10&&enemy->actionFunc==EnCrow_FlyIdle){Check(diveFrames>10&&enemy->actor.colChkInfo.health==4,"Native dive recovers into its home orbit");Nut();Next(5);return;}
 if(phase==5&&enemy->actionFunc==EnCrow_TurnAway){Check(enemy->timer>80&&enemy->actor.colChkInfo.health==4&&gARPGSave.kills==0,"A real Deku nut drives the living bird away without damaging or rewarding it");Next(6);return;}
 if(phase==6&&enemy->actionFunc==EnCrow_FlyIdle){Check(scatterFrames>=90,"Original hundred-frame nut escape finishes normally");Arrow(ARROW_NORMAL);Next(7);return;}
 if(phase==7&&enemy->actor.colChkInfo.health==2){Check(enemy->actionFunc==EnCrow_TurnAway&&enemy->timer<=35&&(enemy->actor.flags&ACTOR_FLAG_ATTENTION_ENABLED)&&gARPGSave.kills==0,"A surviving real arrow hit scatters the scaled Guay without starting a fatal fall or granting rewards");Next(8);return;}
 if(phase==8&&enemy->actionFunc==EnCrow_FlyIdle){Check(enemy->actor.colChkInfo.health==2&&(enemy->collider.base.acFlags&AC_ON),"Damaged Guay returns to targetable flight with remaining health");rewardRng=gARPGSave.rngState;Arrow(ARROW_NORMAL);Next(9);return;}
 if(phase==9&&gARPGSave.kills==1){Check(enemy->actionFunc==EnCrow_Die&&enemy->actor.colChkInfo.health==0&&gARPGSave.rngState!=rewardRng,"Lethal real arrow completes the native falling death, one reward and a loot roll");cycle=1;Duplicate();Next(10);return;}
 if(phase==10&&enemy->actionFunc==EnCrow_Respawn){Check(!enemy->actor.draw&&enemy->actor.scale.x==0&&enemy->timer>=298&&enemy->actor.colChkInfo.health==0,"Native respawn begins hidden at zero scale and health with its full delay");Next(11);return;}
 if(phase==11&&enemy->actionFunc==EnCrow_FlyIdle){Check(frames>=300&&enemy->actor.draw&&enemy->actor.scale.x==.01f&&enemy->actor.colChkInfo.health==4&&gARPGSave.kills==1,"Completed native respawn restores the same scaled maximum and grants no reward by itself");rewardRng=gARPGSave.rngState;Arrow(ARROW_FIRE);Next(12);return;}
 if(phase==12&&gARPGSave.kills==uint32_t(cycle+1)){++cycle;Check(enemy->actor.colChkInfo.health==0&&gARPGSave.rngState!=rewardRng,"A real defeat after respawn grants one new reward and one new loot roll");Duplicate();Next(13);return;}
 if(phase==13&&enemy->actionFunc==EnCrow_Respawn){
  if(cycle==10)Check(enemy->actor.params==1&&enemy->colliderItems[0].dim.worldSphere.radius==s16(20.0f*.03f*100.0f),"Ten native ordinary defeats select the original giant Guay and larger collider");
  if(cycle==11)Check(redRupees>redBeforeGiant&&enemy->actor.params==0,"Native giant death drops its red rupee and schedules a regular Guay");
  enemy->timer=2;Next(14);return;}
 if(phase==14&&enemy->actionFunc==EnCrow_FlyIdle){Check(enemy->actor.colChkInfo.health==4&&GetActorMaximumHealth(&enemy->actor)==4,"Every completed respawn restores scaled health without stacking multipliers");
  if(cycle==10){Check(enemy->actor.scale.x==.03f&&enemy->actor.shape.yOffset==2000&&Model(2)!=Model(2,true),"Naturally grown giant uses the same original rig and authored meshes at threefold scale");redBeforeGiant=redRupees;}
  if(cycle==11){CVarSetInteger("gEnhancements.RandomizedEnemies",1);rewardRng=gARPGSave.rngState;Arrow(ARROW_FIRE);Next(15);return;}
  if(cycle==2){enemy->actor.colChkInfo.health=2;rewardRng=gARPGSave.rngState;Arrow(ARROW_ICE);}else{rewardRng=gARPGSave.rngState;Arrow(ARROW_FIRE);}Next(12);return;}
 if(phase==15&&gARPGSave.kills==12&&!Alive(&enemy->actor)){Check(fireFrames>10&&iceFrames>10&&fireStarts>=9&&iceStarts==1&&notifications-duplicates==12,"Fire and ice deaths retain native reactions; randomized-enemy mode removes the actor after its earned defeat");CVarSetInteger("gEnhancements.RandomizedEnemies",0);Clear();Next(16);return;}
 if(phase==16&&frames>5){auto* p=GET_PLAYER(gPlayState);p->actor.world.pos=fixture;Actor* a=nullptr;for(int i=0;i<16&&!a;++i){p->actor.shape.rot.y=i*0x1000;a=Creatures::SpawnGuayPractice(gPlayState,true);}Check(a&&a->params==1&&a->scale.x==.03f&&reinterpret_cast<EnCrow*>(a)->colliderItems[0].dim.worldSphere.radius==s16(20.0f*.03f*100.0f),"Giant practice preserves native scale and collision proportions");Clear();Next(17);return;}
 if(phase==17&&frames>5){for(int i=0;i<4;++i)Spawn((i%2?1:-1)*70,(i/2?1:-1)*70);Check(!Creatures::SpawnGuayPractice(gPlayState),"Practice observes the four-Guay cap");CVarSetInteger("gEnhancements.ARPG.RemasteredEnemies",0);Next(20);return;}
 if(phase==20&&frames>160){CVarSetInteger("gEnhancements.ARPG.RemasteredEnemies",1);Next(21);return;}
 if(phase==21&&frames>160){Check(nativeTimes.size()>100&&upgradedTimes.size()>100,"Four-bird native/upgraded paced draw samples captured");interior=true;Warp();Next(25);return;}
 if(phase==25&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){Check(gARPGSave.kills==12,"Scene teardown adds no rewards for living or hidden birds");fixture={96,0,399};anchor={-4,95,309};Clear();Camera();Spawn();Next(26);return;}
 if(phase==26){Camera();if(frames>60){Check(poses.size()>=8&&Model(2)!=Model(2,true),"Native flap articulation and authored meshes render in the darker dungeon");if(visual){gPlayState->frameAdvCtx.enabled=true;phase=98;}else phase=99;Write("passed");return;}}
 if(frames>900)throw std::runtime_error("Guay integration timeout");
 }catch(const std::exception& e){phase=99;if(gPlayState)gPlayState->frameAdvCtx.enabled=true;Write("failed",e.what());}}
void Register(){auto* enabled=std::getenv("SOH_ARPG_GUAY_QA");auto* home=std::getenv("SHIP_HOME");if(!enabled||std::string(enabled)!="1"||!home||!std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;output=std::filesystem::path(home)/"guay-results.json";visual=std::getenv("SOH_ARPG_GUAY_UI")!=nullptr;
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* p){if(static_cast<Actor*>(p)->id==ACTOR_EN_CROW)++notifications;});
 // [TIME] | [INFO] | GuayQA.Damage | Capture the native effect before Actor_UpdateAll clears damageEffect at the end of the update. Draw-time classification otherwise loses the original hit type.
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorUpdate>([](void* p){if(p!=enemy||!enemy||enemy->actionFunc!=EnCrow_Damaged)return;int effect=enemy->actor.colChkInfo.damageEffect;if(effect==2||effect==3){lastDeathEffect=effect;fireStarts+=effect==2;iceStarts+=effect==3;}});
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorInit>([](void* p){auto* a=static_cast<Actor*>(p);if(a->id==ACTOR_EN_ITEM00&&(a->params&0xFF)==ITEM00_RUPEE_RED)++redRupees;});
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){auto now=std::chrono::steady_clock::now();if((phase==20||phase==21)&&frames>20&&frames<160)(phase==20?nativeTimes:upgradedTimes).push_back(std::chrono::duration<double,std::milli>(now-lastDraw).count());lastDraw=now;if(!gPlayState||!enemy||!Alive(&enemy->actor))return;++drawFrames;poses.insert({enemy->jointTable[3].x,enemy->jointTable[3].y,enemy->jointTable[3].z,enemy->jointTable[4].x,enemy->jointTable[4].y,enemy->jointTable[4].z});diveFrames+=enemy->actionFunc==EnCrow_DiveAttack;contactFrames+=(enemy->collider.base.atFlags&AT_HIT)!=0;scatterFrames+=enemy->actionFunc==EnCrow_TurnAway;fallFrames+=enemy->actionFunc==EnCrow_Damaged;hiddenFrames+=enemy->actionFunc==EnCrow_Respawn&&!enemy->actor.draw;fireFrames+=enemy->actionFunc==EnCrow_Damaged&&lastDeathEffect==2;iceFrames+=enemy->actionFunc==EnCrow_Damaged&&lastDeathEffect==3;});
}
static RegisterShipInitFunc init(Register);
}
