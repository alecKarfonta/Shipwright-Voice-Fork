#pragma once
#include <memory>
namespace LUS { class ControlDeck; }
namespace ARPG::ControllerConfig {
std::shared_ptr<LUS::ControlDeck> CreateControlDeck();
void ApplyCombatBindings();
bool GamepadConnected();
}
