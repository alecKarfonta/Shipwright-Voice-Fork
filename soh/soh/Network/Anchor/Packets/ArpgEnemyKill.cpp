#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Enhancements/arpg/XPSystem.h"

// [TIME] | [INFO] | ArpgEnemyKill.cpp | ARPG_ENEMY_KILL party kill-share
//
// Diablo II-style party XP: when a player defeats an enemy, teammates in the
// same room receive a share of the XP regardless of who swung the killing
// blow. Each client stays authoritative over its own ARPG save (levels are
// local), so the packet carries only the rolled reward + identifying info;
// receivers apply their own XP-rate scaling on top.

extern "C" {
#include "variables.h"
}

/**
 * ARPG_ENEMY_KILL
 */

void Anchor::SendPacket_ArpgEnemyKill(u32 xp, u8 tier, u8 level, u16 actorId) {
    if (!IsSaveLoaded() || isProcessingIncomingPacket || !isEnabled || !isConnected) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = ARPG_ENEMY_KILL;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["name"] = CVarGetString(CVAR_REMOTE_ANCHOR("Name"), "Player");
    payload["xp"] = xp;
    payload["tier"] = tier;     // 0 normal, 1 champion, 2 elite, 3 boss
    payload["level"] = level;   // enemy level for notifications
    payload["actorId"] = actorId;
    payload["quiet"] = true;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_ArpgEnemyKill(nlohmann::json payload) {
    if (!IsSaveLoaded() || !isEnabled) {
        return;
    }

    // [TIME] | [INFO] | HandlePacket.A RPGEnemyKill | XP is granted by the ARPG skill system on the game thread.
    // The queue guarantees that; here we just parse and forward.
    u32 xp = payload.at("xp").get<u32>();
    u8 tier = payload.at("tier").get<u8>();
    u8 level = payload.at("level").get<u8>();
    std::string name = payload.value("name", "Player");

    ARPG_OnRemoteKill(xp, tier, level, name.c_str());
}
