#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/CharacterTravel.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"

extern "C" {
extern PlayState* gPlayState;
}

/**
 * UPDATE_ROOM_STATE
 */

nlohmann::json Anchor::PrepRoomState() {
    nlohmann::json payload;
    payload["ownerClientId"] = ownClientId;
    payload["listed"] = CVarGetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.Listed"),0)!=0;
    bool isGlobalRoom = (std::string("soh-global") == CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"), ""));

    if (isGlobalRoom) {
        // Global room uses hardcoded settings
        payload["pvpMode"] = 0;
        payload["showLocationsMode"] = 0;
        payload["teleportMode"] = 0;
        payload["syncItemsAndFlags"] = 0;
        payload["difficultyMode"] = 0;
        payload["arpgSeed"] = 0;
    } else {
        payload["pvpMode"] = CVarGetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.PvpMode"), 1);
        payload["showLocationsMode"] = CVarGetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.ShowLocationsMode"), 1);
        payload["teleportMode"] = CVarGetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.TeleportMode"), 1);
        payload["syncItemsAndFlags"] = CharacterTravel::Enabled() ? 0 : CVarGetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.SyncItemsAndFlags"), 1);
        payload["difficultyMode"] = CVarGetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.DifficultyMode"), 1);
        // [TIME] | [INFO] | PrepRoomState.A RPGSeed | Room owner shares one deterministic roll seed; 0 keeps per-save personal rolls.
        payload["arpgSeed"] = CVarGetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.ARPGSeed"), 0);
    }

    return payload;
}

void Anchor::SendPacket_UpdateRoomState() {
    nlohmann::json payload;
    payload["type"] = UPDATE_ROOM_STATE;
    payload["state"] = PrepRoomState();

    Network::SendJsonToRemote(payload);
}

void Anchor::HandlePacket_UpdateRoomState(nlohmann::json payload) {
    if (!payload.contains("state")) {
        return;
    }

    roomState.ownerClientId = payload["state"]["ownerClientId"].get<uint32_t>();
    CVarSetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.Listed"),payload["state"].value("listed",false)?1:0);
    roomState.pvpMode = payload["state"]["pvpMode"].get<u8>();
    roomState.showLocationsMode = payload["state"]["showLocationsMode"].get<u8>();
    roomState.teleportMode = payload["state"]["teleportMode"].get<u8>();
    roomState.syncItemsAndFlags = payload["state"]["syncItemsAndFlags"].get<u8>();
    // Cross-version safety: older servers/clients won't send difficultyMode
    if (payload["state"].contains("difficultyMode")) {
        roomState.difficultyMode = payload["state"]["difficultyMode"].get<u8>();
        EnemyDifficulty::OnRoomStateUpdated();
    }
    // [TIME] | [INFO] | HandlePacket.A RPGSeed | Late joiners adopt the owner's shared seed so their elites match the room.
    if (payload["state"].contains("arpgSeed")) {
        roomState.arpgSeed = payload["state"]["arpgSeed"].get<uint32_t>();
    }
}
