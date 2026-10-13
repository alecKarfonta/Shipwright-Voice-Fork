#include "AdventureItemRules.h"
#include "AcquisitionRules.h"
#include "ArsenalRules.h"
#include "ARPGSaveCodec.h"
#include "CraftingRules.h"
#include <cassert>
#include <iostream>
#include <set>
using namespace ARPG;
ARPGItemInstance Base(int slot) { ARPGItemInstance item{};item.slot=slot;item.baseEquip=1;item.itemLevel=35;return item; }
int main() {
    // First pickups, old-save backfill, full backpacks and save/load must not lose or duplicate equipment.
    {
        ARPGSaveData save{};
        assert(AcquisitionRules::CollectOwned(save,ARPG_SLOT_SWORD,1));
        const auto sword=save.backpack[0];
        assert(sword.baseEquip==1 && sword.slot==ARPG_SLOT_SWORD && sword.modCount==0 &&
               sword.rarity==ARPG_RARITY_COMMON && sword.socketCount==0 && sword.uniqueId==0 && sword.setId==0);
        assert(!AcquisitionRules::CollectOwned(save,ARPG_SLOT_SWORD,1));
        assert(InventoryRules::Equip(save,0));
        assert(InventoryRules::Unequip(save,ARPG_SLOT_SWORD));
        save.backpack[0]={}; // A later salvage must not recreate the original acquisition.
        auto restored=SaveCodec::Decode(SaveCodec::Encode(save),42);
        assert(!AcquisitionRules::CollectOwned(restored,ARPG_SLOT_SWORD,1));
        assert(InventoryRules::Count(restored)==0);
        ARPGSaveData full{};
        for(auto& item:full.backpack) item=Base(ARPG_SLOT_SHIELD);
        assert(!AcquisitionRules::CollectOwned(full,ARPG_SLOT_SWORD,1));
        assert(full.acquiredEquipment==0);
        full.backpack[17]={};
        assert(AcquisitionRules::CollectOwned(full,ARPG_SLOT_SWORD,1));
        assert(full.backpack[17].slot==ARPG_SLOT_SWORD);
        ARPGSaveData legacy{};legacy.backpack[5]=sword;
        assert(!AcquisitionRules::CollectOwned(legacy,ARPG_SLOT_SWORD,1));
        assert(InventoryRules::Count(legacy)==1 && legacy.acquiredEquipment!=0);
        ARPGSaveData worn{};worn.equipment[0]=sword;
        assert(!AcquisitionRules::CollectOwned(worn,ARPG_SLOT_SWORD,1));
        assert(InventoryRules::Count(worn)==0);
        std::set<uint32_t> receipts;
        for(int slot=0;slot<ARPG_LOOT_SLOT_COUNT;++slot) {
            for(int base=1;base<=(Loot::WeaponSlot(slot)?1:3);++base) {
                assert(receipts.insert(AcquisitionRules::Receipt(slot,base)).second);
                ARPGSaveData fresh{};
                assert(AcquisitionRules::CollectOwned(fresh,slot,base));
                assert(InventoryRules::Valid(fresh.backpack[0]));
            }
        }
        assert(receipts.size()==24);
    }

    ARPGSaveData old{};old.seed=42;old.rngState=17;old.level=1;
    SkillRules::Initialize(old);
    old.equipment[0]=Base(0);old.weapons[0]=Base(ARPG_SLOT_BOW);Loot::IdentifyUnique(old.weapons[0],9);
    old.weapons[1]=Base(ARPG_SLOT_HAMMER);Loot::IdentifyUnique(old.weapons[1],10);
    auto v10=SaveCodec::Encode(old);v10["weapons"].erase(v10["weapons"].begin()+2,v10["weapons"].end());
    auto migrated=SaveCodec::Decode(v10,1);
    assert(migrated.weapons[0].uniqueId==9 && migrated.weapons[1].uniqueId==10);
    for(int slot=ARPG_SLOT_BOMBS;slot<ARPG_LOOT_SLOT_COUNT;++slot) {
        assert(!InventoryRules::Present(InventoryRules::Equipped(migrated,slot)));
        assert(InventoryRules::Equipped(migrated,slot).slot==slot);
        ARPGSaveData save=migrated;
        auto pool=Loot::Pool(slot);assert(pool.count>=3 && pool.count<=6);
        std::set<int> poolIds;for(int i=0;i<pool.count;++i)poolIds.insert(pool.ids[i]);assert(int(poolIds.size())==pool.count);
        uint32_t rng=80,replay=80;std::set<int> uniques;
        for(int n=0;n<4000;++n) {
            auto a=Base(slot);a.rarity=n%4;auto b=a;Loot::RollIdentity(a,rng);Loot::RollIdentity(b,replay);
            assert(InventoryRules::Valid(a) && SaveCodec::EncodeItem(a)==SaveCodec::EncodeItem(b));
            if(a.rarity==ARPG_RARITY_UNIQUE) {assert(Loot::Unique(a));uniques.insert(a.uniqueId);}
        }
        assert(uniques.size()==(slot==ARPG_SLOT_HOOKSHOT?3:slot==ARPG_SLOT_BOMBS || slot==ARPG_SLOT_SLINGSHOT || slot==ARPG_SLOT_BOOMERANG?2:1));
        for(int id:uniques) {
            auto item=Base(slot);Loot::IdentifyUnique(item,id);
            assert(Loot::WeaponPowerName(item)[0] && Loot::WeaponPowerDescription(item)[0]);
            for(auto& cell:save.backpack)cell=Base(slot);
            InventoryRules::Equipped(save,slot)=Base(slot);save.backpack[12]=item;
            auto count=InventoryRules::Count(save);assert(InventoryRules::Equip(save,12));
            assert(InventoryRules::Count(save)==count && InventoryRules::Equipped(save,slot).uniqueId==id);
            assert(Loot::TotalPower(save,ARPG_MOD_REACH)==0 && Loot::TotalPower(save,ARPG_MOD_BLAST)==0);
            auto encoded=SaveCodec::Encode(save);assert(SaveCodec::Encode(SaveCodec::Decode(encoded,1))==encoded);
            assert(!InventoryRules::Unequip(save,slot));save.backpack[2]={};assert(InventoryRules::Unequip(save,slot,2));
            save.materials[Loot::Essence]=200;assert(Crafting::AddSocket(save,2));save.materials[1]=5;
            assert(Crafting::Socket(save,2,1));assert(!Crafting::Salvage(save,2));assert(Crafting::Extract(save,2));assert(Crafting::Salvage(save,2));
        }
        for(int material=1;material<=24;++material) {auto bonus=Loot::SocketBonus(material,slot);assert(bonus.modId>0 && bonus.modId<ARPG_MOD_COUNT_ && bonus.power>0);}
        ARPGExpeditionData run{};assert(ExpeditionRules::Start(run,123,slot,1,35));run.phase=ExpeditionRules::Reward;
        for(int depth=1;depth<=30;++depth) {run.depth=depth;auto offer=ExpeditionRules::RewardFor(run,0);assert(InventoryRules::Valid(offer.item) && offer.item.slot==slot);if(depth%3==0)assert(Loot::Unique(offer.item));}
    }
    auto powder=Base(ARPG_SLOT_BOMBS);Loot::IdentifyUnique(powder,11);assert(std::abs(AdventureItems::BlastScale(powder)-1.6f)<.001f);
    auto skyvine=Base(ARPG_SLOT_HOOKSHOT);Loot::IdentifyUnique(skyvine,15);assert(AdventureItems::Travel(skyvine,13)==23 && AdventureItems::Travel(skyvine,26)==46);
    skyvine.mods[0].power=32000;assert(AdventureItems::Travel(skyvine,13)==26);assert(AdventureItems::Travel(skyvine,30000)==32767);
    powder.mods[0].power=32000;assert(AdventureItems::BlastScale(powder)==1.75f);
    auto iron=Base(ARPG_SLOT_HOOKSHOT);Loot::IdentifyUnique(iron,16);assert(AdventureItems::Duration(&iron,80)==120 && AdventureItems::Followup(&iron)==1.25f);
    Arsenal::Status status{};Arsenal::Resolve(status,Arsenal::Weapon::Hookshot,false,true);status.tethered=120;status.tetherBonus=1.25f;
    assert(Arsenal::Resolve(status,Arsenal::Weapon::Hammer,true,false)==2.25f && !status.tethered);
    assert(Arsenal::Resolve(status,Arsenal::Weapon::Hammer,true,false)==1);
    status.frozen=20;status.burning=30;Arsenal::Tick(status,false);assert(status.frozen==20 && Arsenal::Movement(status)==0);
    for(int i=0;i<30;++i)Arsenal::Tick(status,true);assert(!status.frozen && !status.burning && Arsenal::Movement(status)==1);
    auto malformed=v10;malformed["weapons"].push_back({{"baseEquip",3},{"uniqueId",11}});assert(!InventoryRules::Present(SaveCodec::Decode(malformed,1).weapons[2]));
    std::cout<<"Adventure item migration, eight unique identities, 20k deterministic rolls, five targeted caches, full-pack swaps, crafting, isolated stats and bounded combat profiles passed\n";
}
