#pragma once
#include "BroodRules.h"
#include "z64math.h"
struct Actor;struct PlayState;
namespace ARPG::Brood {
bool Enabled();
bool IsPod(const Actor* actor);
bool Targetable(const Actor* actor);
State Status(const Actor* actor);
int LivePods(PlayState* play);
int Larvae(PlayState* play);
int AddedLarvae(PlayState* play);
void Clear(PlayState* play);
Actor* Spawn(PlayState* play,Actor* owner,Vec3f position);
void TickBoss(Actor* actor,PlayState* play);
int Waves(const Actor* actor);
void DrawTesting();
}
