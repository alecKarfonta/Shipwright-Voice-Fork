#pragma once

#include "ARPGTypes.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace ARPG::Rules {

// [TIME] | [INFO] | Rules.NextRandom | Defined integer arithmetic keeps the saved stream portable across platforms.
inline uint32_t NextRandom(uint32_t& state) {
    state += 0x9E3779B9u;
    uint32_t value = state;
    value = (value ^ (value >> 16)) * 0x21F0AAADu;
    value = (value ^ (value >> 15)) * 0x735A2D97u;
    return value ^ (value >> 15);
}

inline uint32_t RandomRange(uint32_t& state, uint32_t min, uint32_t max) {
    if (max <= min) {
        return min;
    }
    uint32_t range = max - min + 1;
    if (range == 0) {
        return NextRandom(state);
    }
    uint32_t threshold = (0u - range) % range;
    uint32_t value;
    do {
        value = NextRandom(state);
    } while (value < threshold);
    return min + value % range;
}

inline uint8_t TakeDistinct(uint32_t& state, uint8_t* pool, uint8_t size, uint8_t index) {
    uint8_t selected = RandomRange(state, index, size - 1);
    std::swap(pool[index], pool[selected]);
    return pool[index];
}

// [TIME] | [INFO] | Rules.XpThreshold | Cumulative XP needed to leave the current level; level 50 is the cap.
inline uint32_t XpThreshold(uint8_t level) {
    if (level == 0) {
        return 0;
    }
    return 80u * level * level + 120u * level;
}

inline uint32_t SaturatingAdd(uint32_t value, uint32_t amount) {
    return static_cast<uint32_t>(std::min<uint64_t>(uint64_t(value) + amount, UINT32_MAX));
}

inline uint8_t GrantXp(ARPGSaveData& save, uint32_t amount) {
    save.level = std::clamp<uint8_t>(save.level, 1, ARPG_MAX_LEVEL);
    uint8_t previous = save.level;
    save.xp = std::min(SaturatingAdd(save.xp, amount), XpThreshold(ARPG_MAX_LEVEL - 1));
    while (save.level < ARPG_MAX_LEVEL && save.xp >= XpThreshold(save.level)) {
        ++save.level;
        if (save.level % 2 == 0) { save.activePoints = std::min<int>(255, save.activePoints + 1); }
        save.passivePoints = std::min<int>(255, save.passivePoints + 1);
    }
    return save.level - previous;
}

inline int ReducedDamage(int damage, float multiplier, int flat) {
    if (damage <= 0) { return 0; }
    // Armor points are percentage reduction, not health units. Temporary
    // defensive skills retain their own multipliers; armor alone caps at 25%.
    float armor = 1.0f - std::clamp(flat, 0, 25) / 100.0f;
    return std::max(1, static_cast<int>(std::ceil(damage * std::clamp(multiplier, 0.0f, 1.0f) * armor)));
}

inline float ResistanceMultiplier(uint16_t divisor) {
    return divisor == UINT16_MAX ? 0.0f : (divisor == 0 ? 1.0f : 256.0f / divisor);
}

// [TIME] | [INFO] | Rules.ClampDamage | Keep fractional positive hits useful without overflowing the engine's u8 accumulator.
inline uint8_t ClampDamage(float damage, uint8_t accumulated) {
    if (!std::isfinite(damage) || damage <= 0.0f) {
        return accumulated;
    }
    return static_cast<uint8_t>(std::min(255.0f, accumulated + std::max(1.0f, std::round(damage))));
}

} // namespace ARPG::Rules
