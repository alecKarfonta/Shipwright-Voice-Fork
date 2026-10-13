#include "SkillCombat.h"
#include "ArsenalSystem.h"
#include "BroodSystem.h"
#include "SkillPatterns.h"
#include "SkillVfx.h"
#include "SkillSystem.h"
#include "ARPG.h"
#include "ARPGBridge.h"
#include "ARPGSave.h"
#include "soh/frame_interpolation.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/ObjectExtension/ActorMaximumHealth.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include <array>
#include <cmath>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_En_M_Thunder/z_en_m_thunder.h"
void EnMThunder_Destroy(Actor*, PlayState*);
void func_80837948(PlayState*, Player*, s32);
s8 Player_ItemToItemAction(s32);
void Player_InitItemAction(PlayState*, Player*, s8);
extern PlayState* gPlayState;
}

namespace ARPG::SkillCombat {
namespace {
struct Attack {
    int id = 0, rune = 0, age = 0, duration = 0;
    uint32_t token = 0;
    int16_t yaw = 0;
    float power = 1;
    Vec3f origin{};
    std::array<ColliderCylinder, 12> colliders{};
    SkillPatterns::Frame geometry;
    std::array<Vec3f, 6> chain{};
    int chainCount = 0;
};
struct HitRecord { uint32_t token = 0; uint16_t strikes = 0; };
struct Target {
    std::array<HitRecord, 16> attacks{};
    uint32_t chillUntil = 0, burnUntil = 0;
};
static ObjectExtension::Register<Attack> attackRegistration;
static ObjectExtension::Register<Target> targetRegistration;
uint32_t nextToken = 1;
std::array<uint32_t, ARPG_SKILL_COUNT_> hits{}, casts{};

bool ClearLine(PlayState* play, Vec3f a, Vec3f b) {
    a.y += 20; b.y += 20;
    Vec3f hit{}; CollisionPoly* poly = nullptr; int bg = BGCHECK_SCENE;
    return !BgCheck_EntityLineTest1(&play->colCtx, &a, &b, &hit, &poly, true, true, true, true, &bg);
}
Vec3f World(const Attack& attack, const SkillPatterns::Shape& s) {
    float sn = Math_SinS(attack.yaw), cs = Math_CosS(attack.yaw);
    return { attack.origin.x + cs * s.x + sn * s.z, attack.origin.y,
             attack.origin.z - sn * s.x + cs * s.z };
}
Target* TargetData(Actor* actor) {
    auto& extensions = ObjectExtension::GetInstance();
    if (!extensions.Has<Target>(actor)) { extensions.Set<Target>(actor, Target{}); }
    return extensions.Get<Target>(actor);
}
void Destroy(Actor* actor, PlayState* play) {
    auto* attack = ObjectExtension::GetInstance().Get<Attack>(actor);
    if (attack) { for (auto& collider : attack->colliders) { Collider_DestroyCylinder(play, &collider); } }
    EnMThunder_Destroy(actor, play);
}
// [TIME] | [INFO] | SkillCombat.Draw | Bespoke meshes share the simulation's origin, rune, age and collision pattern.
void Draw(Actor* actor, PlayState* play) {
    const auto* attack = ObjectExtension::GetInstance().Get<Attack>(actor);
    if (!attack || !SkillsEnabled()) { return; }
    SkillVfx::Draw(actor, play, attack->id, attack->rune, std::max(0, attack->age - 1), attack->duration,
                   attack->origin, attack->yaw, attack->geometry, attack->chain.data(), attack->chainCount);
}
void Animate(PlayState* play, int id, int age) {
    auto* player = GET_PLAYER(play);
    if (!SkillRules::Sword(id) || Player_GetMeleeWeaponHeld(player) < 1 || Player_GetMeleeWeaponHeld(player) > 3 ||
        !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || player->invincibilityTimer < 0) { return; }
    int style = id == ARPG_SKILL_SWORD_CYCLONE ? PLAYER_MWA_SPIN_ATTACK_1H : id == ARPG_SKILL_SWORD_LUNGE ? PLAYER_MWA_STAB_1H :
                id == ARPG_SKILL_SWORD_EXECUTE || id == ARPG_SKILL_SWORD_SHOCKWAVE ? PLAYER_MWA_FORWARD_SLASH_1H :
                (age / 16) % 2 ? PLAYER_MWA_LEFT_SLASH_1H : PLAYER_MWA_RIGHT_SLASH_1H;
    player->unk_845 = 0;
    func_80837948(play, player, style + (Player_HoldsTwoHandedWeapon(player) ? 1 : 0));
}
void Update(Actor* actor, PlayState* play) {
    auto* attack = ObjectExtension::GetInstance().Get<Attack>(actor);
    if (!attack || !SkillsEnabled() || play->transitionTrigger != TRANS_TRIGGER_OFF || gSaveContext.health <= 0) {
        Actor_Kill(actor); return;
    }
    if (GameInteractor::IsGameplayPaused() || !GameInteractor::IsPlayerInControl()) { return; }
    if (attack->age >= attack->duration) { Actor_Kill(actor); return; }
    auto* player = GET_PLAYER(play);
    bool follow = attack->id != ARPG_SKILL_SWORD_BEAM && attack->id != ARPG_SKILL_MAGIC_METEOR && attack->id != ARPG_SKILL_MAGIC_CHAIN;
    if (follow) { attack->origin = player->actor.world.pos; }
    if (attack->id == ARPG_SKILL_SWORD_LUNGE && attack->rune != 2 && attack->age < 12) {
        TryStep(play, attack->rune == 1 ? 18.f : 12.f, attack->yaw);
        attack->origin = player->actor.world.pos;
    }
    if (attack->age == 0 || ((attack->id == ARPG_SKILL_SWORD_CROSSCUT || attack->id == ARPG_SKILL_SWORD_CYCLONE) && attack->age % 16 == 0)) {
        Animate(play, attack->id, attack->age);
    }
    attack->geometry = SkillPatterns::Sample(attack->id, attack->rune, attack->age);
    if (attack->id == ARPG_SKILL_MAGIC_CHAIN) {
        attack->geometry = {};
        for (int i = 0; i < attack->chainCount; ++i) {
            int t = attack->age - i * 4;
            if (t < 0 || t >= 8) { continue; }
            Vec3f delta = { attack->chain[i].x - attack->origin.x, 0, attack->chain[i].z - attack->origin.z };
            float sn = Math_SinS(attack->yaw), cs = Math_CosS(attack->yaw);
            attack->geometry.Add(cs * delta.x - sn * delta.z, sn * delta.x + cs * delta.z, 65,
                1.4f * (attack->rune == 1 ? 0.8f : attack->rune == 2 ? 1.8f : 1), 0, t < 4);
        }
    }
    for (int i = 0; i < attack->geometry.count; ++i) {
        auto& s = attack->geometry.shapes[i];
        auto point = World(*attack, s);
        if (!s.active || !ClearLine(play, attack->origin, point)) { continue; }
        auto& collider = attack->colliders[i];
        collider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
        collider.info.toucherFlags &= ~(TOUCH_HIT | TOUCH_DREW_HITMARK);
        collider.dim.radius = std::max(1, int(s.radius));
        collider.dim.pos = { s16(point.x), s16(point.y + 10), s16(point.z) };
        CollisionCheck_SetAT(play, &play->colChkCtx, &collider.base);

    }
    actor->world.pos = attack->origin;
    ++attack->age;
}
}

bool TryStep(PlayState* play, float distance, int16_t yaw) {
    auto* player = GET_PLAYER(play);
    if (!(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) { return false; }
    Vec3f point = player->actor.world.pos;
    point.x += Math_SinS(yaw) * distance; point.z += Math_CosS(yaw) * distance;
    if (!BgCheck_PosInStaticBoundingBox(&play->colCtx, &point) || !ClearLine(play, player->actor.world.pos, point)) { return false; }
    Vec3f ray = point; ray.y += 60;
    CollisionPoly* poly = nullptr; int bg = BGCHECK_SCENE;
    float floor = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bg, &player->actor, &ray);
    if (!poly || bg != BGCHECK_SCENE || floor <= BGCHECK_Y_MIN || std::abs(floor - point.y) > 35 || poly->normal.y < 24000) { return false; }
    auto property = func_80041EA4(&play->colCtx, poly, bg);
    auto type = SurfaceType_GetFloorType(&play->colCtx, poly, bg);
    if (property == FLOOR_PROPERTY_5 || property == FLOOR_PROPERTY_12 || type == 2 || type == 3 || type == 7 || type == 9 || type == 12 ||
        SurfaceType_GetSceneExitIndex(&play->colCtx, poly, bg) || SurfaceType_IsWallDamage(&play->colCtx, poly, bg)) { return false; }
    float water = 0; WaterBox* box = nullptr;
    if (WaterBox_GetSurface1(play, &play->colCtx, point.x, point.z, &water, &box) && water > floor + 5) { return false; }
    point.y = floor;
    player->actor.world.pos = point;
    return true;
}

bool Cast(PlayState* play, int id, int rune, float power) {
    if (!play || play->actorCtx.total >= 180 || LiveCount(play) >= 4) { return false; }
    auto* player = GET_PLAYER(play);
    if (player->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING) { return false; }
    Attack data;
    data.id = id; data.rune = rune; data.duration = SkillPatterns::Duration(id, rune);
    data.token = nextToken++; if (nextToken == 0) { ++nextToken; }
    data.yaw = player->actor.shape.rot.y; data.origin = player->actor.world.pos;
    data.power = power * SkillTree::Power(gARPGSave,id);
    if (id == ARPG_SKILL_MAGIC_METEOR) {
        Vec3f candidate = data.origin;
        auto* target = player->focusActor;
        float distance = target && (EnemyDifficulty::IsEnemyActor(target) || Brood::Targetable(target)) ? std::min(500.0f, Math_Vec3f_DistXZ(&target->world.pos, &data.origin)) : 250;
        candidate.x += Math_SinS(data.yaw) * distance; candidate.z += Math_CosS(data.yaw) * distance;
        if (!ClearLine(play, data.origin, candidate)) { return false; }
        data.origin = candidate;
    }
    if (id == ARPG_SKILL_MAGIC_CHAIN) {
        Vec3f cursor = data.origin;
        std::array<Actor*, 6> picked{};
        int limit = rune == 1 ? 6 : rune == 2 ? 2 : 4;
        for (int i = 0; i < limit; ++i) {
            Actor* best = nullptr; float nearest = i == 0 ? 500.f : 330.f;
            for (int category : { ACTORCAT_ENEMY, ACTORCAT_BOSS, ACTORCAT_PROP }) {
                for (Actor* a = play->actorCtx.actorLists[category].head; a; a = a->next) {
                    if (!a->update || !a->colChkInfo.health || (!EnemyDifficulty::IsEnemyActor(a) && !Brood::Targetable(a)) || std::find(picked.begin(), picked.end(), a) != picked.end()) { continue; }
                    float distance = Math_Vec3f_DistXYZ(&a->world.pos, &cursor);
                    if (distance < nearest && ClearLine(play, cursor, a->world.pos)) { best = a; nearest = distance; }
                }
            }
            if (!best) { break; }
            picked[i] = best; cursor = best->world.pos; data.chain[data.chainCount++] = cursor;
        }
        if (data.chainCount == 0) { return false; }
    }
    auto* actor = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_M_THUNDER, data.origin.x, data.origin.y, data.origin.z, 0, data.yaw, 0, 1);
    if (!actor || !actor->update) { return false; }
    auto* pulse = reinterpret_cast<EnMThunder*>(actor);
    pulse->isUsingMagic = 0;
    pulse->spinAttackTimer = 0;
    actor->update = Update; actor->draw = Draw; actor->destroy = Destroy;
    actor->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    actor->world.pos = data.origin;
    ObjectExtension::GetInstance().Set<Attack>(actor, std::move(data));
    auto* attack = ObjectExtension::GetInstance().Get<Attack>(actor);
    uint32_t sword = CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_KOKIRI ? DMG_SLASH_KOKIRI :
                     CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_BIGGORON ? DMG_SLASH_GIANT : DMG_SLASH_MASTER;
    uint32_t flags = SkillRules::Sword(id) ? sword : id == ARPG_SKILL_MAGIC_FROST_NOVA ? DMG_MAGIC_ICE :
                     id == ARPG_SKILL_MAGIC_FLAME_BURST || id == ARPG_SKILL_MAGIC_METEOR ? DMG_MAGIC_FIRE : DMG_ARROW_LIGHT;
    pulse->collider.info.toucher.dmgFlags = flags;
    ColliderCylinderInit init = {
        { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_1, COLSHAPE_CYLINDER },
        { ELEMTYPE_UNK2, { flags, 0, 4 }, { 0, 0, 0 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
        { 0, 80, -10, { 0, 0, 0 } }
    };
    for (auto& collider : attack->colliders) {
        Collider_InitCylinder(play, &collider); Collider_SetCylinder(play, &collider, actor, &init);
    }
    if (SkillRules::Sword(id) && (Player_GetMeleeWeaponHeld(player) < 1 || Player_GetMeleeWeaponHeld(player) > 3)) {
        int item = gSaveContext.equips.buttonItems[0];
        if (item == ITEM_SWORD_KOKIRI || item == ITEM_SWORD_MASTER || item == ITEM_SWORD_BGS || item == ITEM_SWORD_KNIFE) {
            int action = Player_ItemToItemAction(item);
            player->heldItemId = item;
            player->nextModelGroup = Player_ActionToModelGroup(player, action);
            Player_InitItemAction(play, player, action);
        }
    }
    if (id >= 0 && id < ARPG_SKILL_COUNT_) { ++casts[id]; }
    Audio_PlayActorSound2(&player->actor, SkillRules::Sword(id) ? NA_SE_IT_SWORD_SWING_HARD : NA_SE_IT_ROLLING_CUT_LV1);
    return true;
}

float DamageMultiplier(Actor* source, Actor* target) {
    auto* attack = ObjectExtension::GetInstance().Get<Attack>(source);
    if (!SkillsEnabled() || !attack) { return 1; }
    float power = attack->geometry.count > 0 ? attack->geometry.shapes[0].power : SkillRules::Definition(attack->id).potency;
    if (attack->id == ARPG_SKILL_SWORD_EXECUTE && target->colChkInfo.health <= GetActorMaximumHealth(target) * (attack->rune == 2 ? 0.6f : 0.35f)) { power *= 2; }
    const auto* status = ObjectExtension::GetInstance().Get<Target>(target);
    if (attack->id == ARPG_SKILL_MAGIC_FROST_NOVA && attack->rune == 2 && status && status->chillUntil > gPlayState->gameplayFrames) { power *= 2; }
    if (ARPG_SKILLS[attack->id].tree == ARPG_TREE_MAGIC) {
        power *= SkillRules::LevelPower(gARPGSave.level) * (1 + 0.10f * gARPGSave.skillRanks[ARPG_SKILL_MAGIC_RESONANCE] * (HasStatus(target) ? 2 : 1));
    }
    return power * attack->power;
}
void OnHit(Actor* source, Actor* target, float damage) {
    auto* attack = ObjectExtension::GetInstance().Get<Attack>(source);
    if (!SkillsEnabled() || !attack || damage <= 0) { return; }
    ++hits[attack->id];
    auto* data = TargetData(target);
    if (attack->id == ARPG_SKILL_MAGIC_FROST_NOVA) { data->chillUntil = gPlayState->gameplayFrames + (attack->rune == 1 ? 240 : 120); }
    if (attack->id == ARPG_SKILL_MAGIC_FLAME_BURST || attack->id == ARPG_SKILL_MAGIC_METEOR) { data->burnUntil = gPlayState->gameplayFrames + 120; }
}
float MovementMultiplier(Actor* actor) {
    const auto* data = ObjectExtension::GetInstance().Get<Target>(actor);
    return SkillsEnabled() && data && gPlayState && data->chillUntil > gPlayState->gameplayFrames && actor->category != ACTORCAT_BOSS ? 0.6f : 1;
}
bool HasStatus(Actor* actor) {
    const auto* data = ObjectExtension::GetInstance().Get<Target>(actor);
    return data && gPlayState && (data->chillUntil > gPlayState->gameplayFrames || data->burnUntil > gPlayState->gameplayFrames);
}
void CancelAll(PlayState* play) {
    if (!play) { return; }
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].head; a; a = a->next) {
        if (ObjectExtension::GetInstance().Has<Attack>(a)) { Actor_Kill(a); }
    }
}
int LiveCount(PlayState* play) {
    int count = 0;
    if (play) { for (Actor* a = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].head; a; a = a->next) { count += a->update && ObjectExtension::GetInstance().Has<Attack>(a); } }
    return count;
}
uint32_t Hits(int id) { return id >= 0 && id < ARPG_SKILL_COUNT_ ? hits[id] : 0; }
uint32_t Casts(int id) { return id >= 0 && id < ARPG_SKILL_COUNT_ ? casts[id] : 0; }
}

// [TIME] | [INFO] | ARPG_SkillCollision | Reject walls and duplicate limb contacts before native hit flags, reactions, or shield bounce are written.
extern "C" int ARPG_SkillCollision(PlayState* play, void* atRef, void* acRef) {
    auto* at = static_cast<Collider*>(atRef);
    auto* ac = static_cast<Collider*>(acRef);
    using namespace ARPG::SkillCombat;
    auto* attack = ObjectExtension::GetInstance().Get<Attack>(at->actor);
    if (!attack) { return ARPG::Arsenal::Collision(play,atRef,acRef); }
    if (!ARPG::SkillsEnabled() || !ac->actor || (!EnemyDifficulty::IsEnemyActor(ac->actor) && !ARPG::Brood::Targetable(ac->actor)) || !ClearLine(play, attack->origin, ac->actor->world.pos)) { return 0; }
    int index = -1;
    for (int i = 0; i < attack->geometry.count; ++i) { if (at == &attack->colliders[i].base) { index = i; break; } }
    if (index < 0 || !attack->geometry.shapes[index].active) { return 0; }
    auto* target = TargetData(ac->actor);
    auto& record = target->attacks[attack->token % target->attacks.size()];
    if (record.token != attack->token) { record = { attack->token, 0 }; }
    uint16_t strike = 1u << std::clamp(attack->geometry.shapes[index].strike, 0, 15);
    if (record.strikes & strike) { return 0; }
    record.strikes |= strike;
    return 1;
}
