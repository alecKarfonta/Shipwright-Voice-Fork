#pragma once
#include "LootCatalog.h"
#include <cmath>
namespace ARPG::AdventureItems {
inline float BlastScale(const ARPGItemInstance& item) { return 1+std::clamp(Loot::Power(item,ARPG_MOD_BLAST),0,75)/100.f; }
inline int Travel(const ARPGItemInstance& item,int frames) { return std::clamp(int(std::lround(frames*(1+std::clamp(Loot::Power(item,ARPG_MOD_REACH),0,100)/100.f))),0,32767); }
inline int Duration(const ARPGItemInstance* item,int frames) { return item?int(std::lround(frames*(1+std::clamp(Loot::Power(*item,ARPG_MOD_CONTROL),0,100)/100.f))):frames; }
inline float Followup(const ARPGItemInstance* item) { return .75f+(item?std::clamp(Loot::Power(*item,ARPG_MOD_FOLLOWUP),0,100)/100.f:0); }
inline bool IsUnique(const ARPGItemInstance* item,int id) { return item && Loot::Unique(*item) && item->uniqueId==id; }
}
