#pragma once
#include <string>
#include "WaystoneState.h"
#include "RelayRules.h"
struct Actor;
namespace ARPG::Expeditions {
bool Busy();
std::string StartRestriction();
bool Start(int slot);
bool Choose(int choice);
bool Claim(int choice);
bool Continue();
void End();
void OnFileLoaded();
int Remaining();
Waystone::View GetWaystoneView();
Relay::View GetRelayView();
float DamageMultiplier(Actor* actor,unsigned int flags);
void DrawJournal();
void DrawHud();
}
