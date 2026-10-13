#pragma once
#include <cstdint>
namespace ARPG::Brood {
constexpr int FallTicks=30, HatchTicks=100, DelayTicks=40, FadeTicks=18;
constexpr int PodCap=2, LarvaCap=3, WaveCooldown=300;
enum class Phase : uint8_t { Falling, Incubating, Broken, Hatched };
struct State { Phase phase=Phase::Falling; int age=0, delayed=0; bool delayUsed=false; };
inline bool Terminal(const State& s){return s.phase==Phase::Broken || s.phase==Phase::Hatched;}
inline bool Advance(State& s,bool active) {
    if(!active)return false;
    if(s.delayed>0){--s.delayed;return false;}
    ++s.age;
    if(s.phase==Phase::Falling && s.age>=FallTicks){s.phase=Phase::Incubating;s.age=0;}
    else if(s.phase==Phase::Incubating && s.age>=HatchTicks){s.phase=Phase::Hatched;s.age=0;return true;}
    return false;
}
inline void Break(State& s){if(!Terminal(s)){s.phase=Phase::Broken;s.age=0;s.delayed=0;}}
inline bool Delay(State& s){if(s.phase!=Phase::Incubating || s.delayUsed)return false;s.delayUsed=true;s.delayed=DelayTicks;return true;}
inline bool Retired(const State& s){return Terminal(s) && s.age>=FadeTicks;}
inline const char* Guide(){return "Broodfall: Gohma marks the ground with silk before dropping two pods. Stun her during their fall to break the summons. Landed pods hatch after five seconds. Cut them apart or shatter them with fire, bombs or a hammer. A nut, boomerang, hookshot or ice hit delays each pod once. Only two additional hatchlings can remain alive alongside her normal brood.";}
}
