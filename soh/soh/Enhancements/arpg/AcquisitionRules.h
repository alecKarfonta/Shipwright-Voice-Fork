#pragma once
#include "InventoryRules.h"
namespace ARPG::AcquisitionRules {
// Permanent first-acquisition receipts prevent duplication after crafting or salvage.
inline uint32_t Receipt(int slot, int base) {
    if(slot < 0 || slot >= ARPG_LOOT_SLOT_COUNT || base < 1 || base > (Loot::WeaponSlot(slot)?1:3)) return 0;
    return 1u << (slot < ARPG_SLOT_COUNT_ ? slot*3+base-1 : 12+slot-ARPG_SLOT_COUNT_);
}
inline bool CollectOwned(ARPGSaveData& save, int slot, int base) {
    uint32_t receipt=Receipt(slot,base);
    if(!receipt || (save.acquiredEquipment & receipt)) return false;
    auto matches=[&](const ARPGItemInstance& item) {
        return InventoryRules::Valid(item) && item.slot==slot && item.baseEquip==base &&
               item.modCount==0 && item.uniqueId==0 && item.setId==0 && item.socketCount==0;
    };
    if(matches(InventoryRules::Equipped(save,slot))) { save.acquiredEquipment |= receipt; return false; }
    for(const auto& item:save.backpack) if(matches(item)) { save.acquiredEquipment |= receipt; return false; }
    ARPGItemInstance item{}; item.slot=slot; item.baseEquip=base;
    if(!InventoryRules::Collect(save,item)) return false;
    save.acquiredEquipment |= receipt;
    return true;
}
}
