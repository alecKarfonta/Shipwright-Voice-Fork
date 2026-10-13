#pragma once
#include <cstdint>
struct Actor;
struct PlayState;
namespace ARPG::SkillCombat {
bool Cast(PlayState* play, int id, int rune, float power);
float DamageMultiplier(Actor* source, Actor* target);
void OnHit(Actor* source, Actor* target, float damage);
float MovementMultiplier(Actor* actor);
bool HasStatus(Actor* actor);
void CancelAll(PlayState* play);
bool TryStep(PlayState* play, float distance, int16_t yaw);
int LiveCount(PlayState* play);
uint32_t Hits(int id);
uint32_t Casts(int id);
}
