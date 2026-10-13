#pragma once
#include "z64.h"
#ifdef __cplusplus
extern "C" {
#endif
void ARPG_OverrideKeeseLimb(PlayState* play, Actor* actor, s32 limb, Gfx** displayList, s32 translucent);
s32 ARPG_IsKeeseHead(Gfx* displayList);
#ifdef __cplusplus
}
#endif
