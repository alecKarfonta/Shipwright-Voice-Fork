#include <nlohmann/json.hpp>

#include "Anchor.h"
#include "Fellowship.h"
#include "CharacterTravel.h"
#include "CharacterPolicy.h"
#include "soh/Enhancements/arpg/QuestParty.h"
#include "soh/Enhancements/nametag.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"
extern PlayState* gPlayState;
}

// MARK: - Overrides

bool Anchor::CanSyncTeamSave() const {
    return roomState.syncItemsAndFlags && !CharacterTravel::Enabled();
}

void Anchor::ClearPacketQueues() {
    std::scoped_lock lock(incomingPacketQueueMutex, outgoingPacketQueueMutex);
    incomingPacketQueue = {};
    outgoingPacketQueue = {};
    incomingBytes = outgoingBytes = 0;
}

void Anchor::Enable() {
    if (isEnabled) return;
    Network::Disable(); // Finish the prior transport before starting another server.
    ClearPacketQueues();
    ARPG::Quests::Party::Reset();
    ownClientId = 0;
    roomState = {};
    RegisterHooks();
    Network::Enable(CVarGetString(CVAR_REMOTE_ANCHOR("Host"), "anchor.hm64.org"),
                    CVarGetInteger(CVAR_REMOTE_ANCHOR("Port"), 43383));
}

void Anchor::Disable() {
    Network::Disable();
    ApplyTransportState();
    ClearPacketQueues();
    ARPG::Quests::Party::Reset();
    justLoadedSave = shouldRefreshActors = false;
    EnemyDifficulty::RegisterHooks(false);
    Fellowship::Reset();
    clients.clear();
    RefreshClientActors();
    RegisterHooks();
}

// These callbacks run on the network thread. Game state, hooks, saves and UI
// are touched only by ApplyTransportState on the game thread.
void Anchor::OnConnected() { ++transportEpoch; }
void Anchor::OnDisconnected() { ++transportEpoch; }

void Anchor::ApplyTransportState() {
    const auto epoch = transportEpoch.load();
    if (epoch == appliedTransportEpoch) return;
    appliedTransportEpoch = epoch;
    {
        std::lock_guard<std::mutex> lock(outgoingPacketQueueMutex);
        outgoingPacketQueue = {};
        outgoingBytes = 0;
    }
    clients.clear();
    ownClientId = 0;
    roomState = {};
    Fellowship::Reset();
    ARPG::Quests::Party::Reset();
    RefreshClientActors();
    RegisterHooks();
    EnemyDifficulty::RegisterHooks(isConnected);
    if (isConnected) {
        SendPacket_Handshake();
        if (IsSaveLoaded()) SendPacket_RequestTeamState();
    }
}

void Anchor::ProcessOutgoingPackets() {
    std::queue<QueuedPacket> packets;
    {
        std::lock_guard<std::mutex> lock(outgoingPacketQueueMutex);
        packets.swap(outgoingPacketQueue);
        outgoingBytes = 0;
    }
    while (!packets.empty()) {
        auto packet = std::move(packets.front());
        packets.pop();
        if (packet.epoch == transportEpoch.load() && isConnected)
            Network::SendJsonToRemote(std::move(packet.payload));
    }
}

void Anchor::SendJsonToRemote(nlohmann::json payload) {
    if (!isConnected || appliedTransportEpoch != transportEpoch.load()) return;
    if (CharacterTravel::Enabled() && CharacterTravel::IsLegacySavePacket(payload.value("type", std::string{}))) return;
    payload["clientId"] = ownClientId;
    const auto bytes = payload.dump().size();
    std::lock_guard<std::mutex> lock(outgoingPacketQueueMutex);
    if (outgoingPacketQueue.size() >= 1024 || outgoingBytes + bytes > 4 * 1024 * 1024) {
        isEnabled = false;
        return;
    }
    outgoingBytes += bytes;
    outgoingPacketQueue.push({appliedTransportEpoch, std::move(payload), bytes});
}

void Anchor::OnIncomingJson(nlohmann::json payload) {
    if (!payload.is_object() || !payload.contains("type") || !payload["type"].is_string()) return;
    const auto bytes = payload.dump().size();
    std::lock_guard<std::mutex> lock(incomingPacketQueueMutex);
    if (incomingPacketQueue.size() >= 1024 || incomingBytes + bytes > 4 * 1024 * 1024) {
        isEnabled = false;
        return;
    }
    incomingBytes += bytes;
    incomingPacketQueue.push({transportEpoch.load(), std::move(payload), bytes});
}

void Anchor::ProcessIncomingPacketQueue() {
    ApplyTransportState();
    // Copy all queued packets while holding the lock, then process them after releasing
    std::queue<QueuedPacket> packetsToProcess;
    {
        std::lock_guard<std::mutex> lock(incomingPacketQueueMutex);
        packetsToProcess.swap(incomingPacketQueue);
        incomingBytes = 0;
    }

    // Process packets without holding the lock
    while (isEnabled && !packetsToProcess.empty()) {
        auto queued = std::move(packetsToProcess.front());
        packetsToProcess.pop();
        if (!isConnected || queued.epoch != appliedTransportEpoch || queued.epoch != transportEpoch.load()) continue;
        auto payload = std::move(queued.payload);

        std::string packetType = payload["type"].get<std::string>();

        if (CharacterTravel::Enabled() && CharacterTravel::IsLegacySavePacket(packetType))
            continue;

        // The client roster is owned by this thread, including version checks.
        if (packetType != ALL_CLIENT_STATE && packetType != UPDATE_CLIENT_STATE && packetType != PLAYER_UPDATE &&
            payload.contains("clientId") && payload["clientId"].is_number_unsigned()) {
            const auto id = payload["clientId"].get<uint32_t>();
            if (clients.contains(id) && clients[id].clientVersion != clientVersion) continue;
        }
        isProcessingIncomingPacket = true;

        try {
            // packetType here is a string so we can't use a switch statement
            if (packetType == "SOCIAL" || packetType == "SERVER_CAPABILITIES")
                Fellowship::Receive(payload);
            else if (packetType == ALL_CLIENT_STATE)
                HandlePacket_AllClientState(payload);
            else if (packetType == "ARPG_QUEST")
                ARPG::Quests::Party::Receive(payload);
            else if (packetType == ARPG_ENEMY_KILL)
                HandlePacket_ArpgEnemyKill(payload);
            else if (packetType == DAMAGE_PLAYER)
                HandlePacket_DamagePlayer(payload);
            else if (packetType == DISABLE_ANCHOR)
                HandlePacket_DisableAnchor(payload);
            else if (packetType == ENTRANCE_DISCOVERED)
                HandlePacket_EntranceDiscovered(payload);
            else if (packetType == GAME_COMPLETE)
                HandlePacket_GameComplete(payload);
            else if (packetType == GIVE_ITEM)
                HandlePacket_GiveItem(payload);
            else if (packetType == OCARINA_SFX)
                HandlePacket_OcarinaSfx(payload);
            else if (packetType == PLAYER_UPDATE)
                HandlePacket_PlayerUpdate(payload);
            else if (packetType == PLAYER_SFX)
                HandlePacket_PlayerSfx(payload);
            else if (packetType == UPDATE_TEAM_STATE)
                HandlePacket_UpdateTeamState(payload);
            else if (packetType == REQUEST_TEAM_STATE)
                HandlePacket_RequestTeamState(payload);
            else if (packetType == REQUEST_TELEPORT)
                HandlePacket_RequestTeleport(payload);
            else if (packetType == SERVER_MESSAGE)
                HandlePacket_ServerMessage(payload);
            else if (packetType == SET_CHECK_STATUS)
                HandlePacket_SetCheckStatus(payload);
            else if (packetType == SET_FLAG)
                HandlePacket_SetFlag(payload);
            else if (packetType == TELEPORT_TO)
                HandlePacket_TeleportTo(payload);
            else if (packetType == UNSET_FLAG)
                HandlePacket_UnsetFlag(payload);
            else if (packetType == UPDATE_BEANS_COUNT)
                HandlePacket_UpdateBeansCount(payload);
            else if (packetType == UPDATE_CLIENT_STATE)
                HandlePacket_UpdateClientState(payload);
            else if (packetType == UPDATE_ROOM_STATE)
                HandlePacket_UpdateRoomState(payload);
            else if (packetType == UPDATE_DUNGEON_ITEMS)
                HandlePacket_UpdateDungeonItems(payload);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[Anchor] Exception while processing incoming packet {}", e.what());
            SPDLOG_ERROR("[Anchor] Packet: {}", payload.dump());
        }

        isProcessingIncomingPacket = false;
    }
}

// MARK: - Misc/Helpers

// Kills all existing anchor actors and respawns them with the new client data

struct DummyPlayerClientId {
    uint32_t clientId = 0;
};
static ObjectExtension::Register<DummyPlayerClientId> DummyPlayerClientIdRegister;

uint32_t Anchor::GetDummyPlayerClientId(const Actor* actor) {
    const DummyPlayerClientId* clientId = ObjectExtension::GetInstance().Get<DummyPlayerClientId>(actor);
    return clientId != nullptr ? clientId->clientId : 0;
}

void Anchor::SetDummyPlayerClientId(const Actor* actor, uint32_t clientId) {
    ObjectExtension::GetInstance().Set<DummyPlayerClientId>(actor, DummyPlayerClientId{ clientId });
}

void Anchor::RefreshClientActors() {
    if (!IsSaveLoaded()) {
        return;
    }

    Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;

    while (actor != NULL) {
        if (actor->id == ACTOR_EN_OE2 && actor->update == DummyPlayer_Update) {
            NameTag_RemoveAllForActor(actor);
            Actor_Kill(actor);
        }
        actor = actor->next;
    }

    for (auto& [clientId, client] : clients) {
        if (!client.online || client.self) {
            continue;
        }

        spawningDummyPlayerForClientId = clientId;
        // We are using a hook `ShouldActorInit` to override the init/update/draw/destroy functions of the Player we
        // spawn We quickly store a mapping of "index" to clientId, then within the init function we use this to get the
        // clientId and store it on player->zTargetActiveTimer (unused s32 for the dummy) for convenience
        auto dummy =
            Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_PLAYER, client.posRot.pos.x, client.posRot.pos.y,
                        client.posRot.pos.z, client.posRot.rot.x, client.posRot.rot.y, client.posRot.rot.z, 0);
        client.player = (Player*)dummy;
    }
    spawningDummyPlayerForClientId = 0;
}

bool Anchor::IsSaveLoaded() {
    if (gPlayState == nullptr) {
        return false;
    }

    if (GET_PLAYER(gPlayState) == nullptr) {
        return false;
    }

    if (gSaveContext.fileNum < 0 || gSaveContext.fileNum > 2) {
        return false;
    }

    if (gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return false;
    }

    return true;
}
