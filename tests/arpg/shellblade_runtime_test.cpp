#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/ShellbladeVisuals.h"
#include "soh/Enhancements/arpg/ShellbladeGeometry.h"
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
#include "overlays/actors/ovl_En_Sb/z_en_sb.h"
#include "overlays/actors/ovl_En_Part/z_en_part.h"
void Sram_InitDebugSave(void);
void EnSb_SetupWaitClosed(EnSb*);void EnSb_SetupWaitOpen(EnSb*);void EnSb_Destroy(Actor*,PlayState*);
void EnSb_WaitClosed(EnSb*,PlayState*);void EnSb_Open(EnSb*,PlayState*);void EnSb_WaitOpen(EnSb*,PlayState*);void EnSb_TurnAround(EnSb*,PlayState*);void EnSb_Lunge(EnSb*,PlayState*);void EnSb_Bounce(EnSb*,PlayState*);void EnSb_Cooldown(EnSb*,PlayState*);
s32 EnSb_IsVulnerable(EnSb*);s32 EnSb_UpdateDamage(EnSb*,PlayState*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0,kills=0,lunges=0,bounces=0,waterFrames=0,flameFrames=0,fragments=0,wetFragments=0;bool visual=false,opened=false,ready=false,fragmentMeshes=true;
EnSb* enemy=nullptr;EnSbActionFunc previous=nullptr;Vec3f fixture={-4,0,499},anchor={-4,0,269},shore{},water{},firstPos{};
Gfx* shells[2]{};std::set<Actor*> seenParts;std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
void Write(const char* status,const std::string& error=""){std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"render_frames",drawFrames},{"land_lunges",lunges},{"land_bounces",bounces},{"water_frames",waterFrames},{"fire_draw_frames",flameFrames},{"fragment_capture_ready",ready},{"authored_fragment_meshes",fragmentMeshes},{"shell_fragments",fragments},{"water_fragments",wetFragments},{"native_defeats",kills},{"checks",checks}}.dump(2);}
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);checks.push_back(label);SPDLOG_INFO("[Shellblade QA] PASS {}",label);Write("running");}
bool Alive(const Actor* actor){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a==actor)return a->update!=nullptr;return false;}
void Warp(bool river){gSaveContext.entranceIndex=river?ENTR_ZORAS_RIVER_WEST_EXIT:ENTR_DEKU_TREE_ENTRANCE;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);enemy=nullptr;}
void Camera(){auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=20;p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};p->actor.shape.rot.y=Math_Vec3f_Yaw(&fixture,&anchor);Vec3f at={anchor.x,anchor.y+20,anchor.z},eye={anchor.x+95,anchor.y+70,anchor.z+120};Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);}
void Fixture(){for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)Actor_Kill(a);Camera();}
EnSb* Spawn(bool wet=false){seenParts.clear();auto p=anchor;if(wet)p.y-=25;auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_SB,p.x,p.y,p.z,0,0,0,0);Check(a&&a->update,"Native Shell Blade initializes with its original actor");a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;return reinterpret_cast<EnSb*>(a);}
Gfx* Model(int limb,bool original=false){auto* dl=static_cast<StandardLimb*>(enemy->skelAnime.skeleton[limb-1])->dList;if(!original)ARPG_OverrideShellbladeLimb(gPlayState,&enemy->actor,limb,&dl);return dl;}
void Models(){Check(enemy->skelAnime.limbCount==9&&enemy->skelAnime.dListCount==4,"Original nine-joint flex skeleton and four native matrices remain intact");bool good=true;std::set<Gfx*> models;for(int l:ShellbladeGeometry::Limbs){auto* dl=Model(l);good&=dl&&dl!=Model(l,true)&&dl==Model(l);models.insert(dl);}Check(good&&models.size()==4,"All four native meshes are replaced with stable cached lists");
 for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"}){CVarSetInteger(key,0);bool off=true;for(int l:ShellbladeGeometry::Limbs)off&=Model(l)==Model(l,true);Check(off,"Feature gate restores original geometry");CVarSetInteger(key,1);}
 auto eye=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;Check(Model(4)==Model(4,true),"Distant Shell Blades use their native geometry");GET_ACTIVE_CAM(gPlayState)->eye=eye;
 Gfx* dl=nullptr;ARPG_OverrideShellbladeLimb(gPlayState,&enemy->actor,2,&dl);Check(!dl,"Native hidden root joints remain hidden");shells[0]=Model(4);shells[1]=Model(6);
 Check(std::abs(enemy->actor.scale.x-.006f)<.00001f&&enemy->collider.dim.radius==30&&enemy->collider.dim.height==40,"Native scale and collision cylinder remain unchanged");}
void Damage(int damage,int effect,int height=20,int angle=0){enemy->actor.colChkInfo.damage=damage;enemy->actor.colChkInfo.damageEffect=effect;enemy->actor.yawTowardsPlayer=enemy->actor.shape.rot.y+angle;enemy->collider.info.bumper.hitPos.y=int(enemy->actor.world.pos.y)+height;enemy->collider.info.bumper.hitPos.x=enemy->actor.world.pos.x;enemy->collider.info.bumper.hitPos.z=enemy->actor.world.pos.z;enemy->collider.base.atFlags&=~AT_HIT;enemy->collider.base.acFlags|=AC_HIT;EnSb_UpdateDamage(enemy,gPlayState);}
void Windows(){auto behavior=enemy->behavior;auto frame=enemy->skelAnime.curFrame;bool good=true;
 for(auto row:std::vector<std::array<int,3>>{{0,1,0},{0,2,1},{0,5,1},{0,6,0},{1,0,1},{1,1,1},{1,3,0},{2,0,1},{2,19,1},{2,20,0},{3,0,1},{3,1,0},{4,2,0},{4,3,1},{4,5,1},{4,6,0}}){enemy->behavior=row[0];enemy->skelAnime.curFrame=row[1];good&=EnSb_IsVulnerable(enemy)==row[2];}
 enemy->behavior=behavior;enemy->skelAnime.curFrame=frame;Check(good,"All original mouth vulnerability frame boundaries remain authoritative");}
bool FindShore(){auto* col=gPlayState->colCtx.colHeader;
    for(int i=0;i<col->numWaterBoxes;++i){auto& b=col->waterBoxes[i];
        for(int x=1;x<8;++x)for(int z=1;z<8;++z){
            Vec3f p={float(b.xMin)+b.xLength*x/8.f,float(b.ySurface)+100,float(b.zMin)+b.zLength*z/8.f};float surface=0;WaterBox* box=nullptr;CollisionPoly* poly=nullptr;
            if(!WaterBox_GetSurfaceImpl(gPlayState,&gPlayState->colCtx,p.x,p.z,&surface,&box))continue;
            float floor=BgCheck_EntityRaycastFloor1(&gPlayState->colCtx,&poly,&p);if(floor<=BGCHECK_Y_MIN || surface-floor<50)continue;
            bool footprint=true;for(int j=0;j<8;++j){auto q=p;q.x+=Math_SinS(j*0x2000)*60;q.z+=Math_CosS(j*0x2000)*60;
                float h=0;WaterBox* b=nullptr;if(!WaterBox_GetSurfaceImpl(gPlayState,&gPlayState->colCtx,q.x,q.z,&h,&b)||std::abs(h-surface)>5){footprint=false;break;}
                float bottom=BgCheck_EntityRaycastFloor1(&gPlayState->colCtx,&poly,&q);if(bottom<=BGCHECK_Y_MIN||h-bottom<25){footprint=false;break;}}
            for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&std::abs(a->world.pos.y-(surface-15))<100&&Math_Vec3f_DistXZ(&a->world.pos,&p)<100)footprint=false;
            if(!footprint)continue;
            for(int a=0;a<16;++a){Vec3f bank={p.x+Math_SinS(a*0x1000)*300,surface+120,p.z+Math_CosS(a*0x1000)*300};
                bank.y=BgCheck_EntityRaycastFloor1(&gPlayState->colCtx,&poly,&bank);if(bank.y<surface-2 || bank.y>surface+70)continue;
                float bankWater=0;WaterBox* bankBox=nullptr;if(WaterBox_GetSurfaceImpl(gPlayState,&gPlayState->colCtx,bank.x,bank.z,&bankWater,&bankBox) && bankWater>bank.y+2)continue;
                auto eye=bank;eye.y+=35;Vec3f target={p.x,surface+5,p.z},hit{};int bgId=0;
                if(BgCheck_EntityLineTest1(&gPlayState->colCtx,&eye,&target,&hit,&poly,true,false,false,true,&bgId))continue;
                fixture=shore=bank;water=anchor={p.x,surface,p.z};return true;
            }
        }
    }return false;
}
void Tick(){if(phase>=98)return;try{++frames;
 if(phase==0&&frames>60&&gGameState){CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_CHILD;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;gARPGSave={};
  for(auto k:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"})CVarSetInteger(k,1);
  for(auto k:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG"})CVarSetInteger(k,0);
  SeedRng(0x5B19);Warp(false);phase=1;frames=0;return;}
 if(gPlayState&&phase>=2&&phase<=22&&phase!=15&&phase!=20)Camera();
 if(phase==1&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){Camera();auto* p=Creatures::SpawnShellbladePractice(gPlayState,0);Check(p,"Land practice places a Shell Blade beside original room enemies");Actor_Kill(p);Check(!Creatures::SpawnShellbladePractice(gPlayState,1),"Water practice rejects a dry room");Fixture();enemy=Spawn();phase=2;frames=0;return;}
 if(phase==2&&frames>30){Models();Windows();Check(enemy->actionFunc==EnSb_WaitClosed&&enemy->actor.colChkInfo.health==2,"Distant native Shell Blade waits closed");fixture={anchor.x,anchor.y,anchor.z+120};phase=3;frames=0;return;}
 if(phase==3){opened|=enemy->actionFunc==EnSb_Open||enemy->actionFunc==EnSb_WaitOpen;if(enemy->actionFunc==EnSb_TurnAround){Check(opened&&enemy->timer==0,"Approach opens the shell and completes the native fifteen-tick warning");fixture={-4,0,599};phase=4;frames=0;previous=nullptr;return;}}
 if(phase==4){if(enemy->actionFunc!=previous){if(enemy->actionFunc==EnSb_Lunge){++lunges;Check(enemy->skelAnime.playSpeed==0&&enemy->actor.gravity==-2,"Dry lunge retains the native held animation and stronger gravity");}if(enemy->actionFunc==EnSb_Bounce)++bounces;previous=enemy->actionFunc;}
  if(lunges>=4&&enemy->actionFunc==EnSb_WaitClosed){Check(bounces>=4&&enemy->bouncesLeft==0,"Native attack completes its opening hop and three repeat bounces");enemy->actor.world.pos=enemy->actor.prevPos=anchor;enemy->actor.speedXZ=0;fixture={-4,0,499};EnSb_SetupWaitClosed(enemy);enemy->skelAnime.curFrame=3;phase=5;frames=0;return;}}
 if(phase==5&&frames>8){Damage(2,15);Check(enemy->actor.colChkInfo.health==2&&!enemy->isDead,"Closed shell deflects ordinary damaging hits");Damage(0,0);Check(enemy->actor.colChkInfo.health==2,"Unsupported utility hit leaves shell health intact");EnSb_SetupWaitOpen(enemy);Damage(1,13,20,0x4000);Check(enemy->actor.colChkInfo.health==2,"Side hits cannot bypass the frontal mouth window");Damage(1,13,30);Check(enemy->actor.colChkInfo.health==2,"Hits at the upper mouth boundary are deflected");Damage(1,13,10);Check(enemy->actor.colChkInfo.health==2,"Hits at the lower mouth boundary are deflected");Damage(1,13);Check(enemy->actor.colChkInfo.health==1&&enemy->actionFunc==EnSb_Cooldown&&enemy->timer==60,"Frontal sword hit damages the open mouth and starts native cooldown");phase=6;frames=0;return;}
 if(phase==6&&frames>90){Check(enemy->actionFunc==EnSb_WaitClosed&&enemy->timer==0,"Native cooldown restores waiting after sixty ticks and ground contact");EnSb_SetupWaitOpen(enemy);Damage(2,1);Check(enemy->isDead&&enemy->bodyBreak.val>0&&kills==0,"Lethal mouth hookshot allocates native fragments before the reward");phase=7;frames=0;return;}
 if(phase==7&&frames>100){Check(!Alive(&enemy->actor)&&kills==1&&gARPGSave.kills==1,"Fragment death produces exactly one native defeat reward");Check(ready&&fragments>=2&&fragmentMeshes,"Both native shell fragments retain the authored rigid valve meshes");enemy=Spawn();phase=8;frames=0;return;}
 if(phase==8&&frames>25){enemy->skelAnime.curFrame=3;Damage(1,2);Check(enemy->actor.colChkInfo.health==1&&enemy->fire==4&&!enemy->isDead,"Fire retains its native closed-shell damage exception");phase=9;frames=0;return;}
 if(phase==9&&frames>100){Check(enemy->fire==0&&enemy->actor.colorFilterTimer==0&&flameFrames>=3,"Fixed fire countdown emits its alternating effect and releases the damage filter");Damage(2,2);Check(enemy->isDead,"Lethal fire still starts native fragmentation");phase=10;frames=0;return;}
 if(phase==10&&frames>100){Check(!Alive(&enemy->actor)&&kills==2&&gARPGSave.kills==2,"Fire death grants one reward without repeating its effect forever");enemy=Spawn();Damage(2,2);Check(enemy->bodyBreak.matrices&&enemy->bodyBreak.dLists&&enemy->bodyBreak.objectIds,"Interrupted death owns all three fragment buffers");EnSb_Destroy(&enemy->actor,gPlayState);enemy->actor.destroy=nullptr;Check(!enemy->bodyBreak.matrices&&!enemy->bodyBreak.dLists&&!enemy->bodyBreak.objectIds&&enemy->bodyBreak.val==BODYBREAK_STATUS_FINISHED,"Interrupted destruction frees and clears pending fragment buffers");Actor_Kill(&enemy->actor);phase=11;frames=0;return;}
 if(phase==11&&frames>15){Check(kills==2,"Interrupted removal does not fabricate an additional defeat");Warp(true);phase=15;frames=0;return;}
 if(phase==15&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){Check(FindShore(),"Water fixture uses a real river and walkable bank");Camera();auto* p=Creatures::SpawnShellbladePractice(gPlayState,1);Check(p,"Underwater practice finds clear water above a real floor");Actor_Kill(p);Fixture();enemy=Spawn(true);phase=16;frames=0;return;}
 if(phase==16&&frames>35){Models();Check(enemy->actor.yDistToWater>0&&enemy->actor.floorHeight<water.y,"Native Shell Blade settles below the real water surface");enemy->attackYaw=0;enemy->actor.shape.rot.y=-0x8000;EnSb_TurnAround(enemy,gPlayState);Check(enemy->actionFunc==EnSb_Lunge&&enemy->actor.speedXZ==5&&enemy->actor.velocity.y==3&&enemy->actor.gravity==-.35f&&enemy->skelAnime.playSpeed==1,"Water lunge retains its distinct speed, lift, gravity and animated closure");firstPos=enemy->actor.world.pos;phase=17;frames=0;return;}
 if(phase==17&&frames>25){Check(waterFrames>10&&Math_Vec3f_DistXYZ(&firstPos,&enemy->actor.world.pos)>20,"Underwater native physics advances the animated attack");EnSb_SetupWaitOpen(enemy);Damage(2,15);Check(enemy->isDead,"Frontal underwater mouth hit starts native death");phase=18;frames=0;return;}
 if(phase==18&&frames>100){Check(!Alive(&enemy->actor)&&kills==3&&gARPGSave.kills==3&&wetFragments>=2,"Underwater shell fragments use their native water type and one reward");Check(!Creatures::SpawnShellbladePractice(gPlayState,-1)&&!Creatures::SpawnShellbladePractice(gPlayState,2),"Unknown practice placement is rejected");Warp(false);fixture={-4,0,499};anchor={-4,0,269};phase=20;frames=0;return;}
 if(phase==20&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){Check(kills==3,"Scene teardown does not repeat defeat rewards");Fixture();std::vector<EnSb*> batch;for(int i=0;i<6;++i)batch.push_back(Spawn());Check(!Creatures::SpawnShellbladePractice(gPlayState,0),"Practice cap counts existing Shell Blades");for(auto* a:batch)Actor_Kill(&a->actor);phase=21;frames=0;return;}
 if(phase==21&&frames>12){Check(kills==3,"Repeated practice teardown adds no rewards");if(!visual){phase=99;Write("passed");return;}enemy=Spawn();fixture={anchor.x,anchor.y,anchor.z+120};EnSb_SetupWaitOpen(enemy);enemy->timer=15;phase=22;frames=0;return;}
 if(phase==22&&frames>5){Vec3f at={anchor.x,anchor.y+21,anchor.z},eye={anchor.x+65,anchor.y+48,anchor.z+95};Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);gPlayState->frameAdvCtx.enabled=true;phase=98;Write("passed");}
 if(frames>1000)throw std::runtime_error("Shell Blade integration timeout");
 }catch(const std::exception& e){phase=99;Write("failed",e.what());}}
void Register(){auto* enabled=std::getenv("SOH_ARPG_SHELLBLADE_QA");auto* home=std::getenv("SHIP_HOME");if(!enabled||std::string(enabled)!="1"||!home||!std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;output=std::filesystem::path(home)/"shellblade-results.json";visual=std::getenv("SOH_ARPG_SHELLBLADE_UI")!=nullptr;
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* p){if(static_cast<Actor*>(p)->id==ACTOR_EN_SB)++kills;});
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(!gPlayState)return;if(enemy&&Alive(&enemy->actor)){++drawFrames;if(phase>=16&&phase<=18)++waterFrames;if(enemy->fire>0)++flameFrames;if(enemy->bodyBreak.val==BODYBREAK_STATUS_READY)ready=true;}
  for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;a;a=a->next)if(a->id==ACTOR_EN_PART&&a->update&&a->parent==reinterpret_cast<Actor*>(enemy)&&!seenParts.contains(a)){seenParts.insert(a);++fragments;wetFragments+=a->params==4;auto* dl=reinterpret_cast<EnPart*>(a)->displayList;fragmentMeshes&=dl==shells[0]||dl==shells[1];}});
}
static RegisterShipInitFunc init(Register);
}
