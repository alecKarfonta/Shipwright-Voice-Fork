#pragma once
#include "CreatureRules.h"
#include "LanternMendRules.h"
#include "z64math.h"
struct Actor;
struct PlayState;
namespace ARPG::Creatures {
bool Enabled();
int ActorId(Kind kind);
bool IsCreature(const Actor* actor);
Actor* Spawn(PlayState* play,Kind kind,Vec3f position);
int NeutralTechnique(Actor* actor,PlayState* play);
float RallySpeed(Actor* actor);
State Status(const Actor* actor);
MendState MendStatus(const Actor* actor);
Actor* MendTarget(const Actor* actor,PlayState* play);
float OnContact(Actor* target,Actor* source,uint32_t flags,uint8_t alignment,float damage);
void DrawGuide();
Actor* SpawnSkulltulaPractice(PlayState* play,int variant);
Actor* SpawnBubblePractice(PlayState* play,int variant);
Actor* SpawnDodongoPractice(PlayState* play);
Actor* SpawnRedeadPractice(PlayState* play,int variant);
Actor* SpawnIronKnucklePractice(PlayState* play,int variant);
Actor* SpawnStingerPractice(PlayState* play,int variant);
Actor* SpawnShellbladePractice(PlayState* play,int variant);
Actor* SpawnArmosPractice(PlayState* play,int variant);
Actor* SpawnAnubisPractice(PlayState* play);
Actor* SpawnGuayPractice(PlayState* play, bool giant = false);
Actor* SpawnPoePractice(PlayState* play);
Actor* SpawnMadScrubPractice(PlayState* play,int variant);
Actor* SpawnBeamosPractice(PlayState* play,int variant);
Actor* SpawnStalchildPractice(PlayState* play,int variant);
Actor* SpawnHandmasterPractice(PlayState* play,int variant);
Actor* SpawnMoblinPractice(PlayState* play,int variant);
Actor* SpawnPeahatPractice(PlayState* play,int variant);
void DrawTesting();
}
