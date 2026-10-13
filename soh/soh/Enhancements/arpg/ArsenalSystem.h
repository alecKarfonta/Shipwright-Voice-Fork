#pragma once
#include "ArsenalRules.h"
struct Actor;
struct PlayState;
namespace ARPG::Arsenal {
Weapon Classify(uint32_t flags);
Element ElementFor(uint32_t flags);
Family FamilyFor(Actor* actor);
float OnContact(Actor* target,Actor* source,uint32_t flags,uint8_t alignment,float damage);
float MovementMultiplier(Actor* actor);
Status GetStatus(Actor* actor);
Status* MutableStatus(Actor* actor);
bool Collision(PlayState* play,void* at,void* ac);
bool IsFault(Actor* actor);
int LiveFaults(PlayState* play);
void Reset(PlayState* play);
void Detonate(Actor* bomb);
void DrawGuide();
int BossWeaponDamage(Actor* actor,uint32_t flags,int nativeDamage);
}
