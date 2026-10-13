#include "EnemyDifficulty.h"
#include "soh/ObjectExtension/ActorMaximumHealth.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
#include "z64.h"
#include "variables.h"
#include "functions.h"
#include "overlays/actors/ovl_Boss_Dodongo/z_boss_dodongo.h"
extern PlayState* gPlayState;
}

// SECTION: constants

// [TIME] | [INFO] | EnemyDifficulty | Scaling formulas (per plan):
//   players = online clients with save loaded; N = players - 1
//   hp:      x(1 + 1.0N) capped x6      damage:  x(1 + 0.5N) capped x4
//   speed:   x(1 + 0.25N) capped x2     aggression: cooldowns x(1 - 0.15N) floor 0.5
static constexpr float HP_PER_EXTRA_PLAYER = 1.0f;
static constexpr float HP_CAP = 6.0f;
static constexpr float DMG_PER_EXTRA_PLAYER = 0.5f;
static constexpr float DMG_CAP = 4.0f;
static constexpr float SPD_PER_EXTRA_PLAYER = 0.25f;
static constexpr float SPD_CAP = 2.0f;
static constexpr float COOLDOWN_REDUCTION_PER_EXTRA_PLAYER = 0.15f;
static constexpr float COOLDOWN_FLOOR = 0.5f;

// Cached multipliers - identical on all clients (derived from the same roster)
static float hpMult = 1.0f;
static float dmgMult = 1.0f;
static float spdMult = 1.0f;
static float cdMult = 1.0f;
static int playerCount = 1;

// SECTION: namespace

namespace EnemyDifficulty {

float HpMult() { return hpMult; }
float DamageMult() { return dmgMult; }
float SpeedMult() { return spdMult; }
float CooldownMult() { return cdMult; }

int UpdatePlayerCount(int players) {
    if (players < 1) {
        players = 1;
    }
    playerCount = players;
    int n = players - 1;
    hpMult = std::clamp(1.0f + HP_PER_EXTRA_PLAYER * n, 1.0f, HP_CAP);
    dmgMult = std::clamp(1.0f + DMG_PER_EXTRA_PLAYER * n, 1.0f, DMG_CAP);
    spdMult = std::clamp(1.0f + SPD_PER_EXTRA_PLAYER * n, 1.0f, SPD_CAP);
    cdMult = std::clamp(1.0f - COOLDOWN_REDUCTION_PER_EXTRA_PLAYER * n, COOLDOWN_FLOOR, 1.0f);
    return players;
}

// SECTION: enemies

// [TIME] | [INFO] | EnemyDifficulty.IsEnemyActor | Flame trails and circling Stinger decoys must not receive combat scaling, affixes or defeat rewards.
bool IsEnemyActor(const Actor* actor) {
    // [TIME] | [INFO] | EnemyDifficulty.IsEnemyActor | Mad Scrub flowers are decorative child actors even while listed in the enemy category.
    if (actor != nullptr && actor->id == ACTOR_EN_DEKUNUTS && actor->params == 10) {
        return false;
    }
    if (actor == nullptr || actor->id == ACTOR_EN_FIRE_ROCK || actor->id == ACTOR_EN_ENCOUNT2 || actor->id == ACTOR_EN_ANUBICE_FIRE ||
        (actor->id == ACTOR_EN_BB && actor->params >= 0) ||
        (actor->id == ACTOR_EN_EIYER && actor->params >= 1 && actor->params <= 3)) {
        return false;
    }
    // [TIME] | [INFO] | EnemyDifficulty.IsEnemyActor | Anubis changes to PROP before its native defeat callback; its earned reward must remain eligible.
    if (actor->id == ACTOR_EN_ANUBICE || actor->category == ACTORCAT_ENEMY || actor->category == ACTORCAT_BOSS) {
        return true;
    }
    // Dark Link (En_Torch2) spawns in ACTORCAT_NPC but is a fight
    return actor->id == ACTOR_EN_TORCH2;
}

// Curated actors receiving speed & aggression tuning in mode 2
bool IsCurated(int16_t actorId) {
    switch (actorId) {
        case ACTOR_BOSS_DODONGO:   // King Dodongo
        case ACTOR_BOSS_GOMA:      // Gohma
        case ACTOR_BOSS_GANONDROF: // Phantom Ganon
        case ACTOR_BOSS_FD:        // Volvagia
        case ACTOR_BOSS_MO:        // Morpha
        case ACTOR_BOSS_SST:       // Bongo Bongo
        case ACTOR_BOSS_TW:        // Twinrova
        case ACTOR_BOSS_VA:        // Barinade
        case ACTOR_BOSS_GANON:
        case ACTOR_BOSS_GANON2:
        case ACTOR_EN_TORCH2:      // Dark Link
        case ACTOR_EN_WF:          // Wolfos
        case ACTOR_EN_TEST:        // Stalfos
        case ACTOR_EN_ZF:          // Lizalfos / Dinolfos
        case ACTOR_EN_IK:          // Iron Knuckle
        case ACTOR_EN_MB:          // Moblins
        case ACTOR_EN_RD:          // ReDead / Gibdo
        case ACTOR_EN_POH:         // Poe
        case ACTOR_EN_PO_FIELD:    // Big Poe
            return true;
        default:
            return false;
    }
}

// SECTION: hp

// Live-rescale bookkeeping per actor pointer
struct ScaledRecord {
    u8 baseHealth;   // health at init, before scaling
    float lastHpMult;
};
static std::unordered_map<Actor*, ScaledRecord> scaledActors;
// King Dodongo keeps HP in its own s16 field (z_boss_dodongo.h:27)
static std::unordered_map<Actor*, std::pair<s16, float>> kdRecords;

static int ClampU8(int v) {
    return v > 255 ? 255 : (v < 0 ? 0 : v);
}

static void ApplyHpScaling(Actor* actor) {
    // [TIME] | [INFO] | EnemyDifficulty.ApplyHpScaling | Use the native struct; N64 offsets are not valid on 64-bit hosts.
    if (actor->id == ACTOR_BOSS_DODONGO) {
        s16* hp = &reinterpret_cast<BossDodongo*>(actor)->health;
        auto it = kdRecords.find(actor);
        if (it == kdRecords.end()) {
            s16 base = *hp;
            kdRecords[actor] = { base, hpMult };
            *hp = (s16)std::clamp((int)std::lround(base * hpMult), 0, 32000);
            return;
        }
        if (hpMult != it->second.second) {
            float unscaled = (float)*hp / it->second.second;
            *hp = (s16)std::clamp((int)std::lround(unscaled * hpMult), 0, 32000);
            it->second.second = hpMult;
        }
        return;
    }

    auto it = scaledActors.find(actor);
    if (it == scaledActors.end()) {
        // First sighting (OnActorInit): store baseline, then scale up
        u8 base = actor->colChkInfo.health;
        scaledActors[actor] = { base, hpMult };
        actor->colChkInfo.health = (u8)ClampU8((int)std::lround(base * hpMult));
        SetActorMaximumHealth(actor, actor->colChkInfo.health);
        return;
    }
    ScaledRecord& rec = it->second;
    if (rec.lastHpMult <= 0.0f || hpMult == rec.lastHpMult) {
        return;
    }
    // Rebase: preserve damage already taken across rescales
    float unscaledNow = (float)actor->colChkInfo.health / rec.lastHpMult;
    actor->colChkInfo.health = (u8)ClampU8((int)std::lround(unscaledNow * hpMult));
    rec.lastHpMult = hpMult;
}

// Rescale all enemies in the scene by sweeping the live actor lists (roster
// change mid-scene). Sweeping the actual lists - instead of a stale-pointer
// bookkeeping map - guarantees we only touch valid actors and also picks up
// enemies spawned before difficulty was enabled.
static void RescaleAll() {
    if (gPlayState == nullptr) {
        return;
    }
    int rescaled = 0;
    // Enemy + Boss lists cover everything except Dark Link (NPC category)
    for (int cat : { ACTORCAT_ENEMY, ACTORCAT_BOSS, ACTORCAT_NPC }) {
        Actor* actor = gPlayState->actorCtx.actorLists[cat].head;
        while (actor != nullptr) {
            if (actor->category != ACTORCAT_NPC || actor->id == ACTOR_EN_TORCH2) {
                if (actor->colChkInfo.health > 0) {
                    ApplyHpScaling(actor);
                    rescaled++;
                }
            }
            actor = actor->next;
        }
    }
    SPDLOG_INFO("[EnemyDifficulty] rescaled {} enemies for {} players (hp x{:.2f})", rescaled, playerCount, hpMult);
}

// SECTION: hooks

static bool hooksActive = false;
static bool movementScalingActive = false;

// Damage-taken scaling: Health_Change() (z_parameter.c) divides all negative
// healthChange by DefenseModifier. Integer semantics: 2 = half damage, 4 = quarter.
static void UpdateDefenseModifier(u8 mode) {
    int defenseMod = 0;
    if (mode >= 2 && dmgMult > 1.0f) {
        defenseMod = -std::max(2, (int)std::floor(dmgMult));
    }
    GameInteractor::State::DefenseModifier = defenseMod;
}

void OnRoomStateUpdated() {
    RegisterHooks(hooksActive);
}

void RegisterHooks(bool active) {
    hooksActive = active;
    auto anchor = Anchor::Instance;
    u8 mode = (anchor != nullptr) ? anchor->roomState.difficultyMode : (u8)0;
    bool hooksOn = active && mode > 0;

    UpdateDefenseModifier(hooksOn ? mode : 0);

    // -- Generic HP scaling at actor init (health already assigned by Init) --
    COND_HOOK(OnActorInit, hooksOn, [](void* actorRef) {
        Actor* actor = (Actor*)actorRef;
        if (IsEnemyActor(actor)) {
            ApplyHpScaling(actor);
        }
    });

    // -- Clear per-scene bookkeeping (all actors are freed on scene change) --
    COND_HOOK(OnSceneInit, hooksOn, [](s16 sceneNum) {
        scaledActors.clear();
        kdRecords.clear();
    });
    COND_HOOK(OnPlayDestroy, hooksOn, []() {
        scaledActors.clear();
        kdRecords.clear();
    });

    movementScalingActive = hooksOn && mode >= 2;
    COND_HOOK(OnActorDestroy, true, [](void* ref) {
        scaledActors.erase(static_cast<Actor*>(ref));
        kdRecords.erase(static_cast<Actor*>(ref));
    });
}

// Called by Anchor integration when the roster changes (players join/leave)
void ApplyHealthMultiplier(Actor* actor, float multiplier) {
    if (actor == nullptr || multiplier <= 0.0f || !std::isfinite(multiplier)) {
        return;
    }
    if (actor->id == ACTOR_BOSS_DODONGO) {
        auto& health = reinterpret_cast<BossDodongo*>(actor)->health;
        if (health > 0) {
            health = std::clamp<int>(std::lround(health * multiplier), 1, 32000);
        }
        return;
    }
    if (actor->colChkInfo.health > 0) {
        actor->colChkInfo.health = std::clamp<int>(std::lround(actor->colChkInfo.health * multiplier), 1, 255);
        SetActorMaximumHealth(actor, actor->colChkInfo.health);
    }
}

void OnRosterChanged() {
    if (!hooksActive) {
        return;
    }
    RescaleAll();
}

// Recount players from the live roster and live-rescale on change.
// Self is counted via Anchor::IsSaveLoaded(); peers via the clients map.
void RecountAndRescale() {
    if (!hooksActive) {
        return;
    }
    auto anchor = Anchor::Instance;
    if (anchor == nullptr) {
        return;
    }
    int players = 0;
    if (anchor->IsSaveLoaded()) {
        players++; // self
    }
    for (auto& [clientId, client] : anchor->clients) {
        if (!client.self && client.online && client.isSaveLoaded) {
            players++;
        }
    }
    if (players != playerCount) {
        UpdatePlayerCount(players);
        RescaleAll();
        SPDLOG_INFO("[EnemyDifficulty] player count {} -> {}", playerCount, players);
    }
}

} // namespace EnemyDifficulty

// [TIME] | [INFO] | EnemyDifficulty_MovementMultiplier | Scale velocity at movement time without changing AI speed state.
extern "C" float EnemyDifficulty_MovementMultiplier(Actor* actor) {
    return EnemyDifficulty::movementScalingActive && actor != nullptr && EnemyDifficulty::IsCurated(actor->id)
               ? EnemyDifficulty::SpeedMult() : 1.0f;
}
