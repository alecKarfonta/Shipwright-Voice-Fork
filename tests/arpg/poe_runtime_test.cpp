#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/PoeVisuals.h"
#include "soh/Enhancements/arpg/PoeGeometry.h"
#include "soh/Enhancements/arpg/CreatureSystem.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
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
#include "overlays/actors/ovl_En_Poh/z_en_poh.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
void Sram_InitDebugSave(void);
void EnPoh_Attack(EnPoh*,PlayState*);void EnPoh_Idle(EnPoh*,PlayState*);void EnPoh_Disappear(EnPoh*,PlayState*);void EnPoh_Appear(EnPoh*,PlayState*);void EnPoh_Death(EnPoh*,PlayState*);
void func_80ADEECC(EnPoh*,PlayState*);void func_80ADF894(EnPoh*,PlayState*);void func_80ADF15C(EnPoh*,PlayState*);void func_80ADFE80(EnPoh*,PlayState*);void func_80ADF574(EnPoh*,PlayState*);
void EnPoh_SetupIdle(EnPoh*);void EnPoh_SetupDisappear(EnPoh*);
s32 EnPoh_OverrideLimbDraw(PlayState*,s32,Gfx**,Vec3f*,Vec3s*,void*,Gfx**);
void EnArrow_Shoot(EnArrow*,PlayState*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0,kills=0,attackFrames=0,fadeFrames=0,invisibleFrames=0,damageFrames=0,fleeFrames=0,burnFrames=0,fallFrames=0,soulFrames=0;
LightInfo* defeatedLight=nullptr;
bool visual=false,interior=false;EnPoh* enemy=nullptr;Vec3f fixture{},anchor{};
std::set<std::array<s16,6>> poses;std::vector<double> nativeTimes,upgradedTimes;std::chrono::steady_clock::time_point lastDraw;
std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
nlohmann::json Timing(std::vector<double> v){if(v.empty())return {};std::sort(v.begin(),v.end());return {{"samples",v.size()},{"median_ms",v[v.size()/2]},{"p95_ms",v[size_t(v.size()*.95)]}};}
bool Alive(const Actor* actor){if(!gPlayState||!actor)return false;for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a==actor)return a->update!=nullptr;return false;}
void Write(const char* status,const std::string& error=""){std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"frames",frames},{"render_frames",drawFrames},{"attack_frames",attackFrames},{"fade_frames",fadeFrames},{"invisible_frames",invisibleFrames},{"damage_frames",damageFrames},{"flee_frames",fleeFrames},{"burn_frames",burnFrames},{"fall_frames",fallFrames},{"soul_frames",soulFrames},{"distinct_poses",poses.size()},{"native_defeats",kills},{"arpg_defeats",gARPGSave.kills},{"four_actor_paced_draw_interval_native",Timing(nativeTimes)},{"four_actor_paced_draw_interval_upgraded",Timing(upgradedTimes)},{"checks",checks}}.dump(2);}
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);checks.push_back(label);SPDLOG_INFO("[Poe QA] PASS {}",label);Write("running");}
bool LightAttached(LightInfo* info){for(auto* n=gPlayState->lightCtx.listHead;n;n=n->next)if(n->info==info)return true;return false;}
void Next(int p){phase=p;frames=0;Write("running");}
void Warp(){gSaveContext.entranceIndex=interior?ENTR_DEKU_TREE_ENTRANCE:ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);enemy=nullptr;}
void Camera(){auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;p->invincibilityTimer=20;p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};
 Vec3f target=anchor;if(enemy&&Alive(&enemy->actor))target=enemy->actor.world.pos;
 p->actor.shape.rot.y=Math_Vec3f_Yaw(&fixture,&target)+(phase==3?0x8000:0);
 Vec3f at={target.x,target.y+35,target.z},eye={target.x+110,target.y+62,target.z+155};if(phase==20||phase==21){at={anchor.x,anchor.y+40,anchor.z};eye={anchor.x+280,anchor.y+150,anchor.z+370};}Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);}
void Clear(){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a->category==ACTORCAT_ENEMY||a->id==ACTOR_EN_POH||a->id==ACTOR_EN_ARROW)Actor_Kill(a);enemy=nullptr;}
EnPoh* Spawn(float x=0,float z=0,int params=0){auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_POH,anchor.x+x,anchor.y,anchor.z+z,0,0,0,params);Check(a&&a->update,"Original Poe initializes with its native object dependency");a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;enemy=reinterpret_cast<EnPoh*>(a);return enemy;}
Gfx* Model(int limb,bool original=false){auto* dl=static_cast<StandardLimb*>(enemy->skelAnime.skeleton[limb-1])->dList;if(!original){Vec3f pos{};Vec3s rot{};Gfx scratch[4];auto* p=scratch;EnPoh_OverrideLimbDraw(gPlayState,limb,&dl,&pos,&rot,enemy,&p);}return dl;}
uint32_t Env(Gfx* dl){for(int i=0;i<2048;++i){int op=dl[i].words.w0>>24;if(op==G_SETENVCOLOR)return uint32_t(dl[i].words.w1);if(op==G_ENDDL)break;}return 0;}
void Models(){Check(enemy->skelAnime.limbCount==21&&enemy->skelAnime.jointTable==enemy->jointTable,"Original twenty-one-entry skeleton retains embedded joint tables");bool replaced=true,hidden=true;int textures=0;
 for(int l=1;l<=20;++l){auto* m=Model(l);if(l==18||!PoeGeometry::Visible(l)){hidden&=!m;continue;}replaced&=m&&m!=Model(l,true);for(int i=0;i<2048;++i){int op=m[i].words.w0>>24;if(op==G_ENDDL)break;textures+=op==G_SETTIMG;}}
 Check(replaced&&hidden,"Eight body meshes follow native joints; lamp and invisible pivots remain suppressed in the skeleton");Check(textures>=15,"Authored material wrappers bind native painted textures");
 auto* lamp=ARPG_PoeLanternDisplayList(gPlayState,&enemy->actor,enemy->info->lanternDisplayList);Gfx expected[2];gSPDisplayList(expected,enemy->info->lanternDisplayList);Check(lamp!=enemy->info->lanternDisplayList&&lamp[0].words.w0==expected[0].words.w0&&lamp[0].words.w1==expected[0].words.w1,"Lantern wrapper draws the original luminous core before its fitted frame");
 for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"}){CVarSetInteger(key,0);Check(Model(5)==Model(5,true)&&ARPG_PoeLanternDisplayList(gPlayState,&enemy->actor,enemy->info->lanternDisplayList)==enemy->info->lanternDisplayList,"Feature gate restores original body and lamp");CVarSetInteger(key,1);}
 auto eye=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;Check(Model(5)==Model(5,true)&&!ARPG_UsePoeVisuals(gPlayState,&enemy->actor),"Distance fallback restores native art");GET_ACTIVE_CAM(gPlayState)->eye=eye;
 auto params=enemy->actor.params;enemy->actor.params=EN_POH_SHARP;Check(!ARPG_UsePoeVisuals(gPlayState,&enemy->actor),"Composer skeleton is excluded from the regular-Poe adapter");enemy->actor.params=EN_POH_FLAT;Check(!ARPG_UsePoeVisuals(gPlayState,&enemy->actor),"Flat's distinct native head remains excluded");enemy->actor.params=params;
 auto color=enemy->lightColor;enemy->lightColor={255,200,180,127};auto* faded=Model(5);auto alpha=Env(faded);enemy->lightColor={80,255,225,255};auto* solid=Model(5);Check(faded!=solid&&(alpha&255)==127&&(Env(solid)&255)==255&&Env(faded)==alpha,"Frame-owned wrappers retain independent actor tint and opacity");enemy->lightColor.a=0;bool null=true;for(int l:PoeGeometry::Limbs)null&=!Model(l);Check(null&&ARPG_PoeLanternDisplayList(gPlayState,&enemy->actor,enemy->info->lanternDisplayList)!=enemy->info->lanternDisplayList,"Invisible ghost hides all body geometry while the upgraded lamp stays visible");enemy->lightColor=color;
 Check(enemy->actor.scale.x==.01f&&enemy->colliderCyl.dim.radius==20&&enemy->colliderCyl.dim.height==40&&enemy->colliderSphItem.dim.worldSphere.radius==10,"Original scale, body cylinder and lantern attack sphere remain unchanged");
 auto& m=enemy->unk_368;auto& sphere=enemy->colliderSphItem.dim.worldSphere.center;Check(std::abs(m.xw+m.xy*1400-sphere.x)<2&&std::abs(m.yw+m.yy*1400-sphere.y)<2&&std::abs(m.zw+m.zy*1400-sphere.z)<2,"Original post-draw lantern matrix still positions the attack sphere");
}
void Arrow(){auto target=enemy->actor.world.pos;target.y+=38;auto start=target;start.x+=230;auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_ARROW,start.x,start.y,start.z,Math_Vec3f_Pitch(&start,&target),Math_Vec3f_Yaw(&start,&target),0,ARROW_NORMAL);Check(a&&a->update,"Real native arrow is launched at the body cylinder");a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;auto* p=GET_PLAYER(gPlayState);auto old=p->unk_A73;p->unk_A73=1;EnArrow_Shoot(reinterpret_cast<EnArrow*>(a),gPlayState);p->unk_A73=old;}
void Tick(){if(phase>=98)return;try{++frames;if(frames%60==0)Write("running");
 if(phase==0&&frames>60&&gGameState){CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_ADULT;gARPGSave={};for(auto k:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"})CVarSetInteger(k,1);for(auto k:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG","gEnhancements.RandomizedEnemies"})CVarSetInteger(k,0);SeedRng(0xB0E);Warp();Next(1);return;}
 if(gPlayState&&phase>=2&&phase<=24)Camera();
 if(phase==1&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){GET_PLAYER(gPlayState)->actor.world.pos.z+=1200;Actor* a=nullptr;for(int i=0;i<16&&!a;++i){GET_PLAYER(gPlayState)->actor.shape.rot.y=i*0x1000;a=Creatures::SpawnPoePractice(gPlayState);}Check(a&&a->id==ACTOR_EN_POH&&a->params==0,"Practice places the original roaming Poe on safe ground");enemy=reinterpret_cast<EnPoh*>(a);a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;anchor=a->world.pos;fixture={anchor.x+90,anchor.y,anchor.z};Next(2);return;}
 if(phase==2&&frames>60&&enemy->lightColor.a==255){Models();Check(enemy->actor.flags&ACTOR_FLAG_ATTENTION_ENABLED,"Native emergence enables targeting after the appearance animation");Next(3);return;}
 if(phase==3&&enemy->actionFunc==EnPoh_Attack&&enemy->unk_198==9){Check(enemy->actor.speedXZ==5&&enemy->skelAnime.playSpeed==2&&attackFrames>=20,"Native pursuit reaches the lantern windup, then accelerates the attack after three loops");enemy->colliderSph.base.atFlags|=AT_HIT;Next(4);return;}
 if(phase==4&&frames==2)Check(enemy->actionFunc==func_80ADF574&&enemy->actor.speedXZ<0,"Lantern contact flag enters native recoil rather than continuing to attack");
 if(phase==4&&frames>45){Check(enemy->actionFunc!=func_80ADF574&&kills==0,"Native contact recoil recovers without defeat credit");EnPoh_SetupIdle(enemy);enemy->visibilityTimer=1;Next(5);return;}
 if(phase==5&&enemy->actionFunc==EnPoh_Disappear){Check(enemy->lightColor.a<255,"Original visibility timer starts the native vanish turn");Next(6);return;}
 if(phase==6&&enemy->lightColor.a==0){Check(!Model(5)&&ARPG_PoeLanternDisplayList(gPlayState,&enemy->actor,enemy->info->lanternDisplayList)!=enemy->info->lanternDisplayList,"Natural fade reaches a hidden body with visible fitted lantern");Next(7);return;}
 if(phase==7&&enemy->lightColor.a==255&&frames>40){Check(invisibleFrames>50&&fadeFrames>=50,"Original invisible interval and both fade directions complete");enemy->visibilityTimer=900;Arrow();Next(8);return;}
 if(phase==8&&enemy->actionFunc==func_80ADEECC){Check(enemy->actor.colChkInfo.health==2&&!(enemy->colliderCyl.base.acFlags&AC_ON)&&kills==0,"A real arrow deals native damage and disables body AC during recoil");Next(9);return;}
 if(phase==9&&enemy->actionFunc==func_80ADF894){Check(enemy->actor.speedXZ==5&&(enemy->colliderCyl.base.acFlags&AC_ON),"Surviving damage enters original fast zigzag flight");Next(10);return;}
 if(phase==10&&frames>60&&enemy->actionFunc!=func_80ADF894){Check(fleeFrames>5&&damageFrames>5,"Native flight opens distance and returns to roaming");enemy->visibilityTimer=900;Arrow();Next(11);return;}
 if(phase==11&&kills==1){Check(gARPGSave.kills==1&&enemy->actionFunc==func_80ADF15C,"Second real arrow starts original burn death and grants exactly one ARPG defeat");defeatedLight=&enemy->lightInfo;Next(12);return;}
 if(phase==12&&enemy->actionFunc==EnPoh_Death){Check(burnFrames>=25&&enemy->actor.scale.x==.01f&&ARPG_PoeLanternDisplayList(gPlayState,&enemy->actor,enemy->info->lanternDisplayList)!=enemy->info->lanternDisplayList,"Original burn collapse transfers the upgraded lamp to the falling actor");Next(13);return;}
 if(phase==13&&enemy->actionFunc==func_80ADFE80){Check(fallFrames>0&&enemy->actor.textId==0x5005&&enemy->colliderCyl.dim.radius==13,"Lamp impact releases the original collectible soul and bottle prompt");auto* soul=enemy;std::vector<EnPoh*> added;for(int i=0;i<3;++i)added.push_back(Spawn(350+i*90,0));Check(!Creatures::SpawnPoePractice(gPlayState),"Surviving soul counts toward the nearby practice cap");for(auto* a:added)Actor_Kill(&a->actor);enemy=soul;fixture={anchor.x+500,anchor.y,anchor.z+500};Next(14);return;}
 if(phase==14&&frames>240){Check(!Alive(&enemy->actor)&&!LightAttached(defeatedLight)&&kills==1&&gARPGSave.kills==1&&soulFrames>100,"Uncollected soul expires, removes its light and cleans up without duplicate rewards");Clear();fixture={anchor.x+40,anchor.y,anchor.z+90};Spawn(-70,-60,1);Next(15);return;}
 if(phase==15&&frames>60&&enemy->lightColor.a==255){Check(enemy->actor.params==1&&ARPG_UsePoeVisuals(gPlayState,&enemy->actor)&&Model(5)!=Model(5,true),"Native rupee Poe uses the same authored rig and completes emergence");Clear();Next(16);return;}
 if(phase==16&&frames>5){fixture={anchor.x,anchor.y,anchor.z+120};for(int i=0;i<4;++i)Spawn((i%2?1:-1)*65,(i/2?1:-1)*65);Check(!Creatures::SpawnPoePractice(gPlayState),"Practice respects the four-Poe nearby cap");CVarSetInteger("gEnhancements.ARPG.RemasteredEnemies",0);Next(20);return;}
 if(phase==20&&frames>160){CVarSetInteger("gEnhancements.ARPG.RemasteredEnemies",1);Next(21);return;}
 if(phase==21&&frames>160){Check(nativeTimes.size()>100&&upgradedTimes.size()>100,"Matched four-Poe draw cadence samples captured");interior=true;Warp();Next(25);return;}
 if(phase==25&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){Check(kills==1&&gARPGSave.kills==1,"Scene teardown adds no defeat credit for living ghosts");fixture={96,0,399};anchor={-4,0,309};Clear();Camera();Spawn();Next(26);return;}
 if(phase==26){Camera();if(frames>100&&enemy->lightColor.a==255){Check(poses.size()>60&&ARPG_UsePoeVisuals(gPlayState,&enemy->actor),"Original articulation and upgraded model remain active in the darker dungeon");if(visual){gPlayState->frameAdvCtx.enabled=true;phase=98;}else phase=99;Write("passed");return;}}
 if(frames>750)throw std::runtime_error("Poe integration timeout");
 }catch(const std::exception& e){phase=99;if(gPlayState)gPlayState->frameAdvCtx.enabled=true;Write("failed",e.what());}}
void Register(){auto* enabled=std::getenv("SOH_ARPG_POE_QA");auto* home=std::getenv("SHIP_HOME");if(!enabled||std::string(enabled)!="1"||!home||!std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;output=std::filesystem::path(home)/"poe-results.json";visual=std::getenv("SOH_ARPG_POE_UI")!=nullptr;
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* p){if(static_cast<Actor*>(p)->id==ACTOR_EN_POH)++kills;});
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){auto now=std::chrono::steady_clock::now();if((phase==20||phase==21)&&frames>20&&frames<160)(phase==20?nativeTimes:upgradedTimes).push_back(std::chrono::duration<double,std::milli>(now-lastDraw).count());lastDraw=now;if(!gPlayState||!enemy||!Alive(&enemy->actor)||!enemy->skelAnime.skeleton)return;++drawFrames;poses.insert({enemy->jointTable[7].x,enemy->jointTable[7].y,enemy->jointTable[7].z,enemy->jointTable[14].x,enemy->jointTable[14].y,enemy->jointTable[14].z});attackFrames+=enemy->actionFunc==EnPoh_Attack;fadeFrames+=enemy->lightColor.a>0&&enemy->lightColor.a<255;invisibleFrames+=enemy->lightColor.a==0;damageFrames+=enemy->actionFunc==func_80ADEECC;fleeFrames+=enemy->actionFunc==func_80ADF894;burnFrames+=enemy->actionFunc==func_80ADF15C;fallFrames+=enemy->actionFunc==EnPoh_Death;soulFrames+=enemy->actionFunc==func_80ADFE80;});
}
static RegisterShipInitFunc init(Register);
}
