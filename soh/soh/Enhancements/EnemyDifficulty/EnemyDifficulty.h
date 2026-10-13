#pragma once

// [TIME] | [INFO] | EnemyDifficulty.h | Player-count-based enemy & boss scaling for Anchor co-op
//
// Modes (RoomState.difficultyMode):
//   0 = off
//   1 = HP only:     enemies/bosses HP x(1 + 1.0N), capped x6 (N = extra players)
//   2 = full:        also damage-taken scaling (via DefenseModifier), enemy
//                    speed & aggression (curated actor set)
//
// All clients compute identical multipliers from the shared room roster, so
// no extra network sync is required.

#include <stdint.h>
struct Actor;
#ifdef __cplusplus
extern "C" {
#endif
float EnemyDifficulty_MovementMultiplier(struct Actor* actor);
#ifdef __cplusplus
}

namespace EnemyDifficulty {
bool IsEnemyActor(const Actor* actor);
bool IsCurated(int16_t actorId);
void ApplyHealthMultiplier(Actor* actor, float multiplier);


// Multiplier getters (cached; recomputed by UpdatePlayerCount)
float HpMult();       // enemy/boss hit points
float DamageMult();   // damage players take
float SpeedMult();    // enemy movement speed
float CooldownMult(); // enemy attack cooldowns (lower = more aggressive)

// Recompute multipliers from the current Anchor roster; returns player count used
int UpdatePlayerCount(int onlineSaveLoadedPlayers);

// Called when the room state packet changes difficultyMode (UpdateRoomState)
void OnRoomStateUpdated();

// Called by Anchor when the roster changes (players join/leave) to live-rescale
void OnRosterChanged();

// Recount players from the live roster (self via IsSaveLoaded, peers via clients map)
// and live-rescale if the count changed. Call from both ALL_CLIENT_STATE and
// UPDATE_CLIENT_STATE handlers - loading a save mid-session must also count.
void RecountAndRescale();

// Register/unregister all game hooks; condition = Anchor connected && mode > 0
void RegisterHooks(bool active);

} // namespace EnemyDifficulty

#endif
