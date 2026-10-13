#pragma once
#include <cstdint>
#include <libultraship/libultra/controller.h>
#include "soh/OTRGlobals.h"
#include "SkillRules.h"

namespace ARPG::CombatInput {
constexpr uint32_t AbilityButtons = BTN_DUP | BTN_DRIGHT | BTN_DDOWN | BTN_DLEFT |
    BTN_CUSTOM_ABILITY5 | BTN_CUSTOM_ABILITY6;
struct Result { int slot; uint32_t held, pressed, released; };

// Keep item, sword, shield and targeting input intact. Only press edges cast;
// holding a direction or stick click never repeats a technique.
inline Result Route(uint32_t held,uint32_t pressed,uint32_t released,bool active) {
    Result result{-1,held,pressed,released};
    if(!active)return result;
    constexpr uint32_t directions[]={BTN_DUP,BTN_DRIGHT,BTN_DDOWN,BTN_DLEFT};
    for(int direction=0;direction<4;++direction) {
        if(pressed & directions[direction])
            result.slot=SkillRules::ControllerSlot(direction,(held & (BTN_L | BTN_CUSTOM_MODIFIER1))!=0);
    }
    if(pressed & BTN_CUSTOM_ABILITY5)result.slot=4;
    if(pressed & BTN_CUSTOM_ABILITY6)result.slot=5;
    result.held &= ~AbilityButtons;
    result.pressed &= ~AbilityButtons;
    result.released &= ~AbilityButtons;
    return result;
}
}
