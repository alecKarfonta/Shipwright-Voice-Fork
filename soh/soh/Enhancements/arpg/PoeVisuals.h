#pragma once
#include "global.h"
#ifdef __cplusplus
extern "C" {
#endif
s32 ARPG_UsePoeVisuals(PlayState* play,Actor* actor);
void ARPG_OverridePoeLimb(PlayState* play,Actor* actor,s32 limb,Gfx** displayList);
Gfx* ARPG_PoeLanternDisplayList(PlayState* play,Actor* actor,Gfx* original);
#ifdef __cplusplus
}
#endif
