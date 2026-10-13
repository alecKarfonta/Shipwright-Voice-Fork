#include "CombatInput.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace ARPG::CombatInput;
    static_assert(sizeof(OSContPad{}.button)==4,"Custom actions must survive the native pad buffer");
    constexpr uint32_t buttons[]={BTN_DUP,BTN_DRIGHT,BTN_DDOWN,BTN_DLEFT,BTN_CUSTOM_ABILITY5,BTN_CUSTOM_ABILITY6};
    constexpr uint32_t native=BTN_A | BTN_B | BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT | BTN_CUP | BTN_Z | BTN_R;
    for(int slot=0;slot<6;++slot) {
        auto result=Route(buttons[slot] | BTN_Z | BTN_R,buttons[slot],0,true);
        assert(result.slot==slot);
        assert(result.held==(BTN_Z | BTN_R) && result.pressed==0);
        result=Route(buttons[slot],0,0,true);
        assert(result.slot==-1); // A held button is not another cast.
        result=Route(0,0,buttons[slot],true);
        assert(result.slot==-1 && result.released==0);
        // Pauses, dialogs and disabled skills preserve native/menu input.
        result=Route(buttons[slot] | native,buttons[slot] | native,buttons[slot],false);
        assert(result.slot==-1 && result.held==(buttons[slot] | native));
        assert(result.pressed==(buttons[slot] | native) && result.released==buttons[slot]);
    }
    auto result=Route(native,native,0,true);
    assert(result.slot==-1 && result.held==native && result.pressed==native);
    for(uint32_t modifier:{BTN_L,BTN_CUSTOM_MODIFIER1}) {
        assert(Route(modifier | BTN_DLEFT,BTN_DLEFT,0,true).slot==4);
        assert(Route(modifier | BTN_DRIGHT,BTN_DRIGHT,0,true).slot==5);
        assert(Route(modifier | BTN_DUP,BTN_DUP,0,true).slot==0);
        assert(Route(modifier | BTN_DDOWN,BTN_DDOWN,0,true).slot==2);
        result=Route(modifier | native,native,0,true);
        assert(result.slot==-1 && result.pressed==native); // LB never steals an item.
    }
    result=Route(BTN_CUSTOM_ABILITY6 | native,BTN_CUSTOM_ABILITY6 | native,0,true);
    assert(result.slot==5 && result.held==native && result.pressed==native);
    // Simultaneous presses resolve deterministically to one ability request.
    assert(Route(AbilityButtons,AbilityButtons,0,true).slot==5);
    assert(Route(BTN_CUSTOM_MODIFIER1,BTN_CUSTOM_MODIFIER1,0,true).slot==-1);
    std::cout << "All six abilities, held/released edges, legacy chords, native combat input and menu gates passed.\n";
}
