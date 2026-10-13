#pragma once
struct PlayState;
extern "C" {
#include "z64math.h"
}
namespace ARPG::SkillAuras {
bool Start(PlayState* play,int skill,int rune,int duration,bool followsBuff=false,const Vec3f* position=nullptr);
void CancelAll(PlayState* play);
int LiveCount(PlayState* play);
}
