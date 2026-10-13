#pragma once
#include "ExpeditionRules.h"

namespace ARPG::Ritual {
inline constexpr int HoldTicks=60; // Three seconds at the native twenty gameplay updates per second.
inline constexpr float Reach=58.f;
struct Channel { int seal=-1, ticks=0, interrupted=0; };
// [TIME] | [INFO] | Ritual.Channel | Moving away, attacking or taking actual health damage cancels only the unfinished seal.
inline bool Tick(ARPGExpeditionData& run,Channel& state,int nearby,bool canHold,bool damaged) {
    if(state.interrupted>0) { --state.interrupted; }
    if(damaged) { state.interrupted=20; }
    if(run.phase!=ExpeditionRules::Combat || run.objective!=ExpeditionRules::Ritual ||
       nearby<0 || nearby>=3 || (run.sealMask&(1<<nearby)) || !canHold || state.interrupted) {
        state.seal=-1; state.ticks=0; return false;
    }
    if(state.seal!=nearby) { state.seal=nearby; state.ticks=0; }
    if(++state.ticks<HoldTicks) { return false; }
    state.seal=-1; state.ticks=0; return ExpeditionRules::BreakSeal(run,nearby);
}
}
