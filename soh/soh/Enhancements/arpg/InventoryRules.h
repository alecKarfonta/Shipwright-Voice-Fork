#pragma once
#include "ARPGRules.h"
#include "LootCatalog.h"
#include <utility>

namespace ARPG::InventoryRules {
inline ARPGItemInstance& Equipped(ARPGSaveData& save,int slot) {
    return slot<ARPG_SLOT_COUNT_ ? save.equipment[slot] : save.weapons[slot-ARPG_SLOT_COUNT_];
}
inline const ARPGItemInstance& Equipped(const ARPGSaveData& save,int slot) {
    return slot<ARPG_SLOT_COUNT_ ? save.equipment[slot] : save.weapons[slot-ARPG_SLOT_COUNT_];
}
inline bool Present(const ARPGItemInstance& item) { return item.baseEquip != 0 || item.modCount != 0; }
inline bool Valid(const ARPGItemInstance& item) {
    if(item.socketCount>Loot::MaxSockets(item) || (item.uniqueId && !Loot::Unique(item)) || (item.setId && !Loot::Set(item))) { return false; }
    for(int i=0;i<ARPG_ITEM_SOCKET_COUNT;++i) {
        if(!item.sockets[i]) { continue; }
        if(i>=item.socketCount || !Loot::SocketMaterial(item.sockets[i])) { return false; }
    }
    return Present(item) && item.slot < ARPG_LOOT_SLOT_COUNT && item.baseEquip <= (Loot::WeaponSlot(item.slot)?1:3) && (!Loot::WeaponSlot(item.slot) || item.baseEquip==1) &&
           item.rarity < ARPG_RARITY_COUNT_ && item.modCount <= ARPG_ITEM_MOD_COUNT;
}
inline int FreeSlot(const ARPGSaveData& save) {
    for (int i = 0; i < ARPG_BACKPACK_CAPACITY; ++i) { if (!Present(save.backpack[i])) { return i; } }
    return -1;
}
inline int Count(const ARPGSaveData& save) {
    int count = 0;
    for (const auto& item : save.backpack) { count += Present(item); }
    return count;
}
inline bool Collect(ARPGSaveData& save, const ARPGItemInstance& item) {
    int index = FreeSlot(save);
    if (index < 0 || !Valid(item)) { return false; }
    save.backpack[index] = item;
    save.itemsFound = Rules::SaturatingAdd(save.itemsFound, 1);
    return true;
}
// [TIME] | [INFO] | InventoryRules.Equip | A swap reuses the source backpack slot even when every slot is occupied.
inline bool Equip(ARPGSaveData& save, int index) {
    if (index < 0 || index >= ARPG_BACKPACK_CAPACITY || !Valid(save.backpack[index])) { return false; }
    int slot = save.backpack[index].slot;
    std::swap(save.backpack[index], Equipped(save,slot));
    if (!Present(save.backpack[index])) { save.backpack[index] = {}; }
    return true;
}
inline bool Unequip(ARPGSaveData& save, int slot, int destination = -1) {
    if (slot < 0 || slot >= ARPG_LOOT_SLOT_COUNT || !Present(Equipped(save,slot))) { return false; }
    if (destination < 0) { destination = FreeSlot(save); }
    if (destination < 0 || destination >= ARPG_BACKPACK_CAPACITY || Present(save.backpack[destination])) { return false; }
    save.backpack[destination] = Equipped(save,slot);
    Equipped(save,slot) = {};
    Equipped(save,slot).slot = slot;
    return true;
}
inline bool Move(ARPGSaveData& save, int from, int to) {
    if (from < 0 || from >= ARPG_BACKPACK_CAPACITY || to < 0 || to >= ARPG_BACKPACK_CAPACITY) { return false; }
    std::swap(save.backpack[from], save.backpack[to]);
    return true;
}
inline int Power(const ARPGItemInstance& item, int modId) { return Loot::Power(item,modId); }
}
