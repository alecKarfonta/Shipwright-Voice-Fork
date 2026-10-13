#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGBridge.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/ARPGSaveCodec.h"
#include "soh/Enhancements/arpg/InventorySystem.h"
#include "soh/Enhancements/arpg/InventoryRules.h"
#include "soh/Enhancements/arpg/ItemModifierSystem.h"
#include "soh/Enhancements/arpg/ArsenalSystem.h"
#include "soh/Enhancements/arpg/GearVisuals.h"
#include "soh/Enhancements/arpg/AdventureJournal.h"
#include "soh/Enhancements/arpg/AffixData.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/ShipInit.hpp"
#include "soh/SaveManager.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "overlays/actors/ovl_En_Test/z_en_test.h"
void Sram_InitDebugSave(void);
void CollisionCheck_ApplyDamage(PlayState*,CollisionCheckContext*,Collider*,ColliderInfo*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,drawFrames=0;bool visual=false;Actor* foe=nullptr;Actor* shot=nullptr;
std::filesystem::path output;nlohmann::json checks=nlohmann::json::array();
void Write(const char* status,const std::string& error="") {std::ofstream(output)<<nlohmann::json{{"status",status},{"checks",checks},{"error",error},{"draw_frames",drawFrames}}.dump(2);}
void Check(bool ok,const char* label) {if(!ok)throw std::runtime_error(label);checks.push_back(label);SPDLOG_INFO("[Weapon loot QA] PASS: {}",label);Write("running");}
ARPGItemInstance Base(int slot){ARPGItemInstance i{};i.slot=slot;i.baseEquip=1;i.itemLevel=35;return i;}
void Noop(Actor*,PlayState*){}
int Hit(Actor* source,uint32_t flags,bool hard=false,uint8_t alignment=AT_TYPE_PLAYER) {
    ++gPlayState->gameplayFrames;auto backup=foe->colChkInfo;Collider a{},b{};ColliderInfo strike{},surface{};
    a.actor=source;a.atFlags=alignment;b.actor=foe;b.acFlags=AC_HIT|(hard?AC_HARD:0);
    strike.toucher.dmgFlags=flags;strike.toucher.damage=2;surface.acHit=&a;surface.acHitInfo=&strike;surface.bumperFlags=BUMP_HIT;
    foe->colChkInfo.damage=0;CollisionCheck_ApplyDamage(gPlayState,&gPlayState->colChkCtx,&b,&surface);
    int damage=foe->colChkInfo.damage;foe->colChkInfo=backup;return damage;
}
void Tick() {
    if(phase==99)return;
    try {
        ++frames;
        if(phase==0 && frames>60 && gGameState) {
            CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();
            gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_ADULT;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;
            for(const char* k:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.Items","gEnhancements.ARPG.RemasteredGear"})CVarSetInteger(k,1);
            for(const char* k:{"gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.Tactics","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG"})CVarSetInteger(k,0);
            gSaveContext.entranceIndex=ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;
            gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);phase=1;frames=0;return;
        }
        if(gPlayState && GameInteractor::IsPlayerInControl()){GET_PLAYER(gPlayState)->invincibilityTimer=40;GET_PLAYER(gPlayState)->actor.speedXZ=0;}
        if(phase==1 && frames>100 && GameInteractor::IsPlayerInControl()) {
            auto* player=GET_PLAYER(gPlayState);auto p=player->actor.world.pos;player->actor.shape.rot.y=0;
            for(int i=0;i<ARPG_SLOT_COUNT_;++i){gARPGSave.equipment[i]={};gARPGSave.equipment[i].slot=i;}for(int i=0;i<ARPG_WEAPON_SLOT_COUNT;++i){gARPGSave.weapons[i]={};gARPGSave.weapons[i].slot=i+ARPG_SLOT_COUNT_;}for(auto& i:gARPGSave.backpack)i={};for(auto& i:gARPGSave.skillRanks)i=0;
            foe=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_TEST,p.x,p.y,p.z+90,0,0x8000,0,STALFOS_TYPE_2);Check(foe && foe->update,"Native Stalfos fixture initializes");foe->update=Noop;foe->colChkInfo.health=200;
            Check(ItemModifierSystem::WeaponOwned(ARPG_SLOT_BOW) && ItemModifierSystem::WeaponOwned(ARPG_SLOT_HAMMER),"Debug adventure owns both native base weapons");
            auto bow=Base(ARPG_SLOT_BOW);bow.rarity=ARPG_RARITY_RARE;bow.modCount=1;bow.mods[0]={ARPG_MOD_ARCHERY,100};
            auto hammer=Base(ARPG_SLOT_HAMMER);hammer.rarity=ARPG_RARITY_RARE;hammer.modCount=1;hammer.mods[0]={ARPG_MOD_BRUTAL,100};
            Check(ItemModifierSystem::GiveItemDrop(bow) && ItemModifierSystem::GiveItemDrop(hammer),"Weapon loot is collected into the shared backpack");
            auto native=gSaveContext.equips;Check(ARPG::Inventory::Equip(0) && ARPG::Inventory::Equip(1),"Native inventory accepts owned bow and hammer rolls");
            Check(gSaveContext.equips.equipment==native.equipment && gSaveContext.equips.buttonItems[0]==native.buttonItems[0],"Weapon gear does not overwrite native sword/armor or B-button");
            Check(ItemModifierSystem::GetModPower(ARPG_MOD_ARCHERY)==0 && ItemModifierSystem::GetModPower(ARPG_MOD_BRUTAL)==0,"Stored weapon bonuses do not become global stats");
            auto age=gSaveContext.linkAge;gSaveContext.linkAge=LINK_AGE_CHILD;Check(!ARPG::Inventory::EquipRestriction(bow).empty() && !ARPG::Inventory::EquipRestriction(hammer).empty(),"Both new loot slots preserve native child-age restrictions");gSaveContext.linkAge=age;
            auto owned=INV_CONTENT(ITEM_BOW);INV_CONTENT(ITEM_BOW)=ITEM_NONE;Check(!ARPG::Inventory::EquipRestriction(bow).empty(),"A rolled bow cannot grant a missing quest weapon");INV_CONTENT(ITEM_BOW)=owned;
            Actor arrow{},seed{},spell{};arrow.id=seed.id=ACTOR_EN_ARROW;spell.id=ACTOR_EN_M_THUNDER;
            CVarSetInteger("gEnhancements.ARPG.Items",0);int normalBow=Hit(&arrow,DMG_ARROW_NORMAL),normalHammer=Hit(&player->actor,DMG_HAMMER_SWING);
            CVarSetInteger("gEnhancements.ARPG.Items",1);Check(Hit(&arrow,DMG_ARROW_NORMAL)>normalBow,"Bow roll scales native arrow damage");
            Check(Hit(&player->actor,DMG_HAMMER_SWING)>=normalHammer*2,"Hammer roll scales native hammer damage");
            bool crit=false;Check(ItemModifierSystem::WeaponDamageMultiplier(foe,&player->actor,DMG_SWORD,&crit)==1 && ItemModifierSystem::WeaponDamageMultiplier(foe,&seed,DMG_SLINGSHOT,&crit)==1 && ItemModifierSystem::WeaponDamageMultiplier(foe,&spell,DMG_HAMMER_SWING,&crit)==1,"Bow and hammer rolls cannot leak to swords, seeds or skill actors");
            Check(Hit(&arrow,DMG_ARROW_NORMAL,true)==0,"Hard shields continue to block rolled weapons");
            Actor boss{};boss.id=ACTOR_BOSS_DODONGO;boss.category=ACTORCAT_BOSS;
            Check(ARPG_BossWeaponDamageWithWeapon(&boss,&player->actor,DMG_HAMMER_SWING,2)==8,"The vulnerable-boss bridge composes hammer weakness and rolled damage");
            auto bossRandom=gARPGSave.rngState;
            Check(ARPG_BossWeaponDamageWithWeapon(&boss,&player->actor,DMG_HAMMER_SWING,2)==8 && bossRandom==gARPGSave.rngState,"Manual boss health resolution reuses the collision roll without spending RNG twice");
            auto plainHammer=gARPGSave.weapons[1];gARPGSave.weapons[1].modCount=2;gARPGSave.weapons[1].mods[1]={ARPG_MOD_KEEN,75};
            bool critCacheValid=true,sawCritical=false,sawNormal=false;
            for(int attempt=0;attempt<32;++attempt){
                ++gPlayState->gameplayFrames;auto before=gARPGSave.rngState;bool ownCritical=false;
                float own=ItemModifierSystem::WeaponDamageMultiplier(&boss,&player->actor,DMG_HAMMER_SWING,&ownCritical);
                auto after=gARPGSave.rngState;gARPGSave.rngState=before;bool globalCritical=true;
                float combined=ItemModifierSystem::WeaponDamageMultiplier(&boss,&player->actor,DMG_HAMMER_SWING,&globalCritical);
                critCacheValid&=combined==2.f && ItemModifierSystem::BossWeaponMultiplier(&boss,&player->actor,DMG_HAMMER_SWING)==own && after==gARPGSave.rngState;
                sawCritical|=ownCritical;sawNormal|=!ownCritical;
            }
            Check(critCacheValid && sawCritical && sawNormal,"Boss phase damage retains its weapon critical without doubling global criticals or rerolling RNG");
            gARPGSave.weapons[1]=plainHammer;
            ObjectExtension::GetInstance().Free(&boss);
            auto random=gARPGSave.rngState;CVarSetInteger("gEnhancements.ARPG.Items",0);Check(ItemModifierSystem::WeaponDamageMultiplier(foe,&arrow,DMG_ARROW_NORMAL,&crit)==1 && gARPGSave.rngState==random,"Items-off removes weapon rolls without spending combat RNG");CVarSetInteger("gEnhancements.ARPG.Items",1);
            shot=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_ARROW,p.x,p.y+50,p.z,0,0,0,ARROW_NORMAL);Check(shot && shot->update,"Native arrow actor is spawned with a weapon snapshot");shot->update=Noop;
            gARPGSave.weapons[0].mods[0].power=10;Check(ItemModifierSystem::WeaponForHit(shot,DMG_ARROW_NORMAL)->mods[0].power==100,"An arrow retains its launch roll after a weapon swap");
            auto unique=Base(ARPG_SLOT_BOW);Loot::IdentifyUnique(unique,9);unique.modCount=0;for(auto& mod:unique.mods)mod={};gARPGSave.weapons[0]=unique;Arsenal::Reset(gPlayState);
            Check(Hit(&arrow,DMG_ARROW_NORMAL)>0 && Arsenal::GetStatus(foe).marked==80,"Windpiercer marks an unmarked enemy on a real native arrow hit");
            Check(Hit(&arrow,DMG_ARROW_NORMAL)>0 && Arsenal::GetStatus(foe).marked==0,"Its next arrow consumes the opening without immediately rearming it");
            Arsenal::Reset(gPlayState);Hit(&arrow,DMG_ARROW_NORMAL,true);Check(!Arsenal::GetStatus(foe).marked,"Windpiercer cannot mark through a hard shield");
            Hit(&arrow,DMG_ARROW_NORMAL,false,AT_TYPE_ENEMY);Check(!Arsenal::GetStatus(foe).marked,"Enemy-aligned shots cannot invoke the bow's unique power");
            unique=Base(ARPG_SLOT_HAMMER);Loot::IdentifyUnique(unique,10);gARPGSave.weapons[1]=unique;Arsenal::Reset(gPlayState);player->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;
            Hit(&player->actor,DMG_HAMMER_SWING);Check(Arsenal::LiveFaults(gPlayState)==1,"Mountainwake launches its native ground-supported fault line");
            Actor* fault=nullptr;for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;a;a=a->next)if(Arsenal::IsFault(a) && a->update){fault=a;break;}
            Check(fault && Arsenal::OnContact(foe,fault,DMG_HAMMER_SWING,AT_TYPE_PLAYER,2)==1.5f,"Mountainwake snapshots full secondary-hit damage into its fault actor");
            for(int i=0;i<10;++i)GameInteractor_ExecuteOnPlayerUpdate();Hit(&player->actor,DMG_HAMMER_SWING);Check(Arsenal::LiveFaults(gPlayState)==2,"Mountainwake recovers after half a second");
            gSaveContext.fileNum=0;auto saved=SaveCodec::Encode(gARPGSave);SaveManager::Instance->SaveFile(0);SaveManager::Instance->ThreadPoolWait();for(auto& i:gARPGSave.weapons)i={};SaveManager::Instance->LoadFile(0);
            std::ofstream(output.parent_path()/"weapon-save-comparison.json")<<nlohmann::json{{"saved",saved},{"loaded",SaveCodec::Encode(gARPGSave)}}.dump(2);
            Check(SaveCodec::Encode(gARPGSave)==saved,"Version-ten asynchronous SaveManager round trip preserves both weapon rolls");gSaveContext.fileNum=0xFF;
            for(int slot=ARPG_SLOT_BOW;slot<=ARPG_SLOT_HAMMER;++slot) {
                bool valid=true;for(int i=0;i<50;++i){auto drop=ItemModifierSystem::RollItemDrop(ARPG_TIER_BOSS,slot,35);valid&=InventoryRules::Valid(drop) && drop.slot==slot;}Check(valid,slot==ARPG_SLOT_BOW?"Fifty native bow drops preserve slot and catalog validity":"Fifty native hammer drops preserve slot and catalog validity");
                auto item=Base(slot);Loot::IdentifyUnique(item,slot==ARPG_SLOT_BOW?9:10);Actor source{};source.world.pos=p;source.world.pos.x+=(slot==ARPG_SLOT_BOW?-80:80);source.world.pos.z+=100;ItemModifierSystem::SpawnItemDrop(&source,item);
            }
            Actor* pickup=nullptr;for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_MISC].head;a;a=a->next)if(a->id==ACTOR_EN_ITEM00 && a->update){pickup=a;break;}
            Check(pickup!=nullptr,"A weapon drop exists as a physical native collectible");
            auto bag=gARPGSave;for(auto& cell:gARPGSave.backpack)cell=Base(ARPG_SLOT_BOW);
            auto count=gARPGSave.itemsFound;GameInteractor_Should(VB_GIVE_ITEM_FROM_ITEM_00,true,reinterpret_cast<EnItem00*>(pickup));
            Check(pickup->update && gARPGSave.itemsFound==count,"A full backpack keeps weapon loot on the ground");
            gARPGSave.backpack[3]={};GameInteractor_Should(VB_GIVE_ITEM_FROM_ITEM_00,true,reinterpret_cast<EnItem00*>(pickup));
            Check(!pickup->update && gARPGSave.itemsFound==count+1 && Loot::WeaponSlot(gARPGSave.backpack[3].slot),"Making room collects one weapon and consumes its physical drop");
            gARPGSave=bag;
            auto pauseState=gPlayState->pauseCtx.state;gPlayState->pauseCtx.state=6;
            Check(AdventureJournal::AssignItem(SLOT_BOW,0) && AdventureJournal::AssignItem(SLOT_HAMMER,1),"Rolled weapons retain native C-button assignment");gPlayState->pauseCtx.state=pauseState;
            for(auto& i:gARPGSave.backpack)i={};InventoryRules::Collect(gARPGSave,Base(ARPG_SLOT_BOW));InventoryRules::Collect(gARPGSave,Base(ARPG_SLOT_HAMMER));
            phase=2;frames=0;return;
        }
        if(phase==2 && frames>70) {Check(drawFrames>=60,"New pickup and held-weapon models render for sixty native frames");Write("passed");phase=visual?98:99;if(visual)ARPG::Inventory::Open();return;}
        if(phase==98){gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;return;}
        if(frames>1500)throw std::runtime_error("Native weapon QA timed out");
    }catch(const std::exception& e){Write("failed",e.what());phase=99;}
}
void Register() {
    const char* run=std::getenv("SOH_ARPG_WEAPON_QA"),*home=std::getenv("SHIP_HOME");
    if(!run || std::string(run)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
    visual=std::getenv("SOH_ARPG_WEAPON_UI")!=nullptr;output=std::filesystem::path(home)/"weapon-loot-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorUpdate>([](void* ref){
        auto* actor=static_cast<Actor*>(ref);if((phase!=2 && !(phase==98 && visual)) || actor->id!=ACTOR_PLAYER)return;
        auto* player=reinterpret_cast<Player*>(actor);
        bool showBow=phase==2?frames<35:(gPlayState->gameplayFrames/400)%2==0;
        if(showBow){player->leftHandType=PLAYER_MODELTYPE_LH_CLOSED;player->rightHandType=PLAYER_MODELTYPE_RH_BOW_SLINGSHOT;player->heldItemAction=PLAYER_IA_BOW;player->itemAction=PLAYER_IA_BOW;}
        else {player->rightHandType=PLAYER_MODELTYPE_RH_CLOSED;player->leftHandType=PLAYER_MODELTYPE_LH_HAMMER;player->heldItemAction=PLAYER_IA_HAMMER;player->itemAction=PLAYER_IA_HAMMER;}
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(phase==2)++drawFrames;});
}
static RegisterShipInitFunc init(Register);
}
