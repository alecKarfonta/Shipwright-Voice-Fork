#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/AffixData.h"
#include "soh/Enhancements/arpg/ARPGBridge.h"
#include "soh/Enhancements/arpg/ARPGSaveCodec.h"
#include "soh/Enhancements/arpg/AdventureItems.h"
#include "soh/Enhancements/arpg/InventorySystem.h"
#include "soh/Enhancements/arpg/ItemModifierSystem.h"
#include "soh/Enhancements/arpg/ArsenalSystem.h"
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
#include <set>
#include <cmath>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "overlays/actors/ovl_En_Test/z_en_test.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_Arms_Hook/z_arms_hook.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "overlays/actors/ovl_En_M_Fire1/z_en_m_fire1.h"
void Sram_InitDebugSave(void);
void ArmsHook_Wait(ArmsHook*,PlayState*);
void EnBom_Update(Actor*,PlayState*);
void EnMFire1_Update(Actor*,PlayState*);
void CollisionCheck_ApplyDamage(PlayState*,CollisionCheckContext*,Collider*,ColliderInfo*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,nodes=0,drawFrames=0;
bool visual=false;
Actor* foe=nullptr;Actor* chain=nullptr;
std::filesystem::path output;
nlohmann::json checks=nlohmann::json::array();
void Report(const char* status,const std::string& error="") {std::ofstream(output)<<nlohmann::json{{"status",status},{"checks",checks},{"error",error},{"native_echo_nodes",nodes},{"draw_frames",drawFrames}}.dump(2);}
void Check(bool ok,const std::string& text) {if(!ok)throw std::runtime_error(text);checks.push_back(text);SPDLOG_INFO("[Adventure Items QA] PASS: {}",text);Report("running");}
void Noop(Actor*,PlayState*){}
ARPGItemInstance Base(int slot) { ARPGItemInstance item{};item.slot=slot;item.baseEquip=1;item.itemLevel=35;return item; }
void Equip(int id) {int slot=Loot::Uniques[id].slot;auto item=Base(slot);Loot::IdentifyUnique(item,id);InventoryRules::Equipped(gARPGSave,slot)=item;}
Actor* Spawn(int id,int params=0,float offset=150) {auto p=GET_PLAYER(gPlayState)->actor.world.pos;return Actor_Spawn(&gPlayState->actorCtx,gPlayState,id,p.x+offset,p.y+5,p.z,0,0,0,params);}
struct Impact {int damage,effect;};
Impact Hit(Actor* source,uint32_t flags,bool hard=false,uint8_t alignment=AT_TYPE_PLAYER) {
    ++gPlayState->gameplayFrames;auto backup=foe->colChkInfo;
    Collider at{},ac{};ColliderInfo strike{},surface{};at.actor=source;at.atFlags=alignment;ac.actor=foe;ac.acFlags=AC_HIT|(hard?AC_HARD:0);
    strike.toucher.dmgFlags=flags;strike.toucher.damage=2;surface.acHit=&at;surface.acHitInfo=&strike;surface.bumperFlags=BUMP_HIT;
    foe->colChkInfo.damage=0;CollisionCheck_ApplyDamage(gPlayState,&gPlayState->colChkCtx,&ac,&surface);
    Impact result{foe->colChkInfo.damage,foe->colChkInfo.damageEffect};foe->colChkInfo=backup;return result;
}
void Tick() {
    if(phase==99)return;
    try {
        ++frames;
        if(phase==0 && frames>60 && gGameState) {
            CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0xFF;
            gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.linkAge=LINK_AGE_ADULT;
            for(const char* key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills"})CVarSetInteger(key,1);
            for(const char* key:{"gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.Tactics","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG"})CVarSetInteger(key,0);
            gSaveContext.entranceIndex=ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;
            gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);phase=1;frames=0;return;
        }
        if(gPlayState && GameInteractor::IsPlayerInControl()) {GET_PLAYER(gPlayState)->invincibilityTimer=40;gSaveContext.health=gSaveContext.healthCapacity;}
        if(phase==1 && frames>100 && gPlayState && GameInteractor::IsPlayerInControl()) {
            for(int slot=0;slot<ARPG_LOOT_SLOT_COUNT;++slot) {auto& item=InventoryRules::Equipped(gARPGSave,slot);item={};item.slot=slot;}for(auto& item:gARPGSave.backpack)item={};
            for(auto& rank:gARPGSave.skillRanks)rank=0;
            for(int slot=ARPG_SLOT_BOW;slot<ARPG_LOOT_SLOT_COUNT;++slot) {int item=AdventureItems::NativeItem(slot);INV_CONTENT(item)=item;Check(ItemModifierSystem::WeaponOwned(slot),std::string("Native ownership: ")+Loot::SlotNames[slot]);}
            foe=Spawn(ACTOR_EN_TEST,STALFOS_TYPE_2,260);Check(foe && foe->update,"Native Stalfos damage-table fixture initialized");foe->update=Noop;foe->colChkInfo.health=200;
            for(int slot=ARPG_SLOT_BOMBS;slot<=ARPG_SLOT_NUTS;++slot) {
                int item=AdventureItems::NativeItem(slot),saved=INV_CONTENT(item);INV_CONTENT(item)=ITEM_NONE;
                Check(!InventoryRules::Present(ItemModifierSystem::RollItemDrop(ARPG_TIER_BOSS,slot,35)),"Unowned adventure item cannot drop: "+std::string(Loot::SlotNames[slot]));INV_CONTENT(item)=saved;
                bool valid=true;for(int n=0;n<100;++n) {auto drop=ItemModifierSystem::RollItemDrop(ARPG_TIER_BOSS,slot,35);valid&=InventoryRules::Valid(drop) && drop.slot==slot;}
                Check(valid,"100 valid native loot rolls: "+std::string(Loot::SlotNames[slot]));
            }
            Equip(14);auto* seed=Spawn(ACTOR_EN_ARROW,ARROW_SEED);Check(seed && seed->update,"Seed actor captures its native launch roll");seed->update=Noop;
            Check(ARPG_AdventureSeedElement(seed)==2,"Winterbud selects the ice projectile visual");
            auto ice=Hit(seed,DMG_SLINGSHOT);Check(ice.damage>0 && ice.effect==(foe->colChkInfo.damageTable->table[12]>>4) && Arsenal::GetStatus(foe).frozen==20,"Native seed collision applies the native ice reaction and a one-second root");
            Equip(13);Check(ItemModifierSystem::WeaponForHit(seed,DMG_SLINGSHOT)->uniqueId==14,"An airborne seed keeps Winterbud after equipping Emberheart");
            auto* fire=Spawn(ACTOR_EN_ARROW,ARROW_SEED);fire->update=Noop;Arsenal::Reset(gPlayState);
            Check(ARPG_AdventureSeedElement(fire)==1 && Hit(fire,DMG_SLINGSHOT).damage>0 && Arsenal::GetStatus(foe).burning==30,"Emberheart uses a distinct fire visual and accepted fire contact");
            Arsenal::Reset(gPlayState);auto* table=foe->colChkInfo.damageTable;auto immune=*table;immune.table[12]=0;foe->colChkInfo.damageTable=&immune;
            Hit(seed,DMG_SLINGSHOT);Check(!Arsenal::GetStatus(foe).frozen,"Native elemental immunity refuses the Winterbud root");foe->colChkInfo.damageTable=table;
            Arsenal::Reset(gPlayState);Hit(seed,DMG_SLINGSHOT,true);Hit(seed,DMG_SLINGSHOT,false,AT_TYPE_ENEMY);
            Check(!Arsenal::GetStatus(foe).frozen,"Hard shields and enemy-aligned projectiles cannot apply seed roots");
            Hit(seed,DMG_SLINGSHOT);auto pause=gPlayState->pauseCtx.state;gPlayState->pauseCtx.state=6;
            for(int i=0;i<30;++i)GameInteractor_ExecuteOnPlayerUpdate();Check(Arsenal::GetStatus(foe).frozen==20,"Pause freezes seed status duration");gPlayState->pauseCtx.state=pause;
            CVarSetInteger("gEnhancements.ARPG.Items",0);GameInteractor_ExecuteOnPlayerUpdate();Check(ARPG_AdventureSeedElement(seed)==0 && Arsenal::MovementMultiplier(foe)==1,"Items-off removes seed effects and clears their statuses");CVarSetInteger("gEnhancements.ARPG.Items",1);
            Actor_Kill(seed);Actor_Kill(fire);
            Equip(15);auto* hook=reinterpret_cast<ArmsHook*>(Spawn(ACTOR_ARMS_HOOK));Check(hook!=nullptr,"Native hookshot actor initialized");
            auto* player=GET_PLAYER(gPlayState);auto action=player->heldItemAction;player->heldItemAction=PLAYER_IA_HOOKSHOT;hook->actor.parent=nullptr;ArmsHook_Wait(hook,gPlayState);
            Check(hook->timer==23,"Skyvine extends the real Hookshot launch timer from 13 to 23");
            player->heldItemAction=PLAYER_IA_LONGSHOT;hook->actor.parent=nullptr;ArmsHook_Wait(hook,gPlayState);Check(hook->timer==46,"Skyvine extends Longshot from 26 to 46");
            INV_CONTENT(ITEM_HOOKSHOT)=ITEM_LONGSHOT;Check(ItemModifierSystem::WeaponOwned(ARPG_SLOT_HOOKSHOT),"Longshot ownership preserves the hookshot loot slot");
            Equip(16);ARPG_AdventureLaunch(&hook->actor,ARPG_SLOT_HOOKSHOT);Arsenal::Reset(gPlayState);Check(Hit(&hook->actor,DMG_HOOKSHOT).damage==0 && Arsenal::GetStatus(foe).tethered==120 && Arsenal::GetStatus(foe).tetherBonus==1.25f,"Ironroot retains a zero-damage native stun and extends tether to six seconds");
            Actor bombSource{};bombSource.id=ACTOR_EN_BOM;int finisher=Hit(&bombSource,DMG_EXPLOSIVE).damage;int plain=Hit(&bombSource,DMG_EXPLOSIVE).damage;
            Check(finisher>plain && !Arsenal::GetStatus(foe).tethered,"Ironroot amplifies the next bomb once");ObjectExtension::GetInstance().Free(&bombSource);
            player->heldItemAction=action;hook->grabbed=nullptr;Actor_Kill(&hook->actor);
            Equip(17);auto* boom=Spawn(ACTOR_EN_BOOM);ARPG_AdventureLaunch(boom,ARPG_SLOT_BOOMERANG);boom->update=Noop;Arsenal::Reset(gPlayState);
            Check(ARPG_AdventureTravel(boom,20)==27 && Hit(boom,DMG_BOOMERANG).damage==0 && Arsenal::GetStatus(foe).marked==120 && Arsenal::GetStatus(foe).markBonus==1.25f,"Gale Crescent extends travel and prepares a stronger ranged finisher without inventing stun damage");Actor_Kill(boom);
            Equip(18);auto* nut=Spawn(ACTOR_EN_ARROW,ARROW_NUT);nut->update=Noop;auto* flash=Spawn(ACTOR_EN_M_FIRE1);ARPG_AdventureCopy(flash,nut);EnMFire1_Update(flash,gPlayState);flash->update=Noop;Arsenal::Reset(gPlayState);
            Check(reinterpret_cast<EnMFire1*>(flash)->collider.dim.radius==280,"Deku Starburst expands the actual flash collider to 280");
            Check(Hit(flash,DMG_DEKU_NUT).damage==0 && Arsenal::GetStatus(foe).marked==108,"Deku Starburst creates a timed ranged setup through a zero-damage nut stun");Actor_Kill(nut);Actor_Kill(flash);
            Equip(11);auto* wide=reinterpret_cast<EnBom*>(Spawn(ACTOR_EN_BOM));ARPG_AdventureLaunch(&wide->actor,ARPG_SLOT_BOMBS);wide->timer=1;EnBom_Update(&wide->actor,gPlayState);
            Equip(12);Check(std::abs(ARPG_AdventureBlastScale(&wide->actor)-1.6f)<.001f,"A lit Powderkeg retains its radius after a satchel swap");
            EnBom_Update(&wide->actor,gPlayState);EnBom_Update(&wide->actor,gPlayState);
            Check(wide->explosionCollider.elements[0].dim.worldSphere.radius==25,"Wideburst scales the native bomb collision volume without accumulating integer rounding");
            CVarSetInteger("gEnhancements.ARPG.Items",0);EnBom_Update(&wide->actor,gPlayState);
            Check(wide->explosionCollider.elements[0].dim.worldSphere.radius==24,"Disabling items restores the native radius even during an active explosion");CVarSetInteger("gEnhancements.ARPG.Items",1);Actor_Kill(&wide->actor);
            auto* normal=Spawn(ACTOR_EN_BOM);Check(ARPG_AdventureBlastScale(normal)==1,"Unattributed world bombs retain their native radius");Actor_Kill(normal);
            InventoryRules::Equipped(gARPGSave,ARPG_SLOT_BOMBS)={};auto* empty=Spawn(ACTOR_EN_BOM);ARPG_AdventureLaunch(empty,ARPG_SLOT_BOMBS);Equip(11);
            Check(ARPG_AdventureBlastScale(empty)==1 && !ItemModifierSystem::WeaponForHit(empty,DMG_EXPLOSIVE),"An already-lit plain bomb cannot acquire a newly equipped satchel");Actor_Kill(empty);Equip(12);
            auto* chorus=reinterpret_cast<EnBom*>(Spawn(ACTOR_EN_BOM));ARPG_AdventureLaunch(&chorus->actor,ARPG_SLOT_BOMBS);chorus->timer=1;EnBom_Update(&chorus->actor,gPlayState);
            Check(Arsenal::LiveFaults(gPlayState)==1,"Bombflower detonation creates one bounded three-node echo actor");ARPG_AdventureDetonate(&chorus->actor);Check(Arsenal::LiveFaults(gPlayState)==1,"Repeated detonation cannot duplicate a chorus");
            for(auto* actor=gPlayState->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;actor;actor=actor->next)if(Arsenal::IsFault(actor))chain=actor;
            phase=2;frames=0;return;
        }
        if(phase==2 && frames>50) {
            Check(nodes==3 && !Arsenal::LiveFaults(gPlayState),"All three native echo nodes fire and the effect actor expires");
            for(int id=11;id<=18;++id) {Equip(id);auto item=InventoryRules::Equipped(gARPGSave,Loot::Uniques[id].slot);InventoryRules::Collect(gARPGSave,item);}
            gSaveContext.fileNum=0;auto before=SaveCodec::Encode(gARPGSave);SaveManager::Instance->SaveFile(0);SaveManager::Instance->ThreadPoolWait();for(auto& item:gARPGSave.weapons)item={};SaveManager::Instance->LoadFile(0);
            if(SaveCodec::Encode(gARPGSave)!=before)std::ofstream(output.parent_path()/"adventure-save-comparison.json")<<nlohmann::json{{"saved",before},{"loaded",SaveCodec::Encode(gARPGSave)}}.dump(2);
            Check(SaveCodec::Encode(gARPGSave)==before,"Versioned native SaveManager round trip preserves utility loadouts and backpack variants");gSaveContext.fileNum=0xFF;
            Check(drawFrames>30,"Adventure effects render through native frames");Report("passed");phase=visual?98:99;if(visual)ARPG::Inventory::Open();return;
        }
        if(phase==98)return;
        if(frames>1000)throw std::runtime_error("Timed out at phase "+std::to_string(phase));
    } catch(const std::exception& e) {Report("failed",e.what());SPDLOG_ERROR("[Adventure Items QA] {}",e.what());phase=99;}
}
void Probe(void* ref) {
    auto* source=static_cast<Actor*>(ref);if(phase!=2 || source!=chain)return;
    try {
        static std::set<int> tested;
        auto& ctx=gPlayState->colChkCtx;
        for(int i=0;i<ctx.colATCount;++i) {
            auto* at=ctx.colAT[i];if(at->actor!=source || at->shape!=COLSHAPE_CYLINDER)continue;
            auto* volume=reinterpret_cast<ColliderCylinder*>(at);int key=volume->dim.pos.x*10000+volume->dim.pos.z;
            if(tested.contains(key))continue;
            Actor probe{};probe.id=ACTOR_EN_TEST;probe.category=ACTORCAT_ENEMY;probe.update=Noop;probe.colChkInfo.health=200;
            probe.world.pos={float(volume->dim.pos.x),float(volume->dim.pos.y),float(volume->dim.pos.z)};
            ColliderCylinder receiver{};ColliderCylinderInit init={{COLTYPE_HIT0,AT_NONE,AC_ON|AC_TYPE_PLAYER,OC1_NONE,OC2_TYPE_1,COLSHAPE_CYLINDER},
                {ELEMTYPE_UNK0,{0,0,0},{0xFFFFFFFF,0,0},TOUCH_NONE,BUMP_ON,OCELEM_NONE},{20,60,0,{0,0,0}}};
            Collider_InitCylinder(gPlayState,&receiver);Collider_SetCylinder(gPlayState,&receiver,&probe,&init);Collider_UpdateCylinder(&probe,&receiver);
            auto saved=ctx;ctx.colATCount=1;ctx.colAT[0]=at;ctx.colACCount=1;ctx.colAC[0]=&receiver.base;
            CollisionCheck_AT(gPlayState,&ctx);CollisionCheck_Damage(gPlayState,&ctx);
            Check((receiver.base.acFlags&AC_HIT) && probe.colChkInfo.damage>0,"Echo node "+std::to_string(nodes+1)+" intersects native collision and damage");
            Check(!Arsenal::Collision(gPlayState,at,&receiver.base),"An echo victim cannot take repeated limb or sibling-node hits");
            ObjectExtension::GetInstance().Free(&probe);probe.category=ACTORCAT_NPC;Check(!Arsenal::Collision(gPlayState,at,&receiver.base),"Echo collision excludes friendly NPCs");
            ctx=saved;at->at=nullptr;at->atFlags&=~AT_HIT;volume->info.atHit=nullptr;volume->info.atHitInfo=nullptr;volume->info.toucherFlags&=~TOUCH_HIT;
            Collider_DestroyCylinder(gPlayState,&receiver);ObjectExtension::GetInstance().Free(&probe);tested.insert(key);++nodes;
        }
    }catch(const std::exception& e){Report("failed",e.what());phase=99;}
}
void Register() {
    const char* enabled=std::getenv("SOH_ARPG_ADVENTURE_QA"),*home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
    visual=std::getenv("SOH_ARPG_ADVENTURE_UI") && std::string(std::getenv("SOH_ARPG_ADVENTURE_UI"))=="1";
    output=std::filesystem::path(home)/"adventure-items-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorUpdate>(Probe);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(phase==2)++drawFrames;});
}
static RegisterShipInitFunc init(Register);
}
