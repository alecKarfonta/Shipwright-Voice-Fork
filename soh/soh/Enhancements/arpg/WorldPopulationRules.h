#pragma once
#include "EnemyRegions.h"
#include <cmath>
#include <cstdint>

namespace ARPG::Enemies {
// One roaming pair per 3 million square world units, before safety limits.
inline int AreaPopulationBudget(double safeArea, int density, int safetyCap, int freeActorSlots) {
    if (!std::isfinite(safeArea) || safeArea<=0) { return 0; }
    int limit=std::min(std::clamp(safetyCap,4,96),std::max(0,freeActorSlots));
    if (limit<2) { return 0; }
    double demand=std::ceil(safeArea/3000000.0*std::clamp(density,50,150)/100.0)*2;
    return int(std::clamp(demand,2.0,double(limit)));
}
// Budget complete route attempts, not actor updates. Frame changes (including
// scene resets and counter wrap) refill it without accumulating unused credit.
struct PatrolPlanningBudget {
    uint32_t frame = 0;
    int remaining = 8;
    bool Take(uint32_t now) {
        if (frame != now) { frame=now;remaining=8; }
        if (remaining==0) { return false; }
        --remaining;return true;
    }
};
struct WildlifeHabitat { bool birds=false, butterflies=false, fish=false; };
inline WildlifeHabitat WildlifeFor(int scene,bool adult=false) {
    if (scene==0x58 && adult) { return {}; } // Frozen Zora's Domain.
    switch (scene) {
        case 0x51: case 0x52: case 0x53: case 0x55: case 0x56:
        case 0x5B: case 0x5F: case 0x63: return {true,true,false};
        case 0x54: case 0x57: return {true,true,true};
        case 0x58: case 0x59: return {false,false,true};
        default: return {};
    }
}
inline int WildlifeBudget(double area,int freeSlots) {
    // Two inhabitants per 12 million square units; towns use a small floor.
    if (!std::isfinite(area) || area<0) { return 0; }
    return std::min(std::max(0,freeSlots),int(std::clamp(std::ceil(area/12000000.0)*2,6.0,24.0)));
}
struct WorldActivity {
    const char* watchName;
    int watchLimit;
    bool borderConflict;
};
// These patrols use the Hylian soldier actor. A regional label is not a
// substitute for an appropriate inhabitant: only explicitly approved human
// patrol locations receive them. Other cultures retain their authored NPCs.
inline WorldActivity ActivityFor(int scene, bool adult) {
    switch (scene) {
        case 0x51: return { "Field watch", 4, true };
        case 0x63: return { "Ranch watch", 4, true };
        default: break;
    }
    if (adult) { return { "", 0, false }; }
    switch (scene) {
        case 0x5F: return { "Castle watch", 4, true };
        case 0x1B: case 0x1C: return { "Gate watch", 2, false };
        case 0x1E: case 0x1F: return { "Alley watch", 1, false };
        case 0x20: case 0x21: return { "Market watch", 3, false };
        default: return { "", 0, false };
    }
}
inline bool IsSettlementEntrance(int scene) {
    switch (scene) {
        case 0x1B: case 0x1C: case 0x1D: case 0x52: case 0x53: case 0x55:
        case 0x58: case 0x5D: case 0x5F: case 0x62: case 0x63: return true;
        default: return false;
    }
}
}
