#pragma once

#include <cstdint>
struct Actor;
struct PlayState;

namespace ARPG::Spells {
bool Cast(PlayState* play, uint8_t skillId);
float DamageMultiplier(Actor* source);
void CancelAura(PlayState* play);
void Reflect(PlayState* play, float strength);
}
