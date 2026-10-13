#pragma once
#include "SkillPatterns.h"
extern "C" {
#include "z64.h"
}
namespace ARPG::SkillVfx {
void Draw(Actor* actor, PlayState* play, int id, int rune, int age, int duration,
          const Vec3f& origin, int16_t yaw, const SkillPatterns::Frame& shapes,
          const Vec3f* chain, int chainCount);
}
