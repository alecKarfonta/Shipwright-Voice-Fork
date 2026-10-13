#pragma once
#include "global.h"
#ifdef __cplusplus
extern "C" {
#endif
s32 ARPG_UseShellbladeVisuals(PlayState* play,Actor* actor);
void ARPG_OverrideShellbladeLimb(PlayState* play,Actor* actor,s32 limb,Gfx** displayList);
#ifdef __cplusplus
}
#endif
