#pragma once

#include "soh/cvar_prefixes.h"

#define CVAR_SPRINT_ENABLED_NAME CVAR_ENHANCEMENT("Sprint.Enabled")
#define CVAR_SPRINT_MULT_NAME CVAR_ENHANCEMENT("Sprint.SpeedMult")
#define CVAR_SPRINT_DRAIN_NAME CVAR_ENHANCEMENT("Sprint.StaminaSeconds")
#define CVAR_SPRINT_REGEN_NAME CVAR_ENHANCEMENT("Sprint.RegenSeconds")
#define CVAR_SPRINT_BTN_NAME CVAR_ENHANCEMENT("Sprint.Btn")

// Sprint enhancement in play: hold the configured button while running for a short burst of speed,
// limited by a stamina meter that drains while sprinting and regenerates while walking or standing.

// True when the sprint enhancement is switched on.
bool Sprint_Enabled();

// Speed multipliers sprint applies to the player, 1.0f when it doesn't.
// Composed into the shared run/jump speed handling alongside the Bunny Hood and Speed Modifier factors.
float Ship_GetSprintRunFactor();
float Ship_GetSprintJumpFactor();

// Current stamina, normalized 0.0f (empty) .. 1.0f (full), for HUD display.
float Sprint_GetStamina01();

// True while the player is actively sprinting this tick.
bool Sprint_IsSprinting();

// True while stamina is fully drained and sprinting is locked until partially recovered.
bool Sprint_IsExhausted();
