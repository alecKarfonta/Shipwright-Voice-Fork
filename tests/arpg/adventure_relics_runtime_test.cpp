#include "soh/Enhancements/arpg/AdventureRelics.h"
#include "soh/Enhancements/arpg/ARPGBridge.h"
#include "soh/Enhancements/arpg/ARPGSaveCodec.h"
#include "soh/Enhancements/arpg/AdventureItems.h"
#include "soh/Enhancements/arpg/ArsenalSystem.h"
#include "soh/Enhancements/arpg/ItemModifierSystem.h"
#include "soh/Enhancements/arpg/InventorySystem.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/ShipInit.hpp"
#include "soh/SaveManager.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <cmath>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "overlays/actors/ovl_En_Test/z_en_test.h"
#include "overlays/actors/ovl_En_Boom/z_en_boom.h"
#include "overlays/actors/ovl_En_Bom_Chu/z_en_bom_chu.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
void Sram_InitDebugSave(void);
void EnBoom_Update(Actor*,PlayState*);
void EnBomChu_Explode(EnBomChu*,PlayState*);
void EnArrow_Shoot(EnArrow*,PlayState*);
s32 func_80842AC4(PlayState*,Player*);
void CollisionCheck_ApplyDamage(PlayState*,CollisionCheckContext*,Collider*,ColliderInfo*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,draws=0;bool collisionChecked=false;bool visual=false;
Actor* foe=nullptr;Actor* lightFoe=nullptr;
std::filesystem::path output;
nlohmann::json checks=nlohmann::json::array();
void Report(const char* status,const std::string& error=""){std::ofstream(output)<<nlohmann::json{{"status",status},{"checks",checks},{"error",error},{"draw_frames",draws}}.dump(2);}
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);checks.push_back(label);SPDLOG_INFO("[Adventure Relics QA] PASS: {}",label);Report("running");}
void Noop(Actor*,PlayState*){}
void Equip(int id){int slot=Loot::Uniques[id].slot;ARPGItemInstance item{};item.slot=slot;item.baseEquip=1;item.itemLevel=35;Loot::IdentifyUnique(item,id);InventoryRules::Equipped(gARPGSave,slot)=item;if(slot<4){gSaveContext.equips.equipment=(gSaveContext.equips.equipment&~(15<<(slot*4)))|(item.baseEquip<<(slot*4));}GameInteractor_ExecuteOnPlayerUpdate();}
Actor* Spawn(int id,int params=0,float x=150){auto p=GET_PLAYER(gPlayState)->actor.world.pos;return Actor_Spawn(&gPlayState->actorCtx,gPlayState,id,p.x+x,p.y,p.z,0,0,0,params);}
int Hit(Actor* source,uint32_t flags,bool hard=false,Actor* target=nullptr,bool setPoint=false) {
    if(!target)target=foe;++gPlayState->gameplayFrames;auto backup=target->colChkInfo;
    Collider at{},ac{};ColliderInfo strike{},surface{};at.actor=source;at.atFlags=AT_TYPE_PLAYER;ac.actor=target;ac.acFlags=AC_HIT|(hard?AC_HARD:0);
    strike.toucher.dmgFlags=flags;strike.toucher.damage=2;surface.acHit=&at;surface.acHitInfo=&strike;surface.bumperFlags=BUMP_HIT;
    if(setPoint){Vec3f point=target->world.pos;point.y+=std::clamp(target->focus.pos.y-point.y,25.f,75.f)*.65f;auto yaw=Actor_WorldYawTowardActor(target,&GET_PLAYER(gPlayState)->actor);point.x+=Math_SinS(yaw)*18;point.z+=Math_CosS(yaw)*18;ARPG_RelicCollision(gPlayState,&at,&ac,&strike,&surface,&point);}
    target->colChkInfo.damage=0;CollisionCheck_ApplyDamage(gPlayState,&gPlayState->colChkCtx,&ac,&surface);int result=target->colChkInfo.damage;target->colChkInfo=backup;return result;
}
void Clean(){Relics::Reset(gPlayState);Arsenal::Reset(gPlayState);GameInteractor_ExecuteOnPlayerUpdate();}
void Tick(){if(phase==99)return;try {
    ++frames;
    if(phase==0 && frames>60 && gGameState){CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0xFF;gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.linkAge=LINK_AGE_ADULT;
        for(const char* key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills"})CVarSetInteger(key,1);
        for(const char* key:{"gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.Tactics","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG","gOpenWindows.ARPGInventory"})CVarSetInteger(key,0);
        gSaveContext.entranceIndex=ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);phase=1;frames=0;return;}
    if(gPlayState && GameInteractor::IsPlayerInControl()){GET_PLAYER(gPlayState)->invincibilityTimer=40;gSaveContext.health=gSaveContext.healthCapacity;}
    if(phase==1 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()) {
        auto* player=GET_PLAYER(gPlayState);
        for(int slot=0;slot<16;++slot){auto& item=InventoryRules::Equipped(gARPGSave,slot);item={};item.slot=slot;}for(auto& item:gARPGSave.backpack)item={};
        for(auto& rank:gARPGSave.skillRanks)rank=0;
        for(int slot=4;slot<16;++slot){int item=AdventureItems::NativeItem(slot);INV_CONTENT(item)=item;}
        gSaveContext.inventory.equipment|=0x7777;gSaveContext.inventory.questItems|=1<<QUEST_SONG_STORMS;
        INV_CONTENT(ITEM_OCARINA_FAIRY)=ITEM_OCARINA_TIME;INV_CONTENT(ITEM_BOTTLE)=ITEM_BLUE_FIRE;
        Check(ItemModifierSystem::WeaponOwned(ARPG_SLOT_OCARINA) && ItemModifierSystem::WeaponOwned(ARPG_SLOT_BOTTLE),"Upgraded ocarina and filled bottles satisfy native ownership");
        foe=Spawn(ACTOR_EN_TEST,STALFOS_TYPE_2,180);lightFoe=Spawn(ACTOR_EN_WF,int16_t(0xFF00),210);Check(foe && foe->update && lightFoe && lightFoe->update,"Native Stalfos and Wolfos fixtures initialized");
        foe->update=lightFoe->update=Noop;foe->colChkInfo.health=lightFoe->colChkInfo.health=200;foe->bgCheckFlags|=BGCHECKFLAG_GROUND;lightFoe->bgCheckFlags|=BGCHECKFLAG_GROUND;
        for(int id=19;id<=30;++id){Equip(id);Check(InventoryRules::Valid(InventoryRules::Equipped(gARPGSave,Loot::Uniques[id].slot)),Loot::Uniques[id].name);}
        Clean();Equip(19);Arsenal::MutableStatus(foe)->frozen=20;Hit(&player->actor,DMG_HAMMER_SWING);
        Check(!Arsenal::GetStatus(foe).frozen && Relics::LiveCount()==1,"Frostbreaker consumes frozen setup and creates ice-fragment colliders");Hit(&player->actor,DMG_HAMMER_SWING);Check(Relics::LiveCount()==1,"Repeated hammer contacts cannot repeat the frost shatter");
        Clean();Equip(20);auto* arrow=Spawn(ACTOR_EN_ARROW,ARROW_NORMAL);arrow->update=Noop;Arsenal::MutableStatus(foe)->tethered=80;Hit(arrow,DMG_ARROW_NORMAL);
        Check(Relics::LiveCount()==1,"Stormthread arrow sends one lightning effect down a tether");Actor_Kill(arrow);
        Clean();Equip(21);auto* boom=reinterpret_cast<EnBoom*>(Spawn(ACTOR_EN_BOOM));ItemModifierSystem::CaptureWeapon(&boom->actor,ARPG_SLOT_BOOMERANG);Hit(&boom->actor,DMG_BOOMERANG);auto pos=boom->actor.world.pos;
        EnBoom_Update(&boom->actor,gPlayState);Check(Relics::LiveCount()==1 && Math_Vec3f_DistXYZ(&pos,&boom->actor.world.pos)>1 && boom->returnTimer==2,"Thornwheel enters a moving orbit and emits a native stun pulse");
        auto* ammo=Spawn(ACTOR_EN_ITEM00,ITEM00_ARROWS_SMALL,180);Check(ammo && ammo->update,"Native ammunition pickup fixture spawned");ammo->world.pos=boom->actor.world.pos;
        EnBoom_Update(&boom->actor,gPlayState);Check(Actor_WorldDistXZToActor(ammo,&player->actor)<1,"Thornwheel gathers nearby ammunition through native pickup actors");Actor_Kill(ammo);
        player->boomerangQuickRecall=true;ARPG_RelicBoomerangUpdate(&boom->actor);Check(boom->returnTimer==0 && boom->moveTo==&player->actor,"Quick recall ends Thornwheel orbit");player->boomerangQuickRecall=false;Actor_Kill(&boom->actor);
        Clean();Equip(22);auto* chu=reinterpret_cast<EnBomChu*>(Spawn(ACTOR_EN_BOM_CHU,0,30));ItemModifierSystem::CaptureWeapon(&chu->actor,ARPG_SLOT_BOMBCHU);chu->axisUp={0,1,0};chu->axisForwards={0,0,1};chu->axisLeft={1,0,0};player->focusActor=foe;ARPG_RelicSteerBombchu(&chu->actor);
        Check(chu->axisForwards.x>0 && std::abs(DOTXYZ(chu->axisForwards,chu->axisUp))<.001f,"Dodongo steering turns toward lock-on while staying on the native surface plane");
        EnBomChu_Explode(chu,gPlayState);Actor* bomb=nullptr;for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_EXPLOSIVE].head;a;a=a->next)if(a->id==ACTOR_EN_BOM && ItemModifierSystem::WeaponForActor(a) && ItemModifierSystem::WeaponForActor(a)->uniqueId==22)bomb=a;
        Check(bomb && ItemModifierSystem::WeaponForHit(bomb,DMG_EXPLOSIVE)->uniqueId==22,"A Bombchu explosion inherits its launcher relic snapshot");Arsenal::MutableStatus(foe)->burning=60;Hit(bomb,DMG_EXPLOSIVE);Check(Relics::LiveCount()==1,"Burning victim triggers a delayed Dodongo blast");Hit(bomb,DMG_EXPLOSIVE);Check(Relics::LiveCount()==1,"Each Bombchu triggers at most one extra blast");Actor_Kill(bomb);Actor_Kill(&chu->actor);
        Clean();Equip(23);AMMO(ITEM_STICK)=10;player->heldItemAction=PLAYER_IA_DEKU_STICK;player->meleeWeaponState=1;player->unk_85C=1;Arsenal::MutableStatus(foe)->burning=60;Hit(&player->actor,DMG_DEKU_STICK);
        Check(Relics::State().cinder==120 && ARPG_RelicPreserveStick(),"Cinderbranch kindles from a burning foe");func_80842AC4(gPlayState,player);Check(AMMO(ITEM_STICK)==10,"Kindling hit preserves the stick until its final swing");
        player->meleeWeaponState=0;GameInteractor_ExecuteOnPlayerUpdate();player->meleeWeaponState=1;Hit(&player->actor,DMG_DEKU_STICK);func_80842AC4(gPlayState,player);
        Check(AMMO(ITEM_STICK)==9 && Relics::LiveCount()==1 && !Relics::State().cinder,"Cinderbranch final hit consumes one stick and releases its fiery sweep");player->meleeWeaponState=0;player->heldItemAction=PLAYER_IA_NONE;
        Clean();Equip(24);player->stateFlags1|=PLAYER_STATE1_SHIELDING;GameInteractor_ExecuteOnPlayerUpdate();GameInteractor_ExecuteOnPlayerHoldUpShield();ARPG_RelicShieldBlock();Check(Relics::State().light==120,"A timed native shield block stores Dawnmirror light");
        arrow=Spawn(ACTOR_EN_ARROW,ARROW_NORMAL);arrow->parent=nullptr;player->unk_A73=1;EnArrow_Shoot(reinterpret_cast<EnArrow*>(arrow),gPlayState);Check(!Relics::State().light && Relics::LiveCount()==1,"Released arrow consumes Dawnmirror charge into a piercing beam");Actor_Kill(arrow);player->stateFlags1&=~PLAYER_STATE1_SHIELDING;
        Clean();Equip(25);gSaveContext.isMagicAcquired=1;gSaveContext.magic=48;gSaveContext.magicCapacity=48;gSaveContext.magicState=MAGIC_STATE_CONSUME_LENS;gPlayState->actorCtx.lensActive=true;player->focusActor=foe;
        arrow=Spawn(ACTOR_EN_ARROW,ARROW_NORMAL);arrow->update=Noop;Hit(arrow,DMG_ARROW_NORMAL,false,nullptr,true);
        Check(Arsenal::GetStatus(foe).frozen>=20,"An arrow hitting the Lens weak point staggers the locked foe");Arsenal::MutableStatus(foe)->frozen=0;Hit(arrow,DMG_ARROW_NORMAL,false,nullptr,true);Check(!Arsenal::GetStatus(foe).frozen,"Lens weak point recovery prevents repeated instant staggers");Actor_Kill(arrow);gPlayState->actorCtx.lensActive=false;gSaveContext.magicState=MAGIC_STATE_IDLE;
        Clean();Equip(26);player->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;for(int i=0;i<42;++i)GameInteractor_ExecuteOnPlayerUpdate();Check(Relics::State().anchor==40,"Standing still in Goron Anchor charges a grounded hammer slam");Hit(&player->actor,DMG_HAMMER_SWING);Check(!Relics::State().anchor,"Hammer contact spends the anchor charge once");
        Clean();Equip(27);player->hoverBootsTimer=19;player->actor.bgCheckFlags&=~BGCHECKFLAG_GROUND;arrow=Spawn(ACTOR_EN_ARROW,ARROW_NORMAL);arrow->parent=nullptr;player->unk_A73=1;EnArrow_Shoot(reinterpret_cast<EnArrow*>(arrow),gPlayState);Check(Relics::State().airborneShot && Relics::State().steady==8,"Arrow released during a hover arms Galecrest landing and steadies aim");
        player->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;GameInteractor_ExecuteOnPlayerUpdate();Check(!Relics::State().airborneShot && Relics::LiveCount()==1,"Landing after an airborne volley creates one gust");Actor_Kill(arrow);
        Clean();Equip(28);gPlayState->msgCtx.lastPlayedSong=OCARINA_SONG_STORMS;gPlayState->msgCtx.ocarinaAction=OCARINA_ACTION_FREE_PLAY_DONE;GameInteractor_ExecuteOnOcarinaSongAction();GameInteractor_ExecuteOnPlayerUpdate();Check(Relics::LiveCount()==1 && Relics::State().rainRecovery==200,"Learned Song of Storms creates a bounded rain patch through the song-completion hook");
        Equip(24);player->stateFlags1|=PLAYER_STATE1_SHIELDING;GameInteractor_ExecuteOnPlayerHoldUpShield();ARPG_RelicShieldBlock();
        arrow=Spawn(ACTOR_EN_ARROW,ARROW_NORMAL);arrow->world.rot={0,0,0};ARPG_RelicArrowReleased(arrow);Actor_Kill(arrow);player->stateFlags1&=~PLAYER_STATE1_SHIELDING;
        phase=2;frames=0;return;
    }
    if(phase==2 && frames>8){auto* player=GET_PLAYER(gPlayState);Check(Relics::Wet(foe)>0,"Rendered rain patch applies wet status to a nearby enemy");Equip(14);auto* seed=Spawn(ACTOR_EN_ARROW,ARROW_SEED);seed->update=Noop;Hit(seed,DMG_SLINGSHOT);Check(Arsenal::GetStatus(foe).frozen==40,"Winterbud freezes a wet foe for two seconds");Actor_Kill(seed);
        Equip(20);auto* arrow=Spawn(ACTOR_EN_ARROW,ARROW_NORMAL);arrow->update=Noop;Arsenal::MutableStatus(foe)->tethered=80;int live=Relics::LiveCount();Hit(arrow,DMG_ARROW_NORMAL);Check(Relics::LiveCount()>live+1,"Stormthread conducts additional arcs between wet enemies");Actor_Kill(arrow);
        Clean();Equip(29);Arsenal::MutableStatus(foe)->marked=160;Check(ARPG_RelicBottleReleased() && Relics::LiveCount()==1,"Releasing bottled blue fire creates a hunting wisp");
        Actor* wisp=nullptr;for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;a;a=a->next)if(a->update && a->id==ACTOR_EN_M_THUNDER)wisp=a;
        Check(wisp!=nullptr,"Native wisp actor is live");auto wispPosition=wisp->world.pos;auto beforeDistance=Math_Vec3f_DistXYZ(&wispPosition,&foe->focus.pos);wisp->update(wisp,gPlayState);
        Check(Math_Vec3f_DistXYZ(&wisp->world.pos,&foe->focus.pos)<beforeDistance,"Released wisp actually pursues the marked target");
        Clean();Equip(30);auto* hook=Spawn(ACTOR_ARMS_HOOK);ItemModifierSystem::CaptureWeapon(hook,ARPG_SLOT_HOOKSHOT);Hit(hook,DMG_HOOKSHOT);auto position=player->actor.world.pos;GameInteractor_ExecuteOnPlayerUpdate();Check(Math_Vec3f_DistXZ(&position,&player->actor.world.pos)>1,"Scarecrow reels Link toward a heavy susceptible foe over safe ground");
        auto lightPosition=lightFoe->world.pos;Hit(hook,DMG_HOOKSHOT,false,lightFoe);GameInteractor_ExecuteOnPlayerUpdate();Check(Math_Vec3f_DistXZ(&lightPosition,&lightFoe->world.pos)>1,"Scarecrow reels a light susceptible foe toward Link");Actor_Kill(hook);
        Clean();Equip(19);Arsenal::MutableStatus(foe)->frozen=20;Hit(&player->actor,DMG_HAMMER_SWING,true);Check(Relics::LiveCount()==0 && Arsenal::GetStatus(foe).frozen==20,"Native hard shields prevent relic contact procs");
        Equip(24);player->stateFlags1|=PLAYER_STATE1_SHIELDING;GameInteractor_ExecuteOnPlayerHoldUpShield();ARPG_RelicShieldBlock();auto pause=gPlayState->pauseCtx.state;gPlayState->pauseCtx.state=6;for(int i=0;i<130;++i)GameInteractor_ExecuteOnPlayerUpdate();Check(Relics::State().light==120,"Pause does not consume prepared light");gPlayState->pauseCtx.state=pause;player->stateFlags1&=~PLAYER_STATE1_SHIELDING;
        CVarSetInteger("gEnhancements.ARPG.Items",0);GameInteractor_ExecuteOnPlayerUpdate();Check(!Relics::State().light && !Relics::LiveCount() && !Relics::Wet(foe),"Items-off clears all transient relic effects and preparations");CVarSetInteger("gEnhancements.ARPG.Items",1);
        for(int id=19;id<=30;++id){Equip(id);InventoryRules::Collect(gARPGSave,InventoryRules::Equipped(gARPGSave,Loot::Uniques[id].slot));}
        gSaveContext.fileNum=0;auto before=SaveCodec::Encode(gARPGSave);SaveManager::Instance->SaveFile(0);SaveManager::Instance->ThreadPoolWait();for(auto& item:gARPGSave.weapons)item={};SaveManager::Instance->LoadFile(0);Check(SaveCodec::Encode(gARPGSave)==before,"Versioned SaveManager persists all twelve relics with campaign and acquisition data");gSaveContext.fileNum=0xFF;
        Check(draws>=8,"Native graphics frames render the relic rain effect");Check(collisionChecked,"Dawnmirror beam verified through native intersection and damage");Report("passed");phase=99;if(visual)ARPG::Inventory::Open();return;
    }
    if(frames>1200)throw std::runtime_error("Timed out at phase "+std::to_string(phase));
}catch(const std::exception& e){Report("failed",e.what());SPDLOG_ERROR("[Adventure Relics QA] {}",e.what());phase=99;}}
void Probe(void* ref){
    auto* actor=static_cast<Actor*>(ref);if(phase!=2 || collisionChecked || actor->id!=ACTOR_EN_M_THUNDER || Arsenal::IsFault(actor))return;
    try{auto& ctx=gPlayState->colChkCtx;
        for(int i=0;i<ctx.colATCount;++i){auto* at=ctx.colAT[i];if(at->actor!=actor || at->shape!=COLSHAPE_CYLINDER)continue;
            auto* volume=reinterpret_cast<ColliderCylinder*>(at);if(volume->info.toucher.dmgFlags!=DMG_ARROW_LIGHT)continue;
            Actor probe{};probe.id=ACTOR_EN_TEST;probe.category=ACTORCAT_ENEMY;probe.update=Noop;probe.colChkInfo.health=200;probe.colChkInfo.damageTable=foe->colChkInfo.damageTable;
            probe.world.pos={float(volume->dim.pos.x),float(volume->dim.pos.y),float(volume->dim.pos.z)};
            ColliderCylinder receiver{};ColliderCylinderInit init={{COLTYPE_HIT0,AT_NONE,AC_ON|AC_TYPE_PLAYER,OC1_NONE,OC2_TYPE_1,COLSHAPE_CYLINDER},
                {ELEMTYPE_UNK0,{0,0,0},{0xFFFFFFFF,0,0},TOUCH_NONE,BUMP_ON,OCELEM_NONE},{20,60,0,{0,0,0}}};
            Collider_InitCylinder(gPlayState,&receiver);Collider_SetCylinder(gPlayState,&receiver,&probe,&init);Collider_UpdateCylinder(&probe,&receiver);
            auto backup=ctx;ctx.colATCount=1;ctx.colAT[0]=at;ctx.colACCount=1;ctx.colAC[0]=&receiver.base;
            CollisionCheck_AT(gPlayState,&ctx);CollisionCheck_Damage(gPlayState,&ctx);bool hit=(receiver.base.acFlags&AC_HIT) && probe.colChkInfo.damage>0;
            Vec3f point=probe.world.pos;bool repeat=ARPG_RelicCollision(gPlayState,at,&receiver.base,&volume->info,&receiver.info,&point);
            probe.category=ACTORCAT_NPC;bool friendly=ARPG_RelicCollision(gPlayState,at,&receiver.base,&volume->info,&receiver.info,&point);
            ctx=backup;at->at=nullptr;at->atFlags&=~AT_HIT;volume->info.atHit=nullptr;volume->info.atHitInfo=nullptr;volume->info.toucherFlags&=~TOUCH_HIT;Collider_DestroyCylinder(gPlayState,&receiver);ObjectExtension::GetInstance().Free(&probe);
            Check(hit,"Piercing light collider intersects a native damage-table receiver");Check(!repeat,"Piercing beam cannot repeatedly hit another limb of the same foe");Check(!friendly,"Relic attacks refuse friendly NPC collision");collisionChecked=true;break;
        }
    }catch(const std::exception& e){Report("failed",e.what());phase=99;}
}
void Register(){const char* enabled=std::getenv("SOH_ARPG_ADVENTURE_RELICS_QA"),*home=std::getenv("SHIP_HOME");if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
    visual=std::getenv("SOH_ARPG_RELICS_UI");output=std::filesystem::path(home)/"adventure-relics-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorUpdate>(Probe);GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(phase==2)++draws;});}
static RegisterShipInitFunc init(Register);
}
