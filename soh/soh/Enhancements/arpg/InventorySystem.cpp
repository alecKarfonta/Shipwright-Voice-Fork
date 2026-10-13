#include "InventorySystem.h"
#include "InventoryRules.h"
#include "AcquisitionRules.h"
#include "AdventureItems.h"
#include "ItemModifierSystem.h"
#include "AdventureJournal.h"
#include "ARPG.h"
#include "ARPGSave.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/libultraship.h>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "overlays/misc/ovl_kaleido_scope/z_kaleido_scope.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
extern u8 gEquipAgeReqs[][4];
extern PlayState* gPlayState;
}

namespace ARPG::Inventory {
static constexpr int32_t inputBlockId = 0x41525047;
static void CaptureInput(bool visible) {
    static bool captured = false;
    if (captured == visible) { return; }
    captured = visible;
    auto* context = Ship::Context::GetRawInstance();
    context->GetWindow()->GetGui()->SetGameModalActive(visible);
    if (visible) {
        context->GetControlDeck()->BlockGameInput(inputBlockId);
        context->GetWindow()->SetMouseCapture(false);
        context->GetWindow()->SetCursorVisibility(true);
    } else {
        context->GetControlDeck()->UnblockGameInput(inputBlockId);
        context->GetWindow()->GetMouseStateManager()->UpdateMouseCapture();
    }
}
static bool requestResume = false;
static bool observedPause = false;
static bool attachedToPause = false;
static auto Window() { return Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow("Loot Inventory"); }
bool IsOpen() { auto window = Window(); return window && window->IsVisible(); }
static bool CanChange() {
    return ItemsEnabled() && gPlayState != nullptr &&
           (GameInteractor::IsPlayerInControl() ||
            (gPlayState->pauseCtx.state == 6 && gPlayState->pauseCtx.unk_1E4 == 0));
}

bool CanCraft() { return CanChange(); }

int GearSlot(int nativeSlot) {
    if(nativeSlot<0 || nativeSlot>=24) { return -1; }
    if(nativeSlot>=SLOT_BOTTLE_1 && nativeSlot<=SLOT_BOTTLE_4) { return ARPG_SLOT_BOTTLE; }
    if(nativeSlot==SLOT_ARROW_FIRE || nativeSlot==SLOT_ARROW_ICE || nativeSlot==SLOT_ARROW_LIGHT) { return ARPG_SLOT_BOW; }
    for(int slot=ARPG_SLOT_BOW;slot<ARPG_LOOT_SLOT_COUNT;++slot) {
        if(SLOT(AdventureItems::NativeItem(slot))==nativeSlot) { return slot; }
    }
    return -1;
}

int NativeSlot(int gearSlot) {
    if(!Loot::WeaponSlot(gearSlot)) { return -1; }
    if(gearSlot==ARPG_SLOT_BOTTLE) {
        for(int slot=SLOT_BOTTLE_1;slot<=SLOT_BOTTLE_4;++slot) {
            if(gSaveContext.inventory.items[slot]!=ITEM_NONE) { return slot; }
        }
        return -1;
    }
    return SLOT(AdventureItems::NativeItem(gearSlot));
}

bool Assign(bool equipped,int index,int button,int nativeSlot) {
    if(index<0 || index>=(equipped?ARPG_LOOT_SLOT_COUNT:ARPG_BACKPACK_CAPACITY)) { return false; }
    const auto item=equipped?InventoryRules::Equipped(gARPGSave,index):gARPGSave.backpack[index];
    if(!EquipRestriction(item).empty()) { return false; }
    if(!Loot::WeaponSlot(item.slot)) {
        if(!AdventureJournal::AssignEquipment(item.slot,item.baseEquip,button)) { return false; }
        if(!equipped) { return Equip(index); }
        return true;
    }
    if(nativeSlot<0) { nativeSlot=NativeSlot(item.slot); }
    if(GearSlot(nativeSlot)!=item.slot) { return false; }
    // Validate and bind through the native path before swapping the roll. A rejected
    // assignment (including a hook veto) must leave the backpack and loadout intact.
    if(!AdventureJournal::AssignItem(nativeSlot,button)) { return false; }
    if(!equipped) { InventoryRules::Equip(gARPGSave,index); }
    return true;
}

std::string EquipRestriction(const ARPGItemInstance& item) {
    if (!InventoryRules::Valid(item)) { return "Select an item from your backpack."; }
    if (!CanChange()) { return "Gear changes are available during play or on the pause screen."; }
    if(Loot::WeaponSlot(item.slot)) {
        int nativeSlot=NativeSlot(item.slot);
        if(!ItemModifierSystem::WeaponOwned(item.slot)) { return "Find this weapon in your adventure first."; }
        if(!CHECK_AGE_REQ_SLOT(nativeSlot)) { return item.slot==ARPG_SLOT_SLINGSHOT || item.slot==ARPG_SLOT_BOOMERANG || item.slot==ARPG_SLOT_STICK ? "Requires child Link." : "Requires adult Link."; }
        return {};
    }
    int base = item.baseEquip ? item.baseEquip : CUR_EQUIP_VALUE(item.slot);
    if (base < 1 || base > 3 || !CHECK_OWNED_EQUIP(item.slot, base - 1)) {
        return "Find this equipment in your adventure to equip it.";
    }
    int age = gEquipAgeReqs[item.slot][base];
    if (!GameInteractor_Should(VB_PLAYER_MEETS_AGE_REQ, age == 9 || age == gSaveContext.linkAge, age)) {
        return age == LINK_AGE_ADULT ? "Requires adult Link." : "Requires child Link.";
    }
    return {};
}

// [TIME] | [INFO] | Inventory.Equip | Update native equipment and its model together with the saved modifier instance.
bool Equip(int index) {
    if (index < 0 || index >= ARPG_BACKPACK_CAPACITY || !EquipRestriction(gARPGSave.backpack[index]).empty()) { return false; }
    auto item = gARPGSave.backpack[index];
    if (item.baseEquip == 0) { item.baseEquip = CUR_EQUIP_VALUE(item.slot); gARPGSave.backpack[index] = item; }
    if (!InventoryRules::Equip(gARPGSave, index)) { return false; }
    if(Loot::WeaponSlot(item.slot)) { return true; }
    Inventory_ChangeEquipment(item.slot, item.baseEquip);
    if (item.slot == ARPG_SLOT_SWORD) {
        static const uint8_t swords[] = { ITEM_NONE, ITEM_SWORD_KOKIRI, ITEM_SWORD_MASTER, ITEM_SWORD_BGS };
        gSaveContext.equips.buttonItems[0] = swords[item.baseEquip];
        if (item.baseEquip == 3 && CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE) && !gSaveContext.bgsFlag) {
            gSaveContext.equips.buttonItems[0] = ITEM_SWORD_KNIFE;
        }
        Flags_UnsetInfTable(INFTABLE_SWORDLESS);
        Interface_LoadItemIcon1(gPlayState, 0);
    }
    Player_SetEquipmentData(gPlayState, GET_PLAYER(gPlayState));
    return true;
}

bool Unequip(int slot, int index) {
    return CanChange() && InventoryRules::Unequip(gARPGSave, slot, index);
}

void Open() {
    if (!Enabled() || gPlayState == nullptr || requestResume) { return; }
    // Never attach to game-over, save prompts, cutscenes or a closing pause.
    if (gPlayState->pauseCtx.state != 0 && gPlayState->pauseCtx.state != 6) { return; }
    requestResume = false;
    if (!IsOpen()) { AdventureJournal::Reset(); }
    // [TIME] | [INFO] | Inventory.Open | Enter the engine's normal pause sequence, preserving its eligibility checks and setup.
    if (GameInteractor::IsPlayerInControl() && gPlayState->pauseCtx.state == 0) {
        auto input = gPlayState->state.input[0];
        gPlayState->state.input[0].press.button = BTN_START;
        gPlayState->state.input[0].cur.button = 0;
        KaleidoSetup_Update(gPlayState);
        gPlayState->state.input[0] = input;
    }
    if (gPlayState->pauseCtx.state == 0) { return; }
    attachedToPause = true;
    if (gPlayState->pauseCtx.state == 6) { gPlayState->pauseCtx.unk_1E4 = 0; }
    if (auto menu = Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetMenu()) { menu->Hide(); }
    if (auto window = Window()) { window->Show(); }
    CaptureInput(true);
}

void Close(bool resume) {
    AdventureJournal::StopSong();
    if (resume) {
        if (ImGui::GetCurrentContext()) { ImGui::SetWindowFocus(nullptr); }
        if (auto menu = Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetMenu()) { menu->Hide(); }
    }
    if (auto window = Window()) { window->Hide(); }
    requestResume = resume && attachedToPause;
    CaptureInput(requestResume);
    observedPause = true;
}

void ObservePause() {
    CaptureInput(IsOpen() || requestResume);
    if (gPlayState == nullptr) {
        if (IsOpen()) { Close(false); }
        CaptureInput(false);
        observedPause = attachedToPause = requestResume = false;
        return;
    }
    if (!Enabled()) {
        if (IsOpen()) { Close(true); }
        if (gPlayState->pauseCtx.state == 0) {
            observedPause = attachedToPause = requestResume = false;
            CaptureInput(false);
        }
        return;
    }
    int state = gPlayState->pauseCtx.state;
    if (state == 0) {
        if (attachedToPause && IsOpen()) { Close(false); }
        observedPause = attachedToPause = requestResume = false;
    } else if (state == 6 && !observedPause) {
        observedPause = true;
        Open();
    } else if (state >= 7 && attachedToPause && IsOpen()) {
        Close(false);
    }
}

// Reconcile after native pickup handling and on existing saves; full bags retry without losing rewards.
static void CollectAdventureEquipment() {
    if(!ItemsEnabled() || !gPlayState || !GameInteractor::IsPlayerInControl()) return;
    for(int slot=0;slot<ARPG_SLOT_COUNT_;++slot) {
        for(int base=1;base<=3;++base) {
            if(CHECK_OWNED_EQUIP(slot,base-1)) AcquisitionRules::CollectOwned(gARPGSave,slot,base);
        }
    }
    for(int slot=ARPG_SLOT_COUNT_;slot<ARPG_LOOT_SLOT_COUNT;++slot) {
        if(ItemModifierSystem::WeaponOwned(slot)) AcquisitionRules::CollectOwned(gARPGSave,slot,1);
    }
}
static void RegisterInventory() {
    // Apply the release correction once to existing profiles as well as fresh installs.
    // A subsequent user preference remains respected.
    if(CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"),1) &&
       !CVarGetInteger(CVAR_ENHANCEMENT("ARPG.VisibilitySaveDefaultsV1"),0)) {
        CVarSetInteger(CVAR_ENHANCEMENT("RememberSaveLocation"),1);
        CVarSetInteger(CVAR_ENHANCEMENT("WidescreenActorCulling"),1);
        CVarSetInteger(CVAR_ENHANCEMENT("DisableDrawDistance"),2);
        CVarSetInteger(CVAR_ENHANCEMENT("ARPG.VisibilitySaveDefaultsV1"),1);
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
    COND_HOOK(OnPlayerUpdate, true, CollectAdventureEquipment);
    COND_ID_HOOK(OnVanillaBehavior, VB_CLOSE_PAUSE_MENU, true, [](GIVanillaBehavior, bool* should, va_list) {
        if (requestResume) { *should = true; requestResume = false; }
    });
    COND_HOOK(OnLoadGame, true, [](int32_t) {
        AdventureJournal::Reset();
        if (auto window = Window()) { window->Hide(); }
        CaptureInput(false);
        observedPause = attachedToPause = requestResume = false;
    });
}
static RegisterShipInitFunc init(RegisterInventory);
}

// [TIME] | [INFO] | Inventory.CustomPauseActive | Replace only the native pause drawing; preserve its simulation, input and save lifecycle.
extern "C" int ARPG_CustomPauseActive(void) {
    if (!gPlayState || (!ARPG::Enabled() && !ARPG::Inventory::attachedToPause)) { return false; }
    int state = gPlayState->pauseCtx.state;
    // Suppress the entire ordinary pause transition, including its first frame.
    // Death/save screens retain their original lifecycle.
    return (state >= 1 && state <= 6) || state == 0x12 || state == 0x13;
}
