#pragma once
#include "WardenPatterns.h"
struct Actor;
struct PlayState;
namespace ARPG::Warden {
struct View {
    Phase phase=Phase::Waiting;
    int oath=0,age=-1;
    float x=0,y=0,z=0,yaw=0;
    int triangles=0;
    bool hit=false;
};
void Reset();
void Tick(PlayState* play,Actor* actor,int oath,bool inControl);
void Draw(PlayState* play);
View GetView();
float OpeningMultiplier(Actor* actor);
}
