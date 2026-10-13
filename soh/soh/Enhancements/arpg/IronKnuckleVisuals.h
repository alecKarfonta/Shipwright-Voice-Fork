#pragma once
#include "global.h"
#ifdef __cplusplus
extern "C" {
#endif
s32 ARPG_UseIronKnuckleVisuals(PlayState* play,Actor* actor);
void ARPG_OverrideIronKnuckleLimb(PlayState* play,Actor* actor,s32 limb,Gfx** displayList);
s32 ARPG_IsIronKnuckleRearArmor(Gfx* displayList);
#ifdef __cplusplus
}
#endif
