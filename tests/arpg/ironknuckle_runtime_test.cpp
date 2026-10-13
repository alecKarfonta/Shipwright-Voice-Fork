#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/IronKnuckleVisuals.h"
#include "soh/Enhancements/arpg/IronKnuckleGeometry.h"
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
#include "overlays/actors/ovl_En_Ik/z_en_ik.h"
#include "overlays/actors/ovl_En_Part/z_en_part.h"
#include "objects/object_ik/object_ik.h"
void Sram_InitDebugSave(void);
void func_80A74AAC(EnIk*); void func_80A74E2C(EnIk*); void func_80A751C8(EnIk*);
void func_80A755F0(EnIk*); void func_80A75C38(EnIk*,PlayState*);
s32 EnIk_OverrideLimbDraw3(PlayState*,s32,Gfx**,Vec3f*,Vec3s*,void*);
void EnIk_Destroy(Actor*,PlayState*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0,kills=0,verticalFrames=0,horizontalFirst=0,horizontalSecond=0,armorFrames=0,exposedFrames=0;
bool visual=false,stuck=false,recovery=false,frontDebris=false,rearDebris=false;
EnIk* enemy=nullptr;Vec3f fixture={-4,0,499},anchor={-4,0,269},firstPos{};
int oldBlur=TOTAL_EFFECT_COUNT,repeat=0;u32 switches[2]{};std::array<Vec3s,30> firstJoints{};
std::set<Gfx*> torsoPalettes;std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
void Write(const char* status,const std::string& error=""){std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"render_frames",drawFrames},{"vertical_active_frames",verticalFrames},{"horizontal_first_active_frames",horizontalFirst},{"horizontal_second_active_frames",horizontalSecond},{"armored_frames",armorFrames},{"exposed_frames",exposedFrames},{"native_creature_defeats",kills},{"checks",checks}}.dump(2);}
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);checks.push_back(label);SPDLOG_INFO("[Iron Knuckle QA] PASS {}",label);Write("running");}
bool Alive(const Actor* actor){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a==actor)return a->update!=nullptr;return false;}
void Warp(){gSaveContext.entranceIndex=ENTR_DEKU_TREE_ENTRANCE;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);enemy=nullptr;}
void Camera(){auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=0;p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};p->actor.shape.rot.y=0x8000;
 Vec3f at={anchor.x,45,anchor.z},eye={anchor.x+180,100,anchor.z+220};Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);}
void Fixture(){for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)Actor_Kill(a);Camera();}
EnIk* Spawn(int variant){auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_IK,anchor.x,anchor.y,anchor.z,0,0,0,0xFF00|(variant+1));Check(a&&a->update,"Native guard spawns with no-switch sentinel");a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED;return reinterpret_cast<EnIk*>(a);}
Gfx* Model(int limb,bool original=false){auto* dl=static_cast<StandardLimb*>(enemy->skelAnime.skeleton[limb-1])->dList;Vec3f pos{};Vec3s rot{};
 auto key="gEnhancements.ARPG.RemasteredEnemies";int setting=CVarGetInteger(key,1);if(original)CVarSetInteger(key,0);EnIk_OverrideLimbDraw3(gPlayState,limb,&dl,&pos,&rot,enemy);if(original)CVarSetInteger(key,setting);return dl;}
void Damage(int damage,int effect,bool immediate=true){enemy->bodyCollider.base.ac=&GET_PLAYER(gPlayState)->actor;enemy->bodyCollider.base.acFlags|=AC_HIT;enemy->actor.colChkInfo.damage=damage;enemy->actor.colChkInfo.damageEffect=effect;if(immediate)func_80A75C38(enemy,gPlayState);}
void Models(){Check(enemy->skelAnime.limbCount==30 && enemy->skelAnime.dListCount==24 && std::abs(enemy->actor.scale.x-.012f)<.00001f && enemy->bodyCollider.dim.radius==25 && enemy->bodyCollider.dim.height==80,"Original skeleton, actor scale and body cylinder remain authoritative");
 std::set<Gfx*> lists;bool replaced=true;for(int limb:IronKnuckleGeometry::Limbs){auto* original=Model(limb,true);auto* dl=Model(limb);if(original){replaced&=dl!=original&&dl==Model(limb);lists.insert(dl);}else replaced&=!dl;}
 Check(replaced&&lists.size()==22,"Twenty-two visible native joints use cached models and hidden torso joints remain hidden");torsoPalettes.insert(Model(26));
 bool off=true;for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"}){CVarSetInteger(key,0);for(int limb:IronKnuckleGeometry::Limbs)off&=Model(limb)==Model(limb,true);CVarSetInteger(key,1);}Check(off,"Both feature gates restore every original limb");
 auto eye=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;Check(Model(17)==Model(17,true),"Distant actors restore the original axe");GET_ACTIVE_CAM(gPlayState)->eye=eye;
 auto params=enemy->actor.params;enemy->actor.params=0;Check(!ARPG_UseIronKnuckleVisuals(gPlayState,&enemy->actor)&&Model(12)==Model(12,true),"Nabooru keeps the native story rig and helmet");enemy->actor.params=params;
 Check(ARPG_IsIronKnuckleRearArmor(Model(27))&&!ARPG_IsIronKnuckleRearArmor(Model(26)),"Cached armor preserves front and rear debris identity");
 std::string copied=object_ik_DL_015380;Check(ARPG_IsIronKnuckleRearArmor(reinterpret_cast<Gfx*>(copied.data()))&&!ARPG_IsIronKnuckleRearArmor(nullptr),"Original rear armor is recognized by resource identity rather than pointer address");
}
void Tick(){if(phase>=98)return;try{++frames;
 if(phase==0&&frames>60&&gGameState){CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_CHILD;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;gARPGSave={};
  for(auto k:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"})CVarSetInteger(k,1);
  for(auto k:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG"})CVarSetInteger(k,0);
  SeedRng(0x1A0);Warp();phase=1;frames=0;return;}
 if(gPlayState&&phase>=2&&phase<=16)Camera();
 if(phase==1&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){
  Camera();auto* practice=Creatures::SpawnIronKnucklePractice(gPlayState,0);Check(practice,"Practice placement works alongside original room enemies");Actor_Kill(practice);Fixture();enemy=Spawn(0);switches[0]=gPlayState->actorCtx.flags.swch;switches[1]=gPlayState->actorCtx.flags.tempSwch;phase=2;frames=0;return;}
 if(phase==2&&frames>25){Models();Check(enemy->unk_2F8==3&&enemy->skelAnime.playSpeed==0&&enemy->actor.colChkInfo.health==30,"Ivory guard waits in its original seated pose");
  std::copy(std::begin(enemy->jointTable),std::end(enemy->jointTable),firstJoints.begin());firstPos=enemy->actor.world.pos;Damage(1,15,false);phase=3;frames=0;return;}
 if(phase==3&&frames>80){bool animated=false;for(int i=1;i<30;++i)animated|=std::memcmp(&enemy->jointTable[i],&firstJoints[i],sizeof(Vec3s))!=0;
  Check(enemy->unk_2F8==5&&animated&&enemy->actor.world.pos.z>firstPos.z+20&&enemy->actor.colChkInfo.health==30,"First strike wakes the native stand-up animation and armored walk without damage");
  Check(std::abs(enemy->actor.speedXZ-.9f)<.001f,"Armored pursuit retains native heavy movement speed");fixture.z=499;func_80A74E2C(enemy);phase=4;frames=0;return;}
 if(phase==4){stuck|=enemy->unk_2F8==7;recovery|=enemy->unk_2F8==8;if(frames>65){Check(verticalFrames>=2&&stuck&&recovery&&enemy->unk_2F8!=6,"Vertical strike has an active axe window, embedded-axe pause and recovery");Check(Effect_GetByIndex(enemy->blureIdx)!=nullptr,"Native axe trail remains allocated during combat");recovery=false;func_80A751C8(enemy);phase=5;frames=0;return;}}
 if(phase==5){recovery|=enemy->unk_2F8==8;if(frames>95){Check(horizontalFirst>3&&horizontalSecond>3&&recovery,"Horizontal attack renders both native sweep windows and recovery");
  func_80A755F0(enemy);phase=6;frames=0;return;}}
 if(phase==6&&frames==2){Check(enemy->unk_2F8==9,"Native block pose remains reachable");int hp=enemy->actor.colChkInfo.health;enemy->shieldCollider.base.acFlags|=AC_BOUNCED;Damage(8,15);Check(enemy->actor.colChkInfo.health==hp&&!(enemy->shieldCollider.base.acFlags&AC_BOUNCED),"Shield bounce cancels body damage");
  Damage(2,14);Damage(0,13);Check(enemy->actor.colChkInfo.health==hp,"Armored guard rejects native arrow and utility effects");Damage(13,15);Check(enemy->actor.colChkInfo.health==17&&enemy->armorStatusFlag==0,"Melee damage hurts the armored guard without removing armor early");phase=7;frames=0;return;}
 if(phase==7&&frames>20){Damage(8,15,false);phase=8;frames=0;return;}
 if(phase==8){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a->id==ACTOR_EN_PART&&a->parent==&enemy->actor){auto* part=reinterpret_cast<EnPart*>(a);if(part->action!=0){if(ARPG_IsIronKnuckleRearArmor(part->displayList))rearDebris|=a->speedXZ<0;else frontDebris|=a->speedXZ>0;}}
  if(frames>12){Check(enemy->actor.colChkInfo.health==9&&enemy->armorStatusFlag==1&&enemy->drawArmorFlag==1,"Crossing ten health removes native chest armor");Check(!Model(26)&&!Model(27)&&Model(28)&&Model(29),"Exposed torso replaces both hidden chest plates");Check(frontDebris&&rearDebris&&enemy->bodyBreak.val==BODYBREAK_STATUS_FINISHED,"Both replacement chest plates detach in opposite native directions and release their buffers");phase=9;frames=0;return;}}
 if(phase==9&&frames>30){func_80A74AAC(enemy);Check(std::abs(enemy->actor.speedXZ-2.5f)<.001f,"Broken armor switches to the faster native run");Damage(2,14);Check(enemy->actor.colChkInfo.health==7,"Exposed guard becomes vulnerable to native arrows");phase=10;frames=0;return;}
 if(phase==10&&frames>25){oldBlur=enemy->blureIdx;Damage(20,15);Check(enemy->unk_2F8==2&&kills==1,"Lethal hit enters native death and emits one defeat");phase=11;frames=0;return;}
 if(phase==11&&frames>110){Check(!Alive(reinterpret_cast<Actor*>(enemy))&&kills==1&&gARPGSave.kills==1,"Native death finishes with exactly one ARPG reward");Check(!Effect_GetByIndex(oldBlur),"Defeat releases the owned axe-trail effect");Check(switches[0]==gPlayState->actorCtx.flags.swch&&switches[1]==gPlayState->actorCtx.flags.tempSwch,"No-switch practice guard leaves scene switches unchanged");enemy=Spawn(1);phase=12;frames=0;return;}
 if(phase==12&&frames>15){Models();Check(enemy->unk_2F8==3&&enemy->skelAnime.curFrame>20,"Black guard retains the standing wake pose");oldBlur=enemy->blureIdx;Actor_Kill(&enemy->actor);phase=13;frames=0;return;}
 if(phase==13&&frames>8){Check(!Effect_GetByIndex(oldBlur),"Actor removal releases the standing guard's axe trail");enemy=Spawn(2);phase=14;frames=0;return;}
 if(phase==14&&frames>15){Models();Check(torsoPalettes.size()==3,"Ivory, black and silver use distinct cached armor palettes");func_80A74AAC(enemy);Damage(21,15);Check(enemy->bodyBreak.val>0&&enemy->bodyBreak.matrices&&enemy->bodyBreak.dLists&&enemy->bodyBreak.objectIds,"Interrupted-break fixture owns pending debris allocations");oldBlur=enemy->blureIdx;
  // [TIME] | [INFO] | IronKnuckleQA.Destroy | Exercise the real destructor before actor removal so cleared ownership can be inspected without reading freed actor memory.
  EnIk_Destroy(&enemy->actor,gPlayState);enemy->actor.destroy=nullptr;Check(enemy->bodyBreak.val==BODYBREAK_STATUS_FINISHED&&!enemy->bodyBreak.matrices&&!enemy->bodyBreak.dLists&&!enemy->bodyBreak.objectIds&&!Effect_GetByIndex(oldBlur),"Interrupted armor break releases every owned buffer and effect");Actor_Kill(&enemy->actor);phase=15;frames=0;return;}
 if(phase==15&&frames>6){if(enemy){Check(!Effect_GetByIndex(oldBlur),"Repeated summon/despawn releases the trail slot");enemy=nullptr;}if(repeat<8){enemy=Spawn(repeat%3);oldBlur=enemy->blureIdx;Check(oldBlur!=TOTAL_EFFECT_COUNT&&Effect_GetByIndex(oldBlur),"Repeated encounter receives a usable axe trail");Actor_Kill(&enemy->actor);++repeat;frames=0;return;}
  Camera();int spawned=0;for(int variant=0;variant<3;++variant){auto* a=Creatures::SpawnIronKnucklePractice(gPlayState,variant);if(a){++spawned;Actor_Kill(a);}}
  Check(spawned==3,"All three practice armor variants pass clearance placement");Check(!Creatures::SpawnIronKnucklePractice(gPlayState,-1)&&!Creatures::SpawnIronKnucklePractice(gPlayState,3),"Unknown practice variants are rejected");
  Flags_SetClear(gPlayState,gPlayState->roomCtx.curRoom.num);Check(!Creatures::SpawnIronKnucklePractice(gPlayState,0),"Practice explains and rejects native cleared-room despawn");Flags_UnsetClear(gPlayState,gPlayState->roomCtx.curRoom.num);Warp();phase=16;frames=0;return;}
 if(phase==16&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){Check(kills==1,"Scene teardown adds no duplicate defeat rewards");if(!visual){phase=99;Write("passed");return;}Fixture();enemy=Spawn(0);Damage(1,15,false);phase=17;frames=0;return;}
 if(phase==17){Camera();if(frames>42){gPlayState->frameAdvCtx.enabled=true;phase=98;Write("passed");}}
 if(frames>1200)throw std::runtime_error("Iron Knuckle integration timeout");
 }catch(const std::exception& e){phase=99;Write("failed",e.what());}}
void Register(){auto* enabled=std::getenv("SOH_ARPG_IRON_QA");auto* home=std::getenv("SHIP_HOME");if(!enabled||std::string(enabled)!="1"||!home||!std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
 output=std::filesystem::path(home)/"ironknuckle-results.json";visual=std::getenv("SOH_ARPG_IRON_UI")!=nullptr;
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(phase>=2&&phase<=14&&enemy&&Alive(&enemy->actor)){++drawFrames;if(enemy->drawArmorFlag)++exposedFrames;else ++armorFrames;
  if(phase==4&&enemy->unk_2FE>0)++verticalFrames;if(phase==5&&enemy->unk_2FE>0){if(enemy->unk_2FF==2)++horizontalFirst;else if(enemy->unk_2FF==3)++horizontalSecond;}}});
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* actor){if(static_cast<Actor*>(actor)->id==ACTOR_EN_IK)++kills;});
}
static RegisterShipInitFunc init(Register);
}
