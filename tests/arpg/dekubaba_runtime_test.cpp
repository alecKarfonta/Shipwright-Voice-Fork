#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/EnemyVisuals.h"
#include "soh/Enhancements/arpg/DekuBabaGeometry.h"
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
#include "overlays/actors/ovl_En_Dekubaba/z_en_dekubaba.h"
#include "objects/object_dekubaba/object_dekubaba.h"
void Sram_InitDebugSave(void);
void EnDekubaba_Wait(EnDekubaba*,PlayState*);void EnDekubaba_Grow(EnDekubaba*,PlayState*);
void EnDekubaba_Lunge(EnDekubaba*,PlayState*);void EnDekubaba_PrepareLunge(EnDekubaba*,PlayState*);
void EnDekubaba_PullBack(EnDekubaba*,PlayState*);void EnDekubaba_Recover(EnDekubaba*,PlayState*);
void EnDekubaba_Hit(EnDekubaba*,PlayState*);void EnDekubaba_StunnedVertical(EnDekubaba*,PlayState*);
void EnDekubaba_PrunedSomersault(EnDekubaba*,PlayState*);void EnDekubaba_DeadStickDrop(EnDekubaba*,PlayState*);
void EnDekubaba_UpdateDamage(EnDekubaba*,PlayState*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0,kills=0,prunedFrames=0,stickFrames=0;
bool visual=false,grew=false,woundUp=false,lunged=false,recovered=false,jawMoved=false,stemMoved=false;
std::array<EnDekubaba*,2> plants{};std::array<Vec3f,2> roots{};std::array<Vec3s,8> firstJoints{};
Vec3f fixture{};std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
void Write(const char* status,const std::string& error=""){
    std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"render_frames",drawFrames},{"pruned_frames",prunedFrames},{"stick_frames",stickFrames},{"checks",checks}}.dump(2);
}
void Check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);checks.push_back(name);SPDLOG_INFO("[Deku Baba QA] PASS {}",name);Write("running");}
void Warp(){gSaveContext.entranceIndex=ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;
    gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);plants={};}
EnDekubaba* Spawn(int type,float x,float z){
    Vec3f p={fixture.x+x,fixture.y+50,fixture.z+z};CollisionPoly* poly=nullptr;
    p.y=BgCheck_EntityRaycastFloor1(&gPlayState->colCtx,&poly,&p);Check(p.y>BGCHECK_Y_MIN,"The rooted practice plant uses real scene ground");
    auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_DEKUBABA,p.x,p.y,p.z,0,0x8000,0,type);
    Check(a && a->update,"An original Deku Baba initializes with its native skeleton and AI");
    a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED|ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    return reinterpret_cast<EnDekubaba*>(a);
}
Gfx* Native(EnDekubaba* e,int part){
    if(part==4 || part==7)return static_cast<StandardLimb*>(e->skelAnime.skeleton[part-1])->dList;
    const char* lists[]={gDekuBabaStemTopDL,gDekuBabaStemMiddleDL,gDekuBabaStemBaseDL,gDekuBabaBaseLeavesDL,gDekuBabaStickDropDL};
    return reinterpret_cast<Gfx*>(const_cast<char*>(lists[part-8]));
}
Gfx* Model(EnDekubaba* e,int part){auto* p=Native(e,part);ARPG_OverrideEnemyLimb(gPlayState,&e->actor,part,&p);return p;}
void Damage(EnDekubaba* e,int amount,int effect){e->collider.base.acFlags|=AC_HIT;e->actor.colChkInfo.damage=amount;e->actor.colChkInfo.damageEffect=effect;EnDekubaba_UpdateDamage(e,gPlayState);}
bool Alive(EnDekubaba* e){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a==reinterpret_cast<Actor*>(e))return a->update!=nullptr;return false;}
void Mechanics(){
    std::set<Gfx*> lists;
    for(auto* e:plants){
        const auto size=e->actor.params==DEKUBABA_BIG?2.5f:1.f;
        Check(e->size==size && e->collider.count==7 && e->collider.elements[0].dim.modelSphere.radius==int(15*size),"Native species size and all seven collision spheres are retained");
        for(int part:DekuBabaGeometry::Limbs){auto* p=Model(e,part);
            Check(p && p!=Native(e,part),"Native jaw, stem, leaf or stick part receives its authored mesh");
            Check(p==Model(e,part),"Repeated draws reuse the same cached native display list");lists.insert(p);
        }
        Check(e->collider.base.colType==COLTYPE_HARD,"The retracted plant retains its native armored state");
        auto health=e->actor.colChkInfo.health;Damage(e,100,15);
        Check(e->actor.colChkInfo.health==health && e->actionFunc==EnDekubaba_Wait,"The native retracted bulb blocks damage");
    }
    Check(lists.size()==14,"Both native sizes have distinct seven-part appearances");
    auto* e=plants[0];auto* native=Native(e,4);Gfx* p=nullptr;ARPG_OverrideEnemyLimb(gPlayState,&e->actor,4,&p);Check(!p,"Invisible jaw rig nodes remain invisible");
    p=native;ARPG_OverrideEnemyLimb(gPlayState,&e->actor,2,&p);Check(p==native,"Unmodeled rig joints stay native");
    for(auto key:{"gEnhancements.ARPG.RemasteredEnemies","gEnhancements.ARPG.Enabled"}){
        CVarSetInteger(key,0);for(int part:DekuBabaGeometry::Limbs)Check(Model(e,part)==Native(e,part),"Disabling ARPG or remastered models restores every native part");CVarSetInteger(key,1);
    }
    auto eye=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;
    Check(Model(e,4)==native && Model(e,11)==Native(e,11),"Distant plants restore native head and root geometry");GET_ACTIVE_CAM(gPlayState)->eye=eye;
    for(int i=0;i<2;++i){roots[i]=plants[i]->actor.home.pos;plants[i]->timer=0;}
    std::copy(std::begin(e->jointTable),std::end(e->jointTable),firstJoints.begin());
}
void Tick(){if(phase>=98)return;
    try{++frames;
        if(phase==0 && frames>60 && gGameState){
            CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();
            gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_CHILD;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;gARPGSave={};
            for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"})CVarSetInteger(key,1);
            for(auto key:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG"})CVarSetInteger(key,0);
            SeedRng(0xDE4B);Warp();phase=1;frames=0;return;
        }
        if(gPlayState && phase>=2 && phase<=6){auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=20;p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};}
        if(phase==1 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()){
            fixture=GET_PLAYER(gPlayState)->actor.world.pos;plants={Spawn(0,-70,150),Spawn(1,130,330)};for(auto* e:plants)e->timer=1000;phase=2;frames=0;return;
        }
        if(phase==2 && frames>10){Mechanics();phase=3;frames=0;return;}
        if(phase==3){for(int i=0;i<2;++i){auto* e=plants[i];
            grew|=e->actionFunc==EnDekubaba_Grow;woundUp|=e->actionFunc==EnDekubaba_PrepareLunge;lunged|=e->actionFunc==EnDekubaba_Lunge;
            recovered|=e->actionFunc==EnDekubaba_PullBack || e->actionFunc==EnDekubaba_Recover;
            stemMoved|=e->stemSectionAngle[0]!=-0x4000 && e->stemSectionAngle[1]!=-0x4000;
            for(int j=2;j<8;++j){auto a=e->jointTable[j],b=firstJoints[j];jawMoved|=a.x!=b.x || a.y!=b.y || a.z!=b.z;}
            if(e->actor.home.pos.x!=roots[i].x || e->actor.home.pos.z!=roots[i].z)throw std::runtime_error("Root drift during native lunge");
        }}
        if(phase==3 && frames>320){
            Check(grew && woundUp && lunged && recovered,"The live native encounter grows, telegraphs, lunges and recovers");
            Check(jawMoved && stemMoved && drawFrames>200,"Native jaw joints and rooted stem transforms animate the replacement meshes");
            for(auto* e:plants){Check(e->boundFloor,"The animated plant retains its original floor attachment");
                for(int i=0;i<7;++i){auto p=e->collider.elements[i].dim.worldSphere.center;Check(std::abs(p.x-e->actor.home.pos.x)<250*e->size && std::abs(p.z-e->actor.home.pos.z)<250*e->size,"Native collision spheres stay on the animated head and stem");}
            }
            plants[0]->collider.base.colType=COLTYPE_HIT6;Damage(plants[0],0,1);Check(plants[0]->actionFunc==EnDekubaba_Hit && plants[0]->timer==2,"A nut hit enters the original stun transition");phase=4;frames=0;return;
        }
        if(phase==4 && plants[0]->actionFunc==EnDekubaba_StunnedVertical){
            Check(plants[0]->actor.world.pos.y==plants[0]->actor.home.pos.y+60,"Stun exposes the original vertical stem and pruning target");
            for(int i=1;i<7;++i)Check(plants[0]->collider.elements[i].info.bumperFlags&BUMP_ON,"Stun activates the native severable stem hitbox");
            Damage(plants[0],100,15);Check(plants[0]->actionFunc==EnDekubaba_PrunedSomersault,"A lethal sword hit follows the original pruning death route");phase=5;frames=0;return;
        }
        if(phase==5){
            if(!Alive(plants[0]))throw std::runtime_error("Pruned plant disappeared before collectible conversion");
            prunedFrames+=plants[0]->actionFunc==EnDekubaba_PrunedSomersault;stickFrames+=plants[0]->actionFunc==EnDekubaba_DeadStickDrop;
            if(frames>80){Check(prunedFrames>5 && stickFrames>5,"The severed head somersaults and converts to the native stick pickup");
                Check(kills==1 && plants[0]->actor.category==ACTORCAT_MISC,"Pruning awards one defeat and keeps the collectible category");
                Check(Model(plants[0],12)!=Native(plants[0],12),"The resulting stick pickup uses its matching authored stalk");
                plants[1]->collider.base.colType=COLTYPE_HIT6;Damage(plants[1],100,2);Check(plants[1]->actionFunc==EnDekubaba_Hit,"Fire enters the native hit-and-shrink death route");phase=6;frames=0;return;}
        }
        if(phase==6 && frames>90){Check(kills==2 && !Alive(plants[1]),"The big plant's fire death awards exactly one defeat and removes the actor");Warp();phase=7;frames=0;return;}
        if(phase==7 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()){
            int count=0;for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a->id==ACTOR_EN_DEKUBABA)++count;
            Check(count==0,"Scene teardown removes native plants and the remaining stick pickup");
            if(!visual){phase=99;Write("passed");return;}
            fixture=GET_PLAYER(gPlayState)->actor.world.pos;plants={Spawn(0,-60,140),Spawn(1,130,350)};phase=8;frames=0;return;
        }
        if(phase==8 && frames>100){gPlayState->frameAdvCtx.enabled=true;phase=98;Write("passed");}
        if(frames>1000)throw std::runtime_error("Deku Baba QA timeout");
    }catch(const std::exception& e){phase=99;Write("failed",e.what());}
}
void Register(){auto* enabled=std::getenv("SOH_ARPG_DEKUBABA_QA");auto* home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
    output=std::filesystem::path(home)/"dekubaba-results.json";visual=std::getenv("SOH_ARPG_DEKUBABA_UI")!=nullptr;
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(phase==3)++drawFrames;});
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* a){for(auto* e:plants)if(a==e)++kills;});
}
static RegisterShipInitFunc init(Register);
}
