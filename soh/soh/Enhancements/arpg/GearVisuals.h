#pragma once
#include "z64.h"
#ifdef __cplusplus
#include "ARPGTypes.h"
#include "WaystoneState.h"
#include "RelayRules.h"
namespace ARPG::GearVisuals {
void DrawRelay(PlayState* play, const Relay::View& view);
void DrawWaystone(PlayState* play, const Waystone::View& view);
bool DrawMaterial(PlayState* play, Actor* actor, int material);
bool DrawLoot(PlayState* play, Actor* actor, const ARPGItemInstance& item);
}
extern "C" {
#endif
void ARPG_OverrideEquipmentLimb(PlayState* play, s32 limb, Gfx** displayList, void* player, s32 pause);
void ARPG_EquipmentTunicColor(PlayState* play, void* player, Color_RGB8* color, s32 pause);
#ifdef __cplusplus
}
#endif
