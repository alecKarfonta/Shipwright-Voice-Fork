#pragma once
#include "InventoryRules.h"

namespace ARPG::Crafting {
// [TIME] | [INFO] | Crafting.Transactions | Validate inventory, capacity and cost before any mutation; failed operations leave the entire save unchanged.
inline bool CanReceive(const ARPGSaveData& save,int material,int count=1) {
    return material>0 && material<ARPG_MATERIAL_COUNT && count>0 && count<=ARPG_MATERIAL_CAPACITY && save.materials[material]<=ARPG_MATERIAL_CAPACITY-count;
}
inline bool Receive(ARPGSaveData& save,int material,int count=1) {
    if(!CanReceive(save,material,count)) { return false; } save.materials[material]+=count; return true;
}
inline bool Socket(ARPGSaveData& save,bool equipped,int index,int material,int socket=-1) {
    if(index<0 || index>=(equipped?ARPG_LOOT_SLOT_COUNT:ARPG_BACKPACK_CAPACITY) ||
       !Loot::SocketMaterial(material) || !save.materials[material] || socket < -1) { return false; }
    auto& item=equipped?InventoryRules::Equipped(save,index):save.backpack[index];
    if(!InventoryRules::Valid(item) || !item.socketCount || item.socketCount>Loot::MaxSockets(item)) { return false; }
    if(socket==-1)for(int i=0;i<item.socketCount;++i)if(!item.sockets[i]) { socket=i; break; }
    if(socket<0 || socket>=item.socketCount || item.sockets[socket])return false;
    item.sockets[socket]=material; --save.materials[material]; return true;
}
inline bool Socket(ARPGSaveData& save,int index,int material) { return Socket(save,false,index,material); }
inline int SocketCost(const ARPGItemInstance& item) { return 12+12*item.socketCount; }
inline bool AddSocket(ARPGSaveData& save,int index) {
    if(index<0 || index>=ARPG_BACKPACK_CAPACITY) { return false; } auto& item=save.backpack[index];
    int cost=SocketCost(item);
    if(!InventoryRules::Valid(item) || Loot::Filled(item) || item.socketCount>=Loot::MaxSockets(item) || save.materials[Loot::Essence]<cost) { return false; }
    ++item.socketCount; save.materials[Loot::Essence]-=cost; return true;
}
inline bool Extract(ARPGSaveData& save,int index) {
    if(index<0 || index>=ARPG_BACKPACK_CAPACITY || save.materials[Loot::Essence]<8) { return false; } auto& item=save.backpack[index];
    if(!InventoryRules::Valid(item) || !Loot::Filled(item)) { return false; }
    std::array<int,ARPG_MATERIAL_COUNT> counts{}; for(auto id:item.sockets) { if(Loot::SocketMaterial(id)) { ++counts[id]; } }
    for(int i=1;i<ARPG_MATERIAL_COUNT;++i) { if(counts[i] && !CanReceive(save,i,counts[i])) { return false; } }
    for(int i=1;i<ARPG_MATERIAL_COUNT;++i) { save.materials[i]+=counts[i]; }
    save.materials[Loot::Essence]-=8; for(auto& socket:item.sockets) { socket=0; } return true;
}
inline bool UpgradeGem(ARPGSaveData& save,int material) {
    if(!Loot::Gem(material) || material>12 || save.materials[material]<3 || !CanReceive(save,material+6)) { return false; }
    save.materials[material]-=3; ++save.materials[material+6]; return true;
}
inline int SalvageValue(const ARPGItemInstance& item) { return 2+item.rarity*3+item.itemLevel/10; }
inline bool Salvage(ARPGSaveData& save,int index) {
    if(index<0 || index>=ARPG_BACKPACK_CAPACITY) { return false; } auto& item=save.backpack[index];
    int reward=SalvageValue(item);
    if(!InventoryRules::Valid(item) || Loot::Filled(item) || !CanReceive(save,Loot::Essence,reward)) { return false; }
    save.materials[Loot::Essence]+=reward; item={}; return true;
}
}
