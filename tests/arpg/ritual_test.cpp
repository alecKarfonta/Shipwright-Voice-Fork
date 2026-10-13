#include "RitualRules.h"
#include "ARPGSaveCodec.h"
#include "WaystoneState.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace ARPG;
using namespace ARPG::ExpeditionRules;
int main() {
    for(uint32_t seed=1;seed<=10000;++seed) {
        ARPGSaveData save{}; auto& r=save.expedition; assert(Start(r,seed,0,1,20));
        int chosen=-1,rituals=0;
        for(int route=0;route<3;++route) {
            auto kind=RouteObjective(r,route); assert(kind==RouteObjective(r,route));
            if(kind==ExpeditionRules::Ritual) { chosen=route; ++rituals; }
        }
        assert(rituals==1); assert(ChooseRoute(r,chosen)); assert(WaveSize(r)==4 && BeginWave(r,4));
        assert(Protected(r,0) && !Protected(r,1)); assert(!Defeat(r,0) && r.defeatedMask==0);
        ARPG::Ritual::Channel channel{};
        for(int seal:{2,0,1}) {
            for(int tick=0;tick<59;++tick) assert(!ARPG::Ritual::Tick(r,channel,seal,true,false));
            assert(ARPG::Ritual::Tick(r,channel,seal,true,false)); assert(!BreakSeal(r,seal));
            auto view=Waystone::FromExpedition(r);
            for(int i=0;i<3;++i) assert(Waystone::LampTone(view,i)==((r.sealMask&(1<<i))?5:r.mutator));
        }
        assert(!Protected(r,0) && BrokenSeals(r)==3 && r.phase==Combat);
        for(int member:{2,0,3,1}) { assert(Defeat(r,member)); assert(!Defeat(r,member)); }
        assert(r.phase==Reward && r.clears==1 && r.bestDepth==1);
        auto restored=SaveCodec::Decode(SaveCodec::Encode(save),1);
        assert(restored.expedition.objective==ExpeditionRules::Ritual && restored.expedition.phase==Reward);
        assert(Waystone::FromExpedition(restored.expedition).seals==7);
        assert(Claim(restored,seed%3)); auto claimed=restored; assert(!Claim(restored,0) && !memcmp(&claimed,&restored,sizeof(claimed)));
        assert(Continue(restored.expedition) && restored.expedition.sealMask==0);
    }
    ARPGSaveData save{}; auto& r=save.expedition; Start(r,42,0,1,1);
    int route=0; while(RouteObjective(r,route)!=ExpeditionRules::Ritual) ++route;
    ChooseRoute(r,route); BeginWave(r,4); ARPG::Ritual::Channel channel{};
    for(int i=0;i<30;++i) ARPG::Ritual::Tick(r,channel,0,true,false);
    assert(channel.ticks==30);
    ARPG::Ritual::Tick(r,channel,0,true,true); assert(channel.ticks==0 && channel.interrupted==20 && !r.sealMask);
    for(int i=0;i<19;++i) ARPG::Ritual::Tick(r,channel,0,true,false);
    assert(channel.ticks==0); ARPG::Ritual::Tick(r,channel,0,true,false); assert(channel.ticks==1);
    ARPG::Ritual::Tick(r,channel,-1,true,false); assert(channel.ticks==0);
    ARPG::Ritual::Tick(r,channel,0,false,false); assert(channel.ticks==0);
    ARPG::Ritual::Tick(r,channel,0,true,false); ARPG::Ritual::Tick(r,channel,1,true,false); assert(channel.ticks==1 && channel.seal==1);
    assert(!BreakSeal(r,-1) && !BreakSeal(r,3)); BreakSeal(r,2); Defeat(r,1);
    auto recovered=SaveCodec::Decode(SaveCodec::Encode(save),1);
    assert(recovered.expedition.phase==Route && !recovered.expedition.sealMask && !recovered.expedition.defeatedMask);
    assert(RouteObjective(recovered.expedition,route)==ExpeditionRules::Ritual);
    auto legacy=SaveCodec::Encode(save); legacy["expedition"].erase("objective");
    legacy["expedition"]["phase"]=Reward; assert(SaveCodec::Decode(legacy,1).expedition.objective==Gauntlet);
    legacy["expedition"]["objective"]=255; assert(SaveCodec::Decode(legacy,1).expedition.objective==Gauntlet);
    r.phase=Reward; auto before=r; assert(!BreakSeal(r,0) && !memcmp(&before,&r,sizeof(r)));
    std::cout<<"10,000 ritual routes, channel interruption, protection, out-of-order seals, duplicate deaths, atomic claims and v7/v8 recovery passed\n";
}
