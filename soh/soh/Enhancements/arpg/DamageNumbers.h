#pragma once

// [TIME] | [INFO] | DamageNumbers.h | World-space floating combat text
//
// Diablo-style damage numbers over enemies: white for normal hits, yellow
// large for critical strikes, grey "Resist" for absorbed/immune hits. Numbers
// spawn at the struck actor's focus point, drift upward, and fade out.
// Rendering piggybacks the nametag pipeline (OnPlayDrawEnd + billboard text
// quads) so no new renderer is needed.

#include <cstdint>

struct Actor;

namespace ARPG::DamageNumbers {

// Spawn a floating number at the target. damage is the post-multiplier hit
// value; crit selects yellow/large; resisted renders grey and smaller.
void Spawn(Actor* target, float damage, bool crit, bool resisted);

// Toggle gate (reads CVAR each frame; default on when ARPG enabled)
bool Enabled();

} // namespace ARPG::DamageNumbers
