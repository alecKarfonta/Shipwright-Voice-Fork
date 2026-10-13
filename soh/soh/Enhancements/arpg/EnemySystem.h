#pragma once
#include "EnemyRules.h"
#include "z64math.h"
#include <string>
#include <array>
#include <vector>

struct Actor;
struct PlayState;

namespace ARPG::Enemies {
bool Enabled();
Region CurrentRegion();
int LiveCount();
int PopulationTarget();
double PopulationArea();
int WatchCount();
int WildlifeCount();
bool IsWildlife(Actor* actor);
uint32_t WatchAttacks();
uint32_t WatchHits();
uint32_t WatchAssists();
uint32_t WatchSearches();
int WatchSecuringCount();
bool IsWatchman(Actor* actor);
bool IsRegional(Actor* actor);
bool RewardEligible(Actor* actor);
int SpawnPack(PackKind kind, bool testing = false);
void ClearPacks();
bool FindWaystonePosition(Vec3f& point);
std::vector<Vec3f> ExplorationSites();
bool FindQuestSitePosition(Vec3f& point,int salt);
bool QuestSiteSupported(Vec3f& point);
Vec3f SafeSpiritPosition(Vec3f point,const Vec3f& refuge);
bool FindRitualPositions(const Vec3f& center,std::array<Vec3f,3>& points);
std::vector<Actor*> SpawnQuestWave(int count);
Actor* SpawnSharedQuestMember(Vec3f point,int quest,int node);
std::vector<Actor*> SpawnExpeditionWave(PackKind kind, int count);
void DrawSettings();
void DrawTesting();
void DrawHud();
void DrawTargetHealth();
const char* TacticName(Actor* actor);
uint32_t AttackCount(Actor* actor);
}
