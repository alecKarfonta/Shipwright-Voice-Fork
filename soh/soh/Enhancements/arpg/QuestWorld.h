#pragma once
#include "QuestCatalog.h"
struct PlayState;
namespace ARPG::Quests {
void DrawProp(PlayState* play,Prop kind,float x,float y,float z,bool complete,bool selected,float progress);
}
