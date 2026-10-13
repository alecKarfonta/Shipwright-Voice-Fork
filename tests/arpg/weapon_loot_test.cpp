#include "CraftingRules.h"
#include "ARPGSaveCodec.h"
#include "ItemPowerRules.h"
#include "WeaponGeometry.h"
#include <cassert>
#include <iostream>
#include <cmath>
using namespace ARPG;
ARPGItemInstance Base(int slot) {ARPGItemInstance i{};i.slot=slot;i.baseEquip=1;i.itemLevel=35;return i;}
int main() {
    auto old=SaveCodec::Decode(nlohmann::json{{"seed",17},{"equipment",nlohmann::json::array({SaveCodec::EncodeItem(Base(0))})}},1);
    assert(old.equipment[0].baseEquip==1 && old.weapons[0].slot==ARPG_SLOT_BOW && old.weapons[1].slot==ARPG_SLOT_HAMMER);
    for(auto w:old.weapons)assert(!InventoryRules::Present(w));
    for(int slot=ARPG_SLOT_BOW;slot<=ARPG_SLOT_HAMMER;++slot) {
        ARPGSaveData save=old;auto item=Base(slot);item.rarity=ARPG_RARITY_RARE;item.modCount=1;item.mods[0]={uint8_t(slot==ARPG_SLOT_BOW?ARPG_MOD_ARCHERY:ARPG_MOD_BRUTAL),37};
        assert(InventoryRules::Collect(save,item));assert(InventoryRules::Equip(save,0));assert(InventoryRules::Equipped(save,slot).mods[0].power==37);
        assert(Loot::TotalPower(save,item.mods[0].modId)==0); // A stowed weapon never supplies global stats.
        for(auto& cell:save.backpack)cell=item;
        save.backpack[12].mods[0].power=51;auto count=save.itemsFound;assert(InventoryRules::Equip(save,12));
        assert(save.backpack[12].mods[0].power==37 && InventoryRules::Equipped(save,slot).mods[0].power==51 && save.itemsFound==count);
        auto full=SaveCodec::Encode(save);assert(!InventoryRules::Unequip(save,slot) && SaveCodec::Encode(save)==full);
        save.backpack[5]={};assert(InventoryRules::Unequip(save,slot,5));assert(save.backpack[5].mods[0].power==51);
        save.backpack[0]=Base(slot);save.materials[Loot::Essence]=200;
        for(int i=0;i<3;++i)assert(Crafting::AddSocket(save,0));
        int word=slot==ARPG_SLOT_BOW?9:10;const auto& recipe=Loot::Runeworks[word];
        for(int id:recipe.runes){save.materials[id]=3;assert(Crafting::Socket(save,0,id));}
        assert(Loot::Runework(save.backpack[0])==word);
        assert(InventoryRules::Equip(save,0));auto key=ItemPowers::EquipmentKey(save,15);
        auto encoded=SaveCodec::Encode(save);assert(SaveCodec::Encode(SaveCodec::Decode(encoded,1))==encoded);
        InventoryRules::Equipped(save,slot).sockets[0]=1;assert(ItemPowers::EquipmentKey(save,15)!=key);
        for(int material=1;material<=24;++material){auto mod=Loot::SocketBonus(material,slot);assert(mod.modId>=ARPG_MOD_BRUTAL && mod.modId<=ARPG_MOD_ARCHERY && mod.power>0);}
        auto unique=Base(slot);Loot::IdentifyUnique(unique,slot==ARPG_SLOT_BOW?9:10);assert(Loot::Unique(unique) && !Loot::Set(unique) && InventoryRules::Valid(unique));
        auto invalid=unique;invalid.baseEquip=3;assert(!InventoryRules::Valid(invalid));assert(!InventoryRules::Present(SaveCodec::DecodeItem(SaveCodec::EncodeItem(invalid),slot)));
        invalid=unique;invalid.uniqueId=0;invalid.setId=1;assert(!Loot::Set(invalid) && !InventoryRules::Valid(invalid));
        uint32_t rng=91,repeat=91;
        for(int n=0;n<10000;++n){auto a=Base(slot);a.rarity=n%4;auto b=a;Loot::RollIdentity(a,rng);Loot::RollIdentity(b,repeat);assert(InventoryRules::Valid(a));assert(SaveCodec::EncodeItem(a)==SaveCodec::EncodeItem(b));if(a.rarity==ARPG_RARITY_UNIQUE)assert(Loot::Unique(a));}
        ARPGExpeditionData expedition{};assert(ExpeditionRules::Start(expedition,22,slot,1,35));expedition.phase=ExpeditionRules::Reward;
        for(int depth=1;depth<=30;++depth){expedition.depth=depth;auto reward=ExpeditionRules::RewardFor(expedition,0);assert(InventoryRules::Valid(reward.item) && reward.item.slot==slot);if(depth%3==0)assert(Loot::Unique(reward.item));}
        ExpeditionRules::Recover(expedition);assert(expedition.phase==ExpeditionRules::Reward);
    }
    for(auto mesh:{WeaponGeometry::Bow(),WeaponGeometry::Bow(false),WeaponGeometry::Hammer()}) {
        assert(mesh.size()>500 && mesh.size()<20000 && mesh.size()%3==0);
        for(auto vertex:mesh) {
            assert(std::isfinite(vertex.p.x) && std::isfinite(vertex.p.y) && std::isfinite(vertex.p.z));
            assert(std::abs(vertex.p.x)<30 && std::abs(vertex.p.y)<30 && std::abs(vertex.p.z)<30);
            float length=vertex.normal.x*vertex.normal.x+vertex.normal.y*vertex.normal.y+vertex.normal.z*vertex.normal.z;assert(std::abs(length-1)<.001);
        }
        for(size_t i=0;i<mesh.size();i+=3){auto a=GearGeometry::Add(mesh[i+1].p,GearGeometry::Mul(mesh[i].p,-1)),b=GearGeometry::Add(mesh[i+2].p,GearGeometry::Mul(mesh[i].p,-1));auto c=GearGeometry::Cross(a,b);assert(c.x*c.x+c.y*c.y+c.z*c.z>1e-12f);}
    }
    std::cout<<"Weapon migration, full-pack swaps, combat-stat isolation, 20k deterministic rolls, sockets, recipes, 60 targeted caches and authored mesh invariants passed\n";
}
