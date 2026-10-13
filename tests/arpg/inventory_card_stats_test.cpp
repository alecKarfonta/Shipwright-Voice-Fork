#include "InventoryCardStats.h"
#include <cassert>
#include <iostream>
using namespace ARPG;

ARPGItemInstance Item(int slot) {
    ARPGItemInstance item{}; item.slot=slot; item.baseEquip=1; return item;
}
const InventoryCardStats::Row& Find(const InventoryCardStats::Comparison& stats,int id) {
    for(int i=0;i<stats.count;++i)if(stats.rows[i].id==id)return stats.rows[i];
    assert(false && "Missing comparison row"); return stats.rows[0];
}
int main() {
    ARPGSaveData save{};
    for(int slot=0;slot<ARPG_LOOT_SLOT_COUNT;++slot) {
        auto incoming=Item(slot),current=Item(slot);
        incoming.modCount=1; incoming.mods[0]={ARPG_MOD_LUCKY,7};
        current.modCount=1; current.mods[0]={ARPG_MOD_LUCKY,int16_t(10+slot)};
        InventoryRules::Equipped(save,slot)=current;
        const auto stats=InventoryCardStats::Build(save,incoming,false);
        assert(stats.hasEquipped);
        assert(Find(stats,ARPG_MOD_LUCKY).item==7);
        assert(Find(stats,ARPG_MOD_LUCKY).equipped==10+slot);
    }
    auto sword=Item(ARPG_SLOT_SWORD),worn=sword;
    sword.modCount=3; sword.mods[0]={ARPG_MOD_BRUTAL,20}; sword.mods[1]={ARPG_MOD_FLAME,8}; sword.mods[2]={ARPG_MOD_SWIFT,3};
    worn.modCount=3; worn.mods[0]={ARPG_MOD_BRUTAL,10}; worn.mods[1]={ARPG_MOD_FROST,9}; worn.mods[2]={ARPG_MOD_LUCKY,5};
    sword.socketCount=1; sword.sockets[0]=1; // Ruby contributes to fire damage.
    save.equipment[ARPG_SLOT_SWORD]=worn;
    const auto full=InventoryCardStats::Build(save,sword,false);
    assert(full.count>3); // Every gained and lost stat stays visible, beyond the old three-row limit.
    assert(Find(full,ARPG_MOD_BRUTAL).item==20 && Find(full,ARPG_MOD_BRUTAL).equipped==10);
    assert(Find(full,ARPG_MOD_FROST).item==0 && Find(full,ARPG_MOD_FROST).equipped==9);
    assert(Find(full,ARPG_MOD_FLAME).item==13);
    assert(Find(full,ARPG_MOD_KEEN).item==0);
    save.equipment[ARPG_SLOT_SWORD]={};
    assert(!InventoryCardStats::Build(save,sword,false).hasEquipped);
    save.equipment[ARPG_SLOT_SWORD]=worn;
    assert(!InventoryCardStats::Build(save,worn,true).hasEquipped);
    assert(InventoryCardStats::Build(save,{},false).count==0);
    auto invalid=Item(ARPG_LOOT_SLOT_COUNT); assert(InventoryCardStats::Build(save,invalid,false).count==0);
    auto plain=Item(ARPG_SLOT_SHIELD); save.equipment[ARPG_SLOT_SHIELD]=plain;
    assert(InventoryCardStats::Build(save,plain,false).count==0);
    std::cout << "All 16 slots compare their own equipment, including lost affixes, sockets, plain items and empty slots.\n";
}
