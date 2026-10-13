#pragma once
#include "ExpeditionRules.h"

namespace ARPG::Waystone {
enum Part { Shrine, Heart, FirstLamp, SecondLamp, ThirdLamp, Coffer, Lid, Treasure, Iron, Wild, Cinder, Winter, Storm, Ward, Seal, PartCount };
enum Cache { Locked, Earned, Claimed };
struct State { int oath=0, cleared=0; Cache cache=Locked; bool ritual=false; int seals=0; };
struct WardView { float x=0,y=0,z=0,progress=0; bool broken=false; };
struct View { bool visible=false; float x=0,y=0,z=0,yaw=0,open=0; State state{}; bool wards=false; std::array<WardView,3> sites{}; };
// [TIME] | [INFO] | Waystone.State | World presentation is derived from the saved reward state; it cannot award or consume loot.
inline State FromExpedition(const ARPGExpeditionData& run) {
    using namespace ExpeditionRules;
    Cache cache=run.phase==Reward?Earned:run.phase==Rest?Claimed:Locked;
    bool ritual=run.objective!=ExpeditionRules::Gauntlet;
    return {std::clamp<int>(run.mutator,0,4),cache==Locked?(ritual?BrokenSeals(run):std::clamp<int>(run.wave,0,2)):3,
            cache,ritual,cache==Locked?run.sealMask:7};
}
// [TIME] | [INFO] | Waystone.Lamps | Gold marks completed waves, the oath color marks the current wave, and the remaining sockets stay dark.
inline int LampTone(const State& state,int lamp) {
    if(state.ritual) { return state.seals&(1<<lamp)?5:state.oath; }
    return lamp<state.cleared?5:lamp==state.cleared?state.oath:6;
}
inline float AdvanceLid(float current,Cache cache) {
    return std::clamp(current+(cache==Locked?-.045f:.045f),0.f,1.f);
}
inline float LidAngle(float open) { open=std::clamp(open,0.f,1.f); return -1.35f*open*open*(3-2*open); }
}
