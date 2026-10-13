#include "soh/Enhancements/arpg/AdventureJournal.h"
#include "soh/Enhancements/arpg/InventorySystem.h"
#include "soh/Enhancements/arpg/InventoryRules.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/AdventureItems.h"
#include "soh/Enhancements/arpg/ARPGBridge.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/SaveManager.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/libultraship.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <cstring>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
extern PlayState* gPlayState;
extern GameState* gGameState;
void Sram_InitDebugSave(void);
}
namespace {
int phase=0, frames=0;
uint32_t pausedFrames=0;
std::filesystem::path resultPath;
nlohmann::json checks=nlohmann::json::array();
void Check(bool ok,const char* message) { if(!ok) { throw std::runtime_error(message); } checks.push_back(message); }
void Result(const char* status,const std::string& error="") {
    std::ofstream(resultPath)<<nlohmann::json{{"status",status},{"checks",checks},{"error",error}}.dump(2); phase=99;
}
void Run() {
    if(phase==99) { return; }
    try {
        ++frames;
        if(phase==0 && frames>60 && gGameState) {
            CVarSetInteger(CVAR_DEVELOPER_TOOLS("DebugSaveFileMode"),2);
            gSaveContext.fileNum=0xFE; Sram_InitDebugSave();
            gSaveContext.gameMode=GAMEMODE_NORMAL; gSaveContext.fileNum=0xFF;
            gSaveContext.linkAge=LINK_AGE_ADULT; gSaveContext.cutsceneIndex=0; gSaveContext.sceneLayer=0;
            gSaveContext.entranceIndex=ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN; gSaveContext.respawnFlag=0;
            gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;
            gGameState->running=false; SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"),1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Items"),1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"),0);
            phase=1; frames=0; return;
        }
        if(phase==1 && frames>60 && GameInteractor::IsPlayerInControl()) {
            ARPG::Inventory::Open(); phase=2; frames=0; return;
        }
        if(phase==2 && gPlayState && gPlayState->pauseCtx.state==6) {
            using namespace ARPG::AdventureJournal;
            Check(ARPG::Inventory::IsOpen() && ARPG_CustomPauseActive(),"Native pause opens exactly one journal");
            Check(Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GamepadNavigationEnabled(),"Journal enables controller navigation");
            Check(!AssignItem(-1,0) && !AssignItem(24,0) && !AssignItem(0,7),"Assignment rejects invalid slots and buttons");
            CVarSetInteger(CVAR_ENHANCEMENT("DpadEquips"),0);
            Check(!AssignItem(SLOT_BOW,3),"Disabled D-pad slots cannot be assigned");
            gSaveContext.inventory.items[SLOT_BOW]=ITEM_BOW;
            gSaveContext.inventory.items[SLOT_HOOKSHOT]=ITEM_HOOKSHOT;
            gSaveContext.inventory.items[SLOT_ARROW_FIRE]=ITEM_ARROW_FIRE;
            for(int i=0;i<7;++i) { gSaveContext.equips.cButtonSlots[i]=SLOT_NONE; gSaveContext.equips.buttonItems[i+1]=ITEM_NONE; }
            Check(AssignItem(SLOT_BOW,0) && AssignItem(SLOT_HOOKSHOT,1),"Native items assign to C-buttons");
            Check(AssignItem(SLOT_BOW,1) && gSaveContext.equips.buttonItems[1]==ITEM_HOOKSHOT && gSaveContext.equips.buttonItems[2]==ITEM_BOW,"Moving an assigned item swaps the displaced item");
            CVarSetInteger(CVAR_ENHANCEMENT("SeparateArrows"),0);
            Check(AssignItem(SLOT_ARROW_FIRE,2) && gSaveContext.equips.buttonItems[3]==ITEM_BOW_ARROW_FIRE && gSaveContext.equips.cButtonSlots[2]==SLOT_BOW,"Elemental arrows use the native combined bow slot");
            CVarSetInteger(CVAR_ENHANCEMENT("SeparateArrows"),1);
            Check(AssignItem(SLOT_ARROW_FIRE,2) && gSaveContext.equips.cButtonSlots[2]==SLOT_ARROW_FIRE,"Separate-arrows setting preserves its own slot");
            CVarSetInteger(CVAR_ENHANCEMENT("DpadEquips"),1);
            Check(AssignItem(SLOT_HOOKSHOT,6) && gSaveContext.equips.buttonItems[7]==ITEM_HOOKSHOT,"D-pad assignment updates the native extended loadout");
            auto age=gSaveContext.linkAge; gSaveContext.linkAge=LINK_AGE_CHILD;
            Check(!AssignItem(SLOT_HOOKSHOT,0),"Child cannot assign adult-only equipment"); gSaveContext.linkAge=age;
            Check(!EquipBase(4,1) && !EquipBase(0,0),"Base gear rejects invalid equipment");
            Check(EquipBase(EQUIP_TYPE_BOOTS,2) && CUR_EQUIP_VALUE(EQUIP_TYPE_BOOTS)==2,"Iron boots equip through the new journal");
            Check(EquipBase(EQUIP_TYPE_BOOTS,1),"Kokiri boots restore normally");
            CVarSetInteger(CVAR_ENHANCEMENT("AssignableTunicsAndBoots"),1);
            Check(AssignEquipment(EQUIP_TYPE_BOOTS,2,0) && gSaveContext.equips.buttonItems[1]==ITEM_BOOTS_IRON,
                  "Assignable equipment uses native C-button loadout");
            CVarSetInteger(CVAR_ENHANCEMENT("AssignableTunicsAndBoots"),0);
            Check(!AssignEquipment(EQUIP_TYPE_BOOTS,2,0),"Equipment assignment honors its enhancement gate");
            {
                using namespace ARPG;
                const auto savedGear=gARPGSave;
                auto base=[](int slot) { ARPGItemInstance item{}; item.slot=slot; item.baseEquip=1; item.itemLevel=35; return item; };
                Check(!ARPG::Inventory::Assign(false,-1,0) && !ARPG::Inventory::Assign(false,ARPG_BACKPACK_CAPACITY,0) &&
                      !ARPG::Inventory::Assign(true,ARPG_LOOT_SLOT_COUNT,0),"Custom assignment rejects invalid inventory indices");
                for(int slot=ARPG_SLOT_BOW;slot<ARPG_LOOT_SLOT_COUNT;++slot) {
                    int native=AdventureItems::NativeItem(slot);
                    gSaveContext.inventory.items[SLOT(native)]=native;
                    gSaveContext.linkAge=(slot==ARPG_SLOT_SLINGSHOT || slot==ARPG_SLOT_BOOMERANG || slot==ARPG_SLOT_STICK)?LINK_AGE_CHILD:LINK_AGE_ADULT;
                    gARPGSave.backpack[0]=base(slot);
                    Check(ARPG::Inventory::Assign(false,0,0) && gSaveContext.equips.buttonItems[1]==native &&
                          gSaveContext.equips.cButtonSlots[0]==SLOT(native),"Every custom item family can equip and assign to a C-button");
                }
                for(int id=1;id<=Loot::UniqueCount;++id) {
                    int slot=Loot::Uniques[id].slot;
                    if(!Loot::WeaponSlot(slot)) { continue; }
                    gSaveContext.linkAge=(slot==ARPG_SLOT_SLINGSHOT || slot==ARPG_SLOT_BOOMERANG || slot==ARPG_SLOT_STICK)?LINK_AGE_CHILD:LINK_AGE_ADULT;
                    for(int button=0;button<3;++button) {
                        auto item=base(slot); Loot::IdentifyUnique(item,id);
                        for(auto& stored:gARPGSave.backpack) { stored=base(ARPG_SLOT_BOW); }
                        gARPGSave.backpack[0]=item;
                        InventoryRules::Equipped(gARPGSave,slot)=base(slot);
                        Check(ARPG::Inventory::Assign(false,0,button) && InventoryRules::Equipped(gARPGSave,slot).uniqueId==id &&
                              gARPGSave.backpack[0].slot==slot && !gARPGSave.backpack[0].uniqueId &&
                              InventoryRules::Count(gARPGSave)==ARPG_BACKPACK_CAPACITY &&
                              gSaveContext.equips.buttonItems[button+1]==AdventureItems::NativeItem(slot),
                              "Unique variant assigns to each C-button and preserves displaced gear in a full backpack");
                    }
                }
                gSaveContext.linkAge=LINK_AGE_ADULT;
                gARPGSave.backpack[0]=base(ARPG_SLOT_HOOKSHOT);
                gSaveContext.inventory.items[SLOT_HOOKSHOT]=ITEM_LONGSHOT;
                Check(ARPG::Inventory::Assign(false,0,0) && gSaveContext.equips.buttonItems[1]==ITEM_LONGSHOT,"Custom hookshot assignment preserves the owned Longshot upgrade");
                gARPGSave.backpack[0]=base(ARPG_SLOT_OCARINA);
                gSaveContext.inventory.items[SLOT_OCARINA]=ITEM_OCARINA_TIME;
                Check(ARPG::Inventory::Assign(false,0,1) && gSaveContext.equips.buttonItems[2]==ITEM_OCARINA_TIME,"Custom ocarina assignment preserves the Ocarina of Time");
                for(int slot=SLOT_BOTTLE_1;slot<=SLOT_BOTTLE_4;++slot) { gSaveContext.inventory.items[slot]=ITEM_NONE; }
                gSaveContext.inventory.items[SLOT_BOTTLE_3]=ITEM_POTION_RED;
                gARPGSave.backpack[0]=base(ARPG_SLOT_BOTTLE);
                Check(ARPG::Inventory::Assign(false,0,2) && gSaveContext.equips.cButtonSlots[2]==SLOT_BOTTLE_3 &&
                      gSaveContext.equips.buttonItems[3]==ITEM_POTION_RED,"Custom bottle finds an owned bottle beyond the first slot and keeps its contents");
                gSaveContext.inventory.items[SLOT_BOTTLE_4]=ITEM_FAIRY;
                Check(ARPG::Inventory::Assign(true,ARPG_SLOT_BOTTLE,1,SLOT_BOTTLE_4) && gSaveContext.equips.buttonItems[2]==ITEM_FAIRY &&
                      gSaveContext.equips.cButtonSlots[1]==SLOT_BOTTLE_4,"Custom bottle can target a specific bottle without changing its contents");
                gARPGSave.backpack[0]=base(ARPG_SLOT_BOW);
                CVarSetInteger(CVAR_ENHANCEMENT("SeparateArrows"),0);
                Check(ARPG::Inventory::Assign(false,0,0,SLOT_ARROW_FIRE) && gSaveContext.equips.buttonItems[1]==ITEM_BOW_ARROW_FIRE &&
                      gSaveContext.equips.cButtonSlots[0]==SLOT_BOW,"Custom bow assignment keeps native elemental-arrow behavior");
                gARPGSave.backpack[0]=base(ARPG_SLOT_HOOKSHOT);
                auto before=InventoryRules::Equipped(gARPGSave,ARPG_SLOT_HOOKSHOT);
                auto buttons=gSaveContext.equips;
                CVarSetInteger(CVAR_ENHANCEMENT("DpadEquips"),0);
                Check(!ARPG::Inventory::Assign(false,0,3) && !ARPG::Inventory::Assign(false,0,7) && !ARPG::Inventory::Assign(false,0,0,SLOT_BOMB),
                      "Custom assignment rejects disabled D-pad, invalid buttons, and mismatched native slots");
                gSaveContext.linkAge=LINK_AGE_CHILD;
                Check(!ARPG::Inventory::Assign(false,0,0),"Custom item assignment enforces native age restrictions");
                gSaveContext.linkAge=LINK_AGE_ADULT;
                gSaveContext.inventory.items[SLOT_HOOKSHOT]=ITEM_NONE;
                Check(!ARPG::Inventory::Assign(false,0,0),"Custom item assignment requires native ownership");
                gSaveContext.inventory.items[SLOT_HOOKSHOT]=ITEM_LONGSHOT;
                auto veto=GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnVanillaBehavior>(
                    VB_EQUIP_ITEM_TO_C_BUTTON,[](GIVanillaBehavior,bool* should,va_list) { *should=false; });
                bool rejected=!ARPG::Inventory::Assign(false,0,0);
                GameInteractor::Instance->UnregisterGameHookForID<GameInteractor::OnVanillaBehavior>(veto);
                Check(rejected && gARPGSave.backpack[0].slot==ARPG_SLOT_HOOKSHOT &&
                      !memcmp(&before,&InventoryRules::Equipped(gARPGSave,ARPG_SLOT_HOOKSHOT),sizeof(before)) &&
                      !memcmp(&buttons,&gSaveContext.equips,sizeof(buttons)),"Rejected assignments preserve both gear and native button bindings");
                CVarSetInteger(CVAR_ENHANCEMENT("DpadEquips"),1);
                Check(ARPG::Inventory::Assign(false,0,6) && gSaveContext.equips.buttonItems[7]==ITEM_LONGSHOT,"Custom variants support enabled D-pad slots");
                for(int id:{24,26,27}) {
                    auto item=base(Loot::Uniques[id].slot); Loot::IdentifyUnique(item,id);
                    gARPGSave.backpack[0]=item;
                    CVarSetInteger(CVAR_ENHANCEMENT("AssignableTunicsAndBoots"),0);
                    Check(!ARPG::Inventory::Assign(false,0,0),"Custom wearable assignment honors the native enhancement setting");
                    CVarSetInteger(CVAR_ENHANCEMENT("AssignableTunicsAndBoots"),1);
                    Check(ARPG::Inventory::Assign(false,0,0) && InventoryRules::Equipped(gARPGSave,item.slot).uniqueId==id &&
                          CUR_EQUIP_VALUE(item.slot)==item.baseEquip,"Custom shield and boot relics equip and bind like their native versions");
                }
                CVarSetInteger(CVAR_ENHANCEMENT("AssignableTunicsAndBoots"),0);
                EquipBase(EQUIP_TYPE_BOOTS,1);
                gARPGSave=savedGear;
            }
            Check(!Save(),"Temporary adventures cannot overwrite a save slot");
            gSaveContext.fileNum=1;
            Check(Save(),"Journal uses native save lifecycle in an isolated slot");
            pausedFrames=gPlayState->gameplayFrames; phase=3; frames=0; return;
        }
        if(phase==3 && frames>25) {
            Check(gPlayState->gameplayFrames==pausedFrames,"Gameplay stays frozen while the journal is open");
            Check(std::filesystem::exists(resultPath.parent_path()/"Save"/"file2.sav"),"Save request writes the isolated file");
            ARPG::Inventory::Close(); phase=4; frames=0; return;
        }
        if(phase==4 && gPlayState->pauseCtx.state==0 && gPlayState->gameplayFrames>pausedFrames+5) {
            Check(!ARPG::Inventory::IsOpen() && !ARPG_CustomPauseActive(),"Resume completes native pause teardown");
            Check(gPlayState->gameplayFrames>pausedFrames,"Gameplay resumes after journal close");
            ARPG::Inventory::Open(); phase=5; frames=0; return;
        }
        if(phase==5 && gPlayState->pauseCtx.state==6) {
            Check(ARPG::Inventory::IsOpen(),"Journal reopens after resume");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"),0);
            phase=6; frames=0; return;
        }
        if(phase==6 && gPlayState->pauseCtx.state==0 && GameInteractor::IsPlayerInControl()) {
            Check(!ARPG::Inventory::IsOpen(),"Disabling ARPG safely closes the journal without exposing uninitialized native cursor data");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"),1);
            gPlayState->nextEntranceIndex=ENTR_DEKU_TREE_ENTRANCE;
            gPlayState->transitionTrigger=TRANS_TRIGGER_START;
            gPlayState->transitionType=TRANS_TYPE_INSTANT;
            phase=7; frames=0; return;
        }
        if(phase==7 && frames>60 && gPlayState->sceneNum==SCENE_DEKU_TREE && GameInteractor::IsPlayerInControl()) {
            ARPG::Inventory::Open(); phase=8; frames=0; return;
        }
        if(phase==8 && gPlayState->pauseCtx.state==6) {
            Check(gPlayState->interfaceCtx.mapSegment[0] && gPlayState->interfaceCtx.mapSegment[1],"Dungeon pause still prepares both native map textures");
            Result("passed");
            if(const char* visual=std::getenv("SOH_ARPG_GEAR_VISUAL"); visual && std::string(visual)=="1") {
                using namespace ARPG;
                gSaveContext.fileNum=0xFF;
                CVarSetInteger(CVAR_ENHANCEMENT("DpadEquips"),0);
                for(auto& stored:gARPGSave.backpack) { stored={}; }
                for(int slot=ARPG_SLOT_BOW;slot<ARPG_LOOT_SLOT_COUNT;++slot) {
                    int native=AdventureItems::NativeItem(slot);
                    gSaveContext.inventory.items[SLOT(native)]=native;
                }
                for(int id=1;id<=Loot::UniqueCount;++id) {
                    int slot=Loot::Uniques[id].slot;
                    if(!Loot::WeaponSlot(slot)) { continue; }
                    ARPGItemInstance item{}; item.slot=slot; item.baseEquip=1; item.itemLevel=35;
                    Loot::IdentifyUnique(item,id); InventoryRules::Collect(gARPGSave,item);
                }
            }
            return;
        }
        if(frames>600) { throw std::runtime_error("Journal timeout in phase "+std::to_string(phase)); }
    } catch(const std::exception& e) { Result("failed",e.what()); }
}
void Register() {
    const char* enabled=std::getenv("SOH_ARPG_JOURNAL_QA"); const char* home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-")) { return; }
    resultPath=std::filesystem::path(home)/"journal-runtime-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Run);
}
static RegisterShipInitFunc init(Register);
}
