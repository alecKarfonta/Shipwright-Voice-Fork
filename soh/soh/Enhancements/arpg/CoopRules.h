#pragma once

// [TIME] | [INFO] | CoopRules.h | Deterministic co-op ARPG rolls
//
// Diablo II lets every player in a party see the same monster affixes. Anchor
// co-op gives each client its own world copy, so the cheapest parity strategy
// is determinism: instead of drawing from the per-save stream RNG, connected
// clients derive every roll from hash(roomSeed, actor identity). The same
// enemy on two screens yields the same tier, stats, affix set and elite name
// with zero extra network traffic. Loot intentionally stays on the personal
// stream so drops remain instanced per player (D2 personal loot).
//
// Actor identity = (scene, actorId, params, occurrence). `occurrence` counts
// how many actors with an identical (actorId, params) pair have already been
// configured in this scene; without it, three spawned Stalfos would share one
// roll. It is assigned in actor-init order, which is deterministic for scene
// spawns and stable in practice for packs spawned from the same seed.

#include "AffixData.h"
#include "ARPGRules.h"
#include "EnemyRules.h"

#include <cstdint>

namespace ARPG::Coop {

// [TIME] | [INFO] | Coop.Hash | FNV-style integer hash; stable on every platform and compiler.
inline uint32_t Hash(uint32_t a, uint32_t b) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < 4; ++i) {
        h = (h ^ (a & 0xFF)) * 16777619u;
        h = (h ^ (b & 0xFF)) * 16777619u;
        a >>= 8;
        b >>= 8;
    }
    // Final avalanche so nearby identities differ wildly
    h ^= h >> 16;
    h *= 0x735A2D97u;
    h ^= h >> 15;
    return h;
}

struct RollContext {
    uint32_t seed;      // room arpgSeed (nonzero when shared rolls active)
    uint32_t scene;     // gSaveContext.sceneNum (or entrance when scene==0)
    uint32_t actorId;   // actor->id
    uint32_t params;    // actor->params as u32
    uint32_t occurrence; // nth identical (actorId, params) actor in this scene
};

// Derive a private stream for one actor from the roll context. Each call
// site gets an independent stream by passing a distinct purpose salt
// (0 = tier, 1 = stats, 2 = affix selection, 3 = elite name).
inline uint32_t StreamFor(const RollContext& ctx, uint32_t purpose) {
    uint32_t s = Hash(Hash(Hash(ctx.seed, ctx.scene), Hash(ctx.actorId, ctx.params)),
                      Hash(ctx.occurrence, purpose));
    // Warm up: burn one draw so low-entropy seeds don't correlate with tier
    Rules::NextRandom(s);
    return s;
}

inline int RollTier(const RollContext& ctx, int chancePct, int championRatioPct, bool isBoss) {
    if (isBoss) {
        return ARPG_TIER_BOSS;
    }
    uint32_t s = StreamFor(ctx, 0);
    int roll = int(Rules::RandomRange(s, 1, 100));
    if (roll > chancePct) {
        return ARPG_TIER_NORMAL;
    }
    int second = int(Rules::RandomRange(s, 1, 100));
    return second <= championRatioPct ? ARPG_TIER_CHAMPION : ARPG_TIER_ELITE;
}

// Same formula as Enemies::RollStats but from the actor's private stream.
inline Enemies::Stats RollStatsFor(const RollContext& ctx, const Enemies::Region& region, int tier) {
    uint32_t s = StreamFor(ctx, 1);
    return Enemies::RollStats(s, region, tier);
}

// Distinct affix selection identical to Rules::TakeDistinct, but from the
// actor's private stream. pool is permuted in place.
inline uint8_t TakeAffix(const RollContext& ctx, uint8_t* pool, uint8_t size, uint8_t index) {
    uint32_t s = StreamFor(ctx, 2);
    return Rules::TakeDistinct(s, pool, size, index);
}

inline const char* EliteName(const RollContext& ctx) {
    static const char* names[8] = { "Ashfang", "Dreadhowl", "Grimscar", "Nightmaw",
                                    "Stormclaw", "Bonebreaker", "Ironspite", "Frostbane" };
    uint32_t s = StreamFor(ctx, 3);
    return names[Rules::RandomRange(s, 0, 7)];
}

} // namespace ARPG::Coop
