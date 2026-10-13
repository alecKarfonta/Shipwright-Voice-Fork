#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/StingerVisuals.h"
#include "soh/Enhancements/arpg/StingerGeometry.h"
#include "soh/Enhancements/arpg/CreatureSystem.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
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
#include "overlays/actors/ovl_En_Eiyer/z_en_eiyer.h"
#include "overlays/actors/ovl_En_Weiyer/z_en_weiyer.h"
void Sram_InitDebugSave(void);
void EnEiyer_UpdateDamage(EnEiyer*,PlayState*);void EnEiyer_SetupStartAttack(EnEiyer*);void EnEiyer_SetupAmbush(EnEiyer*,PlayState*);
void EnEiyer_Ambush(EnEiyer*,PlayState*);void EnEiyer_Glide(EnEiyer*,PlayState*);void EnEiyer_DiveAttack(EnEiyer*,PlayState*);void EnEiyer_Land(EnEiyer*,PlayState*);
void EnEiyer_AppearFromGround(EnEiyer*,PlayState*);void EnEiyer_WanderUnderground(EnEiyer*,PlayState*);void EnEiyer_CircleUnderground(EnEiyer*,PlayState*);
void EnEiyer_Hurt(EnEiyer*,PlayState*);void EnEiyer_Stunned(EnEiyer*,PlayState*);void EnEiyer_Die(EnEiyer*,PlayState*);void EnEiyer_Dead(EnEiyer*,PlayState*);void EnEiyer_Inactive(EnEiyer*,PlayState*);
void EnWeiyer_UpdateDamage(EnWeiyer*,PlayState*);void EnWeiyer_SetupAttack(EnWeiyer*);void EnWeiyer_SetupFreeSwim(EnWeiyer*);
void EnWeiyer_FreeSwim(EnWeiyer*,PlayState*);void EnWeiyer_Attack(EnWeiyer*,PlayState*);void EnWeiyer_OutOfWater(EnWeiyer*,PlayState*);void EnWeiyer_Stunned(EnWeiyer*,PlayState*);void EnWeiyer_Hurt(EnWeiyer*,PlayState*);void EnWeiyer_Dead(EnWeiyer*,PlayState*);
s32 EnEiyer_OverrideLimbDraw(PlayState*,s32,Gfx**,Vec3f*,Vec3s*,void*,Gfx**);
s32 EnWeiyer_OverrideLimbDraw(PlayState*,s32,Gfx**,Vec3f*,Vec3s*,void*,Gfx**);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0,kills=0,landFade=0,waterFade=0,dives=0,leaps=0;bool visual=false,landed=false,returned=false,waterStage=false;
Actor* enemy=nullptr;Actor* dryWater=nullptr;ActorFunc playerUpdate=nullptr;Vec3f fixture={-4,0,499},anchor={-4,0,269},water{},shore{},firstPos{};
Gfx* landBody=nullptr;std::array<Vec3s,19> firstJoints{};std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
EnEiyer* Land(){return reinterpret_cast<EnEiyer*>(enemy);}EnWeiyer* Water(){return reinterpret_cast<EnWeiyer*>(enemy);}
SkelAnime* Skeleton(){return enemy->id==ACTOR_EN_EIYER?&Land()->skelanime:&Water()->skelAnime;}
void Write(const char* status,const std::string& error=""){std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"render_frames",drawFrames},{"land_fade_frames",landFade},{"water_fade_frames",waterFade},{"dive_frames",dives},{"water_leap_frames",leaps},{"native_creature_defeats",kills},{"shore",{shore.x,shore.y,shore.z}},{"water",{water.x,water.y,water.z}},{"checks",checks}}.dump(2);}
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);checks.push_back(label);SPDLOG_INFO("[Stinger QA] PASS {}",label);Write("running");}
bool Alive(const Actor* actor){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a==actor)return a->update!=nullptr;return false;}
int LandCount(){int n=0;for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->id==ACTOR_EN_EIYER&&a->update)++n;return n;}
void Warp(bool river){waterStage=river;gSaveContext.entranceIndex=river?ENTR_ZORAS_RIVER_WEST_EXIT:ENTR_DEKU_TREE_ENTRANCE;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);enemy=nullptr;}
void Camera(){auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=20;p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};p->actor.shape.rot.y=Math_Vec3f_Yaw(&fixture,&anchor);
 Vec3f at={anchor.x,anchor.y+45,anchor.z},eye={anchor.x+130,anchor.y+120,anchor.z+155};Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);}
void Fixture(){for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)Actor_Kill(a);Camera();}
Actor* Spawn(bool wet,int params=10){auto p=anchor;if(wet)p.y-=15;auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,wet?ACTOR_EN_WEIYER:ACTOR_EN_EIYER,p.x,p.y,p.z,0,0,0,params);Check(a&&a->update,"Native Stinger actor initializes");a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;return a;}
Gfx* Model(int limb,bool original=false,bool fading=false){auto* dl=static_cast<StandardLimb*>(Skeleton()->skeleton[limb-1])->dList;Vec3f pos{};Vec3s rot{};Gfx* gfx=nullptr;
 if(enemy->id==ACTOR_EN_EIYER)EnEiyer_OverrideLimbDraw(gPlayState,limb,&dl,&pos,&rot,enemy,&gfx);else EnWeiyer_OverrideLimbDraw(gPlayState,limb,&dl,&pos,&rot,enemy,&gfx);
 if(!original)ARPG_OverrideStingerLimb(gPlayState,enemy,limb,&dl,fading);return dl;}
void Damage(int damage,int effect){enemy->colChkInfo.damage=damage;enemy->colChkInfo.damageEffect=effect;
 if(enemy->id==ACTOR_EN_EIYER){Land()->collider.base.acFlags|=AC_HIT;EnEiyer_UpdateDamage(Land(),gPlayState);}else{Water()->collider.base.acFlags|=AC_HIT;EnWeiyer_UpdateDamage(Water(),gPlayState);}}
void Models(bool buried){Check(Skeleton()->limbCount==19&&Skeleton()->skeletonHeader->skeletonType==SKELANIME_TYPE_NORMAL,"Original nineteen-joint standard rig remains intact");
 std::set<Gfx*> lists;bool replaced=true;for(int limb:StingerGeometry::Limbs){auto* original=Model(limb,true);auto* dl=Model(limb);if(original){replaced&=dl!=original&&dl==Model(limb);lists.insert(dl);}else replaced&=!dl;}
 Check(replaced&&lists.size()==(buried?2:11),buried?"Only the two native underground fins are replaced and visible":"All eleven native limb meshes are replaced and cached");
 bool off=true;for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"}){CVarSetInteger(key,0);for(int limb:StingerGeometry::Limbs)off&=Model(limb)==Model(limb,true);CVarSetInteger(key,1);}Check(off,"Both feature gates restore original visible and hidden limbs");
 auto eye=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;Check(Model(9)==Model(9,true),"Distance fallback restores native fins");GET_ACTIVE_CAM(gPlayState)->eye=eye;
 int limb=buried?9:3;auto* opaque=Model(limb);auto* fade=Model(limb,false,true);Check(fade!=opaque&&fade!=Model(limb,true)&&fade==Model(limb,false,true),"Translucent corpse models are separate, stable cached display lists");
 Gfx* dl=nullptr;ARPG_OverrideStingerLimb(gPlayState,enemy,2,&dl,0);Check(!dl,"Hidden root joints stay hidden");Vec3f p{};Vec3s r{};Gfx* gfx=nullptr;
 if(enemy->id==ACTOR_EN_EIYER)EnEiyer_OverrideLimbDraw(gPlayState,1,&dl,&p,&r,enemy,&gfx);else EnWeiyer_OverrideLimbDraw(gPlayState,1,&dl,&p,&r,enemy,&gfx);
 Check(p.z==(enemy->id==ACTOR_EN_EIYER?2500.f:2000.f),"Habitat-specific native root offset is preserved");
}
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
  SeedRng(0x5719);Warp(false);phase=1;frames=0;return;}
 if(gPlayState&&phase>=2&&phase<=24&&phase!=15&&phase!=23)Camera();
 if(phase==1&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){
  Camera();auto* practice=Creatures::SpawnStingerPractice(gPlayState,0);Check(practice,"Land practice placement works alongside original room enemies");Actor_Kill(practice);Fixture();dryWater=Spawn(true);enemy=Spawn(false);phase=2;frames=0;return;}
 if(phase==2&&frames>75){Check(!Alive(dryWater)&&kills==0,"Water Stinger rejects dry ground without rewards");Models(true);Check(std::abs(enemy->scale.x-.005f)<.00001f&&Land()->collider.dim.radius==27&&Land()->collider.dim.height==17,"Land scale and collision cylinder remain native");Check(Land()->actionFunc==EnEiyer_WanderUnderground,"Native land Stinger rises into underground wandering");
  std::copy(std::begin(Land()->jointTable),std::end(Land()->jointTable),firstJoints.begin());Damage(0,1);Check(Land()->actionFunc==EnEiyer_Ambush&&enemy->colChkInfo.health==2,"Underground utility hit triggers the native backflip ambush without damage");phase=3;frames=0;return;}
 if(phase==3&&frames>35){Models(false);landBody=Model(3);bool animated=false;for(int i=1;i<19;++i)animated|=std::memcmp(&Land()->jointTable[i],&firstJoints[i],sizeof(Vec3s))!=0;
  Check(Land()->actionFunc==EnEiyer_Glide&&enemy->world.pos.y>anchor.y+60&&animated,"Backflip rises into the original animated glide");fixture={enemy->world.pos.x,anchor.y,enemy->world.pos.z+60};EnEiyer_SetupStartAttack(Land());phase=4;frames=0;return;}
 if(phase==4){if(frames==25)fixture={-4,0,499};landed|=Land()->actionFunc==EnEiyer_Land;returned|=Land()->actionFunc==EnEiyer_AppearFromGround;
  if(frames>150){Check(dives>0&&landed&&returned,"Native dive attacks, lands and returns underground");EnEiyer_SetupAmbush(Land(),gPlayState);phase=5;frames=0;return;}}
 if(phase==5&&frames>30){Damage(1,0);Check(enemy->colChkInfo.health==1&&Land()->actionFunc==EnEiyer_Hurt&&Land()->timer==40,"Airborne damage enters native forty-tick recoil");phase=6;frames=0;return;}
 if(phase==6&&frames>50){Check(Land()->actionFunc==EnEiyer_Glide&&(Land()->collider.base.acFlags&AC_ON),"Native recoil restores gliding and damage reception");Damage(0,1);Check(Land()->actionFunc==EnEiyer_Stunned&&Land()->timer==80&&Land()->collider.dim.height==25,"Utility hit starts native stun with its larger floor cylinder");phase=7;frames=0;return;}
 if(phase==7&&frames>90){Check(Land()->actionFunc!=EnEiyer_Stunned&&Land()->collider.dim.height==17&&enemy->gravity==0,"Land stun restores normal collision and gravity");Damage(5,0);Check(Land()->actionFunc==EnEiyer_Die&&kills==1,"Airborne lethal hit starts native spiral death and one defeat");phase=8;frames=0;return;}
 if(phase==8&&frames>100){Check(!Alive(enemy)&&landFade>=40&&kills==1&&gARPGSave.kills==1,"Land corpse sinks and fades with exactly one reward");enemy=Spawn(false);phase=9;frames=0;return;}
 if(phase==9&&frames>70){Damage(1,0);Check(enemy->colChkInfo.health==0&&Land()->actionFunc==EnEiyer_Die&&kills==2&&gARPGSave.kills==2,"One-point underground hit honors native instant defeat and grants its previously missing reward");phase=10;frames=0;return;}
 if(phase==10&&frames>100){Check(!Alive(enemy)&&kills==2,"Underground defeat finishes without duplicate rewards");enemy=Spawn(false,0);phase=11;frames=0;return;}
 if(phase==11&&frames>70){Check(LandCount()==4&&Land()->actionFunc==EnEiyer_CircleUnderground,"Circling encounter creates three native decoys");bool decoys=true;int count=0;
  for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->id==ACTOR_EN_EIYER&&a->params>=1&&a->params<=3&&a->update){++count;decoys&=a->parent==enemy&&!EnemyDifficulty::IsEnemyActor(a);GameInteractor_ExecuteOnEnemyDefeat(a);}
  Check(count==3&&decoys&&gARPGSave.kills==2&&EnemyDifficulty::IsEnemyActor(enemy),"Only the real Stinger qualifies for combat scaling and rewards");Actor_Kill(enemy);phase=12;frames=0;return;}
 if(phase==12&&frames>10){Check(LandCount()==0&&kills==2,"Interrupted circle removes all borrowed-state decoys without rewards");enemy=Spawn(false,0);phase=13;frames=0;return;}
 if(phase==13&&frames>70){Land()->collider.base.ocFlags2|=OC2_HIT_PLAYER;phase=14;frames=0;return;}
 if(phase==14&&frames>35){int hidden=0;for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->id==ACTOR_EN_EIYER&&a->params>=1&&a->params<=3&&a->update)hidden+=reinterpret_cast<EnEiyer*>(a)->actionFunc==EnEiyer_Inactive;
  Check(hidden==3&&Land()->actionFunc==EnEiyer_Glide,"Native contact ambush makes all three decoys retreat");Damage(5,0);Check(kills==3,"Real circling parent defeat emits one reward");phase=21;frames=0;return;}
 if(phase==21&&frames>100){Check(LandCount()==0&&gARPGSave.kills==3,"Normal circle death also removes its decoys");Warp(true);phase=15;frames=0;return;}
 if(phase==15&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){Check(FindShore(),"Water fixture uses a real river and walkable bank");Camera();auto* practice=Creatures::SpawnStingerPractice(gPlayState,2);Check(practice,"Water practice finds a valid deep-water placement");Actor_Kill(practice);Fixture();enemy=Spawn(true,0);phase=16;frames=0;return;}
 if(phase==16&&frames>25){Models(false);Check(Model(3)!=landBody,"Water Stinger uses a distinct blue habitat palette");Check(std::abs(enemy->scale.x-.003f)<.00001f&&Water()->collider.dim.radius==16&&Water()->collider.dim.height==10,"Smaller water scale and native collider remain unchanged");Check(enemy->home.pos.y==water.y-5&&enemy->floorHeight<enemy->home.pos.y,"Water actor initializes between actual river floor and surface");
  fixture={water.x+100,water.y-10,water.z};EnWeiyer_SetupAttack(Water());Check(Water()->collider.base.atFlags&AT_ON,"Native swim attack enables contact damage");firstPos=enemy->world.pos;std::copy(std::begin(Water()->jointTable),std::end(Water()->jointTable),firstJoints.begin());phase=17;frames=0;return;}
 if(phase==17&&frames>150){bool animated=false;for(int i=1;i<19;++i)animated|=std::memcmp(&Water()->jointTable[i],&firstJoints[i],sizeof(Vec3s))!=0;
  Check(animated&&Math_Vec3f_DistXYZ(&firstPos,&enemy->world.pos)>20,"Native swimming articulates the fins and travels through the water");
  // [TIME] | [INFO] | StingerQA.Breach | Supply near-surface positions, water distances and facing after teleport so stale bank collision values cannot contaminate the original breach predicate.
  enemy->world.pos=enemy->prevPos={water.x,water.y-8,water.z};fixture={water.x,water.y-5,water.z+45};Camera();
  CollisionPoly* floorPoly=nullptr;auto probe=water;probe.y+=50;enemy->floorHeight=BgCheck_EntityRaycastFloor1(&gPlayState->colCtx,&floorPoly,&probe);
  enemy->shape.rot.y=enemy->world.rot.y=enemy->yawTowardsPlayer=0;enemy->yDistToWater=8;GET_PLAYER(gPlayState)->actor.yDistToWater=5;
  Water()->collider.base.atFlags&=~AT_HIT;EnWeiyer_SetupAttack(Water());EnWeiyer_Attack(Water(),gPlayState);
  Check(Water()->actionFunc==EnWeiyer_OutOfWater,"Native near-surface and facing predicate starts the breach");playerUpdate=GET_PLAYER(gPlayState)->actor.update;GET_PLAYER(gPlayState)->actor.update=[](Actor*,PlayState*){};phase=22;frames=0;return;}
 if(phase==22&&frames>20){GET_PLAYER(gPlayState)->actor.update=playerUpdate;playerUpdate=nullptr;Check(leaps>0,"Near-surface native pursuit breaches into the original out-of-water attack");fixture=shore;Damage(0,1);Check(Water()->actionFunc==EnWeiyer_Stunned&&Water()->timer==80&&Water()->collider.dim.height==25,"Water utility hit enters native floor stun and enlarged cylinder");phase=18;frames=0;return;}
 if(phase==18&&frames>95){Check(Water()->actionFunc!=EnWeiyer_Stunned&&Water()->collider.dim.height==10,"Water stun restores its normal collision height");Damage(1,0);Check(enemy->colChkInfo.health==1&&Water()->actionFunc==EnWeiyer_Hurt&&Water()->timer==40,"Water nonlethal hit retains native recoil");phase=19;frames=0;return;}
 if(phase==19&&frames>50){Check(Water()->actionFunc!=EnWeiyer_Hurt&&(Water()->collider.base.acFlags&AC_ON),"Water recoil restores damage reception");Damage(4,0);Check(kills==4&&enemy->colChkInfo.health==0,"Water defeat grants one reward");phase=20;frames=0;return;}
 if(phase==20&&frames>110){Check(!Alive(enemy)&&waterFade>=40&&gARPGSave.kills==4,"Water corpse completes its native translucent sinking fade");Check(!Creatures::SpawnStingerPractice(gPlayState,-1)&&!Creatures::SpawnStingerPractice(gPlayState,3),"Unknown practice habitats are rejected");Warp(false);fixture={-4,0,499};anchor={-4,0,269};phase=23;frames=0;return;}
 if(phase==23&&frames>100&&gPlayState&&GameInteractor::IsPlayerInControl()){Check(kills==4,"Scene teardown does not add defeat rewards");if(!visual){phase=99;Write("passed");return;}Fixture();enemy=Spawn(false);EnEiyer_SetupAmbush(Land(),gPlayState);phase=24;frames=0;return;}
 if(phase==24&&frames>35){gPlayState->frameAdvCtx.enabled=true;phase=98;Write("passed");}
 if(frames>1200)throw std::runtime_error("Stinger integration timeout");
 }catch(const std::exception& e){if(playerUpdate){GET_PLAYER(gPlayState)->actor.update=playerUpdate;playerUpdate=nullptr;}phase=99;Write("failed",e.what());}}
void Register(){auto* enabled=std::getenv("SOH_ARPG_STINGER_QA");auto* home=std::getenv("SHIP_HOME");if(!enabled||std::string(enabled)!="1"||!home||!std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
 output=std::filesystem::path(home)/"stinger-results.json";visual=std::getenv("SOH_ARPG_STINGER_UI")!=nullptr;
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(enemy&&phase>=2&&phase<=22&&Alive(enemy)){++drawFrames;if(enemy->id==ACTOR_EN_EIYER){if(Land()->actionFunc==EnEiyer_Dead)++landFade;if(Land()->actionFunc==EnEiyer_DiveAttack)++dives;}else{if(Water()->actionFunc==EnWeiyer_Dead)++waterFade;if(Water()->actionFunc==EnWeiyer_OutOfWater)++leaps;}}});
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* ptr){auto* a=static_cast<Actor*>(ptr);if((a->id==ACTOR_EN_EIYER&&(a->params==0||a->params==10))||a->id==ACTOR_EN_WEIYER)++kills;});
}
static RegisterShipInitFunc init(Register);
}
