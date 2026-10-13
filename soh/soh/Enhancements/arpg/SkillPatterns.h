#pragma once
#include "SkillRules.h"
#include <array>

namespace ARPG::SkillPatterns {
struct Shape { float x, z, radius, power; int strike; bool active; };
struct Frame {
    std::array<Shape, 12> shapes{};
    int count = 0;
    void Add(float x, float z, float radius, float power, int strike, bool active) {
        if (count < int(shapes.size())) { shapes[count++] = { x, z, radius, power, strike, active }; }
    }
};
inline int Duration(int id, int rune) {
    if (id == ARPG_SKILL_SWORD_CYCLONE && rune == 2) { return 80; }
    if (id == ARPG_SKILL_SWORD_CROSSCUT && rune == 1) { return 46; }
    if (id == ARPG_SKILL_SWORD_SHOCKWAVE && rune == 1) { return 48; }
    if (id == ARPG_SKILL_MAGIC_FLAME_BURST && rune == 1) { return 64; }
    if (id == ARPG_SKILL_SWORD_BEAM && rune == 2) { return 62; }
    if (id == ARPG_SKILL_MAGIC_METEOR && rune == 1) { return 76; }
    return SkillRules::Definition(id).duration;
}
// [TIME] | [INFO] | SkillPatterns.Sample | Geometry is expressed in Link's local horizontal plane, in 20 Hz simulation frames.
inline Frame Sample(int id, int rune, int age) {
    Frame f;
    auto d = SkillRules::Definition(id);
    float r = d.radius, power = d.potency;
    auto ring = [&](int start, int strike, float radius, float p) {
        int elapsed = age - start;
        if (elapsed >= 0 && elapsed < 14) {
            f.Add(0, 0, radius * std::min(1.0f, (elapsed + 1) / 6.0f), p, strike, elapsed >= 2 && elapsed < 9);
        }
    };
    auto arc = [&](int start, int strike, float p, float advance = 0) {
        int elapsed = age - start;
        if (elapsed < 0 || elapsed >= 10) { return; }
        for (int i = 0; i < 7; ++i) {
            float a = (-1.1f + i * 2.2f / 6) + (elapsed - 5) * 0.03f;
            f.Add(std::sin(a) * r * 0.7f, std::cos(a) * r * 0.7f + advance, r * 0.38f, p, strike, elapsed >= 2 && elapsed < 7);
        }
    };
    switch (id) {
        case ARPG_SKILL_SWORD_CLEAVE:
            if (rune == 1) { ring(0, 0, r, power * 0.8f); }
            else { arc(0, 0, power * (rune == 2 ? 1.6f : 1)); } break;
        case ARPG_SKILL_SWORD_CROSSCUT:
            arc(0, 0, power * (rune == 2 ? 0.85f : 1), rune == 2 ? age * 12.f : 0.f);
            arc(16, 1, power * (rune == 2 ? 0.85f : 1), rune == 2 ? (age - 16) * 12.f : 0.f);
            if (rune == 1) { arc(32, 2, power); } break;
        case ARPG_SKILL_SWORD_CYCLONE:
            for (int i = 0; i < (rune == 2 ? 5 : 3); ++i) { ring(i * 16, i, r * (rune == 1 ? 1.4f : 1), power * (rune == 1 ? 0.75f : 1)); } break;
        case ARPG_SKILL_SWORD_LUNGE:
            if (age < 16) {
                for (int i = 0; i < (rune == 2 ? 5 : 2); ++i) {
                    f.Add(0, 45.f + i * 50.f, r * 0.65f, power * (rune == 1 ? 0.75f : rune == 2 ? 1.6f : 1), 0, age >= 3 && age < 14);
                }
            } break;
        case ARPG_SKILL_SWORD_BLADESTORM:
            for (int i = 0; i < 3; ++i) {
                float a = age * 0.24f + i * 2.0943951f;
                f.Add(std::sin(a) * r * (rune == 1 ? 1.45f : 1), std::cos(a) * r * (rune == 1 ? 1.45f : 1), 48,
                      power * (rune == 1 ? 0.8f : 1), age / 16, age % 16 < 10);
            } break;
        case ARPG_SKILL_SWORD_EXECUTE:
            if (rune == 1) { ring(6, 0, r, power * 0.75f); } else { arc(6, 0, power); } break;
        case ARPG_SKILL_SWORD_SHOCKWAVE:
            if (rune == 2) {
                for (int i = 0; i < 3; ++i) { f.Add(0, 80.f + i * 110.f, 80, power, 0, age >= 3 + i * 3 && age < 9 + i * 3); }
            } else { ring(0, 0, r, power); if (rune == 1) { ring(28, 1, r, power); } } break;
        case ARPG_SKILL_SWORD_BEAM:
            for (int i = 0; i < (rune == 1 ? 3 : 1); ++i) {
                float a = rune == 1 ? (i - 1) * 0.33f : 0;
                float distance = 25.f + (rune == 2 && age > 30 ? 60 - age : age) * 18.f;
                f.Add(std::sin(a) * distance, std::cos(a) * distance, r, power * (rune == 1 ? 0.65f : 1), rune == 2 && age > 30 ? 1 : 0, age > 0);
            } break;
        case ARPG_SKILL_MAGIC_FLAME_BURST:
            for (int i = 0; i < (rune == 1 ? 3 : 1); ++i) { ring(i * 22, i, r, power * (rune == 1 ? 0.6f : rune == 2 ? 1.7f : 1)); } break;
        case ARPG_SKILL_MAGIC_FROST_NOVA: ring(0, 0, r * (rune == 2 ? 0.8f : 1), power); break;
        case ARPG_SKILL_MAGIC_METEOR:
            for (int i = 0; i < (rune == 1 ? 3 : 1); ++i) {
                int elapsed = age - i * 14;
                if (elapsed >= 0 && elapsed < 48) {
                    f.Add(0, i * 130.f, r * (rune == 2 ? 1.5f : 1), power * (rune == 1 ? 0.6f : rune == 2 ? 1.5f : 1), i,
                          elapsed >= 28 && elapsed < 34);
                }
            } break;
        case ARPG_SKILL_GUARD_RIPOSTE: ring(0, 0, r, power * (rune == 2 ? 2 : 1)); break;
        case ARPG_SKILL_GUARD_VENGEANCE: ring(0, 0, 150, 1); break;
        default: break;
    }
    return f;
}
}
