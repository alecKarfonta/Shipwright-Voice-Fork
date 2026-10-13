#include "EnchantedSystem.h"

// [TIME] | [INFO] | EnchantedSystem.cpp | Fire/Frost/Storm Enchanted elite auras//
// Hooks:
//  - OnDamageActor (enemy hits player): fold elemental bonus into the hit,
//    apply frost chill, roll storm zaps.
//  - OnEnemyDefeat/OnBossDefeat: D2 corpse nova - element ring FX + local
//    damage if the player stands in the blast.
//  - Player movement: chill multiplier feeds ARPG_ActorMovementMultiplier.
//  - OnActorInit: ambient aura FX on enchanted champions/elites.

#include <algorithm>
#include <cmath>

#include "ARPG.h"
#include "ARPGSave.h"
#include "AffixSystem.h"
#include "EnemySystem.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"

#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

extern "C" {
#include "z64.h"
#include "variables.h"
#include "functions.h"
#include "macros.h"
extern PlayState* gPlayState;
}

namespace ARPG::Enchanted {

// The header forward-declares ::Actor; guarantee the engine type is visible here.
using ::Actor;

// SECTION: chill state

// [TIME] | [INFO] | Enchanted.sChill | Countdown in game frames; > 0 means the player is chilled.
static int sChillTimer = 0;

bool EnabledGate() {
    return CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Enchanted"), 1) != 0;
}

float PlayerChillMultiplier() {
    return sChillTimer > 0 ? ChillMultiplier() : 1.0f;
}

// SECTION: FX helpers

static void SpawnNovaFx(PlayState* play, const Vec3f& pos, Element e) {
    Vec3f zero{ 0, 0, 0 };

    Color_RGBA8 prim = e == Element::Fire ? Color_RGBA8{ 255, 100, 20, 0 }
                    : e == Element::Frost ? Color_RGBA8{ 150, 220, 255, 0 }
                                          : Color_RGBA8{ 180, 200, 255, 0 };
    Color_RGBA8 env = e == Element::Fire ? Color_RGBA8{ 255, 200, 60, 0 }
                    : e == Element::Frost ? Color_RGBA8{ 40, 120, 255, 0 }
                                          : Color_RGBA8{ 255, 255, 255, 0 };

    // [TIME] | [INFO] | Enchanted.NovaRing | Expanding element-tinted shockwave marks the blast radius.
    EffectSsBlast_SpawnShockwave(play, const_cast<Vec3f*>(&pos), &zero, &zero, &prim, &env, 22);

    if (e == Element::Fire) {
        // fire: plume burst around the corpse
        for (int i = 0; i < 12; ++i) {
            Vec3f p = pos;
            p.x += ARPG::RandFloat(-70.0f, 70.0f);
            p.z += ARPG::RandFloat(-70.0f, 70.0f);
            Vec3f vel{ ARPG::RandFloat(-2.0f, 2.0f), ARPG::RandFloat(1.0f, 3.0f), ARPG::RandFloat(-2.0f, 2.0f) };
            Vec3f acc{ 0, -0.2f, 0 };
            EffectSsDFire_SpawnFixedScale(play, &p, &vel, &acc, 180, 6);
        }
    } else if (e == Element::Frost) {
        // frost: ice shard cloud
        for (int i = 0; i < 10; ++i) {
            Vec3f p = pos;
            p.x += ARPG::RandFloat(-60.0f, 60.0f);
            p.z += ARPG::RandFloat(-60.0f, 60.0f);
            Vec3f vel{ ARPG::RandFloat(-1.5f, 1.5f), ARPG::RandFloat(0.5f, 2.5f), ARPG::RandFloat(-1.5f, 1.5f) };
            Vec3f acc{ 0, -0.15f, 0 };
            EffectSsEnIce_Spawn(play, &p, ARPG::RandFloat(0.12f, 0.25f), &vel, &acc, &prim, &env, 22);
        }
    } else {
        // storm: bolt crown around the corpse
        for (int i = 0; i < 5; ++i) {
            Vec3f p = pos;
            float angle = (i / 5.0f) * 6.28318f;
            p.x += 60.0f * sinf(angle);
            p.z += 60.0f * cosf(angle);
            EffectSsLightning_Spawn(play, &p, &prim, &env, 14, int16_t(ARPG::RandInt(0, 32767)), 5, 2);
        }
    }
}

// SECTION: combat hooks

// [TIME] | [INFO] | Enchanted.OnPlayerStruck | Fold element bonuses into one engine hit so
// defense applies once and one damage number renders. Storm zaps roll per hit.
void OnPlayerStruck(const ARPGAffixData* affix, float* damage) {
    if (affix == nullptr || damage == nullptr || !ARPG::AffixesEnabled() || !EnabledGate()) {
        return;
    }
    Element e = EnchantOf(*affix);
    if (e == Element::None) {
        return;
    }
    switch (e) {
        case Element::Fire:
            *damage += FireBonusDamage(affix->tier);
            break;
        case Element::Frost:
            sChillTimer = std::min(sChillTimer + ChillDurationFrames(), ChillStackCapFrames());
            break;
        case Element::Storm:
            if (int(ARPG::RandInt(1, 100)) <= StormProcChancePct(affix->tier)) {
                *damage += StormBonusDamage();
            }
            break;
        default:
            break;
    }
}

// [TIME] | [INFO] | Enchanted.OnEnemyDefeat | Corpse nova damages only the local player,
// mirroring D2 where each client resolves its own hazards.
void OnEnemyDefeat(Actor* actor, const ARPGAffixData* affix) {
    if (!ARPG::AffixesEnabled() || !EnabledGate() || actor == nullptr || affix == nullptr || gPlayState == nullptr) {
        return;
    }
    Element e = EnchantOf(*affix);
    if (e == Element::None) {
        return;
    }

    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }
    // Offscreen guard skirmishes must not trigger unexplained death flashes
    // or hazards for a player who never engaged this ambient pack.
    if(!ARPG::Enemies::RewardEligible(actor))return;
    Vec3f pos = actor->world.pos;
    if(Math_Vec3f_DistXYZ(&pos,&player->actor.world.pos)>900)return;
    SpawnNovaFx(gPlayState, pos, e);
    SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &pos, 40,
                                       e == Element::Fire ? NA_SE_EV_EXPLOSION
                                       : e == Element::Frost ? NA_SE_EV_ICE_BROKEN
                                                             : NA_SE_EV_LIGHTNING);

    float dist = Math_Vec3f_DistXYZ(&pos, &player->actor.world.pos);
    if (dist <= NovaRadius(affix->tier)) {
        // falloff: full at the corpse, quarter at the rim (quarter-heart units)
        float t = 1.0f - (dist / NovaRadius(affix->tier)) * 0.75f;
        int damage = std::max(2, int(NovaDamage(affix->tier, affix->level) * t));
        Health_ChangeBy(gPlayState, -damage);
        if (e == Element::Frost) {
            sChillTimer = std::min(sChillTimer + ChillDurationFrames(), ChillStackCapFrames());
        }
        SPDLOG_INFO("[ARPG] {} nova hit player for {} at dist {:.0f}",
                    e == Element::Fire ? "fire" : e == Element::Frost ? "frost" : "storm", damage, dist);
    }
}

// SECTION: registration

static void RegisterEnchantedHooks() {
    // Chill countdown + ambient aura pulse
    COND_HOOK(OnGameFrameUpdate, true, []() {
        if (sChillTimer > 0) {
            sChillTimer--;
        }
    });

    COND_HOOK(OnPlayDestroy, true, []() { sChillTimer = 0; });

    // Death novas ride the existing defeat hooks (fires once per enemy)
    COND_HOOK(OnEnemyDefeat, true, [](void* ref) {
        Actor* actor = static_cast<Actor*>(ref);
        const auto* affix = AffixSystem::GetAffixData(actor);
        if (affix != nullptr) {
            OnEnemyDefeat(actor, affix);
        }
    });
}

static RegisterShipInitFunc initEnchanted(RegisterEnchantedHooks);

} // namespace ARPG::Enchanted
