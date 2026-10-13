#include "ExpeditionRules.h"
#include "ARPGSaveCodec.h"
#include "WaystoneState.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <set>
using namespace ARPG;
using namespace ARPG::ExpeditionRules;
void ClearDepth(ARPGExpeditionData& r) {
    int route=0; while(RouteObjective(r,route)!=Gauntlet) { ++route; }
    assert(ChooseRoute(r,route));
    for(int wave=0;wave<3;++wave) {
        int size=WaveSize(r); assert(r.phase==Queued && r.wave==wave);
        assert(!BeginWave(r,size-1) && r.phase==Queued); assert(BeginWave(r,size));
        assert(!Defeat(r,-1) && !Defeat(r,4));
        assert(Defeat(r,0)); auto once=r; assert(!Defeat(r,0) && !memcmp(&r,&once,sizeof(r)));
        for(int i=1;i<size;++i) { assert(Defeat(r,i)); }
    }
    assert(r.phase==Reward);
}
int main() {
    std::set<int> mutators; std::set<int> gems,runes,uniques,sets;
    for(uint32_t seed=1;seed<=10000;++seed) {
        ARPGSaveData save{}; auto& r=save.expedition;
        assert(Start(r,seed,seed%4,1+seed%3,1+seed%50));
        auto routes=Routes(r); assert(routes==Routes(r)); assert(routes[0]!=routes[1] && routes[0]!=routes[2] && routes[1]!=routes[2]);
        for(auto m:routes) { assert(m>=0 && m<5); mutators.insert(m); }
        auto before=r; assert(!Start(r,42,0,1,1) && !memcmp(&r,&before,sizeof(r)));
        assert(!ChooseRoute(r,3)); ClearDepth(r); assert(r.bestDepth==1 && r.clears==1);
        auto cache=SaveCodec::Decode(SaveCodec::Encode(save),999); assert(cache.expedition.phase==Reward && cache.expedition.seed==seed);
        auto a=RewardFor(r,0),b=RewardFor(cache.expedition,0); // Compare serialized fields: aggregate padding is not part of a deterministic reward.
        assert(SaveCodec::EncodeItem(a.item)==SaveCodec::EncodeItem(b.item) && a.materials==b.materials && InventoryRules::Valid(a.item));
        assert(a.item.slot==seed%4 && a.item.baseEquip==1+seed%3 && a.item.rarity==ARPG_RARITY_RARE);
        for(int choice=1;choice<3;++choice) { auto offer=RewardFor(r,choice); for(int i=1;i<Loot::Essence;++i) { if(offer.materials[i]) { (choice==1?runes:gems).insert(i); } } }
        assert(Claim(save,seed%3)); auto claimed=save; assert(!Claim(save,0) && !memcmp(&save,&claimed,sizeof(save)));
        assert(Continue(r) && r.depth==2); ClearDepth(r); assert(Claim(save,1)); assert(Continue(r)); ClearDepth(r);
        auto third=RewardFor(r,0).item; assert(third.rarity==ARPG_RARITY_UNIQUE && InventoryRules::Valid(third));
        if(third.uniqueId) { uniques.insert(third.uniqueId); } if(third.setId) { sets.insert(third.setId); }
        assert(r.bestDepth==3 && r.clears==3 && r.runs==1); End(r); assert(r.phase==Idle && r.bestDepth==3);
    }
    assert(mutators.size()==5 && gems.size()==18 && runes.size()==6 && uniques.size()==11 && sets.size()==3);
    ARPGSaveData save{}; auto& r=save.expedition; assert(Start(r,77,0,1,30)); ClearDepth(r);
    auto item=RewardFor(r,0).item; for(auto& slot:save.backpack) { slot=item; }
    auto full=save; assert(!Claim(save,0) && !memcmp(&save,&full,sizeof(save)));
    save.backpack[3]={}; save.materials[Loot::Essence]=999; full=save;
    for(int choice=0;choice<3;++choice) { assert(!Claim(save,choice) && !memcmp(&save,&full,sizeof(save))); }
    save.materials[Loot::Essence]=0; assert(Claim(save,0) && save.itemsFound==1 && save.materials[Loot::Essence]==4);
    assert(Continue(r)); int gauntlet=0; while(RouteObjective(r,gauntlet)!=Gauntlet) ++gauntlet; assert(ChooseRoute(r,gauntlet)); assert(BeginWave(r,3)); assert(Defeat(r,0));
    auto loaded=SaveCodec::Decode(SaveCodec::Encode(save),42); assert(loaded.expedition.phase==Route && loaded.expedition.depth==2);
    assert(loaded.expedition.bestDepth==1 && loaded.expedition.defeatedMask==0 && Routes(loaded.expedition)==Routes(r));
    End(r,true); assert(r.phase==Failed); assert(Start(r,5,3,2,50) && r.bestDepth==1 && r.depth==1 && r.runs==2);
    r.phase=Rest; r.depth=UINT32_MAX; auto last=r; assert(!Continue(r) && !memcmp(&r,&last,sizeof(r)));
    for(uint32_t depth:{1u,20u,50u,1000u,UINT32_MAX}) { r.depth=depth; assert(ItemLevel(r)==50 && std::isfinite(IncomingScale(r)) && IncomingScale(r)>0 && OutgoingScale(r)<=8); }
    assert(SaveCodec::Decode(nlohmann::json::object(),1).expedition.phase==Idle);
    for(auto malformed:{nlohmann::json{{"phase",99}},nlohmann::json{{"phase",Reward},{"depth",0}},nlohmann::json{{"phase",Reward},{"depth",1},{"seed",1},{"baseEquip",1},{"targetSlot",255}}}) {
        auto bad=SaveCodec::Decode({{"expedition",malformed}},1); assert(bad.expedition.phase==Failed);
    }
    for(int phase=Queued;phase<=Rest;++phase) {
        for(int oath=0;oath<5;++oath) for(int wave=0;wave<3;++wave) {
            r.phase=phase;r.mutator=oath;r.wave=wave;auto state=Waystone::FromExpedition(r);
            assert(state.oath==oath && state.cleared==(phase>=Reward?3:wave));
            assert(state.cache==(phase==Reward?Waystone::Earned:phase==Rest?Waystone::Claimed:Waystone::Locked));
            for(int lamp=0;lamp<3;++lamp) assert(Waystone::LampTone(state,lamp)==(lamp<state.cleared?5:lamp==state.cleared?oath:6));
        }
    }
    float lid=0;for(int tick=0;tick<30;++tick) lid=Waystone::AdvanceLid(lid,Waystone::Earned);
    assert(lid==1 && Waystone::LidAngle(lid)==-1.35f);
    for(int tick=0;tick<30;++tick) lid=Waystone::AdvanceLid(lid,Waystone::Locked);
    assert(lid==0 && Waystone::LidAngle(lid)==0);
    std::cout<<"10,000 expedition seeds; routes, three-wave completion, duplicate guards, targeting, rewards, atomic capacity, save recovery, depth bounds, oath/lamp/cache presentation passed\n";
}
