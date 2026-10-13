#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace ARPG::Arsenal {
enum class Weapon { Other, Sword, Hammer, Bomb, Bow, Slingshot, Boomerang, Hookshot, Nut };
enum class Element { None, Fire, Ice, Light };
enum class Family { Other, Stalfos, Wolfos, Lizalfos };
struct Status { int marked=0,tethered=0,concussed=0,frozen=0,burning=0; float markBonus=.75f,tetherBonus=.75f; };
inline void Tick(Status& s,bool active) {
    if(!active) return;
    for(int* timer:{&s.marked,&s.tethered,&s.concussed,&s.frozen,&s.burning}) if(*timer>0) --*timer;
}
inline float Movement(const Status& s) { return s.frozen?0.f:s.concussed?.6f:s.tethered?.75f:1.f; }
inline float Affinity(Family family,Weapon weapon,Element element) {
    if(family==Family::Stalfos) {
        if(weapon==Weapon::Hammer) return 1.5f;
        if(weapon==Weapon::Bomb) return 1.25f;
        if(weapon==Weapon::Bow || weapon==Weapon::Slingshot) return .75f;
    }
    if(family==Family::Wolfos && element==Element::Fire) return 1.25f;
    if(family==Family::Lizalfos && element==Element::Ice) return 1.25f;
    return 1.f;
}
// [TIME] | [INFO] | Arsenal.Resolve | Native zero-damage stuns set up finishers; they never acquire direct damage.
inline float Resolve(Status& s,Weapon weapon,bool damaging,bool nativeStun) {
    float scale=1;
    if(damaging) {
        if(s.marked && (weapon==Weapon::Bow || weapon==Weapon::Slingshot)) { scale=1+s.markBonus;s.marked=0;s.markBonus=.75f; }
        if(s.tethered && (weapon==Weapon::Hammer || weapon==Weapon::Bomb)) { scale=1+s.tetherBonus;s.tethered=0;s.tetherBonus=.75f; }
        if(weapon==Weapon::Bomb) s.concussed=40;
    }
    if(nativeStun) {
        if(weapon==Weapon::Boomerang) s.marked=80;
        if(weapon==Weapon::Hookshot) s.tethered=80;
        if(weapon==Weapon::Nut) s.concussed=30;
    }
    return scale;
}
struct FaultNode { float forward; int start; };
inline constexpr std::array<FaultNode,3> FaultLine{{{55,0},{120,4},{185,8}}};
inline bool FaultActive(int node,int age) { return node>=0 && node<3 && age>=FaultLine[node].start && age<FaultLine[node].start+3; }
inline const char* FamilyGuide(Family family) {
    switch(family) {
        case Family::Stalfos: return "Armored bones: hammer +50%, bombs +25%, arrows and seeds -25%. Their shield still blocks frontal hits. Get around it before breaking the formation.";
        case Family::Wolfos: return "Fire deals +25% on top of native fire vulnerability. Catch a lowered guard with the boomerang, then finish with a seed or arrow. Guarded claws still deflect attacks.";
        case Family::Lizalfos: return "Ice deals +25% on top of native ice vulnerability. Hookshot an exposed fighter, then follow with a hammer or bomb. Native fire-magic immunity remains.";
        default: return "";
    }
}
}
