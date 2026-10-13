// Opt-in native save/network test. Never runs against a user's SHIP_HOME.
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/ARPGSaveCodec.h"
#include "soh/Enhancements/arpg/QuestParty.h"
#include "soh/Enhancements/arpg/QuestPartyRules.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/CharacterTravel.h"
#include "soh/Network/Anchor/JsonConversions.hpp"
#include "soh/Network/Anchor/ServerBrowserWindow.h"
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
extern PlayState *gPlayState;
extern GameState *gGameState;
}
namespace SohGui {
extern std::shared_ptr<ServerBrowserWindow> mServerBrowserWindow;
}
namespace {
using namespace ARPG;
using namespace ARPG::Quests;
int phase = 0, frames = 0;
std::filesystem::path home;
json checks = json::array(), baseline, native;
void Report(const char *status, const std::string &error = "") {
  std::ofstream(home / "travel-results.json") << json{
      {"status", status},
      {"phase", phase},
      {"checks", checks},
      {"error", error}}.dump(2);
}
void Check(bool result, const char *text) {
  if (!result) {
    std::ofstream(home / "travel-diff.json") << json::diff(baseline, SaveCodec::Encode(gARPGSave)).dump(2);
    throw std::runtime_error(text);
  }
  checks.push_back(text);
  Report("running");
}
void Join(int port, const char *room) {
  CVarSetString(CVAR_REMOTE_ANCHOR("Host"), "127.0.0.1");
  CVarSetInteger(CVAR_REMOTE_ANCHOR("Port"), port);
  CVarSetString(CVAR_REMOTE_ANCHOR("RoomId"), room);
  CVarSetString(CVAR_REMOTE_ANCHOR("TeamId"), "travel");
  CVarSetString(CVAR_REMOTE_ANCHOR("Name"), "Travel QA");
  CVarSetInteger(CVAR_REMOTE_ANCHOR("PortableCharacters"), 1);
  CVarSetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.SyncItemsAndFlags"), 1);
  CVarSetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.DifficultyMode"), 0);
  Anchor::Instance->Enable();
}
void Tick() {
  if (phase == 99)
    return;
  try {
    ++frames;
    auto *a = Anchor::Instance;
    if (!phase && frames > 60 && gGameState) {
      SaveManager::Instance->LoadFile(0);
      gSaveContext.fileNum = 0;
      gSaveContext.gameMode = GAMEMODE_NORMAL;
      gSaveContext.linkAge = LINK_AGE_CHILD;
      gSaveContext.entranceIndex = ENTR_KOKIRI_FOREST_0;
      gSaveContext.cutsceneIndex = 0;
      gSaveContext.sceneLayer = 0;
      gSaveContext.respawnFlag = 0;
      gSaveContext.dayTime = gSaveContext.skyboxTime = 0x8000;
      gGameState->running = false;
      SET_NEXT_GAMESTATE(gGameState, Play_Init, PlayState);
      for (const char *key :
           {"gEnhancements.ARPG.Enabled", "gEnhancements.ARPG.Quests"})
        CVarSetInteger(key, 1);
      for (const char *key :
           {"gEnhancements.ARPG.Items", "gEnhancements.ARPG.Skills",
            "gEnhancements.ARPG.Enemies", "gEnhancements.ARPG.Packs",
            "gEnhancements.ARPG.Affixes", "gEnhancements.ARPG.RegionalWorld",
            "gOpenWindows.ARPG", "gOpenWindows.ARPGInventory",
            "gEnhancements.LocalNpc.Enabled"})
        CVarSetInteger(key, 0);
      GameInteractor::Instance->ExecuteHooks<GameInteractor::OnLoadGame>(0);
      phase = 1;
      frames = 0;
      Report("starting");
      return;
    }
    if (frames > 2400)
      throw std::runtime_error("Native travel test timed out");
    if (!gPlayState || !GameInteractor::IsPlayerInControl())
      return;
    GET_PLAYER(gPlayState)->invincibilityTimer = 40;
    std::string error;
    if (phase == 1 && frames > 100) {
      for (auto *actor = gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;
           actor; actor = actor->next)
        Actor_Kill(actor);
      gARPGSave.xp = Rules::XpThreshold(12);
      gARPGSave.level = 13;
      gSaveContext.rupees = 77;
      gARPGSave.materials[1] = 17;
      gARPGSave.materials[19] = 3;
      auto &item = gARPGSave.backpack[2];
      item = {};
      item.baseEquip = 1;
      item.slot = 0;
      item.itemLevel = 12;
      item.socketCount = 1;
      item.sockets[0] = 19;
      gARPGSave.campaign = {};
      auto &q = gARPGSave.campaign.quests[0];
      q.state = uint8_t(State::Active);
      q.progress[0] = 4;
      Refresh(gARPGSave.campaign, 1);
      Check(Claim(gARPGSave, 1, 0, false),
            "Character claims a quest reward before travelling");
      Check(Accept(gARPGSave.campaign, 2, false),
            "Character accepts next campaign quest");
      gARPGSave.campaign.quests[1].nodes[0] = 1;
      gARPGSave.campaign.quests[1].progress[0] = 1;
      for (int node=0;node<3;++node)
        gARPGSave.campaign.quests[1].sites[0][node] = {-405.f+node*60, 0, 70, 0, 1, 0};
      gARPGSave = SaveCodec::Decode(SaveCodec::Encode(gARPGSave), 1);
      baseline = SaveCodec::Encode(gARPGSave);
      native = json(gSaveContext.inventory);
      Check(CharacterTravel::Checkpoint(error),
            "Initial character checkpoint reaches disk");
      Join(43396, "character-origin");
      phase = 2;
      frames = 0;
      return;
    }
    if (phase == 2 && a->isConnected && a->ownClientId) {
      Party::Share(2);
      if (!Party::Online(2))
        return;
      Check(a->roomState.syncItemsAndFlags == 0,
            "Portable host advertises personal inventory and story");
      a->roomState.syncItemsAndFlags = 1;
      const auto healthCapacity = gSaveContext.healthCapacity;
      auto other = gSaveContext;
      other.healthCapacity = 0x140;
      other.inventory.questItems = 0xffffffff;
      other.inventory.equipment = 0xffff;
      a->OnIncomingJson({{"type", "UPDATE_TEAM_STATE"},
                         {"state", other},
                         {"queue", json::array()}});
      a->OnIncomingJson({{"type", "UPDATE_BEANS_COUNT"}, {"beansBought", 99}});
      a->ProcessIncomingPacketQueue();
      Check(!a->CanSyncTeamSave() && json(gSaveContext.inventory) == native && gSaveContext.healthCapacity == healthCapacity,
            "A host enabling legacy sync cannot overwrite character inventory "
            "or story");
      auto before = SaveCodec::Encode(gARPGSave);
      // Compare the character at departure, after normal party reconciliation.
      baseline = before;
      for (auto& quest : baseline["campaign"]["quests"]) if (quest.is_object()) quest["party"] = 0;
      std::filesystem::create_directory(home / "Save/file1.sav.temp");
      Check(!CharacterTravel::Checkpoint(error) && !error.empty(),
            "Disk write failure is reported by the real save worker");
      Check(a->isConnected && before == SaveCodec::Encode(gARPGSave),
            "Failed checkpoint leaves connection and character intact");
      Check(CharacterTravel::Checkpoint(error),
            "Checkpoint can be retried successfully");
      // This queued old-room packet must not run after Enable for another
      // server.
      a->OnIncomingJson({{"type", "UPDATE_ROOM_STATE"},
                         {"state",
                          {{"ownerClientId", 999},
                           {"pvpMode", 2},
                           {"showLocationsMode", 2},
                           {"teleportMode", 2},
                           {"syncItemsAndFlags", 1},
                           {"arpgSeed", 999}}}});
      a->Disable();
      Join(43397, "character-destination");
      a->ProcessIncomingPacketQueue();
      Check(a->roomState.arpgSeed != 999,
            "Old server's queued packets are discarded during transfer");
      phase = 3;
      frames = 0;
      return;
    }
    if (phase == 3 && a->isConnected && a->ownClientId && frames > 50) {
      Check(!Party::Shared(2),
            "Previous party membership is cleared on another relay");
      Check(SaveCodec::Encode(gARPGSave) == baseline,
            "Gear, XP, skills, materials, reward history and partial physical "
            "objectives travel unchanged");
      Check(json(gSaveContext.inventory) == native && gSaveContext.rupees == 77,
            "Native items, story and currency travel unchanged");
      Check(!Claim(gARPGSave, 1, 0, false),
            "Changing servers cannot grant a quest reward twice");
      Party::Share(2);
      phase = 4;
      frames = 0;
      return;
    }
    if (phase == 4 && Party::Online(2)) {
      Check(true, "Unfinished quest can be shared with the new server's party");
      Check(CharacterTravel::Checkpoint(error),
            "Destination character checkpoint reaches disk");
      baseline = SaveCodec::Encode(gARPGSave);
      gARPGSave = {};
      gSaveContext.rupees = 0;
      SaveManager::Instance->LoadFile(0);
      gSaveContext.fileNum = 0;
      Check(SaveCodec::Encode(gARPGSave) == baseline &&
                gSaveContext.rupees == 77,
            "Real save-file reload restores the travelled character");
      Check(!Claim(gARPGSave, 1, 0, false),
            "Reward history survives a disk reload");
      phase = 99;
      Report("passed");
      SohGui::mServerBrowserWindow->Show();
    }
  } catch (const std::exception &e) {
    Report("failed", e.what());
    phase = 99;
  }
}
void Register() {
  const char *path = std::getenv("SHIP_HOME");
  if (!std::getenv("SOH_CHARACTER_TRAVEL_TEST") || !path ||
      !std::filesystem::path(path).filename().string().starts_with(
          "soh-arpg-qa-travel-"))
    return;
  home = path;
  GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(
      Tick);
}
static RegisterShipInitFunc init(Register);
} // namespace
