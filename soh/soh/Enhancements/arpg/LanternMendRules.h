#pragma once
#include "CreatureRules.h"

namespace ARPG::Creatures {
constexpr int MendWindup = 44;
constexpr int MendLimit = 2;
struct MendState { uint64_t target = 0; int spent = 0, completed = 0; };
enum class MendResult { Waiting, Cancelled, Completed };
inline int MendAmount(int health, int maximum, int received) {
    if (health <= 0 || health >= maximum || received >= MendLimit) return 0;
    return std::min(maximum - health, std::clamp(maximum / 5, 1, 6));
}
inline bool NeedsMend(int health, int maximum, int received) {
    return health * 5 <= maximum * 3 && MendAmount(health, maximum, received) > 0;
}
inline bool BeginMend(State& state, MendState& mend, uint64_t target) {
    if (!target || mend.target || mend.spent >= MendLimit || state.phase != Phase::Roam || state.cooldown) return false;
    mend.target = target; ++mend.spent; state.phase = Phase::Windup; state.timer = 0;
    return true;
}
// Spending the lantern charge at the start makes interruption a permanent gain.
// No health changes occur until the entire native 20 Hz warning has elapsed.
inline MendResult AdvanceMend(State& state, MendState& mend, bool active, bool connected) {
    if (!active || !mend.target) return MendResult::Waiting;
    if (state.phase != Phase::Windup || !connected) {
        if (state.phase == Phase::Windup) { state.phase = Phase::Recover; state.timer = 20; }
        state.cooldown = std::max(state.cooldown, 80);
        return MendResult::Cancelled;
    }
    if (state.exposed > 0) --state.exposed;
    if (++state.timer < MendWindup) return MendResult::Waiting;
    ++mend.completed; state.phase = Phase::Recover; state.timer = 40; state.cooldown = 160;
    return MendResult::Completed;
}
}
