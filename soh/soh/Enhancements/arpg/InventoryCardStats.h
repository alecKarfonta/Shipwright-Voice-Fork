#pragma once
#include "InventoryRules.h"
#include <array>

namespace ARPG::InventoryCardStats {
struct Row { int id; int item; int equipped; };
struct Comparison {
    std::array<Row, ARPG_MOD_COUNT_ - 1> rows{};
    int count = 0;
    bool hasEquipped = false;
};

// Include stats lost by a swap as well as stats gained, using only this item's slot.
inline Comparison Build(const ARPGSaveData& save, const ARPGItemInstance& item, bool equipped) {
    Comparison result;
    if (!InventoryRules::Present(item) || item.slot >= ARPG_LOOT_SLOT_COUNT) return result;
    const auto& current = InventoryRules::Equipped(save, item.slot);
    result.hasEquipped = !equipped && InventoryRules::Present(current);
    const bool weapon = item.slot == ARPG_SLOT_SWORD || Loot::WeaponSlot(item.slot);
    const int damage = Loot::RangedSlot(item.slot) ? ARPG_MOD_ARCHERY : ARPG_MOD_BRUTAL;
    for (int id = 1; id < ARPG_MOD_COUNT_; ++id) {
        const int incoming = Loot::Power(item, id);
        const int existing = result.hasEquipped ? Loot::Power(current, id) : 0;
        if (incoming || existing || (weapon && (id == damage || id == ARPG_MOD_KEEN))) {
            result.rows[result.count++] = {id, incoming, existing};
        }
    }
    return result;
}
}
