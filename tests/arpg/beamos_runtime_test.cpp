#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/BeamosVisuals.h"
#include "soh/Enhancements/arpg/BeamosGeometry.h"
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
#include "overlays/actors/ovl_En_Vm/z_en_vm.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
void Sram_InitDebugSave(void);
void EnVm_SetupWait(EnVm*);void EnVm_CheckHealth(EnVm*,PlayState*);
s32 EnVm_OverrideLimbDraw(PlayState*,s32,Gfx**,Vec3f*,Vec3s*,void*);
void EnArrow_Shoot(EnArrow*,PlayState*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0,kills=0,beamFrames=0,groundFrames=0,deathFrames=0,bombs=0,arrowHits=0;
bool visual=false,interior=false;EnVm* enemy=nullptr;Vec3f fixture{},anchor{};std::set<s16> scans;std::set<std::array<s16,6>> poses;std::vector<double> nativeTimes,upgradedTimes;std::chrono::steady_clock::time_point lastDraw;std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
nlohmann::json Timing(std::vector<double> v){if(v.empty())return {};std::sort(v.begin(),v.end());return {{"samples",v.size()},{"median_ms",v[v.size()/2]},{"p95_ms",v[size_t(v.size()*.95)]}};}
void Write(const char* status,const std::string& error=""){std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"render_frames",drawFrames},{"beam_frames",beamFrames},{"ground_contact_frames",groundFrames},{"death_flight_frames",deathFrames},{"distinct_poses",poses.size()},{"scan_angles",scans.size()},{"native_defeats",kills},{"bombs",bombs},{"projectile_eye_stuns",arrowHits},{"four_actor_paced_draw_interval_native",Timing(nativeTimes)},{"four_actor_paced_draw_interval_upgraded",Timing(upgradedTimes)},{"checks",checks}}.dump(2);}
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);checks.push_back(label);SPDLOG_INFO("[Beamos QA] PASS {}",label);Write("running");}
bool Alive(const Actor* actor){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a==actor)return a->update!=nullptr;return false;}
void Next(int p){phase=p;frames=0;}
void Warp(){gSaveContext.entranceIndex=interior?ENTR_DEKU_TREE_ENTRANCE:ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);enemy=nullptr;}
void Camera(){auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;p->invincibilityTimer=20;p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};p->actor.shape.rot.y=Math_Vec3f_Yaw(&fixture,&anchor);Vec3f at={anchor.x,anchor.y+42,anchor.z},eye={anchor.x+100,anchor.y+95,anchor.z+170};if(phase==20||phase==21){eye={anchor.x+270,anchor.y+200,anchor.z+400};}Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);}
void Clear(){for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)Actor_Kill(a);enemy=nullptr;}
EnVm* Spawn(int large=0,float x=0,float z=0){auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_VM,anchor.x+x,anchor.y,anchor.z+z,0,0,0,(10<<8)|(large?0:1));Check(a&&a->update,"Original Beamos initializes");a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;enemy=reinterpret_cast<EnVm*>(a);return enemy;}
Gfx* Model(int limb,bool original=false){auto* dl=static_cast<StandardLimb*>(enemy->skelAnime.skeleton[limb-1])->dList;if(!original)ARPG_OverrideBeamosLimb(gPlayState,&enemy->actor,limb,&dl);return dl;}
void Models(){Check(enemy->skelAnime.limbCount==11&&enemy->skelAnime.jointTable==enemy->jointTable,"Original eleven-entry rigid rig retains embedded animation tables");bool replaced=true,cached=true;int textures=0,vertices=0;
 for(int l=1;l<=10;++l){auto* m=Model(l);if(!BeamosGeometry::Visible(l)){Check(!m,"Native invisible pivot remains invisible");continue;}replaced&=m&&m!=Model(l,true);cached&=m==Model(l);for(int i=0;i<50000;++i){int op=m[i].words.w0>>24;if(op==G_ENDDL)break;textures+=op==G_SETTIMG;vertices+=op==G_VTX;}}
 Check(replaced&&cached&&vertices>0,"All four original meshes use persistent authored rigid geometry");Check(textures>0,"Semantic stone and metal materials bind native scene textures");
 for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"}){CVarSetInteger(key,0);bool off=true;for(int l=1;l<=10;++l)off&=Model(l)==Model(l,true);Check(off,"Feature gate restores every original mesh");CVarSetInteger(key,1);}
 auto eye=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;Check(Model(5)==Model(5,true)&&Model(10)==Model(10,true),"Distance fallback restores original head and pedestal");GET_ACTIVE_CAM(gPlayState)->eye=eye;
 Vec3f p{};Vec3s rot{};Gfx* dl=nullptr;enemy->beamRot.x=100;enemy->headRotY=200;EnVm_OverrideLimbDraw(gPlayState,2,&dl,&p,&rot,enemy);Check(rot.x==100&&rot.y==200&&!dl,"Original head pitch and yaw remain authoritative");enemy->beamRot.x=enemy->headRotY=0;
 Check(enemy->beamSightRange==400&&enemy->colliderCylinder.dim.radius==25&&enemy->colliderCylinder.dim.height==70,"Practice supplies native sight range and unchanged body collision");
}
void EyeFixture(){enemy->colliderQuad2.base.acFlags|=AC_HIT;EnVm_CheckHealth(enemy,gPlayState);}
void Bomb(){auto p=enemy->actor.world.pos;p.x+=50;p.y+=12;auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_BOM,p.x,p.y,p.z,0,0,0,BOMB_BODY);Check(a&&a->update,"Real native bomb initializes beside the armored body");a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;Actor_SetScale(a,.01f);reinterpret_cast<EnBom*>(a)->timer=12;}
void Arrow(){Vec3f target{};for(auto p:enemy->colliderQuad2.dim.quad){target.x+=p.x*.25f;target.y+=p.y*.25f;target.z+=p.z*.25f;}Vec3f start=target;auto d=target;d.x-=enemy->beamPos1.x;d.y-=enemy->beamPos1.y;d.z-=enemy->beamPos1.z;float n=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);start.x+=d.x*380/n;start.y+=d.y*380/n;start.z+=d.z*380/n;auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_ARROW,start.x,start.y,start.z,Math_Vec3f_Pitch(&start,&target),Math_Vec3f_Yaw(&start,&target),0,ARROW_NORMAL);Check(a&&a->update,"Real native arrow initializes along the eye collision plane");a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;auto* p=GET_PLAYER(gPlayState);auto old=p->unk_A73;p->unk_A73=1;EnArrow_Shoot(reinterpret_cast<EnArrow*>(a),gPlayState);p->unk_A73=old;}
void Tick(){if(phase>=98)return;try{++frames;
 if(phase==0&&frames>60&&gGameState){CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_ADULT;gARPGSave={};for(auto k:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"})CVarSetInteger(k,1);for(auto k:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG","gEnhancements.RandomizedEnemies"})CVarSetInteger(k,0);SeedRng(0xBEA);Warp();Next(1);return;}
 if(gPlayState&&phase>=2&&phase<=24)Camera();
 if(phase==1&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){fixture=GET_PLAYER(gPlayState)->actor.world.pos;Actor* a=nullptr;for(int i=0;i<16&&!a;++i){GET_PLAYER(gPlayState)->actor.shape.rot.y=i*0x1000;a=Creatures::SpawnBeamosPractice(gPlayState,0);}Check(a&&a->params==BEAMOS_SMALL,"Practice places the native small scanner on safe ground");anchor=a->world.pos;Clear();fixture={anchor.x,anchor.y,anchor.z+600};Camera();Spawn();Models();Check(enemy->actor.scale.x==.01f&&enemy->actor.colChkInfo.health==1,"Small variant retains its original scale and one-bomb health");Next(2);return;}
 if(phase==2&&frames>80){Check(enemy->unk_21C==0&&enemy->beamScale.z==0&&scans.size()>60,"Out-of-range player leaves native scanning active without a beam");fixture.z=anchor.z+145;enemy->headRotY=0;Next(3);return;}
 if(phase==3&&enemy->unk_260>=3&&frames>25){Check(enemy->unk_21C==1&&enemy->timer<=300&&enemy->beamScale.z>20&&poses.size()>3,"Native detection completes its shutter warning and starts the original laser");Check(Math_Vec3f_DistXYZ(&enemy->beamPos1,&enemy->beamPos2)>15&&std::abs(enemy->beamPos1.y-(anchor.y+60))<.2,"Original head center and charge point follow the original scale and pivot");Next(4);return;}
 if(phase==4&&frames>12){Check(beamFrames>10&&groundFrames>0,"Beam tracks the player and clips to real scene terrain");bool quad=false;for(auto p:enemy->colliderQuad1.dim.quad)quad|=std::abs(p.x-anchor.x)>1||std::abs(p.z-anchor.z)>1;Check(quad,"Original attack quad follows the rendered beam");enemy->colliderQuad1.base.atFlags|=AT_HIT;fixture.z=anchor.z+600;Next(5);return;}
 if(phase==5&&frames>10){Check(enemy->unk_21C==0&&enemy->beamScale.z==0,"Beam contact retracts the laser and returns to scanning");EyeFixture();Check(enemy->unk_21C==2&&enemy->timer==180&&enemy->actor.colChkInfo.health==1&&enemy->beamScale.z==0,"Eye hit interrupts without damaging the armored body");Next(6);return;}
 if(phase==6&&frames==20){int t=enemy->timer;EyeFixture();Check(enemy->timer==t&&enemy->actor.colChkInfo.health==1&&kills==0,"Repeated eye hits do not extend stun or grant a defeat");enemy->colliderQuad2.base.acFlags&=~AC_HIT;}
 if(phase==6&&frames>220){Check(enemy->unk_21C==0&&enemy->actor.colChkInfo.health==1,"Native stun recovery reopens the shutters and resumes scanning");enemy->headRotY=0;Next(7);return;}
 if(phase==7&&frames==2){Arrow();}
 if(phase==7&&enemy->unk_21C==2){++arrowHits;Check(enemy->actor.colChkInfo.health==1,"A real arrow collides with the moving eye and produces a nonlethal stun");Bomb();Next(8);return;}
 if(phase==8&&enemy->unk_21C==3){Check(kills==1&&enemy->actor.colChkInfo.health==0&&enemy->actor.shape.yOffset==-5000&&enemy->actor.gravity==-.5f,"One real bomb defeats the small variant and launches its native head");auto* dl=Model(10,true);Vec3f p{};Vec3s rot{};EnVm_OverrideLimbDraw(gPlayState,10,&dl,&p,&rot,enemy);Check(!dl&&Model(5)!=Model(5,true),"Death hides the pedestal while retaining the upgraded flying head");Next(9);return;}
 if(phase==9&&frames>50){Check(!Alive(&enemy->actor)&&deathFrames>=20&&kills==1&&gARPGSave.kills==1,"Flying head explodes and grants one native and ARPG defeat");Spawn(1);Check(enemy->actor.scale.x==.014f&&enemy->actor.colChkInfo.health==2&&enemy->colliderCylinder.dim.radius==25,"Large variant retains its original scale, health and native body radius");Next(10);return;}
 if(phase==10&&frames>8){Bomb();Next(11);return;}
 if(phase==11&&enemy->unk_21C==2){Check(enemy->actor.colChkInfo.health==1&&kills==1,"First real bomb stuns the large Beamos without killing it");Next(12);return;}
 if(phase==12&&frames>30){Bomb();Next(13);return;}
 if(phase==13&&enemy->unk_21C==3){Check(kills==2&&enemy->actor.colChkInfo.health==0,"Second real bomb defeats the large Beamos during its stun window");Next(14);return;}
 if(phase==14&&frames>50){Check(!Alive(&enemy->actor)&&gARPGSave.kills==2,"Large head flight finishes with exactly one additional reward");Check(!Creatures::SpawnBeamosPractice(gPlayState,-1)&&!Creatures::SpawnBeamosPractice(gPlayState,2),"Unknown practice variants are rejected");auto* a=Creatures::SpawnBeamosPractice(gPlayState,1);Check(a&&a->params==BEAMOS_LARGE&&reinterpret_cast<EnVm*>(a)->beamSightRange==400,"Large practice keeps its size and nonzero scan range");Clear();for(int i=0;i<4;++i)Spawn(i%2,(i%2?1:-1)*90,(i/2?1:-1)*70);Check(!Creatures::SpawnBeamosPractice(gPlayState,0),"Practice obeys the four-Beamos nearby cap");fixture.z=anchor.z+900;CVarSetInteger("gEnhancements.ARPG.RemasteredEnemies",0);Next(20);return;}
 if(phase==20&&frames>160){CVarSetInteger("gEnhancements.ARPG.RemasteredEnemies",1);Next(21);return;}
 if(phase==21&&frames>160){Check(nativeTimes.size()>100&&upgradedTimes.size()>100,"Matched four-actor native and upgraded paced draw samples captured");interior=true;Warp();Next(25);return;}
 if(phase==25&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){Check(kills==2&&gARPGSave.kills==2,"Scene teardown clears live scanners without extra rewards");fixture={-4,0,499};anchor={-4,0,269};Clear();Camera();Spawn(1);Next(26);return;}
 if(phase==26){Camera();if(frames>70){Check(enemy->unk_21C==1&&enemy->unk_260>=3,"Large native scanner attacks in the darker dungeon room");if(visual){gPlayState->frameAdvCtx.enabled=true;phase=98;}else phase=99;Write("passed");return;}}
 if(frames>650)throw std::runtime_error("Beamos integration timeout");
 }catch(const std::exception& e){phase=99;if(gPlayState)gPlayState->frameAdvCtx.enabled=true;Write("failed",e.what());}}
void Register(){auto* enabled=std::getenv("SOH_ARPG_BEAMOS_QA");auto* home=std::getenv("SHIP_HOME");if(!enabled||std::string(enabled)!="1"||!home||!std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;output=std::filesystem::path(home)/"beamos-results.json";visual=std::getenv("SOH_ARPG_BEAMOS_UI")!=nullptr;
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* p){if(static_cast<Actor*>(p)->id==ACTOR_EN_VM)++kills;});
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorInit>([](void* p){if(static_cast<Actor*>(p)->id==ACTOR_EN_BOM)++bombs;});
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){auto now=std::chrono::steady_clock::now();if((phase==20||phase==21)&&frames>20&&frames<160)(phase==20?nativeTimes:upgradedTimes).push_back(std::chrono::duration<double,std::milli>(now-lastDraw).count());lastDraw=now;if(!gPlayState||!enemy||!Alive(&enemy->actor))return;++drawFrames;scans.insert(enemy->headRotY);poses.insert({enemy->jointTable[4].x,enemy->jointTable[4].y,enemy->jointTable[4].z,enemy->jointTable[7].x,enemy->jointTable[7].y,enemy->jointTable[7].z});beamFrames+=enemy->unk_260>=3;groundFrames+=enemy->unk_260==4;deathFrames+=enemy->unk_21C==3;});
}
static RegisterShipInitFunc init(Register);
}
