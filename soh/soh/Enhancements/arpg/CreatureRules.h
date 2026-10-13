#pragma once
#include <algorithm>
#include <cstdint>

namespace ARPG::Creatures {
enum class Kind : uint8_t { Sentinel, Wraith };
enum class Phase : uint8_t { Roam, Windup, Recover, Stagger, Dead };
enum class Strike : uint8_t { Other, Blade, Hammer, Blast, Arrow, Light, Ice, Boomerang, Hookshot, Nut };
struct State {
    Phase phase = Phase::Roam;
    int timer = 0, cooldown = 40, exposed = 0;
    uint32_t attacks = 0, interruptions = 0;
};
struct Situation { bool active, visible, supported; float distance, homeDistance; };
inline const char* Name(Kind kind) { return kind==Kind::Sentinel ? "Cairn Sentinel" : "Lantern Wraith"; }
inline int Windup(Kind kind) { return kind==Kind::Sentinel ? 36 : 32; }
inline int Recovery(Kind kind) { return kind==Kind::Sentinel ? 48 : 36; }
inline void Interrupt(State& state,int duration) {
    if(state.phase==Phase::Dead) return;
    if(state.phase==Phase::Windup) ++state.interruptions;
    state.timer=std::max(state.phase==Phase::Stagger?state.timer:0,duration);
    state.phase=Phase::Stagger;state.cooldown=std::max(state.cooldown,duration+30);
}
// Simulation ticks are native 20 Hz updates. Pauses never spend a warning or opening.
inline bool Advance(Kind kind,State& state,const Situation& s) {
    if(!s.active || state.phase==Phase::Dead) return false;
    if(state.exposed>0) --state.exposed;
    if(state.cooldown>0) --state.cooldown;
    if(state.phase==Phase::Stagger || state.phase==Phase::Recover) {
        if(--state.timer<=0) { state.phase=Phase::Roam;state.timer=0; }
        return false;
    }
    if(state.phase==Phase::Windup) {
        if(!s.visible || !s.supported || s.homeDistance>1100) { state.phase=Phase::Recover;state.timer=20;return false; }
        if(++state.timer>=Windup(kind)) {
            state.phase=Phase::Recover;state.timer=Recovery(kind);state.cooldown=Recovery(kind)+40;
            ++state.attacks;return true;
        }
        return false;
    }
    float reach=kind==Kind::Sentinel ? 190.f : 480.f;
    if(!state.cooldown && s.visible && s.supported && s.homeDistance<1000 && s.distance<reach && s.distance>35) {
        state.phase=Phase::Windup;state.timer=0;
    }
    return false;
}
inline float Receive(Kind kind,State& state,Strike strike,bool damaging,bool acceptedStun) {
    if(state.phase==Phase::Dead) return 0;
    bool breaker=strike==Strike::Hammer || strike==Strike::Blast;
    bool snare=strike==Strike::Boomerang || strike==Strike::Hookshot;
    if(kind==Kind::Sentinel) {
        if(damaging && breaker) { state.exposed=100;Interrupt(state,24);return 1.5f; }
        if(acceptedStun && strike==Strike::Nut) Interrupt(state,20);
        if(damaging && strike==Strike::Ice) Interrupt(state,30);
        if(state.exposed) return 1.5f;
        return strike==Strike::Blade || strike==Strike::Arrow ? .4f : 1.f;
    }
    if(acceptedStun && snare) { state.exposed=100;Interrupt(state,60); }
    else if(acceptedStun && strike==Strike::Nut) Interrupt(state,30);
    else if(damaging) Interrupt(state,12);
    if(!damaging) return 1.f;
    float scale=strike==Strike::Light ? 2.f : (strike==Strike::Arrow || strike==Strike::Ice) ? 1.5f : strike==Strike::Blade ? .75f : 1.f;
    return state.exposed ? scale*1.5f : scale;
}
inline const char* Guide(Kind kind) {
    return kind==Kind::Sentinel ?
        "A stone-plated crawler. Its shell resists blades, arrows and seeds. Hammer or explosive hits pry it open for five seconds. Step sideways when its crown flares: the three ground strikes follow the direction it committed to. Ice interrupts its charge." :
        "A roaming lantern spirit that casts spectral bolts and tends wounded allies. A braided green tether warns of a two-second heal: hit or stun the Wraith, finish its ally, or separate them with cover. It has only two healing attempts; interruption spends one. Wraiths cannot heal each other. Arrows and seeds deal extra damage; light is strongest. Boomerang or hookshot expose its core for five seconds. Shields block its bolts.";
}
}
