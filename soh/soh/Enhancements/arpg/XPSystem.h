#pragma once
#include <cstdint>
struct Actor;

namespace XPSystem {
// [TIME] | [INFO] | XPSystem.ResetDefeatCredit | Call only after a native enemy completes a new life, never on stun, healing or an interrupted death.
void ResetDefeatCredit(Actor* actor);
uint32_t ExperienceFor(const Actor* actor);
void RemoteKill(uint32_t xp, uint8_t tier, uint8_t level, const char* name);
}

// [TIME] | [INFO] | ARPG_OnRemoteKill | Receives a teammate's ARPG_ENEMY_KILL on the game thread
// (global trampoline into XPSystem::RemoteKill; implemented in XPSystem.cpp).
void ARPG_OnRemoteKill(uint32_t xp, uint8_t tier, uint8_t level, const char* name);
