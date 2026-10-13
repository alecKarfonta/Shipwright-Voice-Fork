#pragma once
#include <algorithm>
#include <cstdint>
namespace ARPG::Relics {
// Timers use the engine's 20 Hz gameplay update, never wall time or draw frames.
struct Preparation {
    int guard=0, light=0, anchor=0, cinder=0, steady=0, rainRecovery=0, frostRecovery=0, stormRecovery=0;
    bool airborneShot=false, cinderFinal=false;
    void Tick(bool playing, bool grounded, bool still, bool anchorBoots) {
        if(!playing)return;
        for(int* timer:{&guard,&light,&cinder,&steady,&rainRecovery,&frostRecovery,&stormRecovery})if(*timer>0)--*timer;
        anchor=grounded && still && anchorBoots?std::min(40,anchor+1):0;
        if(!cinder)cinderFinal=false;
    }
    bool Block() { if(!guard)return false;guard=0;light=120;return true; }
    bool ShootLight() { if(!light)return false;light=0;return true; }
    bool Slam() { if(anchor<40)return false;anchor=0;return true; }
};
inline bool AmmoPickup(int id) { return id==4 || id==5 || (id>=8 && id<=13) || id==16 || id==25 || id==26; }
inline float ToolRadius(int reach,float base) { return base*(1+std::clamp(reach,0,100)/100.f); }
}
