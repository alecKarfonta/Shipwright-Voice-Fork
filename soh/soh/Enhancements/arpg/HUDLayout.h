#pragma once
#include <algorithm>
#include <cmath>

namespace ARPG::HUDLayout {
// Grow gently with logical viewport height; high-resolution HUDs stay compact.
inline float Scale(float height) {
    return std::clamp(0.85f * std::sqrt(std::max(height, 1.0f) / 1080.0f), 0.8f, 1.15f);
}
inline float SlotWidth(float width, float scale) {
    // Six slots, five gaps, window padding and an outer safe margin.
    return std::max(1.0f, std::min(92.0f * scale, (width - 32.0f - 30.0f * scale) / 6.0f));
}
}
