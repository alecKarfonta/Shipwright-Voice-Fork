#include "ArsenalRules.h"
#include "ItemPowerRules.h"
#include "ARPGSaveCodec.h"
#include "InventoryRules.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace ARPG;
int main() {
    using namespace Arsenal;
    Status s{};
    assert(Resolve(s,Weapon::Boomerang,false,true)==1 && s.marked==80);
    assert(Resolve(s,Weapon::Bow,false,false)==1 && s.marked==80);
    assert(Resolve(s,Weapon::Sword,true,false)==1 && s.marked==80);
    assert(Resolve(s,Weapon::Slingshot,true,false)==1.75f && !s.marked);
    assert(Resolve(s,Weapon::Slingshot,true,false)==1);
    Resolve(s,Weapon::Hookshot,false,true);assert(s.tethered==80 && Movement(s)==.75f);
    assert(Resolve(s,Weapon::Bomb,true,false)==1.75f && !s.tethered && s.concussed==40 && Movement(s)==.6f);
    for(int i=0;i<100;++i)Tick(s,false);assert(s.concussed==40);
    for(int i=0;i<40;++i)Tick(s,true);assert(Movement(s)==1);
    Resolve(s,Weapon::Hookshot,false,false);assert(!s.tethered);
    Resolve(s,Weapon::Boomerang,false,true);for(int i=0;i<80;++i)Tick(s,true);assert(Resolve(s,Weapon::Bow,true,false)==1);
    assert(Affinity(Family::Stalfos,Weapon::Hammer,Element::None)>Affinity(Family::Stalfos,Weapon::Bow,Element::None));
    assert(Affinity(Family::Wolfos,Weapon::Bow,Element::Fire)==1.25f);
    assert(Affinity(Family::Lizalfos,Weapon::Bow,Element::Ice)==1.25f);
    for(int family=0;family<4;++family)for(int weapon=0;weapon<9;++weapon)for(int element=0;element<4;++element) {
        float scale=Affinity(Family(family),Weapon(weapon),Element(element));assert(std::isfinite(scale) && scale>=.75 && scale<=1.5);
    }
    for(int node=0;node<3;++node) { int active=0;for(int age=-1;age<24;++age)active+=FaultActive(node,age);assert(active==3); }
    assert(!FaultActive(-1,0) && !FaultActive(3,0));
    using namespace ItemPowers;
    ARPGSaveData save{};save.level=35;SkillRules::Initialize(save);
    auto base=[](int slot){ARPGItemInstance item{};item.slot=slot;item.baseEquip=Loot::WeaponSlot(slot)?1:2;item.itemLevel=35;return item;};
    for(int id=1;id<=Loot::UniqueCount;++id) {
        save={};auto& item=InventoryRules::Equipped(save,Loot::Uniques[id].slot);item=base(Loot::Uniques[id].slot);Loot::IdentifyUnique(item,id);
        auto bit=Bit(Loot::Uniques[id].style.combatPower);assert(Loot::StyleBonuses(save).combatPowers==bit);
        assert(!Loot::StyleBonuses(save,0).combatPowers);
    }
    for(int set=1;set<=Loot::SetCount;++set) {
        save={};for(int slot=0;slot<4;++slot) { save.equipment[slot]=base(slot);Loot::IdentifySet(save.equipment[slot],set); }
        assert(Loot::StyleBonuses(save).combatPowers==Bit(Loot::Sets[set].style.combatPower));
        for(int missing=0;missing<4;++missing)assert(!Loot::StyleBonuses(save,15^(1<<missing)).combatPowers);
    }
    for(int word=1;word<int(std::size(Loot::Runeworks));++word)for(int slot=0;slot<4;++slot) {
        save={};auto& item=save.equipment[slot];item=base(slot);const auto& r=Loot::Runeworks[word];item.socketCount=r.count;
        for(int i=0;i<r.count;++i)item.sockets[i]=r.runes[i];
        assert(Loot::StyleBonuses(save).combatPowers==((r.slotMask&(1<<slot))?Bit(r.style.combatPower):0));
    }
    State state{};Sync(state,7,1);auto catalogSave=save;auto initialSave=SaveCodec::Encode(save);
    Cast(state,ARPG_SKILL_SWORD_CLEAVE);assert(state.furnace==120);
    auto follow=BasicHit(state);assert(follow.swordRecovery==20 && !state.furnace && state.weaveHits==1);
    assert(!BasicHit(state).swordRecovery);BasicHit(state);assert(FreeSpell(state,ARPG_SKILL_MAGIC_CHAIN));
    assert(!FreeSpell(state,ARPG_SKILL_MAGIC_INFUSE) && !FreeSpell(state,ARPG_SKILL_MAGIC_HOLY_AURA) && !FreeSpell(state,-1));
    Cast(state,ARPG_SKILL_MAGIC_INFUSE);assert(state.weaveHits==3);
    Cast(state,ARPG_SKILL_MAGIC_FROST_NOVA);assert(!state.weaveHits && !FreeSpell(state,ARPG_SKILL_MAGIC_CHAIN));
    Cast(state,ARPG_SKILL_GUARD_DODGE);HealthLoss(state,0);assert(!state.reprisal && state.guard);
    HealthLoss(state,3);assert(state.healing==3 && state.recharge==200 && !state.guard);
    HealthLoss(state,40);assert(state.healing==3);
    assert(BasicHit(state).healing==3 && !BasicHit(state).healing);
    Cast(state,ARPG_SKILL_GUARD_DODGE);assert(!state.guard);
    Sync(state,0,2);Sync(state,7,1);assert(state.recharge==200 && !state.weaveHits);
    for(int i=0;i<200;++i)ItemPowers::Tick(state,false);assert(state.recharge==200);
    for(int i=0;i<200;++i)ItemPowers::Tick(state,true);
    Cast(state,ARPG_SKILL_GUARD_DODGE);HealthLoss(state,99);assert(BasicHit(state).healing==8);
    for(int i=0;i<3;++i)BasicHit(state);
    for(int i=0;i<120;++i)ItemPowers::Tick(state,true);assert(!state.weaveHits && !state.healing && !state.furnace);
    auto key=EquipmentKey(save,15);save.equipment[0].sockets[0]=19;assert(EquipmentKey(save,15)!=key);
    save=catalogSave;assert(SaveCodec::Encode(save)==initialSave);
    int freeSpells=0;state={};Sync(state,Bit(Power::MoonwellWeave),1);
    for(int i=0;i<3000;++i) { BasicHit(state);if(FreeSpell(state,ARPG_SKILL_MAGIC_METEOR)){++freeSpells;Cast(state,ARPG_SKILL_MAGIC_METEOR);} }
    assert(freeSpells==1000);
    std::cout<<"Arsenal setup/finisher conservation, native immunity inputs, pause/expiry, family affinities, staggered fault line, all unique/set/runework traits, gear changes, free-cast rotation and healing caps passed\n";
}
