#pragma once

// [TIME] | [INFO] | ARPGBridge.h | C-callable bridge into the ARPG systems for
// engine .c files (z_parameter.c etc.). Implementation lives in ARPG.cpp.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Applies ARPG defensive reductions to a negative healthChange (in place).
// Returns true when damage was fully absorbed (caller should early-out).
// No-op (returns false, leaves value untouched) when ARPG systems are off.
int ARPG_PlayerDamageReduction(int16_t* healthChange);

struct Actor;
struct PlayState;
int ARPG_IsExplorationSecret(struct Actor* actor);
void ARPG_CopyExplorationSecret(struct Actor* child, struct Actor* parent);
int ARPG_SkillCollision(struct PlayState* play, void* at, void* ac);
int ARPG_BossWeaponDamage(struct Actor* actor,uint32_t flags,int nativeDamage);
int ARPG_BossWeaponDamageWithWeapon(struct Actor* actor,struct Actor* source,uint32_t flags,int nativeDamage);
void ARPG_AdventureLaunch(struct Actor* actor,int slot);
void ARPG_AdventureCopy(struct Actor* child,struct Actor* parent);
int ARPG_AdventureTravel(struct Actor* actor,int frames);
float ARPG_AdventureBlastScale(struct Actor* actor);
int ARPG_AdventureSeedElement(struct Actor* actor);
void ARPG_AdventureDetonate(struct Actor* actor);
int ARPG_RelicCollision(struct PlayState*,void* at,void* ac,void* atInfo,void* acInfo,void* point);
void ARPG_RelicArrowReleased(struct Actor*);
void ARPG_RelicShieldBlock(void);
int ARPG_RelicPreserveStick(void);
void ARPG_RelicStickBroken(void);
int ARPG_RelicBottleReleased(void);
int ARPG_RelicBoomerangUpdate(struct Actor*);
void ARPG_RelicSteerBombchu(struct Actor*);
void ARPG_SkillInput(struct PlayState* play);
int ARPG_CustomPauseActive(void);
// [TIME] | [INFO] | ARPG.EnemyTactics | Neutral-state override: 0 vanilla, 1 movement, 2 native attack, 3 native evade.
struct Actor* ARPG_EnemyTarget(struct Actor* actor, struct PlayState* play);
int ARPG_IsRegionalEnemy(struct Actor* actor);
float ARPG_EnemyAwareness(struct Actor* actor);
int ARPG_EnemyTactics(struct Actor* actor, struct PlayState* play);
float ARPG_ActorMovementMultiplier(struct Actor* actor);
int16_t ARPG_MagicCost(int16_t amount);
uint8_t ARPG_AccumulateDamage(float damage, uint8_t accumulated);

#ifdef __cplusplus
}
#endif
