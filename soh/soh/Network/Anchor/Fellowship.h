#pragma once
#include <nlohmann/json.hpp>
namespace Fellowship {
void Reset();
void Receive(const nlohmann::json& packet);
void DrawJournal();
}
