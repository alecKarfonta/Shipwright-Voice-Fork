#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/JsonConversions.hpp"
#include <nlohmann/json.hpp>
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/QuestSystem.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"

extern "C" {
#include "variables.h"
#include "macros.h"
extern PlayState* gPlayState;
}

/**
 * UPDATE_CLIENT_STATE
 *
 * Contains a small subset of data that is cached on the server and important for the client to know for various reasons
 *
 * Sent on various events, such as changing scenes, soft resetting, finishing the game, opening file select, etc.
 *
 * Note: This packet should be cross version compatible, so if you add anything here don't assume all clients will be
 * providing it, consider doing a `contains` check before accessing any version specific data
 */

nlohmann::json Anchor::PrepClientState() {
    nlohmann::json payload;
    payload["name"] = CVarGetString(CVAR_REMOTE_ANCHOR("Name"), "");
    payload["color"] = CVarGetColor24(CVAR_REMOTE_ANCHOR("Color.Value"), { 100, 255, 100 });
    payload["clientVersion"] = clientVersion;
    payload["teamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["online"] = true;

    if (IsSaveLoaded()) {
        payload["seed"] = IS_RANDO ? Rando::Context::GetInstance()->GetSeed() : 0;
        payload["isSaveLoaded"] = true;
        payload["isGameComplete"] = gSaveContext.ship.stats.gameComplete;
        payload["sceneNum"] = gPlayState->sceneNum;
        payload["curRoomNum"] = gPlayState->roomCtx.curRoom.num;
        payload["entranceIndex"] = gSaveContext.entranceIndex;
        using namespace ARPG::Quests;
        const int storyItems[] = { QUEST_KOKIRI_EMERALD,   QUEST_GORON_RUBY,      QUEST_ZORA_SAPPHIRE,
                                   QUEST_MEDALLION_FOREST, QUEST_MEDALLION_FIRE,  QUEST_MEDALLION_WATER,
                                   QUEST_MEDALLION_SHADOW, QUEST_MEDALLION_SPIRIT };
        int stage = 0;
        while (stage < 8 && CHECK_QUEST_ITEM(storyItems[stage]))
            ++stage;
        if (stage == 8 && GET_EVENTCHKINF(EVENTCHKINF_COMPLETED_FOREST_TRIAL) &&
            GET_EVENTCHKINF(EVENTCHKINF_COMPLETED_FIRE_TRIAL) && GET_EVENTCHKINF(EVENTCHKINF_COMPLETED_WATER_TRIAL) &&
            GET_EVENTCHKINF(EVENTCHKINF_COMPLETED_SHADOW_TRIAL) &&
            GET_EVENTCHKINF(EVENTCHKINF_COMPLETED_SPIRIT_TRIAL) && GET_EVENTCHKINF(EVENTCHKINF_COMPLETED_LIGHT_TRIAL))
            stage = 9;
        if (gSaveContext.ship.stats.gameComplete)
            stage = 10;
        // Randomized saves have no single ordered campaign; do not suggest a false next dungeon.
        payload["campaign"] = { { "storyStage", IS_RANDO ? 11 : stage } };
        const auto& campaign = gARPGSave.campaign;
        const auto* quest = Enabled() ? Find(campaign.tracked) : nullptr;
        if (quest && (Status(campaign, quest->id) == State::Active || Status(campaign, quest->id) == State::Ready)) {
            const auto& p = campaign.quests[quest->id - 1];
            int done = 0, total = 0;
            for (const auto& objective : quest->objectives) {
                if (objective.kind == Kind::None)
                    continue;
                const int i = &objective - quest->objectives.data();
                ++total;
                if (p.progress[i] >= Target(p, objective))
                    ++done;
            }
            payload["campaign"]["questId"] = quest->id;
            payload["campaign"]["questState"] = p.state;
            payload["campaign"]["completedObjectives"] = done;
            payload["campaign"]["totalObjectives"] = total;
        }
    } else {
        payload["seed"] = 0;
        payload["isSaveLoaded"] = false;
        payload["isGameComplete"] = false;
        payload["sceneNum"] = SCENE_ID_MAX;
        payload["curRoomNum"] = -1;
        payload["entranceIndex"] = 0x00;
    }

    return payload;
}

void Anchor::SendPacket_UpdateClientState() {
    nlohmann::json payload;
    payload["type"] = UPDATE_CLIENT_STATE;
    payload["state"] = PrepClientState();

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_UpdateClientState(nlohmann::json payload) {
    uint32_t clientId = payload.at("clientId").get<uint32_t>();

    if (clients.contains(clientId)) {
        AnchorClient client = payload["state"].get<AnchorClient>();
        clients[clientId].clientId = clientId;
        clients[clientId].name = client.name;
        clients[clientId].color = client.color;
        clients[clientId].clientVersion = client.clientVersion;
        clients[clientId].teamId = client.teamId;
        clients[clientId].online = client.online;
        clients[clientId].seed = client.seed;
        clients[clientId].isSaveLoaded = client.isSaveLoaded;
        clients[clientId].isGameComplete = client.isGameComplete;
        clients[clientId].sceneNum = client.sceneNum;
        clients[clientId].curRoomNum = client.curRoomNum;
        clients[clientId].entranceIndex = client.entranceIndex;
    }

    // [TIME] | [INFO] | Anchor.HandlePacket_UpdateClientState | A peer loading/quitting
    // their save changes the effective player count -> difficulty recount
    EnemyDifficulty::RecountAndRescale();
}
