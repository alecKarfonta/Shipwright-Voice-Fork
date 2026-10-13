#pragma once
#include "QuestRules.h"
#include <string>
#include <vector>
struct Actor;
namespace ARPG::Quests {
bool Enabled();
void OnDefeat(Actor* actor,bool boss);
void DrawJournal();
void DrawHud();
void RequestJournal(int quest=0);
bool ConsumeJournalRequest();
bool HasQuests(Actor* actor);
bool IsQuestEnemy(Actor* actor);
void OpenFor(Actor* actor);
std::string DialogueContext(Actor* actor);
std::string AuthoredGreeting(Actor* actor);
std::string AuthoredReply(Actor* actor,bool advice);
enum class TalkAction { About, Why, Advice, Accept, Deliver, Reward, Track };
struct TalkOption { std::string label; TalkAction action; int quest; int value=0; };
std::vector<int> ConversationQuests(Actor* actor);
std::vector<TalkOption> ConversationOptions(Actor* actor,int quest);
std::string ChooseConversationOption(Actor* actor,const TalkOption& option);
bool DeliverNearby(int id,int objective);
void OnSong();
bool AcceptNearby(int id);
bool ClaimNearby(int id,int choice);
int NearbyContact();
// Exposed to the isolated native integration harness as well as the gameplay update hook.
void Tick();
int VisibleSites();
float ChannelProgress();
}
