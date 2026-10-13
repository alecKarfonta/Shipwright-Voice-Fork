#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/MadScrubVisuals.h"
#include "soh/Enhancements/arpg/MadScrubGeometry.h"
#include "soh/Enhancements/arpg/CreatureSystem.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"
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
#include "overlays/actors/ovl_En_Dekunuts/z_en_dekunuts.h"
#include "overlays/actors/ovl_En_Nutsball/z_en_nutsball.h"
#include "objects/object_dekunuts/object_dekunuts.h"
void Sram_InitDebugSave(void);
void EnDekunuts_SetupWait(EnDekunuts*);void EnDekunuts_SetupStand(EnDekunuts*);void EnDekunuts_SetupThrowNut(EnDekunuts*);void EnDekunuts_ColliderCheck(EnDekunuts*,PlayState*);
void EnDekunuts_Wait(EnDekunuts*,PlayState*);void EnDekunuts_Burrow(EnDekunuts*,PlayState*);void EnDekunuts_BeginRun(EnDekunuts*,PlayState*);void EnDekunuts_Run(EnDekunuts*,PlayState*);void EnDekunuts_Gasp(EnDekunuts*,PlayState*);void EnDekunuts_BeStunned(EnDekunuts*,PlayState*);void EnDekunuts_Die(EnDekunuts*,PlayState*);
s32 EnDekunuts_OverrideLimbDraw(PlayState*,s32,Gfx**,Vec3f*,Vec3s*,void*);
void func_80ABBBA8(EnNutsball*,PlayState*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0,kills=0,seeds=0,runFrames=0,gaspFrames=0,stunFrames=0,burrowFrames=0,deathFrames=0,baselineSeeds=0;
bool visual=false,interior=false;EnDekunuts* enemy=nullptr;EnNutsball* seed=nullptr;Actor* flower=nullptr;Vec3f fixture{},anchor{};std::set<std::array<s16,6>> poses;std::vector<double> nativeTimes,upgradedTimes;std::chrono::steady_clock::time_point lastDraw;std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
nlohmann::json Timing(std::vector<double> v){if(v.empty())return {};std::sort(v.begin(),v.end());return {{"samples",v.size()},{"median_ms",v[v.size()/2]},{"p95_ms",v[size_t(v.size()*.95)]}};}
bool Alive(const Actor* actor);
void Write(const char* status,const std::string& error=""){std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"actor_position",enemy&&gPlayState&&Alive(&enemy->actor)?nlohmann::json{enemy->actor.world.pos.x,enemy->actor.world.pos.y,enemy->actor.world.pos.z}:nlohmann::json{}},{"home",nlohmann::json{anchor.x,anchor.y,anchor.z}},{"render_frames",drawFrames},{"run_frames",runFrames},{"gasp_frames",gaspFrames},{"stun_frames",stunFrames},{"burrow_frames",burrowFrames},{"death_frames",deathFrames},{"distinct_poses",poses.size()},{"native_defeats",kills},{"seed_spawns",seeds},{"four_actor_plus_flowers_paced_draw_interval_native",Timing(nativeTimes)},{"four_actor_plus_flowers_paced_draw_interval_upgraded",Timing(upgradedTimes)},{"checks",checks}}.dump(2);}
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);checks.push_back(label);SPDLOG_INFO("[MadScrub QA] PASS {}",label);Write("running");}
bool Alive(const Actor* actor){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a==actor)return a->update!=nullptr;return false;}
void Next(int p){phase=p;frames=0;}
void Warp(){gSaveContext.entranceIndex=interior?ENTR_DEKU_TREE_ENTRANCE:ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);enemy=nullptr;flower=nullptr;seed=nullptr;}
void Camera(){auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;p->invincibilityTimer=20;p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};p->actor.shape.rot.y=Math_Vec3f_Yaw(&fixture,&anchor);Vec3f at={anchor.x,anchor.y+24,anchor.z},eye={anchor.x+80,anchor.y+72,anchor.z+145};if(phase==20||phase==21)eye={anchor.x+220,anchor.y+160,anchor.z+340};if(phase>=8&&phase<=11&&enemy&&Alive(&enemy->actor)){auto p=enemy->actor.world.pos;at={p.x,p.y+24,p.z};eye={p.x+80,p.y+72,p.z+145};}Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);}
void Clear(){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a->category==ACTORCAT_ENEMY||a->id==ACTOR_EN_DEKUNUTS||a->id==ACTOR_EN_NUTSBALL)Actor_Kill(a);enemy=nullptr;flower=nullptr;seed=nullptr;}
EnDekunuts* Spawn(int shots=1,float x=0,float z=0){auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_DEKUNUTS,anchor.x+x,anchor.y,anchor.z+z,0,0,0,shots<<8);Check(a&&a->update,"Original Mad Scrub initializes");a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;enemy=reinterpret_cast<EnDekunuts*>(a);flower=a->child;return enemy;}
Gfx* Model(int limb,bool original=false){auto* dl=limb?static_cast<StandardLimb*>(enemy->skelAnime.skeleton[limb-1])->dList:(Gfx*)gDekuNutsFlowerDL;if(!original)ARPG_OverrideMadScrubLimb(gPlayState,limb?&enemy->actor:flower,limb,&dl);return dl;}
void Models(){Check(enemy->skelAnime.limbCount==25&&enemy->skelAnime.jointTable==enemy->jointTable,"Original twenty-five-entry rig keeps embedded native animation tables");bool replaced=true,cached=true;int textures=0;
 for(int l=0;l<=24;++l){auto* m=Model(l);if(!MadScrubGeometry::Visible(l)){Check(!m,"Native invisible pivot remains invisible");continue;}replaced&=m&&m!=Model(l,true);cached&=m==Model(l);for(int i=0;i<50000;++i){int op=m[i].words.w0>>24;if(op==G_ENDDL)break;textures+=op==G_SETTIMG;}}
 Check(replaced&&cached,"All nine original meshes and the flower use cached authored geometry");Check(textures>0,"Bark and foliage bind semantic scene-lit textures");
 for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"}){CVarSetInteger(key,0);bool off=true;for(int l=0;l<=24;++l)off&=Model(l)==Model(l,true);Check(off,"Feature gate restores every original body and flower mesh");CVarSetInteger(key,1);}
 auto eye=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;Check(Model(4)==Model(4,true)&&Model(0)==Model(0,true),"Distance fallback restores original body and flower");GET_ACTIVE_CAM(gPlayState)->eye=eye;
 Check(enemy->actor.colChkInfo.health==1&&enemy->actor.scale.x==.01f&&enemy->collider.dim.radius==18&&enemy->collider.dim.height==5,"Original health, scale and burrow collision remain unchanged");Check(EnemyDifficulty::IsEnemyActor(&enemy->actor)&&!EnemyDifficulty::IsEnemyActor(flower),"Decorative flower is excluded from scaling, affixes, nameplates and defeat rewards");Check(flower&&flower->params==10&&!(flower->flags&ACTOR_FLAG_HOSTILE),"Native child flower is a separate nonhostile actor");
 auto old=enemy->actionFunc;auto frame=enemy->skelAnime.curFrame;EnDekunuts_SetupThrowNut(enemy);enemy->skelAnime.curFrame=12;Vec3f p{};Vec3s rot{};auto* dl=Model(7,true);EnDekunuts_OverrideLimbDraw(gPlayState,7,&dl,&p,&rot,enemy);Check(dl==Model(7),"Snout remains upgraded after the native squash callback early-return frame");enemy->actionFunc=old;EnDekunuts_SetupWait(enemy);enemy->skelAnime.curFrame=frame;
}
void Hit(int damage,int effect){static ColliderInfo info{};info.toucher.dmgFlags=2;enemy->collider.info.acHitInfo=&info;enemy->collider.base.ac=&GET_PLAYER(gPlayState)->actor;enemy->collider.base.acFlags|=AC_HIT;enemy->actor.colChkInfo.damage=damage;enemy->actor.colChkInfo.damageEffect=effect;EnDekunuts_ColliderCheck(enemy,gPlayState);}
void Tick(){if(phase>=98)return;try{++frames;
 if(phase==0&&frames>60&&gGameState){CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_ADULT;gARPGSave={};for(auto k:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"})CVarSetInteger(k,1);for(auto k:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG","gEnhancements.RandomizedEnemies"})CVarSetInteger(k,0);SeedRng(0xDEC);Warp();Next(1);return;}
 if(gPlayState&&phase>=2&&phase<=24)Camera();
 if(phase==1&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){GET_PLAYER(gPlayState)->actor.world.pos.z+=1200;fixture=GET_PLAYER(gPlayState)->actor.world.pos;Actor* a=nullptr;for(int i=0;i<16&&!a;++i){GET_PLAYER(gPlayState)->actor.shape.rot.y=i*0x1000;a=Creatures::SpawnMadScrubPractice(gPlayState,0);}Check(a&&reinterpret_cast<EnDekunuts*>(a)->shotsPerRound==1,"Practice places a native single-shot Scrub on safe ground");anchor=a->world.pos;Clear();fixture={anchor.x,anchor.y,anchor.z+240};Camera();Spawn();Models();baselineSeeds=seeds;Next(2);return;}
 if(phase==2&&seeds>baselineSeeds){Check(enemy->collider.dim.height==32&&enemy->actor.colChkInfo.mass==MASS_IMMOVABLE,"Native emergence restores rooted collision before spitting");Next(3);return;}
 if(phase==3&&frames==5){int live=0;for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_PROP].head;a;a=a->next)if(a->id==ACTOR_EN_NUTSBALL&&a->update&&a->draw)++live;Check(live>0,"Native seed survives object loading and flies in field practice");fixture.z=anchor.z+80;Next(4);return;}
 if(phase==4&&frames>85){Check(enemy->actionFunc==EnDekunuts_Wait&&enemy->collider.dim.height==5&&!(enemy->collider.base.acFlags&AC_ON),"Close approach causes native burrow and disables incoming damage");fixture.z=anchor.z+300;enemy->shotsPerRound=3;EnDekunuts_SetupThrowNut(enemy);baselineSeeds=seeds;Next(5);return;}
 if(phase==5&&frames>40){Check(seeds-baselineSeeds==3,"Native volley emits exactly three seeds without new attack timing");EnDekunuts_SetupStand(enemy);enemy->animFlagAndTimer=100;enemy->collider.base.acFlags|=AC_ON;enemy->collider.dim.height=32;
 auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_NUTSBALL,anchor.x,anchor.y+12,anchor.z+150,0,0,0,0);Check(a&&a->update,"Reflection fixture initializes a real native seed");seed=reinterpret_cast<EnNutsball*>(a);Next(6);return;}
 if(phase==6&&frames==3){Check(Alive(&seed->actor)&&seed->actor.draw,"Reflection seed finishes native object loading");auto* p=GET_PLAYER(gPlayState);auto shield=p->currentShield;auto matrix=p->shieldMf;p->currentShield=PLAYER_SHIELD_HYLIAN;p->shieldMf={};p->shieldMf.xx=p->shieldMf.yy=p->shieldMf.zz=p->shieldMf.ww=1;seed->collider.base.atFlags|=AT_HIT|AT_BOUNCED|AT_TYPE_ENEMY;func_80ABBBA8(seed,gPlayState);p->shieldMf=matrix;p->currentShield=shield;Check((seed->collider.base.atFlags&AT_TYPE_PLAYER)&&!(seed->collider.base.atFlags&AT_TYPE_ENEMY)&&seed->collider.info.toucher.dmgFlags==2&&seed->timer==30,"Native shield bounce reverses the seed and switches its damage allegiance");Next(7);return;}
 if(phase==7&&enemy->actionFunc==EnDekunuts_BeginRun){Check(enemy->actor.colChkInfo.mass==0x32&&enemy->actor.colChkInfo.health==1&&kills==0,"Returned seed naturally collides and flushes the rooted Scrub without a kill");fixture.z=anchor.z+450;Next(8);return;}
 if(phase==8&&frames>80&&enemy->actionFunc==EnDekunuts_Wait){Check(runFrames>30&&gaspFrames>20&&enemy->actor.colChkInfo.mass==MASS_IMMOVABLE&&Actor_WorldDistXZToPoint(&enemy->actor,&enemy->actor.home.pos)<1,"Native fleeing, gasping and home-seeking complete a full burrow cycle");Hit(1,0);Check(enemy->actionFunc==EnDekunuts_BeginRun,"A rooted hit begins the original unburrow reaction");Next(9);return;}
 if(phase==9&&enemy->actionFunc==EnDekunuts_Run){Hit(0,1);Check(enemy->actionFunc==EnDekunuts_BeStunned&&enemy->actor.colChkInfo.health==1&&enemy->animFlagAndTimer==5,"Deku-nut effect stuns the exposed runner without damage");Next(10);return;}
 if(phase==10&&frames==10){int t=enemy->animFlagAndTimer;Hit(0,1);Check(enemy->animFlagAndTimer==t&&enemy->actor.colChkInfo.health==1,"Repeated stun does not restart the native recovery timer");}
 if(phase==10&&frames>30&&enemy->actionFunc==EnDekunuts_Run){Check(stunFrames>40,"Native stun animation completes before running resumes");Hit(4,2);Check(enemy->actor.colChkInfo.health==0,"Fire damage uses the original exposed-body reaction");Next(11);return;}
 if(phase==11&&frames>40){Check(!Alive(&enemy->actor)&&Alive(flower)&&flower->category==ACTORCAT_PROP&&kills==1&&gARPGSave.kills==1&&deathFrames>5,"Native death grants one reward and leaves its flower prop behind");Check(!Creatures::SpawnMadScrubPractice(gPlayState,-1)&&!Creatures::SpawnMadScrubPractice(gPlayState,2),"Unknown practice variants are rejected");auto* a=Creatures::SpawnMadScrubPractice(gPlayState,1);Check(a&&reinterpret_cast<EnDekunuts*>(a)->shotsPerRound==3,"Volley practice preserves the high-byte native shot count");Clear();for(int i=0;i<4;++i)Spawn(i%2?3:1,(i%2?1:-1)*90,(i/2?1:-1)*70);Check(!Creatures::SpawnMadScrubPractice(gPlayState,0),"Practice obeys the four-live-Scrub nearby cap");fixture.z=anchor.z+340;CVarSetInteger("gEnhancements.ARPG.RemasteredEnemies",0);Next(20);return;}
 if(phase==20&&frames>160){CVarSetInteger("gEnhancements.ARPG.RemasteredEnemies",1);Next(21);return;}
 if(phase==21&&frames>160){Check(nativeTimes.size()>100&&upgradedTimes.size()>100,"Matched four-Scrub plus flower draw cadence samples captured");Clear();for(int i=0;i<8;++i){auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_DEKUNUTS,anchor.x+i*25,anchor.y,anchor.z,0,0,0,10);Check(a&&a->update,"Standalone burrow flower initializes");Actor_ChangeCategory(gPlayState,&gPlayState->actorCtx,a,ACTORCAT_PROP);}Check(!Creatures::SpawnMadScrubPractice(gPlayState,0),"Practice bounds surviving flower props after repeated victories");interior=true;Warp();Next(25);return;}
 if(phase==25&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){Check(kills==1&&gARPGSave.kills==1,"Scene teardown adds no defeats for flowers or live practice actors");fixture={-4,0,499};anchor={-4,0,269};Clear();Camera();Spawn(3);baselineSeeds=seeds;Next(26);return;}
 if(phase==26){Camera();if(frames>110){Check(seeds>baselineSeeds&&poses.size()>25,"Original volley Scrub animates and fires in the darker dungeon");if(visual){gPlayState->frameAdvCtx.enabled=true;phase=98;}else phase=99;Write("passed");return;}}
 if(frames>850)throw std::runtime_error("Mad Scrub integration timeout");
 }catch(const std::exception& e){phase=99;if(gPlayState)gPlayState->frameAdvCtx.enabled=true;Write("failed",e.what());}}
void Register(){auto* enabled=std::getenv("SOH_ARPG_MADSCRUB_QA");auto* home=std::getenv("SHIP_HOME");if(!enabled||std::string(enabled)!="1"||!home||!std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;output=std::filesystem::path(home)/"madscrub-results.json";visual=std::getenv("SOH_ARPG_MADSCRUB_UI")!=nullptr;
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* p){if(static_cast<Actor*>(p)->id==ACTOR_EN_DEKUNUTS)++kills;});
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorInit>([](void* p){if(static_cast<Actor*>(p)->id==ACTOR_EN_NUTSBALL)++seeds;});
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){auto now=std::chrono::steady_clock::now();if((phase==20||phase==21)&&frames>20&&frames<160)(phase==20?nativeTimes:upgradedTimes).push_back(std::chrono::duration<double,std::milli>(now-lastDraw).count());lastDraw=now;if(!gPlayState||!enemy||!Alive(&enemy->actor))return;++drawFrames;poses.insert({enemy->jointTable[3].x,enemy->jointTable[3].y,enemy->jointTable[3].z,enemy->jointTable[6].x,enemy->jointTable[6].y,enemy->jointTable[6].z});runFrames+=enemy->actionFunc==EnDekunuts_Run;gaspFrames+=enemy->actionFunc==EnDekunuts_Gasp;stunFrames+=enemy->actionFunc==EnDekunuts_BeStunned;burrowFrames+=enemy->actionFunc==EnDekunuts_Burrow;deathFrames+=enemy->actionFunc==EnDekunuts_Die;});
}
static RegisterShipInitFunc init(Register);
}
