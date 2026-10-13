#pragma once
#include "global.h"
#ifdef __cplusplus
extern "C" {
#endif
s32 ARPG_UseArmosVisuals(PlayState* play,Actor* actor);
void ARPG_OverrideArmosLimb(PlayState* play,Actor* actor,s32 limb,Gfx** displayList);
#ifdef __cplusplus
}
#endif
