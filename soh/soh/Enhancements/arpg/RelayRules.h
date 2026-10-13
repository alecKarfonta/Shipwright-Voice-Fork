#pragma once
#include "ExpeditionRules.h"

namespace ARPG::Relay {
struct Point { float x=0,y=0,z=0; };
enum class SpiritState { Bound, Loose, Carried, Delivered };
struct Spirit { SpiritState state=SpiritState::Bound; Point position{}; int unattended=0; };
struct State { std::array<Spirit,3> spirits{}; int carried=-1, pickupLock=0; };
struct Input { Point player{}, safeDrop{}, refuge{}; std::array<Point,3> lanterns{}; bool active=false, damaged=false, canInteract=true; };
enum class Event { None, PickedUp, Dropped, Delivered, Returned };
inline constexpr float PickupReach=44, DeliveryReach=58;
inline constexpr int PickupDelay=30, ReturnTicks=600;
inline bool Active(const ARPGExpeditionData& run) {
    return run.phase==ExpeditionRules::Combat && run.objective==ExpeditionRules::SpiritRelay;
}
inline bool Near(Point a,Point b,float reach) { return std::abs(a.y-b.y)<36 && std::hypot(a.x-b.x,a.z-b.z)<=reach; }
// [TIME] | [INFO] | Relay.Release | Only an accepted guard defeat can release its one spirit. Duplicate callbacks cannot manufacture charges.
inline bool Release(const ARPGExpeditionData& run,State& state,int member,Point point) {
    if(!Active(run) || member<1 || member>3 || !(run.defeatedMask&(1<<member))) { return false; }
    auto& spirit=state.spirits[member-1];
    if(spirit.state!=SpiritState::Bound) { return false; }
    spirit={SpiritState::Loose,point,0}; return true;
}
// [TIME] | [INFO] | Relay.Tick | Movement and attacks are allowed; damage drops the carried spirit, and no objective time advances while paused.
inline Event Tick(ARPGExpeditionData& run,State& state,const Input& input) {
    if(!Active(run) || !input.active) { return Event::None; }
    if(state.pickupLock>0) { --state.pickupLock; }
    if(input.damaged) {
        state.pickupLock=PickupDelay;
        if(state.carried>=0) {
            auto& spirit=state.spirits[state.carried]; spirit={SpiritState::Loose,input.safeDrop,0};
            state.carried=-1; return Event::Dropped;
        }
    }
    Event result=Event::None;
    for(auto& spirit:state.spirits) {
        if(spirit.state==SpiritState::Loose && ++spirit.unattended>=ReturnTicks) {
            spirit.position=input.refuge; spirit.unattended=0; result=Event::Returned;
        }
    }
    if(!input.canInteract) { return result; }
    if(state.carried>=0) {
        for(int i=0;i<3;++i) {
            if(!(run.sealMask&(1<<i)) && Near(input.player,input.lanterns[i],DeliveryReach)) {
                run.sealMask|=1<<i;
                state.spirits[state.carried].state=SpiritState::Delivered; state.carried=-1;
                return Event::Delivered;
            }
        }
        return result;
    }
    if(state.pickupLock) { return result; }
    int nearest=-1; float distance=PickupReach+1;
    for(int i=0;i<3;++i) {
        const auto& spirit=state.spirits[i];
        float d=std::hypot(input.player.x-spirit.position.x,input.player.z-spirit.position.z);
        if(spirit.state==SpiritState::Loose && Near(input.player,spirit.position,PickupReach) && d<distance) { nearest=i; distance=d; }
    }
    if(nearest>=0) {
        state.spirits[nearest].state=SpiritState::Carried; state.carried=nearest;
        return Event::PickedUp;
    }
    return result;
}
struct LanternView { Point position{}; bool lit=false; };
struct SpiritView { Point position{}; bool visible=false,carried=false; };
struct View { bool visible=false; std::array<LanternView,3> lanterns{}; std::array<SpiritView,3> spirits{}; int carried=-1; };
}
