#pragma once
#include "AdventureRelicRules.h"
struct Actor;
struct PlayState;
namespace ARPG::Relics {
float OnContact(Actor* target,Actor* source,uint32_t flags,uint8_t alignment,float damage);
bool Collision(PlayState*,void* at,void* ac,void* atInfo,void* acInfo,void* point);
void ArrowReleased(Actor*);
void ShieldBlock();
bool PreserveStick();
void StickBroken();
bool BottleReleased();
bool BoomerangUpdate(Actor*);
void SteerBombchu(Actor*);
void Reset(PlayState*);
int LiveCount();
int Wet(Actor*);
Preparation State();
}
