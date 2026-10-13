#pragma once
#include "global.h"
#ifdef __cplusplus
extern "C" {
#endif
s32 ARPG_UseStingerVisuals(PlayState* play,Actor* actor);
void ARPG_OverrideStingerLimb(PlayState* play,Actor* actor,s32 limb,Gfx** displayList,s32 fading);
Gfx* ARPG_DrawStingerSkeleton(PlayState* play,Actor* actor,SkelAnime* skeleton,OverrideLimbDraw nativeOverride,s32 fading,Gfx* gfx);
#ifdef __cplusplus
}
#endif
