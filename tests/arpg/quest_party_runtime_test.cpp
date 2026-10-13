// Opt-in fixture: two actual game clients and the local relay, isolated SHIP_HOMEs.
#include "soh/Enhancements/arpg/QuestSystem.h"
#include "soh/Enhancements/arpg/QuestParty.h"
#include "soh/Enhancements/arpg/QuestPartyRules.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/ARPGSaveCodec.h"
#include "soh/Enhancements/arpg/InventorySystem.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Network/Anchor/Anchor.h"
#include "soh/ShipInit.hpp"
#include "soh/ObjectExtension/ActorMaximumHealth.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
extern PlayState* gPlayState;extern GameState* gGameState;
void Sram_InitDebugSave(void);
}
namespace {
using namespace ARPG;using namespace ARPG::Quests;
int phase=0,frames=0,contributions=0,quest=1;bool leader=false,packChecked=false;uint32_t oldClient=0;
std::filesystem::path output,home;
nlohmann::json checks=nlohmann::json::array();
void Report(const char* status,const std::string& error="") {
 std::ofstream(output)<<nlohmann::json{{"status",status},{"phase",phase},{"checks",checks},{"error",error},{"campaign",SaveCodec::Encode(gARPGSave)["campaign"]}}.dump(2);
}
void Check(bool condition,const char* text) {if(!condition)throw std::runtime_error(text);checks.push_back(text);Report("running");}
void Move(Vec3f p) {auto* player=GET_PLAYER(gPlayState);player->actor.world.pos=player->actor.prevPos=p;player->actor.velocity={};player->actor.speedXZ=0;}
bool Contact() {
 for(int cat=0;cat<ACTORCAT_MAX;++cat)for(auto* a=gPlayState->actorCtx.actorLists[cat].head;a;a=a->next)if(HasQuests(a)) {
  auto p=a->world.pos;p.x+=65;Move(p);if(NearbyContact()>=0)return true;
 }
 return false;
}
void Noop(Actor*,PlayState*){}
void Defeat() {
 auto p=GET_PLAYER(gPlayState)->actor.world.pos;p.z+=150;
 auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_DEKUBABA,p.x,p.y,p.z,0,0,0,0);
 Check(a&&a->update,"Native enemy initialized on contributing client");a->update=Noop;a->colChkInfo.health=0;
 GameInteractor_ExecuteOnEnemyDefeat(a);auto progress=gARPGSave.campaign.quests[0].progress[0];
 GameInteractor_ExecuteOnEnemyDefeat(a);Check(progress==gARPGSave.campaign.quests[0].progress[0],"Repeated death callback does not duplicate contribution");Actor_Kill(a);
}
void Tick() {
 if(phase==99) {
  auto command=home/"party-preview.txt";
  if(std::filesystem::exists(command)) {
   std::string text;std::ifstream(command)>>text;std::filesystem::remove(command);
   if(text=="invite") {gARPGSave.campaign.quests[4].state=uint8_t(State::Rewarded);Accept(gARPGSave.campaign,6,false);Party::Share(6);}
   if(text=="unlock")gARPGSave.campaign.quests[4].state=uint8_t(State::Rewarded);
  }
  return;
 }
 try {
  ++frames;
  if(!phase&&frames>60&&gGameState) {
   CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0;
   gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.linkAge=LINK_AGE_CHILD;gSaveContext.inventory.questItems=0;
   gSaveContext.entranceIndex=ENTR_KOKIRI_FOREST_0;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;
   gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);
   for(const char* key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills","gEnhancements.ARPG.Quests"})CVarSetInteger(key,1);
   for(const char* key:{"gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Affixes","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG","gOpenWindows.ARPGInventory","gEnhancements.LocalNpc.Enabled"})CVarSetInteger(key,0);
   phase=1;frames=0;return;
  }
  if(!gPlayState||!GameInteractor::IsPlayerInControl()) {if(frames>2400)throw std::runtime_error("No player control");return;}
  GET_PLAYER(gPlayState)->invincibilityTimer=40;
  if(phase==1&&frames>100) {
   for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)Actor_Kill(a);
   gARPGSave.campaign={};for(auto& item:gARPGSave.backpack)item={};Party::Reset();
   Check(Accept(gARPGSave.campaign,1,false),"Opening campaign quest accepted");Move({-405,0,70});
   CVarSetString(CVAR_REMOTE_ANCHOR("Host"),"127.0.0.1");CVarSetInteger(CVAR_REMOTE_ANCHOR("Port"),43394);
   CVarSetString(CVAR_REMOTE_ANCHOR("RoomId"),"quest-native-integration");CVarSetString(CVAR_REMOTE_ANCHOR("TeamId"),"campaign");
   CVarSetString(CVAR_REMOTE_ANCHOR("Name"),leader?"Arin":"Ria");
   CVarSetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.SyncItemsAndFlags"),0);
   CVarSetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.PvpMode"),0);
   CVarSetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.DifficultyMode"),0);
   if(std::getenv("SOH_ARPG_PARTY_UI")) {
    nlohmann::json fixture;std::ifstream(home/"ui-campaign.json")>>fixture;
    gARPGSave.campaign=SaveCodec::Decode({{"campaign",fixture.at("campaign")}},1).campaign;
    Anchor::Instance->Enable();phase=99;RequestJournal(5);ARPG::Inventory::Open();Report("visual_review");return;
   }
   Anchor::Instance->Enable();phase=2;frames=0;Report("connecting");return;
  }
  if(frames>4000)throw std::runtime_error("Party fixture timed out");
  if(phase==2) {
   if(!Party::Shared(quest)&&(leader||Party::Invitations()))Party::Share(quest);
   if(!Party::Online(quest)||gARPGSave.campaign.quests[quest-1].partySize<2)return;
   Check(PartySize(gARPGSave.campaign.quests[quest-1])==2,"Both native clients joined the same party quest");
   if(quest==1)Check(Target(gARPGSave.campaign.quests[0],Catalog[0].objectives[0])==6,"Two-player hunt scales from four kills to six");
   phase=3;frames=0;return;
  }
  if(phase==3) {
   if(quest==1&&contributions<3&&frames%30==0) {Defeat();++contributions;}
   if((quest==2||quest==4)&&leader) {
    auto& p=gARPGSave.campaign.quests[quest-1];
    for(int n=0;n<3;++n)if(p.sites[0][n].placed&&!(p.nodes[0]&(1<<n))) {auto s=p.sites[0][n];Move({s.x,s.y,s.z});break;}
   }
   if(quest==5&&frames>90) {
    int live=0;
    for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(IsQuestEnemy(a)&&a->update) {
     ++live;
     if(!packChecked)Check(GetActorMaximumHealth(a)>=12,"Two-player quest pack receives its native HP scaling floor");
     if(leader&&frames%45==0) {a->colChkInfo.health=0;GameInteractor_ExecuteOnEnemyDefeat(a);Actor_Kill(a);break;}
    }
    if(live)packChecked=true;
   }
   if(Status(gARPGSave.campaign,quest)!=State::Ready||!Party::CanClaim(quest))return;
   Check(true,quest==1?"Local and remote kills complete the shared hunt":quest==2?"Gathered nodes and positions replicate to the other game":"World restoration completes on both game clients");
   auto snapshot=SaveCodec::Encode(gARPGSave);auto restored=SaveCodec::Decode(snapshot,1);
   Check(restored.campaign.quests[quest-1].partySize==2&&restored.campaign.quests[quest-1].party!=0,"Party membership and scaling survive save serialization");
   if(quest==4) {
    quest=5;Check(Accept(gARPGSave.campaign,5,false),"Named pack quest accepted");
    for(int e=0;e<ENTR_MAX;++e)if(gEntranceTable[e].scene==SCENE_HYRULE_FIELD&&gEntranceTable[e].spawn==0) {gPlayState->nextEntranceIndex=e;break;}
    gPlayState->transitionTrigger=TRANS_TRIGGER_START;gPlayState->transitionType=TRANS_TYPE_INSTANT;
    phase=4;frames=0;return;
   }
   if(quest==5) {
    Check(packChecked,"Named pack existed on both native clients before shared defeat");
    Check(gARPGSave.campaign.quests[4].nodes[0]==7,"Named enemy identities grant shared credit exactly once");
    oldClient=Anchor::Instance->ownClientId;if(!leader)Anchor::Instance->Disable();
    phase=5;frames=0;return;
   }
   if(!Contact())throw std::runtime_error("No quest giver nearby");auto xp=gARPGSave.xp;
   Check(ClaimNearby(quest,0),"Native NPC grants this player's quest reward");
   Check(!ClaimNearby(quest,0),"Quest reward cannot be claimed twice");
   Check(gARPGSave.xp==xp+RewardXp(gARPGSave.campaign.quests[quest-1],*Find(quest)),"Native reward uses the displayed party-scaled XP");
   quest=quest==1?2:4;
   if(quest==4)gARPGSave.campaign.quests[2].state=uint8_t(State::Rewarded); // Isolate restoration from a full Gohma playthrough.
   Check(Accept(gARPGSave.campaign,quest,false),"Next fixture quest accepted with prerequisites satisfied");
   Move({-405,0,70});phase=2;frames=0;return;
  }
  if(phase==4&&gPlayState->sceneNum==SCENE_HYRULE_FIELD&&frames>80) {phase=2;frames=0;return;}
  if(phase==5) {
   if(!leader&&frames==40)Anchor::Instance->Enable();
   bool rejoined=false;
   if(leader)for(const auto& [id,peer]:Anchor::Instance->clients)if(id>2&&peer.online)rejoined=true;
   if(!leader)rejoined=Anchor::Instance->ownClientId!=oldClient;
   if(rejoined&&Party::Online(5)&&frames>100) {
    Check(gARPGSave.campaign.quests[4].partySize==2&&gARPGSave.campaign.quests[4].nodes[0]==7,"Native disconnect and reconnect preserve party progress and difficulty");
    Report("passed");phase=99;RequestJournal(5);ARPG::Inventory::Open();
   }
  }
 }catch(const std::exception& e) {Report("failed",e.what());phase=99;}
}
void Register() {
 const char* role=std::getenv("SOH_ARPG_QUEST_PARTY_ROLE"),*path=std::getenv("SHIP_HOME");
 if(!role||!path||!std::filesystem::path(path).filename().string().starts_with("soh-arpg-qa-party-"))return;
 leader=std::string(role)=="A";home=path;output=home/"quest-party-results.json";
 GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
}
static RegisterShipInitFunc init(Register);
}
