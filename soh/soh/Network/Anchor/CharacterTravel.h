#pragma once
#include <string>

namespace CharacterTravel {
// Local saves are the source of truth, even on a relay configured for legacy save syncing.
bool Enabled();
bool Checkpoint(std::string& error);
std::string Summary();
} // namespace CharacterTravel
