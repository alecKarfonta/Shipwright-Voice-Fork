#include "ControllerConfig.h"
#include "soh/OTRGlobals.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include <libultraship/libultraship.h>
#include <libultraship/controller/controldeck/ControlDeck.h>
#include <libultraship/controller/controldevice/controller/mapping/ControllerDefaultMappings.h>
#include <ship/controller/controldevice/controller/mapping/sdl/SDLButtonToButtonMapping.h>
#include <array>

namespace ARPG::ControllerConfig {
namespace {
struct Binding { uint32_t action; SDL_GameControllerButton button; };
constexpr std::array<Binding,6> combatBindings{{
    {BTN_CLEFT,SDL_CONTROLLER_BUTTON_X}, {BTN_CDOWN,SDL_CONTROLLER_BUTTON_Y},
    {BTN_CRIGHT,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
    {BTN_CUSTOM_MODIFIER1,SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
    {BTN_CUSTOM_ABILITY5,SDL_CONTROLLER_BUTTON_LEFTSTICK},
    {BTN_CUSTOM_ABILITY6,SDL_CONTROLLER_BUTTON_RIGHTSTICK}
}};
class Defaults : public LUS::ControllerDefaultMappings {
public:
    Defaults() {
        auto buttons=GetDefaultSDLButtonToButtonMappings();
        // LB is sprint, not the native L minimap toggle. Menus read raw LB/RB.
        buttons[BTN_L].erase(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        for(auto binding:combatBindings)buttons[binding.action].insert(binding.button);
        Ship::ControllerDefaultMappings::SetDefaultSDLButtonToButtonMappings(buttons);
    }
};
}

std::shared_ptr<LUS::ControlDeck> CreateControlDeck() {
    return std::make_shared<LUS::ControlDeck>(std::vector<CONTROLLERBUTTONS_T>{
        BTN_CUSTOM_MODIFIER1,BTN_CUSTOM_MODIFIER2,BTN_CUSTOM_ABILITY5,BTN_CUSTOM_ABILITY6,
        BTN_CUSTOM_OCARINA_NOTE_D4,BTN_CUSTOM_OCARINA_NOTE_F4,BTN_CUSTOM_OCARINA_NOTE_A4,
        BTN_CUSTOM_OCARINA_NOTE_B4,BTN_CUSTOM_OCARINA_NOTE_D5,BTN_CUSTOM_OCARINA_DISABLE_SONGS,
        BTN_CUSTOM_OCARINA_PITCH_UP,BTN_CUSTOM_OCARINA_PITCH_DOWN
    },std::make_shared<Defaults>(),std::unordered_map<CONTROLLERBUTTONS_T,std::string>{
        {BTN_A,"A"},{BTN_B,"B"},{BTN_L,"L"},{BTN_R,"R"},{BTN_Z,"Z"},{BTN_START,"Start"},
        {BTN_CLEFT,"CLeft"},{BTN_CRIGHT,"CRight"},{BTN_CUP,"CUp"},{BTN_CDOWN,"CDown"},
        {BTN_DLEFT,"DLeft"},{BTN_DRIGHT,"DRight"},{BTN_DUP,"DUp"},{BTN_DDOWN,"DDown"}
    });
}

void ApplyCombatBindings() {
    auto deck=Ship::Context::GetRawInstance()->GetControlDeck();
    auto controller=deck->GetControllerByPort(0);
    // Existing profiles already have saved bindings, so changing defaults alone
    // would leave them on the old layout. Change only these six physical buttons.
    // Preserve keyboard, axes, dead zones, rumble and the other controller ports.
    if(controller->HasConfig()) {
        controller->ReloadAllMappingsFromConfig();
        for(const auto& [mask,button]:controller->GetAllButtons()) {
            for(const auto& binding:combatBindings) {
                const auto id="P0-B"+std::to_string(mask)+"-SDLB"+std::to_string(binding.button);
                if(button->GetButtonMappingById(id))button->ClearButtonMapping(id);
            }
        }
        for(const auto& binding:combatBindings) {
            auto button=controller->GetButton(binding.action);
            auto mapping=std::make_shared<Ship::SDLButtonToButtonMapping>(0,binding.action,binding.button);
            button->AddButtonMapping(mapping);
            mapping->SaveToConfig();
            button->SaveButtonMappingIdsToConfig();
        }
    }
    CVarSetInteger(CVAR_ENHANCEMENT("Sprint.Btn"),BTN_CUSTOM_MODIFIER1);
    CVarSetInteger(CVAR_ENHANCEMENT("Sprint.Enabled"),1);
    CVarSetInteger(CVAR_ENHANCEMENT("DpadEquips"),0);
    CVarSetInteger(CVAR_IMGUI_CONTROLLER_NAV,1);
    CVarSetInteger(CVAR_ENHANCEMENT("ARPG.CombatControllerV1"),1);
    CVarSave();
    ShipInit::Init(CVAR_ENHANCEMENT("Sprint.Btn"));
}

bool GamepadConnected() {
    return !Ship::Context::GetRawInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager()
        ->GetConnectedSDLGamepadsForPort(0).empty();
}

static void Migrate() {
    if(CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"),1) &&
       !CVarGetInteger(CVAR_ENHANCEMENT("ARPG.CombatControllerV1"),0))ApplyCombatBindings();
}
static RegisterShipInitFunc init(Migrate);
}
