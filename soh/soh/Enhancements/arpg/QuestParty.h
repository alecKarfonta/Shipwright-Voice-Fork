#pragma once
#include "ARPGTypes.h"
#include <nlohmann/json_fwd.hpp>
namespace ARPG::Quests::Party {
void Tick();
void Reset();
void Receive(const nlohmann::json& packet);
void Publish(const ARPGCampaignData& before);
bool Shared(int quest);
bool Online(int quest);
bool CanClaim(int quest);
bool PlaceSites(int quest);
float PackHealthRatio(int quest);
void Share(int quest);
void DrawOverview();
void DrawDetail(int quest);
void DrawBoard();
void DrawTracker(int quest);
int Invitations();
}
