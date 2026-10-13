#include "CraftingRules.h"
#include "ARPGSaveCodec.h"
#include <cassert>
#include <iostream>
using namespace ARPG;
static ARPGItemInstance Base(int slot=0) { ARPGItemInstance item{};item.slot=slot;item.baseEquip=Loot::WeaponSlot(slot)?1:2;item.itemLevel=35;return item; }
static void Unchanged(const ARPGSaveData& before,const ARPGSaveData& after) { assert(SaveCodec::Encode(before)==SaveCodec::Encode(after)); }
int main() {
    ARPGSaveData save{};save.level=1;SkillRules::Initialize(save);
    save.backpack[0]=Base();save.materials[Loot::Essence]=100;
    assert(Crafting::AddSocket(save,0)); assert(Crafting::AddSocket(save,0));
    assert(save.materials[Loot::Essence]==64 && save.backpack[0].socketCount==2);
    assert(Crafting::Receive(save,19,2) && Crafting::Receive(save,20,2));
    auto original=save;
    assert(!Crafting::Socket(save,-1,19) && !Crafting::Socket(save,0,25) && !Crafting::Socket(save,0,999)); Unchanged(original,save);
    assert(Crafting::Socket(save,0,19));
    original=save; assert(!Crafting::AddSocket(save,0)); assert(!Crafting::Salvage(save,0)); Unchanged(original,save);
    assert(Crafting::Socket(save,0,20)); assert(Loot::Runework(save.backpack[0])==1);
    assert(Loot::Power(save.backpack[0],ARPG_MOD_FLAME)==31);
    original=save; assert(!Crafting::Socket(save,0,20)); Unchanged(original,save);
    auto wrong=save.backpack[0];std::swap(wrong.sockets[0],wrong.sockets[1]);assert(!Loot::Runework(wrong));
    wrong=save.backpack[0];wrong.socketCount=3;assert(!Loot::Runework(wrong));
    wrong=save.backpack[0];wrong.rarity=ARPG_RARITY_MAGIC;assert(!Loot::Runework(wrong));
    wrong=save.backpack[0];wrong.slot=ARPG_SLOT_BOOTS;assert(!Loot::Runework(wrong));
    save.materials[19]=999;original=save;assert(!Crafting::Extract(save,0));Unchanged(original,save);
    save.materials[19]=1;assert(Crafting::Extract(save,0));assert(save.materials[19]==2 && save.materials[20]==2 && save.materials[Loot::Essence]==56);
    assert(!Loot::Runework(save.backpack[0]) && save.backpack[0].socketCount==2);
    assert(Crafting::Receive(save,1,3));assert(Crafting::UpgradeGem(save,1));assert(save.materials[1]==0 && save.materials[7]==1);
    assert(Loot::SocketBonus(7,0).power==10 && Loot::SocketBonus(13,0).power==15);
    original=save;assert(!Crafting::UpgradeGem(save,13) && !Crafting::UpgradeGem(save,19) && !Crafting::UpgradeGem(save,-1));Unchanged(original,save);
    assert(Crafting::Socket(save,0,7));assert(Crafting::Extract(save,0));
    assert(Crafting::Salvage(save,0));assert(!InventoryRules::Present(save.backpack[0]));
    for(int id=1;id<=Loot::UniqueCount;++id) {
        auto item=Base(Loot::Uniques[id].slot);Loot::IdentifyUnique(item,id);
        assert(Loot::Unique(item) && InventoryRules::Valid(item));
        assert(SaveCodec::EncodeItem(SaveCodec::DecodeItem(SaveCodec::EncodeItem(item),item.slot))==SaveCodec::EncodeItem(item));
        save.backpack[id]=item;
    }
    for(int id=1;id<=Loot::SetCount;++id) {
        for(int slot=0;slot<4;++slot) { auto item=Base(slot);Loot::IdentifySet(item,id);save.equipment[slot]=item; }
        assert(Loot::SetPieces(save,id)==4);
        assert(Loot::SetPieces(save,id,3)==2);
        assert(Loot::StyleBonuses(save).power[Loot::Sets[id].style.tree]==Loot::Sets[id].style.power);
        assert(Loot::StyleBonuses(save,7).power[Loot::Sets[id].style.tree]==0);
        for(auto bonus:Loot::Sets[id].two) {
            int raw=Loot::Power(save.equipment[0],bonus.modId)+Loot::Power(save.equipment[1],bonus.modId);
            assert(Loot::TotalPower(save,bonus.modId,3)==raw+bonus.power);
        }
    }
    for(size_t word=1;word<std::size(Loot::Runeworks);++word) {
        const auto& recipe=Loot::Runeworks[word];
        for(int slot=0;slot<4;++slot) {
            auto item=Base(slot);item.socketCount=recipe.count;
            for(int n=0;n<recipe.count;++n) { item.sockets[n]=recipe.runes[n]; }
            assert(Loot::Runework(item)==((recipe.slotMask&(1<<slot))?int(word):0));
            assert(InventoryRules::Valid(item));
        }
    }
    uint32_t rng=123,repeat=123;int uniques=0,sets=0,sockets=0;
    for(int sample=0;sample<30000;++sample) {
        auto item=Base(sample%4);item.rarity=sample%4;auto twin=item;
        Loot::RollIdentity(item,rng);Loot::RollIdentity(twin,repeat);
        assert(SaveCodec::EncodeItem(item)==SaveCodec::EncodeItem(twin));assert(InventoryRules::Valid(item));
        if(item.rarity==ARPG_RARITY_UNIQUE) { assert(Loot::Unique(item)||Loot::Set(item)); }
        uniques+=bool(Loot::Unique(item));sets+=bool(Loot::Set(item));sockets+=item.socketCount>0;
    }
    assert(uniques>1000 && sets>1000 && sockets>5000);
    auto encoded=SaveCodec::Encode(save);assert(SaveCodec::Encode(SaveCodec::Decode(encoded,9))==encoded);
    auto malformed=SaveCodec::EncodeItem(Base());malformed["socketCount"]=255;malformed["sockets"]={19,-1,20};malformed["uniqueId"]=255;malformed["setId"]=255;
    auto safe=SaveCodec::DecodeItem(malformed,0);assert(safe.socketCount==3 && safe.sockets[0]==19 && safe.sockets[1]==0 && safe.sockets[2]==0 && InventoryRules::Valid(safe));
    auto legacy=SaveCodec::EncodeItem(Base());legacy.erase("uniqueId");legacy.erase("setId");legacy.erase("socketCount");legacy.erase("sockets");
    auto old=SaveCodec::DecodeItem(legacy,0);assert(old.baseEquip==2 && !old.socketCount && !old.uniqueId && !old.setId);
    save={}; save.backpack[0]=Base();save.backpack[0].socketCount=2;save.backpack[0].sockets[0]=save.backpack[0].sockets[1]=19;save.materials[19]=998;save.materials[Loot::Essence]=100;
    original=save;assert(!Crafting::Extract(save,0));Unchanged(original,save);
    assert(!Crafting::Receive(save,0) && !Crafting::Receive(save,26) && !Crafting::Receive(save,1,-1));Unchanged(original,save);
    // Direct placement targets the chosen mount, including gaps, on worn and backpack gear.
    for(int slot=0;slot<ARPG_LOOT_SLOT_COUNT;++slot)for(bool equipped:{false,true}) {
        save={};
        auto& item=equipped?InventoryRules::Equipped(save,slot):save.backpack[7];
        item=Base(slot);item.socketCount=3;save.materials[19]=2;save.materials[20]=1;
        const int index=equipped?slot:7;
        assert(Crafting::Socket(save,equipped,index,19,2));
        assert(!item.sockets[0] && !item.sockets[1] && item.sockets[2]==19 && save.materials[19]==1);
        assert(InventoryRules::Valid(item));
        auto roundtrip=SaveCodec::DecodeItem(SaveCodec::EncodeItem(item),slot);
        assert(SaveCodec::EncodeItem(roundtrip)==SaveCodec::EncodeItem(item));
        original=save;
        for(int socket:{-2,2,3,999})assert(!Crafting::Socket(save,equipped,index,20,socket));
        assert(!Crafting::Socket(save,equipped,index,1,0));
        assert(!Crafting::Socket(save,equipped,index,999,0));
        assert(!Crafting::Socket(save,equipped,-1,20,0));
        assert(!Crafting::Socket(save,equipped,equipped?ARPG_LOOT_SLOT_COUNT:ARPG_BACKPACK_CAPACITY,20,0));
        Unchanged(original,save);
        assert(Crafting::Socket(save,equipped,index,20,1));
        assert(Crafting::Socket(save,equipped,index,19));
        assert(item.sockets[0]==19 && item.sockets[1]==20 && item.sockets[2]==19);
        assert(!save.materials[19] && !save.materials[20] && InventoryRules::Valid(item));
    }
    save={};save.backpack[0]=Base();save.backpack[0].socketCount=2;save.materials[19]=save.materials[20]=1;
    assert(Crafting::Socket(save,false,0,20,1) && !Loot::Runework(save.backpack[0]));
    assert(Crafting::Socket(save,false,0,19,0) && Loot::Runework(save.backpack[0])==1);
    std::cout<<"Loot catalog, all recipes/sets, 30,000 deterministic rolls, crafting conservation and save migration passed.\n";
}
