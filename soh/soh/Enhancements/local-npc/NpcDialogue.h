#pragma once
struct Actor;
namespace LocalNpc {
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
