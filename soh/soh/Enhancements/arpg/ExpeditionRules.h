#pragma once
#include "CraftingRules.h"
#include <cmath>

namespace ARPG::ExpeditionRules {
enum Phase : uint8_t { Idle, Route, Queued, Combat, Reward, Rest, Failed };
enum Objective : uint8_t { Gauntlet, Ritual, SpiritRelay, ObjectiveCount };
inline constexpr const char* ObjectiveNames[]={"Oathbound gauntlet","Break the ritual","Spirit relay"};
inline constexpr const char* ObjectiveDescriptions[]={
    "Defeat three waves, ending with the oath's warden.",
    "Break three ward stones to expose the protected warden, then defeat the entire guard.",
    "Defeat spirit bearers, carry their released spirits to three lanterns, then slay the exposed warden."};
struct Modifier { const char* name; const char* description; int hp, damage, speed, enchantment; };
inline constexpr Modifier Modifiers[] = {
    {"Iron procession", "Armored foes resist sword strikes. Hammers, arrows and explosives punish openings.", 120,100,100,0},
    {"Wild hunt", "Faster enemies strike harder, but have less health. Arrows and seeds punish their light protection.", 80,125,120,0},
    {"Cinder oath", "Fire-enchanted foes erupt when slain. Ice punishes them; make space before the killing blow.", 100,110,100,11},
    {"Winter vigil", "Frost-enchanted foes chill on contact. Fire exploits their weakness.", 110,100,100,12},
    {"Storm covenant", "Storm-enchanted foes punish close contact. Hammers exploit their weakness; light is resisted.", 100,110,110,13}
};
inline bool Running(const ARPGExpeditionData& r) { return r.phase>=Route && r.phase<=Rest; }
inline uint32_t DepthSeed(const ARPGExpeditionData& r, uint32_t salt=0) {
    uint32_t x=r.seed ^ (r.depth*0x9E3779B9u) ^ salt;
    x^=x>>16; x*=0x7FEB352Du; x^=x>>15; x*=0x846CA68Bu; return (x^(x>>16))|1;
}
inline std::array<int,3> Routes(const ARPGExpeditionData& r) {
    uint32_t rng=DepthSeed(r); uint8_t pool[]={0,1,2,3,4};
    return {Rules::TakeDistinct(rng,pool,5,0),Rules::TakeDistinct(rng,pool,5,1),Rules::TakeDistinct(rng,pool,5,2)};
}
// [TIME] | [INFO] | ExpeditionRules.Objectives | Every set of route offers includes all three objectives; reloads retain the same choices.
inline Objective RouteObjective(const ARPGExpeditionData& r,int choice) {
    if(choice<0 || choice>=3) { return Gauntlet; }
    int ritual=int(DepthSeed(r,0x5EA15u)%3);
    return choice==ritual?Ritual:choice==(ritual+1)%3?SpiritRelay:Gauntlet;
}
inline bool Start(ARPGExpeditionData& r,uint32_t seed,int slot,int base,int level) {
    if(Running(r) || slot<0 || slot>=ARPG_LOOT_SLOT_COUNT || base<1 || base>(Loot::WeaponSlot(slot)?1:3)) { return false; }
    r.seed=seed?seed:1; r.depth=1; r.phase=Route; r.wave=r.encounterSize=r.defeatedMask=0;
    r.mutator=r.objective=r.sealMask=0; r.targetSlot=slot; r.baseEquip=base; r.baseLevel=std::clamp(level,1,50);
    r.runs=Rules::SaturatingAdd(r.runs,1); return true;
}
inline bool ChooseRoute(ARPGExpeditionData& r,int choice) {
    if(r.phase!=Route || choice<0 || choice>=3) { return false; }
    r.mutator=Routes(r)[choice]; r.objective=RouteObjective(r,choice);
    r.wave=r.encounterSize=r.defeatedMask=r.sealMask=0; r.phase=Queued; return true;
}
inline int WaveSize(const ARPGExpeditionData& r) { return r.objective!=Gauntlet || r.wave==1?4:3; }
inline bool Warden(const ARPGExpeditionData& r,int member) { return member==0 && (r.objective!=Gauntlet || r.wave==2); }
inline bool Protected(const ARPGExpeditionData& r,int member) {
    return r.phase==Combat && r.objective!=Gauntlet && Warden(r,member) && r.sealMask!=7;
}
inline int BrokenSeals(const ARPGExpeditionData& r) { return (r.sealMask&1)+((r.sealMask>>1)&1)+((r.sealMask>>2)&1); }
inline bool BreakSeal(ARPGExpeditionData& r,int seal) {
    if(r.phase!=Combat || r.objective!=Ritual || seal<0 || seal>=3 || (r.sealMask&(1<<seal))) { return false; }
    r.sealMask|=1<<seal; return true;
}
inline bool BeginWave(ARPGExpeditionData& r,int size) {
    if(r.phase!=Queued || size!=WaveSize(r)) { return false; }
    r.encounterSize=size; r.defeatedMask=r.sealMask=0; r.phase=Combat; return true;
}
// [TIME] | [INFO] | ExpeditionRules.Defeat | Only registered wave members count; repeated death callbacks cannot create rewards.
inline bool Defeat(ARPGExpeditionData& r,int member) {
    if(r.phase!=Combat || member<0 || member>=r.encounterSize || (r.defeatedMask&(1<<member)) || Protected(r,member)) { return false; }
    r.defeatedMask|=1<<member;
    if(r.defeatedMask==((1<<r.encounterSize)-1)) {
        if(r.objective!=Gauntlet || r.wave==2) {
            r.phase=Reward; r.bestDepth=std::max(r.bestDepth,r.depth); r.clears=Rules::SaturatingAdd(r.clears,1);
        } else { ++r.wave; r.phase=Queued; }
    }
    return true;
}
inline bool Continue(ARPGExpeditionData& r) {
    if(r.phase!=Rest || r.depth==UINT32_MAX) { return false; }
    ++r.depth; r.phase=Route; r.wave=r.encounterSize=r.defeatedMask=r.sealMask=0; return true;
}
inline void End(ARPGExpeditionData& r,bool failed=false) {
    r.phase=failed?Failed:Idle; r.encounterSize=r.defeatedMask=r.sealMask=0;
}
inline int ItemLevel(const ARPGExpeditionData& r) { return static_cast<int>(std::min<uint64_t>(50,uint64_t(r.baseLevel)+uint64_t(r.depth)*2)); }
inline float IncomingScale(const ARPGExpeditionData& r) { return 1.f/std::sqrt(1.f+std::min(r.depth,10000u)*.10f); }
inline float OutgoingScale(const ARPGExpeditionData& r) { return std::min(8.f,1.f+std::sqrt(float(r.depth))*.12f); }
struct Offer { ARPGItemInstance item{}; std::array<uint16_t,ARPG_MATERIAL_COUNT> materials{}; };
inline Offer RewardFor(const ARPGExpeditionData& r,int choice) {
    Offer offer{}; if(choice<0 || choice>2 || r.targetSlot>=ARPG_LOOT_SLOT_COUNT || r.baseEquip<1 || r.baseEquip>(Loot::WeaponSlot(r.targetSlot)?1:3)) { return offer; }
    uint32_t rng=DepthSeed(r,0xA11CEu+choice); int level=ItemLevel(r);
    if(choice==0) {
        auto& item=offer.item; item.slot=r.targetSlot; item.baseEquip=r.baseEquip; item.itemLevel=level;
        item.rarity=r.depth%3==0?ARPG_RARITY_UNIQUE:ARPG_RARITY_RARE; item.modCount=2;
        auto pool=Loot::Pool(item.slot);
        for(int n=0;n<2;++n) {
            int id=Rules::TakeDistinct(rng,pool.ids.data(),pool.count,n);
            int power=id==ARPG_MOD_AEGIS||id==ARPG_MOD_MAGICAFF?Rules::RandomRange(rng,1,3):
                id==ARPG_MOD_SWIFT||id==ARPG_MOD_KEEN?Rules::RandomRange(rng,4,10):Rules::RandomRange(rng,12,22)+level/3;
            item.mods[n]={uint8_t(id),int16_t(power)};
        }
        Loot::RollIdentity(item,rng);
        offer.materials[Loot::Essence]=4;
    } else {
        int count=std::min<uint32_t>(8,2+r.depth/3);
        if(choice==1) { offer.materials[Rules::RandomRange(rng,Loot::RuneFirst,level>=24?Loot::RuneLast:Loot::RuneFirst+3)]=count; }
        else { int grade=level>=32?2:level>=14?1:0; offer.materials[Rules::RandomRange(rng,1,6)+grade*6]=count+1; }
        offer.materials[Loot::Essence]=std::min<uint32_t>(40,8+r.depth/2);
    }
    return offer;
}
inline bool Claim(ARPGSaveData& save,int choice) {
    auto& r=save.expedition; if(r.phase!=Reward || choice<0 || choice>2) { return false; }
    const auto offer=RewardFor(r,choice);
    if(choice==0 && (!InventoryRules::Valid(offer.item) || InventoryRules::FreeSlot(save)<0)) { return false; }
    for(int i=1;i<ARPG_MATERIAL_COUNT;++i) { if(offer.materials[i] && !Crafting::CanReceive(save,i,offer.materials[i])) { return false; } }
    if(choice==0) { InventoryRules::Collect(save,offer.item); }
    for(int i=1;i<ARPG_MATERIAL_COUNT;++i) { save.materials[i]+=offer.materials[i]; }
    r.phase=Rest; return true;
}
// [TIME] | [INFO] | ExpeditionRules.Reload | A save cannot recreate live enemies; retry the unfinished depth with the same seed and route offers.
inline void Recover(ARPGExpeditionData& r) {
    if(r.objective>=ObjectiveCount) { r.objective=Gauntlet; }
    r.sealMask=0;
    if(Running(r)) { r.baseLevel=std::max<uint8_t>(1,r.baseLevel); }
    if(r.phase>Failed || (Running(r) && (!r.depth || !r.seed || r.baseEquip<1 || r.baseEquip>(Loot::WeaponSlot(r.targetSlot)?1:3) || r.targetSlot>=ARPG_LOOT_SLOT_COUNT))) { End(r,true); }
    if(r.phase==Queued || r.phase==Combat) { r.phase=Route; r.wave=r.encounterSize=r.defeatedMask=0; }
}
}
