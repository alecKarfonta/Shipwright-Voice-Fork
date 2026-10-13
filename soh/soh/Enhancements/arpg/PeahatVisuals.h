#pragma once
#include "global.h"
#ifdef __cplusplus
extern "C" {
#endif
s32 ARPG_UsePeahatVisuals(PlayState* play,Actor* actor);
void ARPG_OverridePeahatLimb(PlayState* play,Actor* actor,s32 limb,Gfx** displayList);
void ARPG_OverridePeahatRoot(PlayState* play,Actor* actor,Gfx** displayList);
#ifdef __cplusplus
}
#endif
