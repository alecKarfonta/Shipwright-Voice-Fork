#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/AnubisVisuals.h"
#include "soh/Enhancements/arpg/AnubisGeometry.h"
#include "soh/Enhancements/arpg/CreatureSystem.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"
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
#include "overlays/actors/ovl_En_Anubice/z_en_anubice.h"
#include "overlays/actors/ovl_En_Anubice_Tag/z_en_anubice_tag.h"
#include "overlays/actors/ovl_En_Anubice_Fire/z_en_anubice_fire.h"
#include "overlays/actors/ovl_Bg_Hidan_Curtain/z_bg_hidan_curtain.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
void Sram_InitDebugSave(void);
void EnAnubice_Idle(EnAnubice*,PlayState*);void EnAnubice_ShootFireball(EnAnubice*,PlayState*);void EnAnubice_Die(EnAnubice*,PlayState*);void EnAnubice_FindFlameCircles(EnAnubice*,PlayState*);
s32 EnAnubice_OverrideLimbDraw(PlayState*,s32,Gfx**,Vec3f*,Vec3s*,void*);
void EnArrow_Shoot(EnArrow*,PlayState*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0,kills=0,fireballs=0,attackFrames=0,hoverFrames=0,deathFrames=0,knockbackFrames=0;
bool visual=false,interior=false,fireOrigin=true,fireExcluded=true;
EnAnubice* enemy=nullptr;EnAnubiceTag* tag=nullptr;BgHidanCurtain* circle=nullptr;Vec3f fixture{},anchor{};
std::set<std::array<s16,6>> poses;std::vector<double> nativeTimes,upgradedTimes;std::chrono::steady_clock::time_point lastDraw;
std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
nlohmann::json Timing(std::vector<double> v){if(v.empty())return {};std::sort(v.begin(),v.end());return {{"samples",v.size()},{"median_ms",v[v.size()/2]},{"p95_ms",v[size_t(v.size()*.95)]}};}
bool Alive(const Actor* actor){if(!gPlayState||!actor)return false;for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a==actor)return a->update!=nullptr;return false;}
void Write(const char* status,const std::string& error=""){std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"frames",frames},{"render_frames",drawFrames},{"attack_frames",attackFrames},{"hover_frames",hoverFrames},{"knockback_frames",knockbackFrames},{"death_frames",deathFrames},{"distinct_poses",poses.size()},{"native_defeats",kills},{"fireball_spawns",fireballs},{"four_actor_paced_draw_interval_native",Timing(nativeTimes)},{"four_actor_paced_draw_interval_upgraded",Timing(upgradedTimes)},{"checks",checks}}.dump(2);}
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);checks.push_back(label);SPDLOG_INFO("[Anubis QA] PASS {}",label);Write("running");}
void Next(int p){phase=p;frames=0;}
void Warp(){gSaveContext.entranceIndex=interior?ENTR_DEKU_TREE_ENTRANCE:ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);enemy=nullptr;tag=nullptr;circle=nullptr;}
void Camera(){auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;p->invincibilityTimer=20;p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};p->meleeWeaponState=phase==4&&frames<4?1:0;p->actor.shape.rot.y=Math_Vec3f_Yaw(&fixture,&anchor);
 Vec3f target=anchor;if(enemy&&Alive(&enemy->actor))target=enemy->actor.world.pos;
 Vec3f at={target.x,target.y+50,target.z},eye={target.x+120,target.y+78,target.z+165};if(phase==20||phase==21){at={anchor.x,anchor.y+45,anchor.z};eye={anchor.x+350,anchor.y+220,anchor.z+490};}Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);}
void Clear(){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a->category==ACTORCAT_ENEMY||a->id==ACTOR_EN_ANUBICE||a->id==ACTOR_EN_ANUBICE_TAG||a->id==ACTOR_BG_HIDAN_CURTAIN||a->id==ACTOR_EN_ARROW)Actor_Kill(a);enemy=nullptr;tag=nullptr;circle=nullptr;}
void Spawn(float x=0,float z=0){auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_ANUBICE_TAG,anchor.x+x,anchor.y,anchor.z+z,0,0,0,4);Check(a&&a->update,"Original mirroring controller initializes");tag=reinterpret_cast<EnAnubiceTag*>(a);enemy=nullptr;}
void Body(){if(tag&&Alive(&tag->actor)&&tag->anubis&&Alive(&tag->anubis->actor)){enemy=tag->anubis;enemy->actor.flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;}}
Gfx* Model(int limb,bool original=false){auto* dl=static_cast<StandardLimb*>(enemy->skelAnime.skeleton[limb-1])->dList;if(!original)ARPG_OverrideAnubisLimb(gPlayState,&enemy->actor,limb,&dl);return dl;}
void Models(){Check(enemy->skelAnime.limbCount==16&&enemy->skelAnime.jointTable==enemy->jointTable,"Original sixteen-entry rig keeps embedded animation tables");bool replaced=true,cached=true;int textures=0;
 for(int l=1;l<=15;++l){auto* m=Model(l);if(!AnubisGeometry::Visible(l)){Check(!m,"Native invisible pivot remains invisible");continue;}replaced&=m&&m!=Model(l,true);cached&=m==Model(l);for(int i=0;i<50000;++i){int op=m[i].words.w0>>24;if(op==G_ENDDL)break;textures+=op==G_SETTIMG;}}
 Check(replaced&&cached,"Eight original meshes use cached authored geometry");Check(textures>=24,"Every semantic material part binds a scene-lit texture");
 for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"}){CVarSetInteger(key,0);bool off=true;for(int l=1;l<=15;++l)off&=Model(l)==Model(l,true);Check(off,"Feature gate restores every original mesh");CVarSetInteger(key,1);}
 auto eye=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;Check(Model(13)==Model(13,true),"Distance fallback restores original anatomy");GET_ACTIVE_CAM(gPlayState)->eye=eye;
 Check(enemy->actor.scale.x==.015f&&enemy->collider.dim.radius==29&&enemy->collider.dim.height==103,"Original scale and full-height body collision remain unchanged");Check(enemy->actor.parent==&tag->actor&&tag->actor.child==&enemy->actor&&enemy->actor.params==0,"Practice uses the original zero-parameter owned child");
 Vec3f p{};Vec3s rot{};auto* dl=Model(13,true);auto old=enemy->headRot;enemy->headRot=900;EnAnubice_OverrideLimbDraw(gPlayState,13,&dl,&p,&rot,enemy);Check(rot.z==900&&dl==Model(13),"Head override preserves the native rotational callback");enemy->headRot=old;
 Check(enemy->fireballPos.y>enemy->actor.world.pos.y+45&&enemy->fireballPos.y<enemy->actor.world.pos.y+90,"Original post-draw head anchor follows the floating body");
}
void Arrow(int type){auto target=enemy->actor.world.pos;target.y+=45;auto start=target;start.x+=230;auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_ARROW,start.x,start.y,start.z,Math_Vec3f_Pitch(&start,&target),Math_Vec3f_Yaw(&start,&target),0,type);Check(a&&a->update,"Real native arrow initializes toward the body cylinder");a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;auto* p=GET_PLAYER(gPlayState);auto old=p->unk_A73;p->unk_A73=1;EnArrow_Shoot(reinterpret_cast<EnArrow*>(a),gPlayState);p->unk_A73=old;}
void Circle(float x,float z){auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_BG_HIDAN_CURTAIN,x,anchor.y,z,0,0,0,(4<<12)|55);Check(a&&a->update,"Original switched flame circle initializes");circle=reinterpret_cast<BgHidanCurtain*>(a);}
void Tick(){if(phase>=98)return;try{++frames;
 if(phase==0&&frames>60&&gGameState){CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_ADULT;gARPGSave={};for(auto k:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"})CVarSetInteger(k,1);for(auto k:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG","gEnhancements.RandomizedEnemies"})CVarSetInteger(k,0);SeedRng(0xA11);Warp();Next(1);return;}
 if(gPlayState&&phase>=2&&phase<=24){Body();Camera();}
 if(phase==1&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){GET_PLAYER(gPlayState)->actor.world.pos.z+=1200;Actor* a=nullptr;for(int i=0;i<16&&!a;++i){GET_PLAYER(gPlayState)->actor.shape.rot.y=i*0x1000;a=Creatures::SpawnAnubisPractice(gPlayState);}Check(a&&a->id==ACTOR_EN_ANUBICE_TAG&&a->params==4,"Practice places a native mirror anchor on safe ground");anchor=a->world.pos;tag=reinterpret_cast<EnAnubiceTag*>(a);fixture={anchor.x+110,anchor.y,anchor.z+100};Next(2);return;}
 if(phase==2&&frames>60&&enemy&&enemy->actor.shape.yOffset==0){Models();Check(enemy->isMirroringLink&&(enemy->actor.flags&ACTOR_FLAG_ATTENTION_ENABLED),"Native emergence makes the hovering mummy targetable");fixture={anchor.x-100,anchor.y,anchor.z+90};Next(3);return;}
 if(phase==3&&frames>55){auto p=enemy->actor.world.pos;Check(std::abs(p.x-(2*anchor.x-fixture.x))<2&&std::abs(p.z-(2*anchor.z-fixture.z))<2,"Original controller mirrors Link across its anchor in both axes");Next(4);return;}
 if(phase==4&&fireballs>0){Check(enemy->actionFunc==EnAnubice_ShootFireball&&fireOrigin&&fireExcluded,"A melee windup triggers the original frame-twelve fireball at the native head anchor; projectile is excluded from enemy rewards");Next(5);return;}
 if(phase==5&&frames>70&&enemy->actionFunc==EnAnubice_Idle){Check(fireballs==1&&kills==0&&gARPGSave.kills==0,"One sword swing produces one fireball and returns to hover without rewards");Arrow(ARROW_NORMAL);Next(6);return;}
 if(phase==6&&enemy->isKnockedback){Check(enemy->knockbackTimer<=10&&enemy->knockbackTimer>0&&kills==0,"A real ordinary arrow causes the native nonlethal spinning knockback");Next(7);return;}
 if(phase==7&&frames>30){Check(!enemy->isKnockedback&&enemy->actor.velocity.x==0&&enemy->actor.velocity.z==0&&kills==0,"Knockback recovers without bypassing the fire weakness");fixture={anchor.x+450,anchor.y,anchor.z+400};Next(8);return;}
 if(phase==8&&frames>100&&enemy->actor.shape.yOffset==-4230){Check(!enemy->isMirroringLink&&enemy->actionFunc==EnAnubice_FindFlameCircles,"Leaving range completes native retreat and hidden idle");fixture={anchor.x+100,anchor.y,anchor.z+90};Next(9);return;}
 if(phase==9&&frames>65&&enemy->actor.shape.yOffset==0){Check(enemy->isMirroringLink,"Returning to range raises the original mummy again");Arrow(ARROW_FIRE);Next(10);return;}
 if(phase==10&&kills==1){Check(enemy->actor.category==ACTORCAT_PROP&&EnemyDifficulty::IsEnemyActor(&enemy->actor)&&gARPGSave.kills==1,"A real fire arrow starts native burning death and awards one ARPG defeat after the category change");Next(11);return;}
 if(phase==11&&frames==12)Check(Alive(&enemy->actor)&&!Alive(&tag->actor)&&enemy->actor.parent==nullptr,"Dying controller detaches but preserves the burning body's death animation");
 if(phase==11&&frames>70){Check(!Alive(&enemy->actor)&&kills==1&&gARPGSave.kills==1&&deathFrames>10,"Original burning collapse and collectible drop finish without a second reward");Spawn();Next(12);return;}
 if(phase==12&&frames==5){Check(enemy&&Alive(&enemy->actor),"Cleanup fixture owns a native body");Actor_Kill(&enemy->actor);}
 if(phase==12&&frames>15){Check(!Alive(&tag->actor)&&kills==1,"Removing the zero-parameter body clears its controller safely without defeat credit");Spawn();Next(13);return;}
 if(phase==13&&frames==5){Check(enemy&&Alive(&enemy->actor),"Reverse cleanup fixture owns a native body");Actor_Kill(&tag->actor);}
 if(phase==13&&frames>15){Check(!Alive(&enemy->actor)&&kills==1,"Removing a live controller removes its owned body without defeat credit");Circle(anchor.x+500,anchor.z+500);Spawn();Next(14);return;}
 if(phase==14&&frames>65&&enemy&&enemy->actor.shape.yOffset==0){bool found=false;for(auto* c:enemy->flameCircles)found|=c==circle;Check(found,"Native activation discovers a real scene flame circle");Actor_Kill(&circle->actor);Next(15);return;}
 if(phase==15&&frames>8){bool cleared=true;for(auto* c:enemy->flameCircles)cleared&=c!=circle;Check(cleared&&Alive(&enemy->actor),"Removed flame circles are cleared before cached pointers are read");Clear();Flags_UnsetSwitch(gPlayState,55);Circle(anchor.x-100,anchor.z-90);Spawn();Next(16);return;}
 if(phase==16&&frames>65&&enemy&&enemy->actor.shape.yOffset==0){Check(kills==1&&circle->timer==0,"Inactive native fire-circle timer does not defeat the mummy");Flags_SetSwitch(gPlayState,55);Next(17);return;}
 if(phase==17&&kills==2){Check(gARPGSave.kills==2&&enemy->actor.category==ACTORCAT_PROP,"A switched native flame circle exploits the original fire weakness and grants one reward");Next(18);return;}
 if(phase==18&&frames>70){Check(!Alive(&enemy->actor)&&kills==2,"Flame-circle death completes normally");Clear();fixture={anchor.x,anchor.y,anchor.z+180};for(int i=0;i<4;++i)Spawn((i%2?1:-1)*80,(i/2?1:-1)*60);Check(!Creatures::SpawnAnubisPractice(gPlayState),"Practice obeys the four-controller nearby cap before children spawn");CVarSetInteger("gEnhancements.ARPG.RemasteredEnemies",0);Next(20);return;}
 if(phase==20&&frames>160){CVarSetInteger("gEnhancements.ARPG.RemasteredEnemies",1);Next(21);return;}
 if(phase==21&&frames>160){Check(nativeTimes.size()>100&&upgradedTimes.size()>100,"Matched four-mummy draw cadence samples captured");interior=true;Warp();Next(25);return;}
 if(phase==25&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){Check(kills==2&&gARPGSave.kills==2,"Scene teardown adds no defeats for live controllers or projectiles");fixture={96,0,399};anchor={-4,0,309};Clear();Camera();Spawn();Next(26);return;}
 if(phase==26){Body();Camera();if(frames>120&&enemy&&enemy->actor.shape.yOffset==0){Check(enemy->isMirroringLink&&poses.size()>35&&attackFrames>40&&hoverFrames>200,"Original hover and attack articulation remain active in the darker dungeon");if(visual){gPlayState->frameAdvCtx.enabled=true;phase=98;}else phase=99;Write("passed");return;}}
 if(frames>750)throw std::runtime_error("Anubis integration timeout");
 }catch(const std::exception& e){phase=99;if(gPlayState)gPlayState->frameAdvCtx.enabled=true;Write("failed",e.what());}}
void Register(){auto* enabled=std::getenv("SOH_ARPG_ANUBIS_QA");auto* home=std::getenv("SHIP_HOME");if(!enabled||std::string(enabled)!="1"||!home||!std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;output=std::filesystem::path(home)/"anubis-results.json";visual=std::getenv("SOH_ARPG_ANUBIS_UI")!=nullptr;
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* p){if(static_cast<Actor*>(p)->id==ACTOR_EN_ANUBICE)++kills;});
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorInit>([](void* p){auto* a=static_cast<Actor*>(p);if(a->id==ACTOR_EN_ANUBICE_FIRE){++fireballs;fireExcluded&=!EnemyDifficulty::IsEnemyActor(a);if(enemy&&Alive(&enemy->actor))fireOrigin&=std::abs(a->world.pos.x-enemy->fireballPos.x)<.01&&std::abs(a->world.pos.y-enemy->fireballPos.y-15)<.01&&std::abs(a->world.pos.z-enemy->fireballPos.z)<.01;}});
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){auto now=std::chrono::steady_clock::now();if((phase==20||phase==21)&&frames>20&&frames<160)(phase==20?nativeTimes:upgradedTimes).push_back(std::chrono::duration<double,std::milli>(now-lastDraw).count());lastDraw=now;if(!gPlayState||!enemy||!Alive(&enemy->actor))return;++drawFrames;poses.insert({enemy->jointTable[3].x,enemy->jointTable[3].y,enemy->jointTable[3].z,enemy->jointTable[15].x,enemy->jointTable[15].y,enemy->jointTable[15].z});attackFrames+=enemy->actionFunc==EnAnubice_ShootFireball;hoverFrames+=enemy->actor.shape.yOffset==0;knockbackFrames+=enemy->isKnockedback;deathFrames+=enemy->actionFunc==EnAnubice_Die;});
}
static RegisterShipInitFunc init(Register);
}
