#pragma once
#include "soh/Enhancements/mixamo/TownReactionRules.h"
struct Actor;
namespace LocalNpc {
bool CanReact(Actor* actor, Mixamo::TownReactions::Event event);
bool TryTownReaction(Actor* actor, Mixamo::TownReactions::Event event);
// TalkTo uses an offered campaign interaction first; OpenConversation starts
// local topics only when no message/cutscene owns the player.
bool TalkTo(Actor* actor);
bool OpenConversation(Actor* actor);
bool IsOpen();
void EndConversation();
void Draw();
void DrawSettings();
void Shutdown();
}
