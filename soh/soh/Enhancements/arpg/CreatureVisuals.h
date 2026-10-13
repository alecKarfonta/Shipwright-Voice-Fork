#pragma once
#include "CreatureRules.h"
struct Actor;struct PlayState;
namespace ARPG::Creatures {
void DrawCreature(Actor*,PlayState*);
void DrawCreaturePart(PlayState*,int part,float glow=0);
void DrawWarning(PlayState*,const Actor*,float radius,float height,float strength,bool rally=false);
}
