#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/EnemyVisuals.h"
#include "soh/Enhancements/arpg/BubbleGeometry.h"
#include "soh/Enhancements/arpg/CreatureSystem.h"
#include "soh/Enhancements/arpg/AffixSystem.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"
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
#include "overlays/actors/ovl_En_Bb/z_en_bb.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_En_Part/z_en_part.h"
void Sram_InitDebugSave(void);
void EnBb_Blue(EnBb*,PlayState*);void EnBb_Down(EnBb*,PlayState*);void EnBb_Red(EnBb*,PlayState*);
void EnBb_White(EnBb*,PlayState*);void EnBb_Green(EnBb*,PlayState*);void EnBb_Death(EnBb*,PlayState*);
void EnBb_Stunned(EnBb*,PlayState*);void EnBb_CollisionCheck(EnBb*,PlayState*);void EnBb_SetupDeath(EnBb*,PlayState*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0,kills=0,partFrames=0,stunFrames=0,heldFireFrames=0;
bool visual=false,charged=false,moved=false,jointsMoved=false,greenOff=false,greenOnAgain=false;
EnBb* enemy=nullptr;Vec3f fixture={-4,0,499},anchor={-4,0,289},firstPos{};std::array<Vec3s,16> firstJoints{};
std::set<Gfx*> deathLists;std::set<Gfx*> blueLists;std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
Vec3s routePoints[4]={{-90,75,180},{90,90,180},{90,80,330},{-90,90,330}};Path routes[1]={{4,routePoints}};
ActorFunc heldUpdate=nullptr;uint32_t beforeTrails=0;
void Write(const char* status,const std::string& error="") {std::ofstream(output)<<nlohmann::json{{"status",status},{"error",error},{"phase",phase},{"render_frames",drawFrames},{"part_frames",partFrames},{"stun_frames",stunFrames},{"held_fire_frames",heldFireFrames},{"native_creature_defeats",kills},{"checks",checks}}.dump(2);}
void Check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);checks.push_back(name);SPDLOG_INFO("[Bubble QA] PASS {}",name);Write("running");}
void Warp(){gSaveContext.entranceIndex=ENTR_DEKU_TREE_ENTRANCE;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);enemy=nullptr;}
bool Alive(EnBb* e){for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a==reinterpret_cast<Actor*>(e))return a->update!=nullptr;return false;}
EnBb* Spawn(int param){auto p=anchor;if((param&255)==0xFC || (param&255)==0xFB)p.y+=100;
    auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_BB,p.x,p.y,p.z,0,0,0,param);Check(a && a->update,"Native Bubble initializes with its original rig and action controller");a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED;return reinterpret_cast<EnBb*>(a);}
Gfx* Native(EnBb* e,int limb){return static_cast<StandardLimb*>(e->skelAnime.skeleton[limb-1])->dList;}
Gfx* Model(EnBb* e,int limb){auto* p=Native(e,limb);ARPG_OverrideEnemyLimb(gPlayState,&e->actor,limb,&p);return p;}
void DeathMeshes(){deathLists.clear();for(int limb:BubbleGeometry::Limbs)deathLists.insert(Model(enemy,limb));}
void Damage(int damage,int effect){static ColliderInfo source{};source.toucher.dmgFlags=effect==5?DMG_MAGIC_FIRE:DMG_ARROW_NORMAL;source.toucher.damage=damage;
    enemy->collider.base.acFlags|=AC_HIT;enemy->collider.elements[0].info.acHitInfo=&source;enemy->actor.colChkInfo.damage=damage;enemy->actor.colChkInfo.damageEffect=effect;EnBb_CollisionCheck(enemy,gPlayState);}
void Camera(){auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=20;p->actor.world.pos=p->actor.prevPos=fixture;p->actor.speedXZ=0;p->actor.velocity={};p->actor.shape.rot.y=0x8000;
    Vec3f at={anchor.x,75,anchor.z},eye={anchor.x+85,110,fixture.z+140};Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);}
void Fixture(){gPlayState->setupPathList=routes;for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)Actor_Kill(a);Camera();}
void Models(){
    Check(enemy->actor.params==ENBB_BLUE && enemy->skelAnime.limbCount==16 && std::abs(enemy->actor.scale.x-.01f)<.0001f && enemy->elements[0].dim.worldSphere.radius==12,"Original Bubble skeleton, scale and collision sphere remain authoritative");
    for(int limb:BubbleGeometry::Limbs){auto* dl=Model(enemy,limb);Check(dl && dl!=Native(enemy,limb),"Every visible native limb receives the authored mesh");Check(dl==Model(enemy,limb),"Native limb display lists remain stable for detached body parts");blueLists.insert(dl);}
    Check(blueLists.size()==6,"Skull, jaw and four moving wing sections have independent cached lists");
    for(int param:{0xFE,0xFD,0x3FC,0x3FB}){auto* e=Spawn(param);std::set<Gfx*> lists;for(int limb:BubbleGeometry::Limbs){auto* dl=Model(e,limb);Check(dl!=Native(e,limb) && dl==Model(e,limb),"Each native variant retains a stable colored limb set");lists.insert(dl);}
        Check(lists.size()==6 && !blueLists.contains(Model(e,15)),"Variant palettes remain distinct from blue skulls");
        if(param==0x3FB)Check(std::abs(e->actor.scale.x-.03f)<.0001f && e->elements[0].dim.worldSphere.radius==66,"Great green keeps native three-times scale and its large collision sphere");Actor_Kill(&e->actor);}
    for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"}){CVarSetInteger(key,0);for(int limb:BubbleGeometry::Limbs)Check(Model(enemy,limb)==Native(enemy,limb),"Off gates restore every original limb");CVarSetInteger(key,1);}
    auto eye=GET_ACTIVE_CAM(gPlayState)->eye;GET_ACTIVE_CAM(gPlayState)->eye.x+=10000;Check(Model(enemy,15)==Native(enemy,15),"Distant actors keep their lightweight original mesh");GET_ACTIVE_CAM(gPlayState)->eye=eye;
    Gfx* dl=nullptr;ARPG_OverrideEnemyLimb(gPlayState,&enemy->actor,1,&dl);Check(!dl,"Hidden dummy bones do not gain geometry");
    std::copy(std::begin(enemy->jointTable),std::end(enemy->jointTable),firstJoints.begin());firstPos=enemy->actor.world.pos;
}
void TrailRewards(){for(auto key:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills"})CVarSetInteger(key,1);
    auto* trail=Spawn(0);Check(!EnemyDifficulty::IsEnemyActor(&trail->actor) && !AffixSystem::GetAffixData(&trail->actor),"Flame-trail actors are excluded from enemy classification and affix assignment");
    Check(Model(trail,15)==Native(trail,15),"Flame trails never receive a replacement skull mesh");
    auto xp=gARPGSave.xp,defeats=gARPGSave.kills,items=gARPGSave.itemsFound;EnBb_SetupDeath(trail,gPlayState);
    Check(gARPGSave.xp==xp && gARPGSave.kills==defeats && gARPGSave.itemsFound==items,"Fading a flame trail grants no XP, kill count or loot");Actor_Kill(&trail->actor);
    for(auto key:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills"})CVarSetInteger(key,0);
}
void Hold(Actor*,PlayState*){}
void Tick(){if(phase>=98)return;try{++frames;
    if(phase==0 && frames>60 && gGameState){CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();
        gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_CHILD;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;gARPGSave={};
        for(auto key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.RemasteredEnemies"})CVarSetInteger(key,1);
        for(auto key:{"gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Creatures","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG"})CVarSetInteger(key,0);
        SeedRng(0xBABB1E);Warp();phase=1;frames=0;return;}
    if(gPlayState && phase>=2 && phase<=15){Camera();for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a->id==ACTOR_EN_PART && deathLists.contains(reinterpret_cast<EnPart*>(a)->displayList))++partFrames;}
    if(phase==1 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()){Fixture();enemy=reinterpret_cast<EnBb*>(Creatures::SpawnBubblePractice(gPlayState,0));Check(enemy && enemy->actor.params==ENBB_BLUE,"Production practice placement creates a blue Bubble over clear floor");phase=2;frames=0;return;}
    if(phase==2 && frames>35){Models();TrailRewards();phase=3;frames=0;return;}
    if(phase==3){charged|=enemy->maxSpeed>=6;moved|=std::hypot(enemy->actor.world.pos.x-firstPos.x,enemy->actor.world.pos.z-firstPos.z)>40;
        for(int i=1;i<16;++i){auto a=enemy->jointTable[i],b=firstJoints[i];jointsMoved|=a.x!=b.x || a.y!=b.y || a.z!=b.z;}
        if(frames>180){Check(charged && moved && jointsMoved && drawFrames>120,"Live blue Bubble flies, charges and articulates its jaw and wings");
            auto p=enemy->actor.world.pos;auto* bomb=reinterpret_cast<EnBom*>(Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_BOM,p.x+30,p.y-30,p.z,0,0,0,0));Check(bomb!=nullptr,"Native bomb lure is available");bomb->timer=90;enemy->actor.home.pos=enemy->actor.world.pos;EnBb_Blue(enemy,gPlayState);Check(enemy->targetActor==&bomb->actor,"Blue Bubble retains its native attraction to nearby bombs");enemy->targetActor=nullptr;Actor_Kill(&bomb->actor);
            enemy->collider.base.atFlags|=AT_BOUNCED;EnBb_CollisionCheck(enemy,gPlayState);Check(enemy->actionFunc==EnBb_Down && enemy->timer==200 && enemy->flameScaleX==0,"Shield bounce removes blue flame protection and starts native vulnerability");phase=4;frames=0;return;}}
    if(phase==4){if(enemy->actionFunc==EnBb_Stunned)++stunFrames;
        if(frames==25){Damage(1,0);Check(enemy->actor.colChkInfo.health==3,"Ordinary weapons damage the extinguished blue skull");Damage(0,15);Check(enemy->actionFunc==EnBb_Stunned && enemy->actor.colorFilterTimer==80,"A utility hit preserves the native eighty-tick stun");}
        if(frames>360){Check(stunFrames>=75 && enemy->actionFunc==EnBb_Blue && enemy->flameScaleX>90,"Stun and vulnerability expire through native recovery into flight");DeathMeshes();Damage(8,14);Check(enemy->actionFunc==EnBb_Death && kills==1,"Lethal piercing damage enters native dismemberment once");phase=5;frames=0;return;}}
    if(phase==5 && frames>90){Check(!Alive(enemy) && partFrames>10 && gARPGSave.kills==1,"Native detached parts retain upgraded geometry and one defeat reward");enemy=Spawn(0xFE);beforeTrails=gARPGSave.kills;phase=6;frames=0;return;}
    if(phase==6 && frames>85){Check(enemy->actionFunc==EnBb_Red && enemy->actionState==1 && enemy->actor.child && enemy->flameScaleX>80,"Red Bubble retains its native ambush leap and linked flame trail");
        int trailCount=0;for(auto* a=enemy->actor.child;a;a=a->child){Check(a->id==ACTOR_EN_BB && a->params>=0 && !EnemyDifficulty::IsEnemyActor(a),"Each linked trail remains a non-enemy visual actor");++trailCount;}
        Check(trailCount==5,"Native red ambush produces five linked flame segments");enemy->collider.base.atFlags|=AT_BOUNCED;EnBb_CollisionCheck(enemy,gPlayState);Check(enemy->actionFunc==EnBb_Down && !enemy->actor.child,"Shield deflection detaches the red trail and exposes the skull");phase=7;frames=0;return;}
    if(phase==7 && frames>45){Check(gARPGSave.kills==beforeTrails,"All five fading red-trail segments leave the kill count unchanged");DeathMeshes();Damage(8,9);Check(enemy->actionFunc==EnBb_Stunned && enemy->fireIceTimer==48,"Ice preserves native frozen rendering before red Bubble death");phase=8;frames=0;return;}
    if(phase==8 && frames>165){Check(!Alive(enemy) && kills==2 && gARPGSave.kills==2,"Frozen red death completes without extra trail rewards");enemy=Spawn(0x3FC);moved=false;firstPos=enemy->actor.world.pos;phase=9;frames=0;return;}
    if(phase==9){moved|=std::hypot(enemy->actor.world.pos.x-firstPos.x,enemy->actor.world.pos.y-firstPos.y)>30;greenOff|=enemy->flameScaleX<5;greenOnAgain|=greenOff && enemy->flameScaleX>90;
        if(frames==25){Damage(1,0);Check(enemy->actor.colChkInfo.health==1,"Active green flame rejects ordinary weapon damage");enemy->collider.base.acFlags&=~AC_HIT;}
        if(frames>310 && enemy->flameScaleX<5){Check(greenOff && greenOnAgain && moved,"Green Bubble orbits and cycles its flame vulnerability naturally");DeathMeshes();Damage(1,0);Check(enemy->actionFunc==EnBb_Death && kills==3,"An exposed green skull dies to an ordinary weapon hit");phase=10;frames=0;return;}}
    if(phase==10 && frames>90){Check(!Alive(enemy) && gARPGSave.kills==3,"Green death preserves one reward");enemy=Spawn(0xFD);firstPos=enemy->actor.world.pos;phase=11;frames=0;return;}
    if(phase==11 && frames>140){Check(enemy->actionFunc==EnBb_White && enemy->blureIdx>=0 && std::hypot(enemy->actor.world.pos.x-firstPos.x,enemy->actor.world.pos.z-firstPos.z)>40,"White Bubble follows its native route and retains its blur effect");
        DeathMeshes();Damage(8,5);Check(enemy->actionFunc==EnBb_Death && enemy->fireIceTimer==48 && (enemy->actor.colorFilterParams&0xFF)>0,"Lethal fire initializes a valid color-filter duration before halted updates");heldUpdate=enemy->actor.update;enemy->actor.update=Hold;phase=12;frames=0;return;}
    if(phase==12){if(frames>60){Check(heldFireFrames>=50 && enemy->fireIceTimer==0 && (enemy->actor.colorFilterParams&0xFF)>0,"Fire draws safely to completion while the actor update is halted");enemy->actor.update=heldUpdate;phase=13;frames=0;return;}}
    if(phase==13 && frames>90){Check(!Alive(enemy) && kills==4 && gARPGSave.kills==4,"White fire death resumes native dismemberment and rewards once");enemy=Spawn(0x3FB);firstPos=enemy->actor.home.pos;phase=14;frames=0;return;}
    if(phase==14 && frames>180){Check(enemy->actionFunc==EnBb_Green && enemy->actor.scale.x>.029f && std::hypot(enemy->actor.home.pos.x-firstPos.x,enemy->actor.home.pos.z-firstPos.z)>40,"Great green moves its native orbit center along the configured route");DeathMeshes();Damage(8,14);phase=15;frames=0;return;}
    if(phase==15 && frames>100){Check(!Alive(enemy) && kills==5 && gARPGSave.kills==5,"Great green death also produces exactly one reward");Warp();phase=16;frames=0;return;}
    if(phase==16 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()){Check(kills==5,"Scene teardown creates no extra defeat events");if(!visual){phase=99;Write("passed");return;}Fixture();enemy=Spawn(0xFD);phase=17;frames=0;return;}
    if(phase==17){Camera();auto at=enemy->actor.world.pos;at.y+=2;auto eye=at;eye.x+=Math_SinS(enemy->actor.shape.rot.y+0x1000)*125;eye.z+=Math_CosS(enemy->actor.shape.rot.y+0x1000)*125;eye.y+=25;Play_CameraSetAtEye(gPlayState,CAM_ID_MAIN,&at,&eye);if(frames>50){gPlayState->frameAdvCtx.enabled=true;phase=98;Write("passed");}}
    if(frames>1100)throw std::runtime_error("Bubble integration timeout");
}catch(const std::exception& e){phase=99;Write("failed",e.what());}}
void Register(){auto* enabled=std::getenv("SOH_ARPG_BUBBLE_QA");auto* home=std::getenv("SHIP_HOME");if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
    output=std::filesystem::path(home)/"bubble-results.json";visual=std::getenv("SOH_ARPG_BUBBLE_UI")!=nullptr;
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(phase>=2 && phase<=15)++drawFrames;if(phase==12)++heldFireFrames;});
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* a){auto* actor=static_cast<Actor*>(a);if(actor->id==ACTOR_EN_BB && actor->params<0)++kills;});
}
static RegisterShipInitFunc init(Register);
}
