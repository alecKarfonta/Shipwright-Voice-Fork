#include "ARPGSpells.h"
#include "ARPG.h"
#include "ARPGSave.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "macros.h"
#include "overlays/actors/ovl_En_M_Thunder/z_en_m_thunder.h"
#include "overlays/actors/ovl_Magic_Fire/z_magic_fire.h"
void MagicFire_Draw(Actor*, PlayState*);
void MagicWind_Destroy(Actor*, PlayState*);
void MagicDark_Destroy(Actor*, PlayState*);
extern PlayState* gPlayState;
}

namespace ARPG::Spells {

struct SpellData {
    uint8_t skill;
    float damage;
};
static ObjectExtension::Register<SpellData> spellDataRegister;

// [TIME] | [INFO] | Spells.UpdatePulse | Reuse the spin-attack collider, renderer, and cleanup with independent lifetime/position.
static void UpdatePulse(EnMThunder* pulse, PlayState* play) {
    const auto* spell = ObjectExtension::GetInstance().Get<SpellData>(pulse);
    if (!SkillsEnabled() || spell == nullptr || GameInteractor::IsGameplayPaused() ||
        Math_StepToF(&pulse->spinAttackTimer, 0.0f, 1.0f / 20.0f)) {
        Actor_Kill(&pulse->actor);
        return;
    }
    bool beam = spell->skill == ARPG_SKILL_SWORD_BEAM;
    if (beam) {
        pulse->actor.world.pos.x += Math_SinS(pulse->actor.world.rot.y) * 18.0f;
        pulse->actor.world.pos.z += Math_CosS(pulse->actor.world.rot.y) * 18.0f;
        Actor_UpdateBgCheckInfo(play, &pulse->actor, 5.0f, 10.0f, 5.0f, 5);
        if (pulse->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
            Actor_Kill(&pulse->actor);
            return;
        }
    }
    Actor_SetScale(&pulse->actor, beam ? 1.2f : (1.0f - pulse->spinAttackTimer) * 6.0f);
    pulse->collider.dim.radius = static_cast<int16_t>(pulse->actor.scale.x * 25.0f);
    Collider_UpdateCylinder(&pulse->actor, &pulse->collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &pulse->collider.base);
    pulse->spinAttackAlpha = pulse->spinAttackTimer;
    pulse->spinTrailTexScroll += 2.0f;
}

static Actor* SpawnPulse(PlayState* play, uint8_t skill, float multiplier) {
    Player* player = GET_PLAYER(play);
    if (player->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING) {
        return nullptr;
    }
    Actor* actor = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_M_THUNDER,
                              player->actor.world.pos.x, player->actor.world.pos.y + 20.0f,
                              player->actor.world.pos.z, 0, player->actor.shape.rot.y, 0, 1);
    if (actor == nullptr || actor->update == nullptr) {
        return nullptr;
    }
    auto* pulse = reinterpret_cast<EnMThunder*>(actor);
    ObjectExtension::GetInstance().Set<SpellData>(actor, SpellData{ skill, multiplier });
    pulse->actionFunc = UpdatePulse;
    pulse->spinAttackTimer = 1.0f;
    pulse->spinAttackAlpha = 1.0f;
    pulse->chargeAlpha = 0;
    pulse->attackStrength = skill == ARPG_SKILL_MAGIC_FROST_NOVA ? 1 : 0;
    pulse->isUsingMagic = 0;
    pulse->followPlayerTimer = 0;
    uint32_t swordDamage = CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_KOKIRI ? DMG_SLASH_KOKIRI :
                           CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_BIGGORON ? DMG_SLASH_GIANT : DMG_SLASH_MASTER;
    pulse->collider.info.toucher.dmgFlags = skill == ARPG_SKILL_MAGIC_FROST_NOVA ? DMG_MAGIC_ICE :
                                          skill == ARPG_SKILL_SWORD_SHOCKWAVE ? DMG_HAMMER_SWING :
                                          skill == ARPG_SKILL_SWORD_BEAM ? swordDamage : DMG_ARROW_LIGHT;
    pulse->collider.info.toucher.damage = 4;
    return actor;
}

static void DrawFlameBurst(Actor* actor, PlayState* play) {
    Matrix_Scale(1.5f, 1.5f, 1.5f, MTXMODE_APPLY);
    MagicFire_Draw(actor, play);
}

// [TIME] | [INFO] | Spells.DestroyUtility | Our skills own their meter transactions; an older utility actor must not reset a newer spell's debit.
static void DestroyUtility(Actor* actor, PlayState* play) {
    auto state = gSaveContext.magicState;
    auto previous = gSaveContext.prevMagicState;
    if (actor->id == ACTOR_MAGIC_WIND) { MagicWind_Destroy(actor, play); }
    else { MagicDark_Destroy(actor, play); }
    gSaveContext.magicState = state;
    gSaveContext.prevMagicState = previous;
}

bool Cast(PlayState* play, uint8_t skill) {
    Player* player = GET_PLAYER(play);
    if (skill == ARPG_SKILL_MAGIC_RECALL) {
        Actor* actor = Actor_Spawn(&play->actorCtx, play, ACTOR_MAGIC_WIND, static_cast<float>(gSaveContext.fw.pos.x),
                                  static_cast<float>(gSaveContext.fw.pos.y), static_cast<float>(gSaveContext.fw.pos.z), 0, gSaveContext.fw.yaw, 0, 1);
        if (actor == nullptr || actor->update == nullptr) { return false; }
        actor->destroy = DestroyUtility;
        Player_ReleaseLockOn(player);
        player->actor.world.pos = { float(gSaveContext.fw.pos.x), float(gSaveContext.fw.pos.y), float(gSaveContext.fw.pos.z) };
        player->actor.prevPos = player->actor.world.pos;
        player->actor.world.rot.y = player->actor.shape.rot.y = gSaveContext.fw.yaw;
        player->actor.speedXZ = player->linearVelocity = 0;
        player->actor.velocity = {};
        player->invincibilityTimer = std::max<int>(player->invincibilityTimer, 20);
        return true;
    }
    if (skill == ARPG_SKILL_MAGIC_FLAME_BURST) {
        Actor* actor = Actor_Spawn(&play->actorCtx, play, ACTOR_MAGIC_FIRE, player->actor.world.pos.x,
                                  player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0, 0);
        if (actor == nullptr || actor->update == nullptr) { return false; }
        ObjectExtension::GetInstance().Set<SpellData>(actor, SpellData{ skill, 2.0f });
        actor->draw = DrawFlameBurst;
        return true;
    }
    if (skill == ARPG_SKILL_MAGIC_HOLY_AURA) {
        if (gSaveContext.nayrusLoveTimer != 0) { return false; }
        gSaveContext.nayrusLoveTimer = 1;
        Actor* actor = Actor_Spawn(&play->actorCtx, play, ACTOR_MAGIC_DARK, player->actor.world.pos.x,
                                  player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0, 0);
        if (actor == nullptr || actor->update == nullptr) { gSaveContext.nayrusLoveTimer = 0; return false; }
        actor->destroy = DestroyUtility;
        ObjectExtension::GetInstance().Set<SpellData>(actor, SpellData{ skill, 1.0f });
        return true;
    }
    return SpawnPulse(play, skill, skill == ARPG_SKILL_SWORD_BEAM ? 2.0f : 1.0f) != nullptr;
}

// [TIME] | [INFO] | Spells.CancelAura | Native Nayru actors increment their timer even after it is cleared; remove only our owned actor.
void CancelAura(PlayState* play) {
    if (!play) { return; }
    bool owned = false;
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].head; actor; actor = actor->next) {
        const auto* data = ObjectExtension::GetInstance().Get<SpellData>(actor);
        if (actor->id == ACTOR_MAGIC_DARK && data && data->skill == ARPG_SKILL_MAGIC_HOLY_AURA) {
            Actor_Kill(actor); owned = true;
        }
    }
    if (owned) {
        gSaveContext.nayrusLoveTimer = 0;
        if (GET_PLAYER(play)->invincibilityTimer == -100) { GET_PLAYER(play)->invincibilityTimer = 0; }
    }
}

float DamageMultiplier(Actor* source) {
    const auto* spell = ObjectExtension::GetInstance().Get<SpellData>(source);
    return SkillsEnabled() && spell != nullptr ? spell->damage : 1.0f;
}

void Reflect(PlayState* play, float strength) {
    SpawnPulse(play, ARPG_SKILL_GUARD_VENGEANCE, strength);
}

static void RegisterSpellHooks() {
    COND_ID_HOOK(OnActorUpdate, ACTOR_MAGIC_FIRE, true, [](void* ref) {
        auto* fire = static_cast<MagicFire*>(ref);
        const auto* spell = ObjectExtension::GetInstance().Get<SpellData>(ref);
        if (spell == nullptr) { return; }
        if (!SkillsEnabled()) {
            Actor_Kill(&fire->actor);
            return;
        }
        // [TIME] | [INFO] | Spells.FlameBurst | Derive dimensions from scale so the pre-cast delay cannot compound them.
        fire->collider.dim.radius = static_cast<int16_t>(fire->actor.scale.x * 487.5f);
        fire->collider.dim.height = static_cast<int16_t>(fire->actor.scale.y * 675.0f);
        fire->collider.dim.yShift = static_cast<int16_t>(fire->actor.scale.y * -337.5f);
    });
}
static RegisterShipInitFunc initSpells(RegisterSpellHooks);

} // namespace ARPG::Spells
