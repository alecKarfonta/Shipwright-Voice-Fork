#pragma once
#include "LootCatalog.h"
#include "SkillRules.h"

namespace ARPG::ItemPowers {
using Power=Loot::CombatPower;
constexpr int Window=120, ReprisalRecharge=200, FurnaceRecovery=20;
constexpr uint8_t Bit(Power power) { return power==Power::None?0:1u<<(int(power)-1); }
inline bool OffensiveSpell(int id) {
    return id==ARPG_SKILL_MAGIC_FLAME_BURST || id==ARPG_SKILL_MAGIC_FROST_NOVA ||
           id==ARPG_SKILL_MAGIC_CHAIN || id==ARPG_SKILL_MAGIC_METEOR;
}
// [TIME] | [INFO] | ItemPowers.State | Runtime-only preparations never enter a save or advance the loot RNG.
struct State {
    uint8_t powers=0;
    uint64_t equipmentKey=0;
    int furnace=0, weave=0, weaveHits=0, guard=0, reprisal=0, healing=0, recharge=0;
};
struct FollowUp { int swordRecovery=0, healing=0; };
inline uint64_t EquipmentKey(const ARPGSaveData& save,uint8_t mask) {
    uint64_t key=14695981039346656037ull;
    auto add=[&](uint32_t value) { key=(key^value)*1099511628211ull; };
    add(mask);
    for(int slot=0;slot<ARPG_LOOT_SLOT_COUNT;++slot) {
        if(slot<ARPG_SLOT_COUNT_ && !(mask&(1<<slot))) continue;
        const auto& item=slot<ARPG_SLOT_COUNT_?save.equipment[slot]:save.weapons[slot-ARPG_SLOT_COUNT_];
        add(item.slot);add(item.baseEquip);add(item.uniqueId);add(item.setId);add(item.rarity);
        add(item.itemLevel);add(item.socketCount);
        for(auto socket:item.sockets) add(socket);
        add(item.modCount);for(auto mod:item.mods) { add(mod.modId);add(uint16_t(mod.power)); }
    }
    return key;
}
inline void Sync(State& state,uint8_t powers,uint64_t key) {
    if(state.powers==powers && state.equipmentKey==key) return;
    // Equipment changes discard prepared effects but cannot bypass the guardian recharge.
    int recharge=state.recharge;
    state={};state.powers=powers;state.equipmentKey=key;state.recharge=recharge;
}
inline void Tick(State& state,bool active) {
    if(!active) return;
    for(int* timer:{&state.furnace,&state.weave,&state.guard,&state.reprisal,&state.recharge}) if(*timer>0) --*timer;
    if(!state.weave) state.weaveHits=0;
    if(!state.reprisal) state.healing=0;
}
inline bool FreeSpell(const State& state,int id) {
    return (state.powers&Bit(Power::MoonwellWeave)) && state.weave>0 && state.weaveHits==3 && OffensiveSpell(id);
}
inline void Cast(State& state,int id) {
    if(!SkillRules::Castable(id)) return;
    if(FreeSpell(state,id)) { state.weave=state.weaveHits=0; }
    if((state.powers&Bit(Power::FurnaceRhythm)) && SkillRules::Sword(id)) state.furnace=Window;
    if((state.powers&Bit(Power::BriarReprisal)) && ARPG_SKILLS[id].tree==ARPG_TREE_GUARDIAN && !state.recharge)
        state.guard=Window;
}
inline void HealthLoss(State& state,int loss) {
    if(loss<=0 || !(state.powers&Bit(Power::BriarReprisal)) || !state.guard || state.recharge) return;
    state.guard=0;state.reprisal=Window;state.healing=std::min(8,loss);state.recharge=ReprisalRecharge;
}
// Caller supplies only credited basic sword contacts, after native hard-surface and protected-target gates.
inline FollowUp BasicHit(State& state) {
    FollowUp result;
    if((state.powers&Bit(Power::FurnaceRhythm)) && state.furnace) { result.swordRecovery=FurnaceRecovery;state.furnace=0; }
    if((state.powers&Bit(Power::MoonwellWeave))) { state.weaveHits=std::min(3,state.weaveHits+1);state.weave=Window; }
    if((state.powers&Bit(Power::BriarReprisal)) && state.reprisal) { result.healing=state.healing;state.reprisal=state.healing=0; }
    return result;
}
}
