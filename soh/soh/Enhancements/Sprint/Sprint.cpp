#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/Sprint/Sprint.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/OTRGlobals.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
extern PlayState* gPlayState;
}

// [TIME] | [INFO] | Sprint.defaults | defaults shared with the menu widgets in SohMenuEnhancements.cpp
static constexpr int32_t CVAR_SPRINT_ENABLED_DEFAULT = 0;
static constexpr float CVAR_SPRINT_MULT_DEFAULT = 1.5f;
static constexpr int32_t CVAR_SPRINT_DRAIN_DEFAULT = 5;  // seconds of full sprint from full stamina
static constexpr int32_t CVAR_SPRINT_REGEN_DEFAULT = 3;  // seconds to refill from empty while moving
static constexpr int32_t CVAR_SPRINT_BTN_DEFAULT = BTN_CUSTOM_MODIFIER1;

#define CVAR_SPRINT_ENABLED_VALUE CVarGetInteger(CVAR_SPRINT_ENABLED_NAME, CVAR_SPRINT_ENABLED_DEFAULT)
#define CVAR_SPRINT_MULT_VALUE CVarGetFloat(CVAR_SPRINT_MULT_NAME, CVAR_SPRINT_MULT_DEFAULT)
#define CVAR_SPRINT_DRAIN_VALUE CVarGetInteger(CVAR_SPRINT_DRAIN_NAME, CVAR_SPRINT_DRAIN_DEFAULT)
#define CVAR_SPRINT_REGEN_VALUE CVarGetInteger(CVAR_SPRINT_REGEN_NAME, CVAR_SPRINT_REGEN_DEFAULT)
#define CVAR_SPRINT_BTN_VALUE CVarGetInteger(CVAR_SPRINT_BTN_NAME, CVAR_SPRINT_BTN_DEFAULT)

// [TIME] | [INFO] | Sprint.state | live stamina state; intentionally not persisted to the save file
static float sStamina = 1.0f;
static bool sSprinting = false;
static bool sExhausted = false;

bool Sprint_Enabled() {
    return CVAR_SPRINT_ENABLED_VALUE != 0;
}

float Ship_GetSprintRunFactor() {
    return sSprinting ? CVAR_SPRINT_MULT_VALUE : 1.0f;
}

float Ship_GetSprintJumpFactor() {
    // [TIME] | [INFO] | Ship_GetSprintJumpFactor | no added jump height; nonzero only so the vanilla
    // jump-speed clamp lifts and sprint run momentum carries into the jump (same lift as Bunny Hood)
    return sSprinting ? CVAR_SPRINT_MULT_VALUE : 1.0f;
}

float Sprint_GetStamina01() {
    return sStamina;
}

bool Sprint_IsSprinting() {
    return sSprinting;
}

bool Sprint_IsExhausted() {
    return sExhausted;
}

// [TIME] | [INFO] | Sprint.CanSprintHere | sprinting only applies while free-running; blocked in
// cutscenes/dialog/death, first person, swimming, on horseback, climbing, crawling and while carrying things
static bool Sprint_CanSprintHere(Player* player) {
    u32 flags1 = player->stateFlags1;

    if (flags1 & (PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_TALKING | PLAYER_STATE1_DEAD |
                  PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_INPUT_DISABLED |
                  PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_WATER | PLAYER_STATE1_CLIMBING_LADDER |
                  PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_DAMAGED)) {
        return false;
    }
    if (player->stateFlags2 & PLAYER_STATE2_CRAWLING) {
        return false;
    }
    if (player->rideActor != NULL) {
        return false;
    }
    return true;
}

// [TIME] | [INFO] | Sprint.Update | stamina state machine, one tick per player update (20Hz)
static void Sprint_Update() {
    Player* player = GET_PLAYER(gPlayState);
    if (player == NULL) {
        return;
    }

    Input* input = &gPlayState->state.input[0];
    bool buttonHeld = CVAR_SPRINT_BTN_VALUE != 0 && CHECK_BTN_ALL(input->cur.button, CVAR_SPRINT_BTN_VALUE);
    bool moving = player->linearVelocity != 0.0f || input->cur.stick_y != 0 || input->cur.stick_x != 0;

    bool wantsSprint = buttonHeld && moving && !sExhausted && Sprint_CanSprintHere(player) &&
        !(input->cur.button & (BTN_Z | BTN_R)); // Targeting and guarding keep their normal movement.

    if (wantsSprint) {
        sSprinting = true;
        sStamina -= 1.0f / (CVAR_SPRINT_DRAIN_VALUE * 20.0f);
        if (sStamina <= 0.0f) {
            sStamina = 0.0f;
            sSprinting = false;
            sExhausted = true;
        }
    } else {
        sSprinting = false;
        // [TIME] | [INFO] | Sprint.Update | regen doubles while standing still; exhaustion clears at 30%
        float regenRate = (player->linearVelocity == 0.0f) ? 2.0f : 1.0f;
        sStamina += regenRate / (CVAR_SPRINT_REGEN_VALUE * 20.0f);
        if (sStamina >= 1.0f) {
            sStamina = 1.0f;
        }
        if (sExhausted && sStamina >= 0.3f) {
            sExhausted = false;
        }
    }
}

// [TIME] | [INFO] | RegisterSprint | hooks re-evaluated whenever the enable or button cvars change;
// toggling off clears live state so the HUD hides and no stale boost lingers
static void RegisterSprint() {
    if (!Sprint_Enabled()) {
        sSprinting = false;
        sExhausted = false;
        sStamina = 1.0f;
    }
    COND_HOOK(OnPlayerUpdate, Sprint_Enabled(), Sprint_Update);
}

static RegisterShipInitFunc initFunc(RegisterSprint, { CVAR_SPRINT_ENABLED_NAME, CVAR_SPRINT_BTN_NAME });
