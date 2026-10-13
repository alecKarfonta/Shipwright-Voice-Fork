#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/EnemyVisuals.h"
#include "soh/Enhancements/arpg/OctorokGeometry.h"
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
#include "overlays/actors/ovl_En_Okuta/z_en_okuta.h"
#include "objects/object_okuta/object_okuta.h"
void Sram_InitDebugSave(void);
void EnOkuta_WaitToAppear(EnOkuta*,PlayState*);void EnOkuta_Appear(EnOkuta*,PlayState*);
void EnOkuta_WaitToShoot(EnOkuta*,PlayState*);void EnOkuta_Shoot(EnOkuta*,PlayState*);
void EnOkuta_Hide(EnOkuta*,PlayState*);void EnOkuta_Freeze(EnOkuta*,PlayState*);
void EnOkuta_WaitToDie(EnOkuta*,PlayState*);void EnOkuta_Die(EnOkuta*,PlayState*);
void EnOkuta_SetupAppear(EnOkuta*,PlayState*);void EnOkuta_ColliderCheck(EnOkuta*,PlayState*);
void EnOkuta_ProjectileFly(EnOkuta*,PlayState*);s32 EnOkuta_GetSnoutScale(EnOkuta*,f32,Vec3f*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0,kills=0,shots=0,frozenFrames=0;
bool visual=false,appeared=false,shot=false,floated=false,jointsMoved=false,snoutMoved=false,headMoved=false;
EnOkuta* enemy=nullptr;Vec3f fixture{},water{};std::array<Vec3s,38> firstJoints{};
std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
void Write(const char* status,const std::string& error="") {
    std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"render_frames",drawFrames},{"projectiles",shots},{"frozen_frames",frozenFrames},{"shore",{fixture.x,fixture.y,fixture.z}},{"water",{water.x,water.y,water.z}},{"checks",checks}}.dump(2);
}
void Check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);checks.push_back(name);SPDLOG_INFO("[Octorok QA] PASS {}",name);Write("running");}
void Warp(){gSaveContext.entranceIndex=ENTR_ZORAS_RIVER_WEST_EXIT;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;
    gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);enemy=nullptr;}
bool Alive(EnOkuta* e){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a==reinterpret_cast<Actor*>(e))return a->update!=nullptr;return false;}
bool FindShore(){auto* col=gPlayState->colCtx.colHeader;
    for(int i=0;i<col->numWaterBoxes;++i){auto& b=col->waterBoxes[i];
        for(int x=1;x<8;++x)for(int z=1;z<8;++z){
            Vec3f p={float(b.xMin)+b.xLength*x/8.f,float(b.ySurface)+100,float(b.zMin)+b.zLength*z/8.f};float surface=0;WaterBox* box=nullptr;CollisionPoly* poly=nullptr;
            if(!WaterBox_GetSurfaceImpl(gPlayState,&gPlayState->colCtx,p.x,p.z,&surface,&box))continue;
            float floor=BgCheck_EntityRaycastFloor1(&gPlayState->colCtx,&poly,&p);if(floor<=BGCHECK_Y_MIN || surface-floor<50)continue;
            for(int a=0;a<16;++a){Vec3f bank={p.x+Math_SinS(a*0x1000)*300,surface+120,p.z+Math_CosS(a*0x1000)*300};
                bank.y=BgCheck_EntityRaycastFloor1(&gPlayState->colCtx,&poly,&bank);if(bank.y<surface-2 || bank.y>surface+70)continue;
                float bankWater=0;WaterBox* bankBox=nullptr;if(WaterBox_GetSurfaceImpl(gPlayState,&gPlayState->colCtx,bank.x,bank.z,&bankWater,&bankBox) && bankWater>bank.y+2)continue;
                auto eye=bank;eye.y+=40;Vec3f target={p.x,surface+30,p.z},hit{};int bgId=0;
                if(BgCheck_EntityLineTest1(&gPlayState->colCtx,&eye,&target,&hit,&poly,true,false,false,true,&bgId))continue;
                fixture=bank;water={p.x,surface,p.z};return true;
            }
        }
    }return false;
}
EnOkuta* Spawn(int param=0){auto p=water;if(param)p.y+=100;auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_OKUTA,p.x,p.y,p.z,0,0,0,param);
    Check(a && a->update,"Native Octorok actor initializes over a real water box");a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;return reinterpret_cast<EnOkuta*>(a);}
Gfx* Native(int limb){return static_cast<StandardLimb*>(enemy->skelAnime.skeleton[limb-1])->dList;}
Gfx* Model(int limb){auto* p=Native(limb);ARPG_OverrideEnemyLimb(gPlayState,&enemy->actor,limb,&p);return p;}
void Damage(int damage,int effect){enemy->collider.base.acFlags|=AC_HIT;enemy->actor.colChkInfo.damage=damage;enemy->actor.colChkInfo.damageEffect=effect;EnOkuta_ColliderCheck(enemy,gPlayState);}
void Models(){
    Check(enemy->actor.params==0 && enemy->skelAnime.limbCount==38 && enemy->collider.dim.radius==20,"Original skeleton, creature parameter and collision radius are preserved");
    std::set<Gfx*> lists;
    for(int limb:OctorokGeometry::Limbs){auto* p=Model(limb);Check(p && p!=Native(limb),"Each visible native joint uses its authored geometry");Check(p==Model(limb),"The native joint display list is cached and stable");lists.insert(p);}
    Check(lists.size()==15,"All fifteen visible joints retain separate animation transforms");
    for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"}){CVarSetInteger(key,0);for(int limb:OctorokGeometry::Limbs)Check(Model(limb)==Native(limb),"Disabling the upgrade restores every native limb");CVarSetInteger(key,1);}
    Gfx* p=nullptr;ARPG_OverrideEnemyLimb(gPlayState,&enemy->actor,7,&p);Check(!p,"Hidden joints stay invisible");
    auto eye=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;Check(Model(5)==Native(5),"Distant enemies use native geometry");GET_ACTIVE_CAM(gPlayState)->eye=eye;
    std::copy(std::begin(enemy->jointTable),std::end(enemy->jointTable),firstJoints.begin());
}
void Reflect(){auto* player=GET_PLAYER(gPlayState);int oldShield=player->currentShield,age=gSaveContext.linkAge;
    for(int test=0;test<3;++test){auto* rock=Spawn(0x10);player->currentShield=test==0?PLAYER_SHIELD_DEKU:PLAYER_SHIELD_HYLIAN;gSaveContext.linkAge=test==1?LINK_AGE_ADULT:LINK_AGE_CHILD;
        rock->actor.floorHeight=water.y-80;rock->collider.base.atFlags|=AT_HIT|AT_BOUNCED|AT_TYPE_ENEMY;
        Gfx* dl=reinterpret_cast<Gfx*>(const_cast<char*>(gOctorokProjectileDL));auto* original=dl;ARPG_OverrideEnemyLimb(gPlayState,&rock->actor,5,&dl);
        Check(dl==original && rock->actor.category==ACTORCAT_PROP,"Projectile actor keeps its own native drawing and category");
        EnOkuta_ProjectileFly(rock,gPlayState);
        if(test<2)Check(rock->actor.update && (rock->collider.base.atFlags&AT_TYPE_PLAYER) && !(rock->collider.base.atFlags&AT_TYPE_ENEMY) && rock->collider.info.toucher.dmgFlags==2 && rock->timer==30,"Native Deku and adult Hylian shields reflect shots back as player attacks");
        else Check(!rock->actor.update,"The child Hylian shield keeps the native projectile-shatter rule");
        if(rock->actor.update)Actor_Kill(&rock->actor);
    }player->currentShield=oldShield;gSaveContext.linkAge=age;
}
void Tick(){if(phase>=98)return;try{++frames;
    if(phase==0 && frames>60 && gGameState){
        CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();
        gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_CHILD;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;gARPGSave={};
        for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"})CVarSetInteger(key,1);
        for(auto key:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG"})CVarSetInteger(key,0);
        SeedRng(0x0C70);Warp();phase=1;frames=0;return;
    }
    if(gPlayState && phase>=2 && phase<=7){auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=20;p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};p->actor.shape.rot.y=Math_Vec3f_Yaw(&fixture,&water);
        Vec3f at={water.x,water.y+15,water.z},eye={fixture.x+90,fixture.y+70,fixture.z+100};Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);}
    if(phase==1 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()){
        Check(FindShore(),"Practice encounter uses a real river and walkable shore with clear line of sight");
        for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a->id==ACTOR_EN_OKUTA)Actor_Kill(a);
        enemy=Spawn();phase=2;frames=0;return;
    }
    if(phase==2 && frames>40){Models();phase=3;frames=0;return;}
    if(phase==3){appeared|=enemy->actionFunc==EnOkuta_Appear;shot|=enemy->actionFunc==EnOkuta_Shoot;floated|=enemy->actionFunc==EnOkuta_WaitToShoot;
        headMoved|=enemy->headScale.y!=1;Vec3f scale{};snoutMoved|=EnOkuta_GetSnoutScale(enemy,enemy->skelAnime.curFrame,&scale) && (scale.x!=1 || scale.y!=1);
        for(int i=9;i<37;++i){auto a=enemy->jointTable[i],b=firstJoints[i];jointsMoved|=a.x!=b.x || a.y!=b.y || a.z!=b.z;}
        if(frames>240){Check(shot && floated && shots>0,"The native encounter surfaces, floats and spits moving projectiles");Check(headMoved && snoutMoved && jointsMoved && drawFrames>100,"The rendered head, nozzle and all arm chains retain native motion");
            Check(enemy->actor.home.pos.y==water.y && enemy->collider.dim.height>0,"Native water height and animated collision remain authoritative");
            enemy->actor.xzDistToPlayer=100;EnOkuta_WaitToShoot(enemy,gPlayState);Check(enemy->actionFunc==EnOkuta_Hide,"Approaching inside the native safe distance triggers hiding");phase=4;frames=0;return;}
    }
    if(phase==4 && enemy->actionFunc==EnOkuta_WaitToAppear){Check(!enemy->actor.draw && !(enemy->actor.flags&ACTOR_FLAG_ATTENTION_ENABLED),"The completed hide removes drawing and targeting underwater");
        EnOkuta_SetupAppear(enemy,gPlayState);Damage(0,0);Check(enemy->actor.colChkInfo.health==1,"Zero-damage utility hits retain native immunity");Damage(4,3);
        Check(enemy->actionFunc==EnOkuta_Freeze && enemy->timer==80,"Ice starts the native eighty-tick freeze and ice effects");phase=5;frames=0;return;
    }
    if(phase==5){if(Alive(enemy))frozenFrames+=enemy->actionFunc==EnOkuta_Freeze;
        if(frames>160){Check(frozenFrames>=75 && !Alive(enemy) && kills==1,"Freeze finishes through the original death route with one defeat reward");enemy=Spawn();EnOkuta_SetupAppear(enemy,gPlayState);Damage(1,0);Check(enemy->actionFunc==EnOkuta_WaitToDie,"Ordinary damage retains the native hit-then-collapse route");phase=6;frames=0;return;}
    }
    if(phase==6 && frames>100){Check(!Alive(enemy) && kills==2,"Ordinary death removes the actor and rewards exactly once");Reflect();Warp();phase=7;frames=0;return;}
    if(phase==7 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()){
        Check(kills==2,"Scene teardown does not duplicate defeats");
        if(!visual){phase=99;Write("passed");return;}
        Check(FindShore(),"The visual scene uses the same grounded river practice fixture");enemy=Spawn();phase=8;frames=0;return;
    }
    if(phase==8){auto* p=GET_PLAYER(gPlayState);p->actor.world.pos=p->actor.prevPos=fixture;p->actor.shape.rot.y=Math_Vec3f_Yaw(&fixture,&water);
        Vec3f at={water.x,water.y+15,water.z},eye={fixture.x+90,fixture.y+70,fixture.z+100};Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);
        if(frames>70){gPlayState->frameAdvCtx.enabled=true;phase=98;Write("passed");}}
    if(frames>1000)throw std::runtime_error("Octorok integration timeout");
}catch(const std::exception& e){phase=99;Write("failed",e.what());}}
void Register(){auto* enabled=std::getenv("SOH_ARPG_OCTOROK_QA");auto* home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
    output=std::filesystem::path(home)/"octorok-results.json";visual=std::getenv("SOH_ARPG_OCTOROK_UI")!=nullptr;
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(phase==3)++drawFrames;});
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* a){if(a==enemy)++kills;});
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorInit>([](void* a){auto* actor=static_cast<Actor*>(a);if(actor->id==ACTOR_EN_OKUTA && actor->params==0x10)++shots;});
}
static RegisterShipInitFunc init(Register);
}
