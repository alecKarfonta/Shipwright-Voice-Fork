#include "AdventureRelicRules.h"
#include "ARPGSaveCodec.h"
#include "AcquisitionRules.h"
#include "CraftingRules.h"
#include "ItemAppearance.h"
#include <cassert>
#include <set>
#include <iostream>
using namespace ARPG;
ARPGItemInstance Relic(int id) {ARPGItemInstance i{};i.slot=Loot::Uniques[id].slot;i.baseEquip=1;i.itemLevel=35;Loot::IdentifyUnique(i,id);return i;}
int main() {
    assert(Loot::UniqueCount==30 && ARPG_LOOT_SLOT_COUNT==16 && ARPG_WEAPON_SLOT_COUNT==12);
    ARPGSaveData save{};save.seed=53;save.level=12;save.xp=Rules::XpThreshold(11);SkillRules::Initialize(save);
    for(int slot=0;slot<16;++slot)InventoryRules::Equipped(save,slot).slot=slot;
    for(int id=19;id<=30;++id) {
        auto item=Relic(id);assert(InventoryRules::Valid(item));assert(Loot::WeaponPowerName(item)[0] && Loot::WeaponPowerDescription(item)[0]);
        assert(InventoryRules::Collect(save,item));
        auto appearance=ItemAppearance::Resolve(item);assert(appearance.kind>=ItemAppearance::Kind::Native); // New armor must not index the old eight-relic art array.
        auto required=Loot::Uniques[id].requiredBase;
        if(required){auto invalid=item;invalid.baseEquip=1;assert(!InventoryRules::Valid(invalid));assert(!Loot::Unique(invalid));}
    }
    for(int i=0;i<12;++i)assert(InventoryRules::Equip(save,i));
    for(int slot=0;slot<16;++slot)for(int base=1;base<=(slot<4?3:1);++base)save.acquiredEquipment|=AcquisitionRules::Receipt(slot,base);
    save.campaign.quests[31].state=1;save.campaign.quests[31].progress[0]=1;save.campaign.quests[31].nodes[0]=1;save.campaign.tracked=32;
    auto encoded=SaveCodec::Encode(save);assert(SaveCodec::Encode(SaveCodec::Decode(encoded,9))==encoded);assert(save.acquiredEquipment==0xFFFFFF);
    auto legacy=encoded;legacy["weapons"].erase(legacy["weapons"].begin()+7,legacy["weapons"].end());legacy["acquiredEquipment"]=0x7FFFF;
    auto migrated=SaveCodec::Decode(legacy,9);assert(migrated.campaign.quests[31].state==1 && migrated.campaign.tracked==32 && migrated.acquiredEquipment==0x7FFFF);
    for(int slot=11;slot<16;++slot)assert(!InventoryRules::Present(InventoryRules::Equipped(migrated,slot)) && InventoryRules::Equipped(migrated,slot).slot==slot);
    for(int slot:{1,3})for(int base=1;base<=3;++base){uint32_t rng=123;std::set<int> seen;
        for(int n=0;n<8000;++n){ARPGItemInstance item{};item.slot=slot;item.baseEquip=base;item.itemLevel=35;item.rarity=ARPG_RARITY_UNIQUE;Loot::RollIdentity(item,rng);assert(item.baseEquip==base);assert(InventoryRules::Valid(item));seen.insert(item.uniqueId);}
        assert(seen.contains(24)==(slot==1 && base==3));assert(seen.contains(26)==(slot==3 && base==2));assert(seen.contains(27)==(slot==3 && base==3));
    }
    Relics::Preparation p;p.guard=6;
    for(int i=0;i<5;++i)p.Tick(true,false,false,false);assert(p.Block() && p.light==120 && !p.Block());assert(p.ShootLight() && !p.ShootLight());
    p.guard=6;for(int i=0;i<6;++i)p.Tick(true,false,false,false);assert(!p.Block());
    p.light=120;for(int i=0;i<400;++i)p.Tick(false,true,true,true);assert(p.light==120 && !p.anchor);
    for(int i=0;i<120;++i)p.Tick(true,true,true,true);assert(!p.light && p.anchor==40 && p.Slam() && !p.Slam());
    for(int i=0;i<40;++i)p.Tick(true,true,true,true);p.Tick(true,true,false,true);assert(!p.anchor);
    for(int i=0;i<40;++i)p.Tick(true,true,true,true);p.Tick(true,true,true,false);assert(!p.anchor);
    p.cinder=1;p.cinderFinal=true;p.Tick(true,false,false,false);assert(!p.cinder && !p.cinderFinal);
    assert(Relics::ToolRadius(10000,170)==340 && Relics::ToolRadius(-5,170)==170);
    assert(Relics::AmmoPickup(16) && Relics::AmmoPickup(26));for(int id:{0,3,6,7,14,15,17,18,19,20,21,22,23,24,27,28,29,255})assert(!Relics::AmmoPickup(id));
    std::cout<<"12 relic identities, native-base restrictions, 48k loot rolls, v12 migration with campaign data, save round-trip, charge windows, caps and ammo filters passed\n";
}
