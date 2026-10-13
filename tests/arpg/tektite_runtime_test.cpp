#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/EnemyVisuals.h"
#include "soh/Enhancements/arpg/TektiteGeometry.h"
#include "soh/Enhancements/arpg/TektiteBehavior.h"
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
#include "overlays/actors/ovl_En_Tite/z_en_tite.h"
#include "overlays/actors/ovl_En_Part/z_en_part.h"
void Sram_InitDebugSave(void);
void EnTite_SetupIdle(EnTite*);void EnTite_Idle(EnTite*,PlayState*);
void EnTite_SetupAttack(EnTite*);void EnTite_Attack(EnTite*,PlayState*);
void EnTite_SetupFlipOnBack(EnTite*);void EnTite_SetupFlipUpright(EnTite*);
void EnTite_FlipUpright(EnTite*,PlayState*);void EnTite_Stunned(EnTite*,PlayState*);
void EnTite_CheckDamage(Actor*,PlayState*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0,kills=0,partFrames=0;
bool visual=false,crouched=false,jumped=false,landed=false,articulated=false,waterRecovered=false;
std::array<EnTite*,2> enemies{};EnTite* victim=nullptr;EnTite* waterEnemy=nullptr;
std::set<Gfx*> deathLists;Vec3f fixture{},waterFixture{};std::array<Vec3s,25> firstJoints{};
float waterPeak=0;std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
void Write(const char* status,const std::string& error=""){
    std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"render_frames",drawFrames},{"detached_part_frames",partFrames},{"water_peak",waterPeak},{"water_recovered",waterRecovered},{"checks",checks}}.dump(2);
}
void Check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);checks.push_back(name);SPDLOG_INFO("[Tektite QA] PASS {}",name);Write("running");}
void Warp(bool water=false){
    gSaveContext.entranceIndex=water?ENTR_ZORAS_RIVER_WEST_EXIT:ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;
    gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;
    gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);enemies={};waterEnemy=nullptr;victim=nullptr;
}
EnTite* Spawn(int params,Vec3f p){
    auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_TITE,p.x,p.y,p.z,0,0x8000,0,params);
    Check(a && a->update,"An original Tektite initializes with its native skeleton and action controller");
    a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    return reinterpret_cast<EnTite*>(a);
}
Gfx* Native(EnTite* e,int limb){return static_cast<StandardLimb*>(e->skelAnime.skeleton[limb-1])->dList;}
Gfx* Model(EnTite* e,int limb){auto* p=Native(e,limb);ARPG_OverrideEnemyLimb(gPlayState,&e->actor,limb,&p);return p;}
void Damage(EnTite* e,int damage,int effect){e->collider.base.acFlags|=AC_HIT;e->actor.colChkInfo.damage=damage;e->actor.colChkInfo.damageEffect=effect;EnTite_CheckDamage(&e->actor,gPlayState);}
void Mechanics(){
    std::set<Gfx*> lists;
    for(auto* e:enemies){
        Check(e->actor.scale.x==.01f && e->collider.elements[0].dim.worldSphere.radius==20,"Native collision and actor scale are preserved");
        for(int limb:TektiteGeometry::Limbs){
            auto* original=Native(e,limb);auto* p=Model(e,limb);
            Check(original && p && p!=original,"A native visible limb receives its replacement");
            Check(p==Model(e,limb),"Native rendering reuses a stable display list");lists.insert(p);
        }
    }
    Check(lists.size()==18,"Both Tektite species use distinct limb geometry");
    auto* e=enemies[0];auto* native=Native(e,4);Gfx* p=nullptr;
    ARPG_OverrideEnemyLimb(gPlayState,&e->actor,4,&p);Check(!p,"Invisible native limbs remain invisible");
    p=native;ARPG_OverrideEnemyLimb(gPlayState,&e->actor,2,&p);Check(p==native,"Unmodeled rig joints retain native behavior");
    for(auto key:{"gEnhancements.ARPG.RemasteredEnemies","gEnhancements.ARPG.Enabled"}){
        CVarSetInteger(key,0);Check(Model(e,4)==native,"Disabling ARPG or the models restores native geometry");CVarSetInteger(key,1);
    }
    auto eye=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;
    Check(Model(e,4)==native,"Distant Tektites use native lightweight geometry");GET_ACTIVE_CAM(gPlayState)->eye=eye;
    auto* hurt=Spawn(TEKTITE_RED,{fixture.x-200,fixture.y+20,fixture.z+120});hurt->actor.colChkInfo.health=5;
    Damage(hurt,0,1);Check(hurt->actionFunc==EnTite_Stunned && hurt->actor.colorFilterTimer==80,"Native nut stun freezes the pose and keeps the damage filter");
    EnTite_SetupIdle(hurt);Damage(hurt,1,15);Check(hurt->actionFunc==EnTite_Stunned && hurt->spawnIceTimer==48,"Native ice damage retains the four-foot ice effect sequence");
    Check(Model(hurt,4)==Model(e,4),"Damage states retain the upgraded model");Actor_Kill(&hurt->actor);
    for(auto* t:enemies){EnTite_SetupAttack(t);t->actor.colChkInfo.health=20;}
    std::copy(std::begin(e->jointTable),std::end(e->jointTable),firstJoints.begin());
}
bool FindWater(){
    auto* col=gPlayState->colCtx.colHeader;
    for(int i=0;i<col->numWaterBoxes;++i){auto& b=col->waterBoxes[i];
        for(int x=1;x<5;++x)for(int z=1;z<5;++z){
            Vec3f p={float(b.xMin)+b.xLength*x/5.f,float(b.ySurface)+100,float(b.zMin)+b.zLength*z/5.f};
            float surface=0;WaterBox* box=nullptr;CollisionPoly* poly=nullptr;
            if(!WaterBox_GetSurfaceImpl(gPlayState,&gPlayState->colCtx,p.x,p.z,&surface,&box))continue;
            const float floor=BgCheck_EntityRaycastFloor1(&gPlayState->colCtx,&poly,&p);
            if(floor>BGCHECK_Y_MIN && surface-floor>50 && b.xLength>120 && b.zLength>120){waterFixture={p.x,surface,p.z};return true;}
        }
    }return false;
}
void Tick(){
    if(phase>=98)return;
    try{
        ++frames;
        if(phase==0 && frames>60 && gGameState){
            CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();
            gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_CHILD;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;gARPGSave={};
            for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"})CVarSetInteger(key,1);
            for(auto key:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG"})CVarSetInteger(key,0);
            SeedRng(0x7EC7);Warp();phase=1;frames=0;return;
        }
        if(gPlayState && ((phase>=2 && phase<=5) || (phase>=7 && phase<=9))){
            auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=20;
            p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};
        }
        if(phase==1 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()){
            fixture=GET_PLAYER(gPlayState)->actor.world.pos;
            enemies={Spawn(TEKTITE_RED,{fixture.x-50,fixture.y+20,fixture.z+150}),Spawn(TEKTITE_BLUE,{fixture.x+50,fixture.y+20,fixture.z+150})};phase=2;frames=0;return;
        }
        if(phase==2 && frames>15){Mechanics();phase=3;frames=0;return;}
        if(phase==3){for(auto* e:enemies){
            crouched|=e->actionFunc==EnTite_Attack && e->actionVar1==0;
            jumped|=e->actor.velocity.y>2;
            landed|=jumped && (e->actor.bgCheckFlags&BGCHECKFLAG_GROUND_TOUCH);
            for(int i=5;i<25;++i){auto joint=e->jointTable[i],initial=firstJoints[i];articulated|=joint.x!=initial.x || joint.y!=initial.y || joint.z!=initial.z;}
        }}
        if(phase==3 && frames>220){
            Check(crouched && jumped && landed,"A live native encounter crouches, launches and lands");
            Check(articulated && drawFrames>100,"The native leg rig articulates while upgraded meshes render");
            for(auto* e:enemies)for(auto p:{e->frontLeftFootPos,e->frontRightFootPos,e->backRightFootPos,e->backLeftFootPos})
                Check(std::isfinite(p.x) && std::abs(p.x-e->actor.world.pos.x)<100 && std::abs(p.z-e->actor.world.pos.z)<100,"The native foot effect anchor stays attached to its animated leg");
            victim=enemies[0];for(int limb:TektiteGeometry::Limbs)deathLists.insert(Model(victim,limb));
            EnTite_SetupIdle(victim);victim->actor.colorFilterTimer=0;Damage(victim,100,0);phase=4;frames=0;return;
        }
        if(phase==4){
            for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)
                if(a->id==ACTOR_EN_PART && deathLists.contains(reinterpret_cast<EnPart*>(a)->displayList))++partFrames;
            if(frames>45){Check(kills==1,"Native dismemberment awards exactly one enemy defeat");Check(partFrames>9,"Detached native body parts retain and render the upgraded mesh lists");
                EnTite_SetupFlipOnBack(enemies[1]);phase=5;frames=0;return;}
        }
        if(phase==5){
            if(frames==45){Check(std::abs(enemies[1]->actor.shape.rot.z)>25000,"The original back-flip rotates the complete articulated model");EnTite_SetupFlipUpright(enemies[1]);}
            if(frames>90){Check(enemies[1]->actionFunc!=EnTite_FlipUpright && enemies[1]->actor.shape.rot.z==0,"Land recovery returns the original actor upright");Warp(true);phase=6;frames=0;return;}
        }
        if(phase==6 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()){
            Check(FindWater(),"A real river waterbox provides a deep-water fixture");fixture=GET_PLAYER(gPlayState)->actor.world.pos;
            waterEnemy=Spawn(TEKTITE_BLUE,waterFixture);phase=7;frames=0;return;
        }
        if(phase==7){
            EnTite_SetupIdle(waterEnemy);waterEnemy->actionVar1=1000;
            if(frames>25){
                Check((waterEnemy->actor.bgCheckFlags&BGCHECKFLAG_WATER) && waterEnemy->actor.gravity==0,"Native blue Tektite flotation rests on real scene water");
                CVarSetInteger("gEnhancements.ARPG.Enabled",0);EnTite_SetupFlipUpright(waterEnemy);phase=8;frames=0;return;
            }
        }
        if(phase==8 && frames>35){
            Check(waterEnemy->actionFunc==EnTite_FlipUpright && waterEnemy->actor.gravity==0 && waterEnemy->actor.world.pos.y>waterFixture.y+300,"ARPG-off reproduces the native zero-gravity recovery bug without altering it");
            CVarSetInteger("gEnhancements.ARPG.Enabled",1);waterEnemy->actor.world.pos=waterFixture;waterEnemy->actor.velocity.y=0;waterEnemy->actor.gravity=0;
            waterEnemy->actor.bgCheckFlags=BGCHECKFLAG_WATER;waterEnemy->actor.yDistToWater=0;waterEnemy->actor.shape.rot.z=0x7fff;
            EnTite_SetupFlipUpright(waterEnemy);phase=9;frames=0;return;
        }
        if(phase==9){
            waterPeak=std::max(waterPeak,waterEnemy->actor.world.pos.y-waterFixture.y);
            waterRecovered|=waterEnemy->actionFunc==EnTite_Idle && waterEnemy->actor.shape.rot.z==0 && waterEnemy->actor.gravity==0;
            if(waterRecovered){EnTite_SetupIdle(waterEnemy);waterEnemy->actionVar1=1000;}
            if(frames>55){Check(std::abs(waterPeak-78.f*R_UPDATE_RATE*.5f)<3.f,"Water recovery follows the native gravity and position-update rate");Check(waterRecovered,"Water landing completes recovery and restores native flotation");
                Check(std::abs(waterEnemy->actor.world.pos.y-waterFixture.y)<3,"Recovered blue Tektite stays on the real river surface");Warp();phase=10;frames=0;return;}
        }
        if(phase==10 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()){
            int count=0;for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->id==ACTOR_EN_TITE)++count;
            Check(count==0,"Scene teardown removes the original practice actors cleanly");
            if(!visual){phase=99;Write("passed");return;}
            fixture=GET_PLAYER(gPlayState)->actor.world.pos;
            enemies={Spawn(TEKTITE_RED,{fixture.x-40,fixture.y+5,fixture.z+100}),Spawn(TEKTITE_BLUE,{fixture.x+40,fixture.y+5,fixture.z+100})};phase=11;frames=0;return;
        }
        if(phase==11 && frames>10){gPlayState->frameAdvCtx.enabled=true;phase=98;Write("passed");}
        if(frames>1000)throw std::runtime_error("Tektite QA timeout");
    }catch(const std::exception& e){phase=99;Write("failed",e.what());}
}
void Register(){
    auto* enabled=std::getenv("SOH_ARPG_TEKTITE_QA");auto* home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
    output=std::filesystem::path(home)/"tektite-results.json";visual=std::getenv("SOH_ARPG_TEKTITE_UI")!=nullptr;
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(phase==3)++drawFrames;});
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* actor){if(actor==victim)++kills;});
}
static RegisterShipInitFunc init(Register);
}
