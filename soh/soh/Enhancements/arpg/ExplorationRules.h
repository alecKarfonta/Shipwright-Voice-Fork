#pragma once
#include "ARPGTypes.h"
#include <algorithm>
#include <cstdint>
namespace ARPG::Exploration {
// Append-only scene slots: two caches and one gold spider per age. Placement
// must pass the runtime ground/clearance tests; unsafe scenes can have fewer.
inline constexpr int Scenes[] = {0x55,0x51,0x5b,0x52,0x53,0x54,0x56,0x57,0x58,0x59,0x5a,0x5c,0x5d,0x5f,0x60,0x62,0x63,
                                0x00,0x01,0x02,0x03,0x05,0x06,0x07,0x08,0x09,0x0b};
inline int FirstId(int scene, bool adult) {
    for (int i=0;i<int(sizeof(Scenes)/sizeof(Scenes[0]));++i) if (Scenes[i]==scene) return (i*2+int(adult))*3;
    return -1;
}
inline bool Skull(int id) { return id>=0 && id<ARPG_SECRET_COUNT && id%3==2; }
inline int SkullCount(const ARPGSaveData& save) {
    int count=0;for (int i=2;i<ARPG_SECRET_COUNT;i+=3) count+=save.secrets[i].claimed!=0;return count;
}
inline bool Claim(ARPGSaveData& save,int id) {
    if(id<0 || id>=ARPG_SECRET_COUNT || !save.secrets[id].placed || save.secrets[id].claimed) return false;
    save.secrets[id].claimed=1;return true;
}
inline int RewardTier(int native, const ARPGSaveData& save) { return std::clamp((native+SkullCount(save))/5,0,32); }
}
