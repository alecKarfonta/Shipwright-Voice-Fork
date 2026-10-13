#include "HUDLayout.h"
#include "ExplorationRules.h"
#include "EnemySystem.h"
#include "CreatureSystem.h"
#include "WorldPopulationRules.h"
#include "ExpeditionSystem.h"
#include "ARPGTheme.h"
#include "ARPG.h"
#include "ARPGBridge.h"
#include "ARPGSave.h"
#include "ARPGTesting.h"
#include "AffixSystem.h"
#include "soh/Enhancements/nametag.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/ObjectExtension/ActorMaximumHealth.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/SohGui/UIWidgets.hpp"
#include <libultraship/bridge/consolevariablebridge.h>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>
#include <vector>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "overlays/actors/ovl_En_Heishi2/z_en_heishi2.h"
#include "overlays/actors/ovl_En_Bird/z_en_bird.h"
#include "objects/object_sd/object_sd.h"
extern PlayState* gPlayState;
}

namespace ARPG::Enemies {
namespace {
struct Member {
    uint16_t pack;
    bool regional = false, playerEngaged = false, generated = true;
    uint32_t originalCulling = 0;
    float originalForward = 0, originalScale = 0, originalDownward = 0;
    PackKind kind = PackKind::Warband;
};
PatrolPlanningBudget patrolPlanning;
struct Patrol { Vec3f destination{}, lastPosition{}; uint32_t retryAt = 0; int blocked = 0, routeAttempt = 0; int16_t routeYaw = 0; bool valid = false; };
struct Watchman {
    Patrol patrol{}; ColliderCylinder spear{};
    std::string name; const char* action = "";
    int cooldown = 0, recovery = 0, swing = 0, animation = -1, aidCooldown = 0, stagger = 0;
    bool support = false, engaged = false, sharedFight = false;
    Vec3f lastBattle{};
    int aftermath = 0, quietFrames = 0;
    uint32_t attacks = 0, hits = 0, assists = 0, searches = 0;
};
static ObjectExtension::Register<Watchman> watchmanRegister;
struct Wildlife { ActorFunc nativeUpdate=nullptr; };
static ObjectExtension::Register<Wildlife> wildlifeRegister;
bool wildlifePopulated = false;
bool regionPopulated = false;
int populatedRoom = -1;
int populationTarget = 0;
double populationArea = 0;
Vec3f roomEntry{};
struct Brain {
    Patrol patrol{};
    uint32_t nextDecision = 0, nextAttack = 0, nextEvade = 0, leaseEnd = 0, attacks = 0;
    uint32_t swingStart = 0;
    bool sawSwing = false;
    int flank = 1;
    Role role = Role::Vanguard;
    Tactic tactic = Tactic::Approach;
};
static ObjectExtension::Register<Member> memberRegister;
static ObjectExtension::Register<Brain> brainRegister;
uint16_t nextPack = 1;
uint32_t nextEncounter = 0, nextAttackSlot = 0;
std::set<int> expandedRooms;
std::vector<Vec3f> explored;
const char* groundFailure = "none";
std::string lastSpawn = "Explore a region or summon a test pack.";

float Distance(const Vec3f& a, const Vec3f& b) {
    return std::hypot(a.x - b.x, a.z - b.z);
}
bool ClearLine(PlayState* play, Vec3f a, Vec3f b) {
    a.y += 45; b.y += 45;
    Vec3f hit{};
    CollisionPoly* poly = nullptr;
    int bg = BGCHECK_SCENE;
    return !BgCheck_EntityLineTest1(&play->colCtx, &a, &b, &hit, &poly, true, true, true, true, &bg);
}
// [TIME] | [INFO] | Enemies.Ground | Reject unsupported surfaces before spawning or moving; no teleporting across walls or pits.
bool Ground(PlayState* play, Actor* actor, Vec3f& point, float referenceY, float tolerance) {
    groundFailure = "outside collision bounds";
    if (!BgCheck_PosInStaticBoundingBox(&play->colCtx, &point)) { return false; }
    Vec3f ray = point; ray.y = referenceY + 100;
    CollisionPoly* poly = nullptr;
    int bg = BGCHECK_SCENE;
    float floor = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bg, actor, &ray);
    groundFailure = "missing, steep, dynamic, or distant floor";
    if (poly == nullptr || bg != BGCHECK_SCENE || floor <= BGCHECK_Y_MIN ||
        std::abs(floor - referenceY) > tolerance || poly->normal.y < 24000) { return false; }
    auto property = func_80041EA4(&play->colCtx, poly, bg);
    auto floorType = SurfaceType_GetFloorType(&play->colCtx, poly, bg);
    groundFailure = "hazardous floor or scene exit";
    if (property == FLOOR_PROPERTY_5 || property == FLOOR_PROPERTY_12 ||
        (floorType == 2 || floorType == 3 || floorType == 7 || floorType == 9 || floorType == 12) ||
        SurfaceType_GetSceneExitIndex(&play->colCtx, poly, bg) != 0 ||
        SurfaceType_IsWallDamage(&play->colCtx, poly, bg)) { return false; }
    float water = 0;
    WaterBox* box = nullptr;
    groundFailure = "water above floor";
    if (WaterBox_GetSurface1(play, &play->colCtx, point.x, point.z, &water, &box) && water > floor + 5) { return false; }
    groundFailure = "none";
    point.y = floor + 2;
    return true;
}
// Keep a destination until arrival. Validate the whole route, not just a rotating
// point in front of the actor, so patrols do not endlessly push into a gate or ledge.
bool PatrolRoute(PlayState* play, Actor* actor, Vec3f destination) {
    if (!ClearLine(play,actor->world.pos,destination)) { return false; }
    float distance=Distance(actor->world.pos,destination);
    int steps=std::max(1,int(std::ceil(distance/50.f)));
    float height=actor->world.pos.y;
    int16_t yaw=Math_Vec3f_Yaw(&actor->world.pos,&destination);
    for (int i=1;i<=steps;++i) {
        float t=float(i)/steps;
        Vec3f point{actor->world.pos.x+(destination.x-actor->world.pos.x)*t,height,
                    actor->world.pos.z+(destination.z-actor->world.pos.z)*t};
        if (!Ground(play,actor,point,height,35)) { return false; }
        for (int side:{-1,1}) {
            Vec3f edge{point.x+Math_CosS(yaw)*side*25,point.y,point.z-Math_SinS(yaw)*side*25};
            if (!Ground(play,actor,edge,point.y,35) || !ClearLine(play,point,edge)) { return false; }
        }
        height=point.y;
    }
    return true;
}
bool PatrolDestination(PlayState* play, Actor* actor, Patrol& patrol, float radius, Vec3f& destination) {
    uint32_t now=play->gameplayFrames;
    if (patrol.valid && actor->speedXZ>.1f && Distance(actor->world.pos,patrol.lastPosition)<.15f) { ++patrol.blocked; }
    else { patrol.blocked=0; }
    patrol.lastPosition=actor->world.pos;
    if (patrol.valid && (Distance(actor->world.pos,patrol.destination)<40 || patrol.blocked>8)) {
        patrol.valid=false;patrol.retryAt=now;patrol.blocked=0;
    }
    if (!patrol.valid && now>=patrol.retryAt) {
        patrol.retryAt=now+20;
        if (patrol.routeAttempt==0) { patrol.routeYaw=RandInt(0,UINT16_MAX); }
        for (int slice=0;slice<2 && patrol.routeAttempt<24;++slice) {
            // Share expensive route probes across all inhabitants. Existing
            // patrols and combat still advance every tick while new routes wait.
            if (!patrolPlanning.Take(now)) { break; }
            int attempt=patrol.routeAttempt++;
            int16_t yaw=int16_t(patrol.routeYaw+attempt*8192);
            float reach=radius*(1.f-.3f*(attempt/8));
            Vec3f point{actor->home.pos.x+Math_SinS(yaw)*reach,actor->home.pos.y,
                        actor->home.pos.z+Math_CosS(yaw)*reach};
            if (Distance(point,actor->world.pos)<100 || !Ground(play,actor,point,actor->home.pos.y,90) || !PatrolRoute(play,actor,point)) { continue; }
            patrol.destination=point;patrol.valid=true;break;
        }
        if (patrol.valid || patrol.routeAttempt==24) { patrol.routeAttempt=0; }
        else { patrol.retryAt=now+1; }
    }
    destination=patrol.destination;
    return patrol.valid;
}
bool HasSpace(PlayState* play, const Vec3f& point, bool watch = false) {
    if (play->actorCtx.total >= 180) { return false; }
    int enemies = 0;
    for (int category = 0; category < ACTORCAT_MAX; ++category) {
        for (Actor* a = play->actorCtx.actorLists[category].head; a != nullptr; a = a->next) {
            if (a->update == nullptr) { continue; }
            if (category == ACTORCAT_ENEMY) { ++enemies; }
            float clearance = category == ACTORCAT_NPC || category == ACTORCAT_DOOR ? (watch ? 120.f : 300.f) :
                              category == ACTORCAT_ENEMY || category == ACTORCAT_PLAYER ? 85.f : 0.f;
            if (clearance && std::abs(a->world.pos.y - point.y) < 120 && Distance(point, a->world.pos) < clearance) { return false; }
        }
    }
    return enemies < 112;
}
struct Base { int16_t id, params; };
Base ChooseBase(const Region& region, int member, PackKind kind) {
    if (region.minLevel == 1 && region.maxLevel <= 3) { return { ACTOR_EN_TITE, -1 }; }
    // A specialist joins selected packs; the native species still form their backbone.
    if (Creatures::Enabled() && member==(kind==PackKind::Champions?1:2) && (region.population==Population::Wilderness || region.population==Population::Dungeon)) {
        auto creature=region.biome==Biome::Crypt || region.biome==Biome::Woods || region.biome==Biome::Water ? Creatures::Kind::Wraith : Creatures::Kind::Sentinel;
        int id=Creatures::ActorId(creature);if(id>=0)return {int16_t(id),0};
    }
    bool leader = member == 0 || kind == PackKind::Champions;
    switch (region.biome) {
        case Biome::Crypt: return { ACTOR_EN_TEST, 2 };
        case Biome::Desert: case Biome::Fortress:
            return member % 3 == 2 ? Base{ ACTOR_EN_TEST, 2 } : Base{ ACTOR_EN_ZF, -2 };
        case Biome::Mountain:
            return leader || member % 2 == 0 ? Base{ ACTOR_EN_ZF, -2 } : Base{ ACTOR_EN_TITE, -1 };
        case Biome::Water:
            return leader ? Base{ ACTOR_EN_WF, int16_t(0xFF01) } : Base{ ACTOR_EN_TITE, -2 };
        case Biome::Plains:
            if (LINK_IS_ADULT && member % 3 == 2) { return { ACTOR_EN_TEST, 2 }; }
            break;
        default: break;
    }
    return { ACTOR_EN_WF, int16_t(0xFF00) };
}
int SpawnAt(const Region& region, PackKind kind, const Vec3f& center, int requested, std::vector<Actor*>* members = nullptr, bool regional = false, bool quest = false) {
    auto* play = gPlayState;
    auto* player = GET_PLAYER(play);
    int cap = regional && region.population==Population::Wilderness ?
        std::clamp(CVarGetInteger(CVAR_ENHANCEMENT("ARPG.PopulationSafetyCap"),32),4,96) :
        std::clamp(CVarGetInteger(CVAR_ENHANCEMENT("ARPG.EnemyCap"),18),4,20);
    // Authored quest packs have their own three-actor budget. HasSpace still enforces the
    // global enemy/actor ceilings; dense ambient populations must not starve story objectives.
    if(quest)cap=LiveCount()+std::min(requested,3);
    if (members && requested > cap - LiveCount()) { return 0; }
    requested = std::min(requested, cap - LiveCount());
    if (requested <= 0) { lastSpawn = "The live encounter limit is reached."; return 0; }
    uint16_t pack = nextPack++;
    if (nextPack == 0) { nextPack = 1; }
    int spawned = 0;
    for (int attempt = 0; attempt < requested * 16 && spawned < requested; ++attempt) {
        int16_t yaw = RandInt(0, UINT16_MAX);
        float radius = RandFloat(60, region.population == Population::Dungeon ? 230.f : 250.f);
        Vec3f point = { center.x + Math_SinS(yaw) * radius, center.y, center.z + Math_CosS(yaw) * radius };
        if (!Ground(play, &player->actor, point, center.y, 85) || !HasSpace(play, point) ||
            !ClearLine(play, center, point) || (!regional && !ClearLine(play, player->actor.world.pos, point))) { continue; }
        // Ambient encounters never occupy the arrival bubble. Quest/test packs
        // retain explicit placement; native room encounters remain authored.
        if (regional && (Distance(point, roomEntry) < 1000.f || Distance(point, player->actor.world.pos) < 850.f)) { continue; }
        Base base = ChooseBase(region, spawned, kind);
        Actor* actor = Actor_Spawn(&play->actorCtx, play, base.id, point.x, point.y, point.z, 0,
                                  Math_Vec3f_Yaw(&point, &player->actor.world.pos), 0, base.params);
        if (actor == nullptr || actor->update == nullptr) { continue; }
        ObjectExtension::GetInstance().Set<Member>(actor, Member{ pack, regional });
        ObjectExtension::GetInstance().Get<Member>(actor)->kind=kind;
        AffixSystem::Configure(actor, PackTier(kind, spawned), pack, spawned);
        if (regional && spawned==0) {
            const char* activity=kind==PackKind::EliteGuard ? "Elite encampment" : kind==PackKind::Champions ? "Hunting pack" : "Roaming warband";
            NameTag_RegisterForActorWithOptions(actor,activity,{"arpg-activity",32,{230,205,145,255},false,.55f});
        }
        if (regional) {
            // Native limb callbacks also update weapon hitboxes; keep them running for offscreen skirmishes.
            actor->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
            actor->uncullZoneForward = 20000; actor->uncullZoneScale = 1000; actor->uncullZoneDownward = 1000;
        }
        if (members) { members->push_back(actor); }
        ++spawned;
    }
    int minimum = members ? requested : regional || region.population == Population::Dungeon || region.population == Population::Settlement ? 2 : 3;
    if (spawned < minimum) {
        for (Actor* a = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; a != nullptr; a = a->next) {
            auto* member = ObjectExtension::GetInstance().Get<Member>(a);
            if (member != nullptr && member->pack == pack) { Actor_Kill(a); }
        }
        if (members) { members->clear(); }
        spawned = 0;
    }
    lastSpawn = spawned ? "Encounter: " + std::to_string(spawned) + " enemies." : "No safe ground found nearby. Move into open space.";
    return spawned;
}
PackKind RollPack() {
    int roll = RandInt(1, 100);
    return roll <= 25 ? PackKind::Champions : roll <= 45 ? PackKind::EliteGuard : PackKind::Warband;
}
void ClearWatchmen() {
    if (!gPlayState) { return; }
    for (auto* a = gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head; a; a = a->next) {
        if (ObjectExtension::GetInstance().Has<Watchman>(a)) { Actor_Kill(a); }
    }
}
Actor* CombatTarget(Actor* actor, PlayState* play) {
    auto& extensions = ObjectExtension::GetInstance();
    auto* member = extensions.Get<Member>(actor);
    auto* player = &GET_PLAYER(play)->actor;
    Actor* target = player;
    if (!Enabled() || !member || !member->regional || !CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Packs"), 1)) { return target; }
    float nearest = std::min(700.0f, Distance(actor->world.pos, player->world.pos));
    for (auto* a = play->actorCtx.actorLists[ACTORCAT_NPC].head; a; a = a->next) {
        auto* guard = extensions.Get<Watchman>(a);
        if (!a->update || !guard || guard->recovery || std::abs(a->world.pos.y-actor->world.pos.y)>90) { continue; }
        float distance = Distance(actor->world.pos,a->world.pos);
        if (distance < nearest && ClearLine(play,actor->world.pos,a->world.pos)) { nearest=distance; target=a; }
    }
    return target;
}
void WatchAnimation(EnHeishi2* guard, Watchman& state, int animation) {
    if (state.animation == animation) { return; }
    state.animation = animation;
    const char* resource = animation == 1 ? gEnHeishiWalkAnim : animation == 2 ? gEnHeishiSlamSpearAnim : gEnHeishiIdleAnim;
    Animation_Change(&guard->skelAnime, (AnimationHeader*)resource, 1, 0,
                     Animation_GetLastFrame((AnimationHeader*)resource), animation == 2 ? ANIMMODE_ONCE : ANIMMODE_LOOP, -4);
}
void WatchDestroy(Actor* actor, PlayState* play) {
    auto* guard = reinterpret_cast<EnHeishi2*>(actor);
    auto* state = ObjectExtension::GetInstance().Get<Watchman>(actor);
    if (state) { Collider_DestroyCylinder(play, &state->spear); }
    Collider_DestroyCylinder(play, &guard->collider);
}
void WatchAction(Actor* actor, Watchman& state, const char* action) {
    if (state.action==action) { return; }
    state.action=action;
    NameTag_RemoveForActorByTag(actor,"arpg-watch");
    std::string label=state.name+" · "+action;
    NameTag_RegisterForActorWithOptions(actor,label.c_str(),{"arpg-watch",15,{120,210,255,255},false,.60f});
}
void WatchUpdate(Actor* actor, PlayState* play) {
    auto& extensions = ObjectExtension::GetInstance();
    auto* state = extensions.Get<Watchman>(actor);
    if (!state || !Enabled() || !CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Packs"),1) || Expeditions::Busy()) { Actor_Kill(actor); return; }
    if (!GameInteractor::IsPlayerInControl() || GameInteractor::IsGameplayPaused()) { return; }
    auto* guard = reinterpret_cast<EnHeishi2*>(actor);
    if (guard->collider.base.acFlags & AC_HIT) {
        guard->collider.base.acFlags &= ~AC_HIT;
        ++state->hits;
        actor->colChkInfo.health = uint8_t(std::max(0,int(actor->colChkInfo.health)-std::max(1,int(actor->colChkInfo.damage))));
        Actor_SetColorFilter(actor, 0x4000, 255, 0, 10);
        state->stagger=8;state->swing=0;state->quietFrames=0;
        if (actor->colChkInfo.health<=5) { state->recovery=240; }
    }
    if (state->cooldown) { --state->cooldown; }
    if (state->aidCooldown) { --state->aidCooldown; }
    if (state->stagger) { --state->stagger; }
    if (state->recovery) { --state->recovery; if (!state->recovery) { actor->colChkInfo.health=24; } }
    Actor* enemy = nullptr;
    float nearest = 320;
    for (auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head; a; a=a->next) {
        auto* member=extensions.Get<Member>(a);
        if (!member || !member->regional || !a->update || !a->colChkInfo.health || std::abs(a->world.pos.y-actor->world.pos.y)>90 || Distance(a->world.pos,actor->home.pos)>420) { continue; }
        float distance=Distance(a->world.pos,actor->world.pos);
        if (distance<nearest && ClearLine(play,actor->world.pos,a->world.pos)) { enemy=a; nearest=distance; }
    }
    // Remember a position, never a target pointer: defeated actors may be freed before our next update.
    if (enemy) {
        state->engaged=true;state->lastBattle=enemy->world.pos;state->aftermath=0;state->quietFrames=0;
        state->sharedFight |= extensions.Get<Member>(enemy)->playerEngaged;
    } else {
        if (state->engaged) {
            state->engaged=false;state->aftermath=180;++state->searches;
            state->patrol.valid=false;
        } else if (state->aftermath) {
            --state->aftermath;
            if (!state->aftermath) { state->sharedFight=false; }
        }
        // Wounds mend during a genuinely quiet stretch, not while trading hits.
        if (!state->recovery && ++state->quietFrames>=120) {
            state->quietFrames=0;
            actor->colChkInfo.health=uint8_t(std::min(24,int(actor->colChkInfo.health)+2));
        }
    }
    Actor* wounded=nullptr;
    if (!state->recovery && !state->aidCooldown && (!enemy || nearest>130)) {
        float distance=500;
        for(auto* a=play->actorCtx.actorLists[ACTORCAT_NPC].head;a;a=a->next) {
            auto* ally=extensions.Get<Watchman>(a);
            if(a==actor || !a->update || !ally || a->colChkInfo.health>12 || std::abs(a->world.pos.y-actor->world.pos.y)>70) { continue; }
            float d=Distance(a->world.pos,actor->world.pos);
            if(d<distance && Distance(a->world.pos,actor->home.pos)<420 && ClearLine(play,actor->world.pos,a->world.pos)) { wounded=a;distance=d; }
        }
    }
    Vec3f destination=actor->home.pos;
    bool securing=!enemy && state->aftermath && !state->recovery;
    bool watching=false;
    bool thanking=securing && state->sharedFight && state->aftermath<=60 &&
                  Distance(actor->world.pos,GET_PLAYER(play)->actor.world.pos)<300;

    if(wounded) {
        destination=wounded->world.pos;
        if(Distance(actor->world.pos,destination)<105) {
            wounded->colChkInfo.health=uint8_t(std::min(24,int(wounded->colChkInfo.health)+8));
            auto* ally=extensions.Get<Watchman>(wounded);
            ally->recovery=std::min(ally->recovery,60);
            state->aidCooldown=180;++state->assists;
            Actor_SetColorFilter(wounded,0,100,0,16);
        }
    } else if (enemy && !state->recovery) {
        destination=enemy->world.pos;
        int16_t approach=Math_Vec3f_Yaw(&enemy->world.pos,&actor->home.pos);
        float side=state->support ? 80.f : -80.f;
        destination.x+=Math_CosS(approach)*side;destination.z-=Math_SinS(approach)*side;
    }
    else if (securing) {
        destination=state->lastBattle;
        watching=state->aftermath<=60 || Distance(actor->world.pos,destination)<85;
        if (watching || !PatrolRoute(play,actor,destination)) { destination=actor->world.pos;watching=true; }
    }
    else if (!state->recovery) {
        if (!PatrolDestination(play,actor,state->patrol,180,destination)) { destination=actor->world.pos; }
    }
    // Every activity, including pursuing and helping, stays near the assigned post.
    if(Distance(destination,actor->home.pos)>420 || Distance(actor->world.pos,actor->home.pos)>440) {
        destination=actor->home.pos; enemy=nullptr; wounded=nullptr; securing=false; watching=false;
        state->patrol.valid=false; state->swing=0;
    }
    int16_t yaw=Math_Vec3f_Yaw(&actor->world.pos,&destination);
    actor->speedXZ=0;
    if (!state->swing && !state->recovery && !state->stagger && !wounded && enemy && nearest<115 && !state->cooldown) {
        state->swing=28; state->cooldown=65; ++state->attacks;
        state->animation=-1; WatchAnimation(guard,*state,2);
        state->spear.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
        state->spear.info.toucherFlags &= ~TOUCH_HIT;
    }
    if (state->swing) {
        --state->swing;
        // One contact window per spear animation; the native enemy AC collider handles damage and hit reactions.
        if (state->swing<=15 && state->swing>=12 && !(state->spear.base.atFlags & (AT_HIT | AT_BOUNCED))) {
            state->spear.dim.pos={int16_t(actor->world.pos.x+Math_SinS(actor->shape.rot.y)*65),
                                 int16_t(actor->world.pos.y),int16_t(actor->world.pos.z+Math_CosS(actor->shape.rot.y)*65)};
            CollisionCheck_SetAT(play,&play->colChkCtx,&state->spear.base);
        }
    } else {
        float remaining=Distance(actor->world.pos,destination);
        Vec3f step{actor->world.pos.x+Math_SinS(yaw)*45,actor->world.pos.y,actor->world.pos.z+Math_CosS(yaw)*45};
        if (!state->stagger && remaining>(wounded ? 90 : 35) && Ground(play,actor,step,actor->world.pos.y,40) && ClearLine(play,actor->world.pos,step)) {
            actor->speedXZ=state->recovery ? 1.8f : enemy ? 2.5f : 1.1f;
        }
    }
    actor->world.rot.y=yaw;
    int16_t facing=enemy && !wounded && !state->recovery && nearest<150 ? Math_Vec3f_Yaw(&actor->world.pos,&enemy->world.pos) : yaw;
    if (watching && !wounded) {
        facing=thanking ? Math_Vec3f_Yaw(&actor->world.pos,&GET_PLAYER(play)->actor.world.pos) :
               int16_t(actor->home.rot.y+Math_SinS(int16_t(play->gameplayFrames*160))*10000);
    }
    Math_SmoothStepToS(&actor->shape.rot.y,facing,3,0x900,0x100);
    WatchAction(actor,*state,state->recovery ? "Regrouping" : state->stagger ? "Hit" : wounded ? "Helping ally" : state->swing ? "Spear strike" : enemy ? "Engaging" : thanking ? "Thanks for the help!" : watching ? "Watching road" : securing ? "Securing area" : "Patrolling");
    Vec3f before=actor->world.pos;
    Actor_MoveXZGravity(actor); Actor_UpdateBgCheckInfo(play,actor,40,20,70,0x1D); Actor_SetFocus(actor,65);
    if (!state->swing) {
        bool moved=Distance(before,actor->world.pos)>.15f;
        WatchAnimation(guard,*state,moved ? 1 : 0);
        if (!moved && actor->speedXZ>.1f) { state->patrol.valid=false;state->patrol.retryAt=play->gameplayFrames+10; }
    }
    SkelAnime_Update(&guard->skelAnime);
    Collider_UpdateCylinder(actor,&guard->collider);
    CollisionCheck_SetOC(play,&play->colChkCtx,&guard->collider.base);
    if (!state->recovery) { CollisionCheck_SetAC(play,&play->colChkCtx,&guard->collider.base); }
}
bool SpawnWatchman(PlayState* play, Vec3f point, const char* name) {
    if (WatchCount()>=ActivityFor(play->sceneNum,LINK_IS_ADULT).watchLimit || play->actorCtx.total>=178 || !Ground(play,&GET_PLAYER(play)->actor,point,point.y,100) || !HasSpace(play,point,true)) { return false; }
    for (int16_t yaw:{int16_t(0),int16_t(16384),int16_t(-16384),int16_t(-32768)}) {
        Vec3f edge{point.x+Math_SinS(yaw)*30,point.y,point.z+Math_CosS(yaw)*30};
        if (!Ground(play,&GET_PLAYER(play)->actor,edge,point.y,35) || !ClearLine(play,point,edge)) { return false; }
    }
    auto* actor=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_HEISHI2,point.x,point.y,point.z,0,RandInt(0,32767),0,2);
    if (!actor || !actor->update) { return false; }
    auto& extensions=ObjectExtension::GetInstance();
    extensions.Set<Watchman>(actor,Watchman{});
    auto* state=extensions.Get<Watchman>(actor);
    state->name=name;state->support=(WatchCount()%2)==0;
    auto* guard=reinterpret_cast<EnHeishi2*>(actor);
    ColliderCylinderInit spear = {
        { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_1, COLSHAPE_CYLINDER },
        { ELEMTYPE_UNK2, { DMG_SLASH_KOKIRI, 0, 1 }, { 0, 0, 0 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
        { 35, 65, 0, {0,0,0} }
    };
    Collider_InitCylinder(play,&state->spear); Collider_SetCylinder(play,&state->spear,actor,&spear);
    guard->collider.base.acFlags=AC_ON | AC_TYPE_ENEMY;
    guard->collider.info.bumperFlags=BUMP_ON;
    guard->collider.info.bumper.dmgFlags=0xFFFFFFFF;
    actor->colChkInfo.health=24; actor->colChkInfo.mass=80;
    actor->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    actor->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    actor->uncullZoneForward=20000; actor->uncullZoneScale=1000; actor->uncullZoneDownward=1000;
    actor->update=WatchUpdate; actor->destroy=WatchDestroy;
    NameTag_RegisterForActorWithOptions(actor,name,{"arpg-watch",15,{120,210,255,255},false,.65f});
    return true;
}
bool CanWake(Actor* actor) {
    return actor->id == ACTOR_EN_WF || actor->id == ACTOR_EN_TITE ||
           (actor->id == ACTOR_EN_ZF && actor->params == -2) ||
           (actor->id == ACTOR_EN_TEST && actor->params == 2);
}
void WakeResidents(PlayState* play) {
    auto& extensions=ObjectExtension::GetInstance();
    for (auto* actor=play->actorCtx.actorLists[ACTORCAT_ENEMY].head; actor; actor=actor->next) {
        if (!actor->update || actor->init || !actor->colChkInfo.health || !CanWake(actor) ||
            (actor->room>=0 && actor->room!=play->roomCtx.curRoom.num) || extensions.Has<Member>(actor)) { continue; }
        constexpr uint32_t culling=ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
        extensions.Set<Member>(actor,Member{0,true,false,false,actor->flags & culling,
                               actor->uncullZoneForward,actor->uncullZoneScale,actor->uncullZoneDownward});
        actor->flags |= culling;
        actor->uncullZoneForward=20000;actor->uncullZoneScale=1000;actor->uncullZoneDownward=1000;
    }
}
// In streamed rooms and towns, use fixed room-entry and authored NPC positions to
// keep placement on the loaded, inhabited floor. Link's later movement never adds candidates.
bool WorldPoint(PlayState* play, Vec3f& point, bool fromFloor=false) {
    auto* header=play->colCtx.colHeader;
    if (!header) { return false; }
    bool local=play->numRooms>1 || CurrentRegion().population==Population::Settlement || CurrentRegion().population==Population::None;
    float reference=fromFloor ? point.y : local ? roomEntry.y : float(header->maxBounds.y);
    point.y=reference;
    if (!Ground(play,&GET_PLAYER(play)->actor,point,reference,fromFloor || local ? 100 : float(header->maxBounds.y-header->minBounds.y)+100)) { return false; }
    if (!local) { return true; }
    auto reachable=[&](const Vec3f& anchor) {
        return Distance(point,anchor)<1600 && std::abs(point.y-anchor.y)<100 && ClearLine(play,anchor,point);
    };
    if (reachable(roomEntry)) { return true; }
    for (auto* a=play->actorCtx.actorLists[ACTORCAT_NPC].head;a;a=a->next) {
        if (a->update && (a->room<0 || a->room==play->roomCtx.curRoom.num) && !IsWatchman(a) && reachable(a->home.pos)) { return true; }
    }
    return false;
}
std::vector<Vec3f> WorldSites(PlayState* play) {
    std::vector<Vec3f> sites;
    auto* header=play->colCtx.colHeader;
    if (!header) { return sites; }
    for (int attempt=0;attempt<121;++attempt) {
        int cell=(attempt*37+60)%121;
        Vec3f point{header->minBounds.x+(header->maxBounds.x-header->minBounds.x)*(float(cell%11)+.5f)/11,
                    0,header->minBounds.z+(header->maxBounds.z-header->minBounds.z)*(float(cell/11)+.5f)/11};
        if (WorldPoint(play,point)) { sites.push_back(point); }
    }
    // Narrow paths and islands can fall between grid cells. Their actual floor
    // polygons provide a second set of candidates, including separate height levels.
    int stride=std::max(1,int(header->numPolygons)/384);
    for (int i=0;i<header->numPolygons;i+=stride) {
        auto& poly=header->polyList[i];
        if (poly.normal.y<24000) { continue; }
        Vec3f point{};
        for (int j=0;j<3;++j) {
            auto v=header->vtxList[poly.vtxData[j]&0x1FFF];
            point.x+=v.x/3.f;point.y+=v.y/3.f;point.z+=v.z/3.f;
        }
        if (WorldPoint(play,point,true)) { sites.push_back(point); }
    }
    return sites;
}
int PostWatch(PlayState* play, const Vec3f& center, const char* name, int count) {
    int spawned=0;
    for (int attempt=0;attempt<48 && spawned<count;++attempt) {
        int16_t yaw=int16_t(attempt*4096);
        float radius=340.f-70.f*(attempt/16);
        Vec3f point{center.x+Math_SinS(yaw)*radius,center.y,center.z+Math_CosS(yaw)*radius};
        if (Ground(play,&GET_PLAYER(play)->actor,point,center.y,65) && ClearLine(play,center,point) && SpawnWatchman(play,point,name)) { ++spawned; }
    }
    return spawned;
}
struct BorderSite { Vec3f position; int destination; };
std::vector<BorderSite> Borders(PlayState* play) {
    std::vector<BorderSite> result;
    auto* header=play->colCtx.colHeader;
    if (!header || !play->setupExitList) { return result; }
    struct Exit { Vec3f sum{}; int count=0; } exits[32]{};
    for (int i=0;i<header->numPolygons;++i) {
        auto* poly=&header->polyList[i];
        int index=SurfaceType_GetSceneExitIndex(&play->colCtx,poly,BGCHECK_SCENE);
        if (!index || index>=32) { continue; }
        for (int j=0;j<3;++j) {
            auto v=header->vtxList[poly->vtxData[j]&0x1FFF];
            exits[index].sum.x+=v.x;exits[index].sum.y+=v.y;exits[index].sum.z+=v.z;++exits[index].count;
        }
    }
    for (int i=1;i<32;++i) {
        if (!exits[i].count) { continue; }
        int entrance=play->setupExitList[i-1];
        if (entrance<0 || entrance>=ENTR_MAX) { continue; }
        auto& e=exits[i];
        result.push_back({{e.sum.x/e.count,e.sum.y/e.count,e.sum.z/e.count},gEntranceTable[entrance].scene});
    }
    std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b) {
        auto priority=[](int scene) { return scene==SCENE_LON_LON_RANCH ? 0 : scene>=SCENE_MARKET_ENTRANCE_DAY && scene<=SCENE_MARKET_ENTRANCE_RUINS ? 1 : IsSettlementEntrance(scene) ? 2 : 3; };
        return priority(a.destination)<priority(b.destination);
    });
    return result;
}
double WalkableArea(PlayState* play) {
    auto* header=play->colCtx.colHeader;
    if (!header) { return 0; }
    double area=0;
    for (int i=0;i<header->numPolygons;++i) {
        auto& poly=header->polyList[i];
        if (poly.normal.y<24000) { continue; }
        auto a=header->vtxList[poly.vtxData[0]&0x1FFF];
        auto b=header->vtxList[poly.vtxData[1]&0x1FFF];
        auto c=header->vtxList[poly.vtxData[2]&0x1FFF];
        Vec3f center{(a.x+b.x+c.x)/3.f,(a.y+b.y+c.y)/3.f,(a.z+b.z+c.z)/3.f};
        float floorY=center.y;
        if (!WorldPoint(play,center,true) || std::abs(center.y-floorY)>5) { continue; }
        area+=std::abs(double(b.x-a.x)*(c.z-a.z)-double(b.z-a.z)*(c.x-a.x))*.5;
    }
    return area;
}
void ClearWildlife() {
    if (!gPlayState) { return; }
    for (auto& list:gPlayState->actorCtx.actorLists) for (auto* a=list.head;a;a=a->next) {
        if (ObjectExtension::GetInstance().Has<Wildlife>(a)) { Actor_Kill(a); }
    }
}
void WildlifeBirdUpdate(Actor* actor,PlayState* play) {
    auto* wildlife=ObjectExtension::GetInstance().Get<Wildlife>(actor);
    if (!wildlife || !wildlife->nativeUpdate) { return; }
    wildlife->nativeUpdate(actor,play);
    // The unused native bird animates and steers, but never applies its velocity.
    if (actor->update) { Actor_MoveXZGravity(actor); }
}
void WildlifeBirdDestroy(Actor* actor,PlayState* play) {
    // The unused native bird allocates its joint tables but has an empty destroy.
    SkelAnime_Free(&reinterpret_cast<EnBird*>(actor)->skelAnime,play);
}
bool SpawnWildlife(PlayState* play,int id,Vec3f point,int params) {
    if (play->actorCtx.total>=168) { return false; }
    auto* actor=Actor_Spawn(&play->actorCtx,play,id,point.x,point.y,point.z,0,RandInt(0,32767),0,params);
    if (!actor || !actor->update) { return false; }
    ObjectExtension::GetInstance().Set<Wildlife>(actor,Wildlife{actor->update});
    actor->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    actor->flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED);
    actor->uncullZoneForward=id==ACTOR_EN_BIRD ? 12000 : 6000;
    actor->uncullZoneScale=200;actor->uncullZoneDownward=600;
    if (id==ACTOR_EN_BIRD) {
        auto* bird=reinterpret_cast<EnBird*>(actor);
        bird->flightDistance=180;bird->speedTarget=3;bird->rotYStep=1400;
        actor->shape.shadowDraw=nullptr;actor->update=WildlifeBirdUpdate;actor->destroy=WildlifeBirdDestroy;
    }
    return true;
}
bool WildlifeWater(PlayState* play,Vec3f& point) {
    float surface=point.y;WaterBox* water=nullptr;
    if (!WaterBox_GetSurface1(play,&play->colCtx,point.x,point.z,&surface,&water)) { return false; }
    // A swimming pocket needs depth and room on all sides, not just one wet pixel.
    for (Vec3f offset:std::initializer_list<Vec3f>{{0,0,0},{120,0,0},{-120,0,0},{0,0,120},{0,0,-120}}) {
        Vec3f probe{point.x+offset.x,surface+10,point.z+offset.z};float edge=surface-20;WaterBox* box=nullptr;
        if (!WaterBox_GetSurface1(play,&play->colCtx,probe.x,probe.z,&edge,&box) || std::abs(edge-surface)>5) { return false; }
        CollisionPoly* floor=nullptr;int bg=BGCHECK_SCENE;
        float y=BgCheck_EntityRaycastFloor5(play,&play->colCtx,&floor,&bg,&GET_PLAYER(play)->actor,&probe);
        if (!floor || bg!=BGCHECK_SCENE || y<=BGCHECK_Y_MIN || y>surface-40 ||
            SurfaceType_GetSceneExitIndex(&play->colCtx,floor,bg)!=0 || SurfaceType_IsWallDamage(&play->colCtx,floor,bg)) { return false; }
    }
    point.y=surface-20;return true;
}
void PopulateWildlife(PlayState* play,const std::vector<Vec3f>& sites) {
    wildlifePopulated=true;
    auto habitat=WildlifeFor(play->sceneNum,LINK_IS_ADULT);
    if (!habitat.birds && !habitat.butterflies && !habitat.fish) { return; }
    int budget=WildlifeBudget(populationArea,168-play->actorCtx.total),spawned=0;
    // Reserve part of waterside populations for fish instead of filling every slot on shore.
    int landBudget=!(habitat.birds || habitat.butterflies) ? 0 : habitat.fish ? budget/2 : budget;
    std::vector<Vec3f> occupied;
    for (auto center:sites) {
        if (spawned>=landBudget) { break; }
        bool close=false;for (const auto& used:occupied) { close |= Distance(center,used)<750; }
        if (close || !HasSpace(play,center,true)) { continue; }
        bool placed=false;
        for (int animal=0;animal<4 && spawned<landBudget;++animal) {
            bool bird=habitat.birds && (!habitat.butterflies || animal<2);
            Vec3f point{center.x+(animal%2 ? 45.f : -45.f),center.y,center.z+(animal/2)*45.f};
            if (!WorldPoint(play,point,true) || !ClearLine(play,center,point)) { continue; }
            if (bird) {
                Vec3f sky=point;sky.y+=120;
                if (!ClearLine(play,point,sky)) { continue; }
                bool open=true;
                for (int side=0;side<4;++side) {
                    Vec3f edge{sky.x+Math_SinS(side*16384)*240,sky.y,sky.z+Math_CosS(side*16384)*240};
                    open &= ClearLine(play,sky,edge);
                }
                if (!open) { continue; }
                point=sky;
            } else { point.y+=25; }
            if (SpawnWildlife(play,bird ? ACTOR_EN_BIRD : ACTOR_EN_BUTTE,point,0)) { ++spawned;placed=true; }
        }
        if (placed) { occupied.push_back(center); }
    }
    auto* header=play->colCtx.colHeader;
    if (!habitat.fish || !header) { return; }
    // Static water boxes are filtered again through the engine's current-room query.
    for (int i=0;i<header->numWaterBoxes && spawned<budget;++i) {
        const auto& box=header->waterBoxes[i];
        for (int attempt=0;attempt<81 && spawned<budget;++attempt) {
            int cell=(attempt*31+40)%81;
            Vec3f point{box.xMin+box.xLength*(float(cell%9)+.5f)/9,float(box.ySurface)-20,
                        box.zMin+box.zLength*(float(cell/9)+.5f)/9};
            bool close=false;for (const auto& used:occupied) { close |= Distance(point,used)<200; }
            if (close || !WildlifeWater(play,point)) { continue; }
            if (SpawnWildlife(play,ACTOR_EN_FISH,point,1)) { ++spawned;occupied.push_back(point); }
        }
    }
}
void PopulateRegion(PlayState* play, const Region& region) {
    const auto activity=ActivityFor(play->sceneNum,LINK_IS_ADULT);
    const bool wilderness=region.population==Population::Wilderness;
    const int cap=std::clamp(CVarGetInteger(CVAR_ENHANCEMENT("ARPG.PopulationSafetyCap"),32),4,96);
    const int density=std::clamp(CVarGetInteger(CVAR_ENHANCEMENT("ARPG.PackDensity"),70),50,150);
    populationArea=wilderness ? WalkableArea(play) : 0;
    const int budget=wilderness ? AreaPopulationBudget(populationArea,density,region.maxLevel<=3 ? 4 : cap,168-play->actorCtx.total) : 0;
    populationTarget=budget;
    std::vector<Vec3f> occupied;
    auto separated=[&](const Vec3f& point,float distance) {
        for (auto& site:occupied) { if (Distance(site,point)<distance) { return false; } }
        return true;
    };
    // Give civilization approaches first claim on the finite enemy/watch budget.
    if (wilderness && activity.borderConflict) for (const auto& border:Borders(play)) {
        if (LiveCount()+(wilderness ? 3 : 2)>budget || WatchCount()>=activity.watchLimit) { break; }
        if (wilderness && (!IsSettlementEntrance(border.destination) ||
            (ActivityFor(border.destination,LINK_IS_ADULT).watchLimit==0 &&
             !(play->sceneNum==SCENE_HYRULE_FIELD && border.destination>=SCENE_MARKET_ENTRANCE_DAY && border.destination<=SCENE_MARKET_ENTRANCE_RUINS)))) { continue; }
        if (!wilderness && RegionFor(border.destination,LINK_IS_ADULT).population!=Population::Wilderness && RegionFor(border.destination,LINK_IS_ADULT).population!=Population::Settlement) { continue; }
        for (int attempt=0;attempt<48;++attempt) {
            int16_t yaw=int16_t(attempt*4096);
            float radius=(wilderness ? 650.f : 350.f)+(wilderness ? 450.f : 150.f)*(attempt/16);
            Vec3f point{border.position.x+Math_SinS(yaw)*radius,border.position.y,border.position.z+Math_CosS(yaw)*radius};
            if (!WorldPoint(play,point,true) || !separated(point,wilderness ? 1000.f : 500.f)) { continue; }
            std::vector<Actor*> members;
            if (!SpawnAt(region,PackKind::Warband,point,wilderness ? 3 : 2,&members,true)) { continue; }
            const char* name=activity.watchName;
            if (play->sceneNum==SCENE_HYRULE_FIELD) {
                name=border.destination>=SCENE_MARKET_ENTRANCE_DAY && border.destination<=SCENE_MARKET_ENTRANCE_RUINS ? "Castle watch" : ActivityFor(border.destination,LINK_IS_ADULT).watchName;
            }
            if (!PostWatch(play,border.position,name,2)) { for (auto* actor:members) { Actor_Kill(actor); } continue; }
            occupied.push_back(point);break;
        }
    }
    auto sites=WorldSites(play);
    if (wilderness) {
        // Seed the travel corridors from every exit on region entry. These are
        // fixed scene landmarks, not Link's changing position or proximity.
        std::vector<Vec3f> routes;
        auto* header=play->colCtx.colHeader;
        auto borders=Borders(play);
        if (header) for (float fraction:{.25f,.5f,.75f}) for (const auto& border:borders) {
            Vec3f point{border.position.x+((header->minBounds.x+header->maxBounds.x)*.5f-border.position.x)*fraction,
                        0,border.position.z+((header->minBounds.z+header->maxBounds.z)*.5f-border.position.z)*fraction};
            if (WorldPoint(play,point)) { routes.push_back(point); }
        }
        sites.insert(sites.begin(),routes.begin(),routes.end());
        struct Candidate { Vec3f point; float clearance; };
        std::vector<Candidate> candidates;
        candidates.reserve(sites.size());
        for (const auto& point:sites) {
            float nearest=20000;
            for (const auto& used:occupied) { nearest=std::min(nearest,Distance(point,used)); }
            candidates.push_back({point,nearest});
        }
        float spacing=900.f/std::sqrt(density/100.f);
        while (!candidates.empty() && LiveCount()+2<=budget) {
            // Cache nearest distances: only a successful placement can change
            // them. Preserve stable candidate ordering and largest-gap coverage.
            auto best=std::max_element(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b) {
                return a.clearance<b.clearance;
            });
            Vec3f point=best->point;
            if (best->clearance<spacing) { break; }
            candidates.erase(best);
            PackKind kind=RollPack();
            int count=kind==PackKind::EliteGuard && budget-LiveCount()>=3 ? 3 : 2;
            if (SpawnAt(region,kind,point,count,nullptr,true)) {
                occupied.push_back(point);
                for (auto& candidate:candidates) {
                    candidate.clearance=std::min(candidate.clearance,Distance(candidate.point,point));
                }
                if (WatchCount()<activity.watchLimit) { PostWatch(play,point,activity.watchName,2); }
            }
        }
    }
    // Approved human settlements get patrols even when there is no
    // safe border skirmish. These never replace or take control of a quest NPC.
    for (auto point:sites) {
        if (WatchCount()>=activity.watchLimit) { break; }
        if (separated(point,300) && SpawnWatchman(play,point,activity.watchName)) { occupied.push_back(point); }
    }
    if (CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Wildlife"),1)) { PopulateWildlife(play,sites); }
}

void Reset() {
    patrolPlanning = {};wildlifePopulated=false;
    regionPopulated = false; populationTarget=0;populationArea=0; populatedRoom = -1; expandedRooms.clear(); explored.clear(); nextPack = 1; nextEncounter = 20; nextAttackSlot = 0;
}
void RefreshRegionalDrawing(PlayState* play) {
    auto& extensions=ObjectExtension::GetInstance();
    for (auto* actor=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;actor;actor=actor->next) {
        auto* member=extensions.Get<Member>(actor);
        if (!actor->update || !member || !member->regional) { continue; }
        bool combat=Distance(actor->world.pos,GET_PLAYER(play)->actor.world.pos)<1000 ||
                    (member->originalCulling & ACTOR_FLAG_DRAW_CULLING_DISABLED);
        for (auto* guard=play->actorCtx.actorLists[ACTORCAT_NPC].head;guard && !combat;guard=guard->next) {
            combat=guard->update && extensions.Has<Watchman>(guard) && Distance(actor->world.pos,guard->world.pos)<1000;
        }
        // Skeleton draw callbacks maintain native hitboxes. Force those near
        // combat; distant patrols still update and draw normally when in view.
        if (combat) { actor->flags |= ACTOR_FLAG_DRAW_CULLING_DISABLED; }
        else { actor->flags &= ~ACTOR_FLAG_DRAW_CULLING_DISABLED; }
    }
}
void Director() {
    auto* play = gPlayState;
    if (play == nullptr || !GameInteractor::IsSaveLoaded(true)) { return; }
    if (Expeditions::Busy()) { ClearWatchmen();ClearWildlife();wildlifePopulated=false;return; }
    bool active = Enabled() && CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Packs"), 1);
    if (!active) { ClearPacks(); ClearWatchmen();ClearWildlife();wildlifePopulated=false; regionPopulated = false; expandedRooms.clear(); return; }
    if (!CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Wildlife"),1)) { ClearWildlife();wildlifePopulated=false; }
    if (!GameInteractor::IsPlayerInControl() || play->transitionTrigger != TRANS_TRIGGER_OFF) { return; }
    Region region = CurrentRegion();
    if (play->roomCtx.status != 0 || (play->numRooms>1 && play->roomCtx.prevRoom.num>=0)) { return; }
    int room=play->roomCtx.curRoom.num;
    if (room!=populatedRoom) {
        populatedRoom=room;regionPopulated=false;wildlifePopulated=false;roomEntry=GET_PLAYER(play)->actor.world.pos;
        nextEncounter=play->gameplayFrames+160;
    }
    if (play->gameplayFrames < nextEncounter || play->gameplayFrames < 20) { return; }
    nextEncounter = play->gameplayFrames + 20;
    RefreshRegionalDrawing(play);
    for (auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next) {
        if (a->room==room && a->update && a->init) { nextEncounter=play->gameplayFrames+2;return; }
    }
    if (region.population==Population::Wilderness || region.population==Population::Settlement || region.population==Population::Dungeon) { WakeResidents(play); }
    if (region.population==Population::Wilderness || region.population==Population::Settlement || ActivityFor(play->sceneNum,LINK_IS_ADULT).watchLimit>0) {
        if (!regionPopulated) { PopulateRegion(play,region);regionPopulated=true;RefreshRegionalDrawing(play); }
        if (!wildlifePopulated && CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Wildlife"),1)) { PopulateWildlife(play,WorldSites(play)); }
    } else if (region.population==Population::Dungeon) {
        if (expandedRooms.count(room) || Flags_GetClear(play,room)) { return; }
        // Snapshot every eligible anchor before spawning. A room gets its full
        // encounter on entry, never when Link later approaches a hidden enemy.
        std::vector<Vec3f> anchors;
        for (Actor* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next) {
            auto* member=ObjectExtension::GetInstance().Get<Member>(a);
            bool eligible=CanWake(a) || a->id==ACTOR_EN_RD || a->id==ACTOR_EN_DEKUBABA;
            if (!eligible || a->room!=room || !a->update || a->init || !a->colChkInfo.health || (member && member->generated)) { continue; }
            bool separate=true;for (auto& anchor:anchors) { if (Distance(anchor,a->home.pos)<400 && std::abs(anchor.y-a->home.pos.y)<120) { separate=false; } }
            if (separate) { anchors.push_back(a->home.pos); }
        }
        for (const auto& anchor:anchors) {
            // Only a minority of eligible rooms receive reinforcements. A
            // stable scene/room roll prevents rerolling a doorway for enemies.
            uint32_t roll = gARPGSave.seed ^ (uint32_t(play->sceneNum) * 1103515245u) ^ (uint32_t(room) * 2654435761u);
            if (region.maxLevel <= 3 || roll % 4 != 0) { break; }
            if (LiveCount()+2>std::clamp(CVarGetInteger(CVAR_ENHANCEMENT("ARPG.EnemyCap"),18),4,20)) { break; }
            SpawnAt(region,RollPack(),anchor,std::clamp<int>(RandInt(region.minPack,region.maxPack)-1,2,5),nullptr,true);
        }
        expandedRooms.insert(room);
    }
}
}

Actor* RegionalTarget(Actor* actor, PlayState* play) { return CombatTarget(actor, play); }
bool RewardEligible(Actor* actor) {
    auto* member=ObjectExtension::GetInstance().Get<Member>(actor);
    return !member || !member->regional || member->playerEngaged;
}
bool IsWatchman(Actor* actor) { return actor && ObjectExtension::GetInstance().Has<Watchman>(actor); }
bool IsRegional(Actor* actor) { auto* member=ObjectExtension::GetInstance().Get<Member>(actor); return member && member->regional; }
int PopulationTarget() { return populationTarget; }
double PopulationArea() { return populationArea; }
int WildlifeCount() {
    int count=0;if (!gPlayState) { return 0; }
    for (auto& list:gPlayState->actorCtx.actorLists) for (auto* a=list.head;a;a=a->next) {
        count+=a->update && ObjectExtension::GetInstance().Has<Wildlife>(a);
    }
    return count;
}
bool IsWildlife(Actor* actor) { return ObjectExtension::GetInstance().Has<Wildlife>(actor); }
int WatchCount() {
    if (!gPlayState) { return 0; } int count=0;
    for (auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;a;a=a->next) { count+=a->update && ObjectExtension::GetInstance().Has<Watchman>(a); }
    return count;
}
uint32_t WatchAttacks() {
    if (!gPlayState) { return 0; } uint32_t count=0;
    for (auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;a;a=a->next) { auto* s=ObjectExtension::GetInstance().Get<Watchman>(a); if (s) { count+=s->attacks; } }
    return count;
}
uint32_t WatchSearches() {
    uint32_t count=0;
    if (gPlayState) for (auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;a;a=a->next) {
        auto* state=ObjectExtension::GetInstance().Get<Watchman>(a);
        if (a->update && state) { count+=state->searches; }
    }
    return count;
}
int WatchSecuringCount() {
    int count=0;
    if (gPlayState) for (auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;a;a=a->next) {
        auto* state=ObjectExtension::GetInstance().Get<Watchman>(a);
        if (a->update && state && state->aftermath) { ++count; }
    }
    return count;
}
uint32_t WatchAssists() {
    if (!gPlayState) { return 0; } uint32_t count=0;
    for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;a;a=a->next) { auto* state=ObjectExtension::GetInstance().Get<Watchman>(a);if(state) { count+=state->assists; } }
    return count;
}
uint32_t WatchHits() {
    if (!gPlayState) { return 0; } uint32_t count=0;
    for (auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;a;a=a->next) { auto* s=ObjectExtension::GetInstance().Get<Watchman>(a); if (s) { count+=s->hits; } }
    return count;
}
bool Enabled() { return AffixesEnabled() && CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Enemies"), 1); }
Region CurrentRegion() { return RegionFor(gPlayState != nullptr ? gPlayState->sceneNum : -1, LINK_IS_ADULT); }
int LiveCount() {
    if (gPlayState == nullptr) { return 0; }
    int count = 0;
    for (Actor* a = gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head; a != nullptr; a = a->next) {
        auto* member=ObjectExtension::GetInstance().Get<Member>(a);
        count += a->update != nullptr && a->colChkInfo.health > 0 && member && member->generated;
    }
    return count;
}
void ClearPacks() {
    if (gPlayState == nullptr) { return; }
    for (Actor* a = gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head; a != nullptr; a = a->next) {
        auto& extensions = ObjectExtension::GetInstance();
        auto* member = extensions.Get<Member>(a);
        if (!member) { continue; }
        if (member->generated) { Actor_Kill(a); }
        else {
            constexpr uint32_t culling = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
            a->flags = (a->flags & ~culling) | member->originalCulling;
            a->uncullZoneForward=member->originalForward; a->uncullZoneScale=member->originalScale; a->uncullZoneDownward=member->originalDownward;
            extensions.Remove<Member>(a);
        }
    }
}
static int SpawnNear(PackKind kind, bool testing, int exactCount=0, std::vector<Actor*>* members=nullptr, bool quest=false) {
    if (!(quest?ARPG::Enabled():Enabled()) || !GameInteractor::IsPlayerInControl()) { return 0; }
    Region region = CurrentRegion();
    if (region.population == Population::None || region.population == Population::Boss || region.population == Population::Settlement) {
        lastSpawn = "This region is protected. Visit a wilderness region to test packs.";
        return 0;
    }
    auto* player = GET_PLAYER(gPlayState);
    for (int attempt = 0; attempt < 8; ++attempt) {
        int16_t angle = player->actor.shape.rot.y + int16_t(attempt * 8192);
        float distance = testing ? 320.f : region.population == Population::Dungeon ? 300.f : 470.f;
        Vec3f center = { player->actor.world.pos.x + Math_SinS(angle) * distance, player->actor.world.pos.y,
                        player->actor.world.pos.z + Math_CosS(angle) * distance };
        if (!Ground(gPlayState, &player->actor, center, player->actor.world.pos.y, 80)) {
            if (testing) { ARPG::Log("info", "Pack placement rejected: " + std::string(groundFailure)); }
            continue;
        }
        if (!ClearLine(gPlayState, player->actor.world.pos, center)) {
            if (testing) { ARPG::Log("info", "Pack placement rejected: blocked line of sight"); }
            continue;
        }
        int density = std::clamp(CVarGetInteger(CVAR_ENHANCEMENT("ARPG.PackDensity"), 70), 50, 150);
        int count = std::clamp<int>(RandInt(region.minPack, region.maxPack) * density / 100, 3, 10);
        int spawned = SpawnAt(region, kind, center, exactCount ? exactCount : count, members, false, quest);
        if (spawned > 0) {
            if (testing) {
                nextEncounter = gPlayState->gameplayFrames + 600;
                explored.push_back(player->actor.world.pos);
                if (explored.size() > 64) { explored.erase(explored.begin()); }
            }
            return spawned;
        }
    }
    lastSpawn = "No safe ground found nearby. Move into open space.";
    return 0;
}

bool FindWaystonePosition(Vec3f& point) {
    if(!gPlayState) { return false; } auto* player=GET_PLAYER(gPlayState);
    for(int offset:{-16384,16384,-24576,24576,0,32767}) {
        int16_t angle=player->actor.shape.rot.y+offset;
        Vec3f candidate={player->actor.world.pos.x+Math_SinS(angle)*120,player->actor.world.pos.y,
                         player->actor.world.pos.z+Math_CosS(angle)*120};
        if(Ground(gPlayState,&player->actor,candidate,player->actor.world.pos.y,45) && HasSpace(gPlayState,candidate) &&
           ClearLine(gPlayState,player->actor.world.pos,candidate)) { point=candidate; return true; }
    }
    return false;
}
bool QuestSiteSupported(Vec3f& point) {
    return gPlayState && Ground(gPlayState,&GET_PLAYER(gPlayState)->actor,point,point.y,24);
}
std::vector<Vec3f> ExplorationSites() {
    std::vector<Vec3f> result;
    if(!gPlayState || !gPlayState->colCtx.colHeader)return result;
    auto* play=gPlayState;auto* player=GET_PLAYER(play);auto* header=play->colCtx.colHeader;
    // Use fixed collision landmarks, never follow Link with newly rolled loot.
    struct Candidate { Vec3f point; int shelter; uint32_t order; };
    std::vector<Candidate> candidates;
    int stride=std::max(1,int(header->numPolygons)/512);
    for(int i=0;i<header->numPolygons;i+=stride) {
        auto& poly=header->polyList[i];if(poly.normal.y<30000)continue;
        Vec3f p{};
        for(int j=0;j<3;++j) { auto v=header->vtxList[poly.vtxData[j]&0x1fff];p.x+=v.x/3.f;p.y+=v.y/3.f;p.z+=v.z/3.f; }
        if(!Ground(play,&player->actor,p,p.y,20) || !HasSpace(play,p,true))continue;
        // A dry roof or isolated ledge is not a reachable hiding spot. Require
        // a supported walkable approach from the arrival floor in every scene.
        const float arrivalDistance=Distance(p,player->actor.world.pos);
        if(arrivalDistance<300 || arrivalDistance>1800 || std::abs(p.y-player->actor.world.pos.y)>80 ||
           !ClearLine(play,player->actor.world.pos,p))continue;
        bool safe=true;int shelter=0;
        for(int side=0;side<8 && safe;++side) {
            int16_t yaw=side*8192;
            Vec3f edge{p.x+Math_SinS(yaw)*65,p.y,p.z+Math_CosS(yaw)*65};
            safe=Ground(play,&player->actor,edge,p.y,12) && ClearLine(play,p,edge);
            Vec3f wall{p.x+Math_SinS(yaw)*230,p.y,p.z+Math_CosS(yaw)*230};
            shelter+=!ClearLine(play,p,wall);
        }
        if(!safe || !shelter || !PatrolRoute(play,&player->actor,p))continue;
        uint32_t order=gARPGSave.seed ^ (uint32_t(play->sceneNum)*2654435761u) ^ (uint32_t(i)*2246822519u);
        candidates.push_back({p,shelter,order});
    }
    std::sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b) {
        return a.shelter==b.shelter ? a.order<b.order : a.shelter>b.shelter;
    });
    for(const auto& candidate:candidates) {
        bool separate=true;for(auto p:result)if(Distance(p,candidate.point)<280)separate=false;
        if(separate)result.push_back(candidate.point);
        if(result.size()==12)break;
    }
    return result;
}
bool FindQuestSitePosition(Vec3f& point,int salt) {
    if(!gPlayState)return false;
    auto* player=GET_PLAYER(gPlayState);const auto origin=player->actor.world.pos;
    for(int attempt=0;attempt<24;++attempt) {
        int16_t angle=static_cast<int16_t>((salt*7001+attempt*4051)&0xffff);
        float radius=160.f+((salt+attempt)%5)*70.f;
        Vec3f p={origin.x+Math_SinS(angle)*radius,origin.y,origin.z+Math_CosS(angle)*radius};
        if(!Ground(gPlayState,&player->actor,p,origin.y,65) || !HasSpace(gPlayState,p) || !ClearLine(gPlayState,origin,p))continue;
        bool safe=true;float lastY=origin.y;
        for(int step=1;step<=10 && safe;++step) {
            float t=step/10.f;Vec3f sample={origin.x+(p.x-origin.x)*t,lastY,origin.z+(p.z-origin.z)*t};
            safe=Ground(gPlayState,&player->actor,sample,lastY,24);lastY=sample.y;
        }
        if(safe){point=p;return true;}
    }
    return false;
}
int SpawnPack(PackKind kind, bool testing) { return SpawnNear(kind,testing); }
// [TIME] | [INFO] | Enemies.RitualSites | Require three distinct, supported approaches before committing any ward placement.
bool FindRitualPositions(const Vec3f& center,std::array<Vec3f,3>& points) {
    if(!gPlayState) { return false; } auto* player=GET_PLAYER(gPlayState);
    for(int rotation=0;rotation<12;++rotation) {
        std::array<Vec3f,3> candidate{}; bool valid=true;
        for(int i=0;i<3 && valid;++i) {
            int16_t angle=rotation*5461+i*21845;
            candidate[i]={center.x+Math_SinS(angle)*240,center.y,center.z+Math_CosS(angle)*240};
            auto& point=candidate[i];
            valid=Ground(gPlayState,&player->actor,point,center.y,45) && HasSpace(gPlayState,point) && ClearLine(gPlayState,center,point);
            // Sample the entire approach: a clear ray alone can still cross a pit or a hazardous floor.
            float lastY=center.y;
            for(int step=1;step<=6 && valid;++step) {
                float t=step/6.f; Vec3f sample={center.x+(point.x-center.x)*t,center.y,center.z+(point.z-center.z)*t};
                valid=Ground(gPlayState,&player->actor,sample,lastY,20); lastY=sample.y;
            }
        }
        if(valid) { points=candidate; return true; }
    }
    return false;
}
// [TIME] | [INFO] | Enemies.SpiritDrop | Dead enemies or knocked-back players can be above pits; return an unsafe drop to the supported shrine approach.
Vec3f SafeSpiritPosition(Vec3f point,const Vec3f& refuge) {
    if(!gPlayState || !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) { return refuge; }
    if(Distance(point,refuge)>1000 || !Ground(gPlayState,&GET_PLAYER(gPlayState)->actor,point,point.y,70) ||
       !ClearLine(gPlayState,refuge,point)) { return refuge; }
    return point;
}
Actor* SpawnSharedQuestMember(Vec3f point,int quest,int node) {
    if(!gPlayState || !ARPG::Enabled() || CurrentRegion().population!=Population::Wilderness ||
       !QuestSiteSupported(point) || !HasSpace(gPlayState,point))return nullptr;
    // Named packs use a stable species, position and node identity on every
    // client. Combat stays native; a party defeat removes the corresponding copy.
    auto* player=GET_PLAYER(gPlayState);
    auto* actor=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_WF,point.x,point.y,point.z,0,
        Math_Vec3f_Yaw(&point,&player->actor.world.pos),0,int16_t(0xFF00));
    if(actor&&actor->update)AffixSystem::Configure(actor,PackTier(PackKind::Warband,node),uint16_t(0xF000+quest),node);
    return actor;
}
std::vector<Actor*> SpawnQuestWave(int count) {
    std::vector<Actor*> members;
    if(count>=1 && count<=3 && CurrentRegion().population==Population::Wilderness)SpawnNear(PackKind::Warband,true,count,&members,true);
    // QuestSystem owns these lifetimes, including when ambient packs/affixes are disabled.
    for(auto* actor:members)ObjectExtension::GetInstance().Remove<Member>(actor);
    return members;
}
std::vector<Actor*> SpawnExpeditionWave(PackKind kind,int count) {
    std::vector<Actor*> members;
    if(count>=3 && count<=4 && CurrentRegion().population==Population::Wilderness) { SpawnNear(kind,true,count,&members); }
    return members;
}

const char* TacticName(Actor* actor) {
    auto state=Creatures::Status(actor);
    if(Creatures::MendStatus(actor).target)return "Mending ally";
    if(Creatures::IsCreature(actor))return state.phase==Creatures::Phase::Windup?"Charging":state.phase==Creatures::Phase::Stagger?"Interrupted":state.exposed?"Core exposed":"Hunting";
    if(state.phase==Creatures::Phase::Windup)return actor->id==ACTOR_EN_WF?"Rallying howl":"Bone volley";
    static const char* names[] = { "Closing", "Flanking", "Guarding", "Evading", "Attacking", "Recovering", "Returning" };
    auto* brain = ObjectExtension::GetInstance().Get<Brain>(actor);
    return brain != nullptr ? names[int(brain->tactic)] : "Native behavior";
}
uint32_t AttackCount(Actor* actor) {
    auto* brain = ObjectExtension::GetInstance().Get<Brain>(actor);
    return brain != nullptr ? brain->attacks : 0;
}

// [TIME] | [INFO] | Enemies.Tactics | Runs only from mobile melee neutral states; native attacks, damage, stun, and death keep control.
int Tactics(Actor* actor, PlayState* play) {
    if (!Enabled() || !CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Tactics"), 1) || actor->colChkInfo.health == 0 ||
        (actor->id == ACTOR_EN_ZF && actor->params != -2) ||
        (actor->id == ACTOR_EN_TEST && (actor->params == 0 || actor->params >= 3))) { return 0; }
    if (!GameInteractor::IsPlayerInControl()) { actor->speedXZ = 0; return 1; }
    Actor* opponent = CombatTarget(actor,play);
    auto& extensions = ObjectExtension::GetInstance();
    auto* brain = extensions.Get<Brain>(actor);
    auto* affix = AffixSystem::GetAffixData(actor);
    uint32_t now = play->gameplayFrames;
    if (brain == nullptr) {
        Brain initial;
        initial.role = affix != nullptr && affix->packId ? Role(affix->role) : Role(RandInt(0, 2));
        initial.flank = RandInt(0, 1) ? 1 : -1;
        initial.nextAttack = now + RandInt(15, 35);
        extensions.Set<Brain>(actor, std::move(initial));
        brain = extensions.Get<Brain>(actor);
    }
    auto* player = GET_PLAYER(play);
    auto* member = extensions.Get<Member>(actor);
    if (member && member->regional && opponent == &player->actor &&
        (actor->xzDistToPlayer > (member->playerEngaged ? 650.f : 360.f) ||
         (!member->playerEngaged && Distance(player->actor.world.pos,roomEntry)<650.f))) {
        Vec3f destination = actor->home.pos;
        if (!PatrolDestination(play,actor,brain->patrol,CurrentRegion().population==Population::Settlement ? 220.f : member->kind==PackKind::EliteGuard ? 200.f : member->kind==PackKind::Champions ? 650.f : 500.f,destination)) {
            actor->speedXZ=0;brain->tactic=Tactic::Guard;return 1;
        }
        if (!member->playerEngaged && Distance(destination,roomEntry)<850.f) {
            actor->speedXZ=0;brain->tactic=Tactic::Guard;return 1;
        }
        int16_t yaw = Math_Vec3f_Yaw(&actor->world.pos, &destination);
        Vec3f step{actor->world.pos.x + Math_SinS(yaw)*45, actor->world.pos.y, actor->world.pos.z + Math_CosS(yaw)*45};
        actor->speedXZ = Distance(actor->world.pos,destination)>35 && Ground(play,actor,step,actor->world.pos.y,45) && ClearLine(play,actor->world.pos,step) ? 2.0f : 0;
        actor->world.rot.y = yaw; Math_SmoothStepToS(&actor->shape.rot.y,yaw,3,0x500,0x100);
        brain->tactic = Tactic::Guard; return 1;
    }
    if (opponent==&player->actor && Creatures::NeutralTechnique(actor,play)) { return 1; }
    if (player->meleeWeaponState > 0 && !brain->sawSwing) { brain->swingStart = now; }
    brain->sawSwing = player->meleeWeaponState > 0;
    bool visible = ClearLine(play, actor->world.pos, opponent->world.pos) && std::abs(actor->yDistToPlayer) < 90;
    if (now >= brain->nextDecision) {
        int active = 0;
        for (Actor* other = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; other != nullptr; other = other->next) {
            auto* otherBrain = extensions.Get<Brain>(other);
            if (other != actor && other->update && other->colChkInfo.health > 0 && otherBrain != nullptr && otherBrain->leaseEnd > now && Distance(other->world.pos,actor->world.pos)<900) { ++active; }
        }
        brain->nextDecision = now + 8;
        Situation situation{ actor->xzDistToPlayer, Distance(actor->world.pos, actor->home.pos), visible,
            opponent == &player->actor && brain->sawSwing && now >= brain->swingStart + 6 && now >= brain->nextEvade,
            opponent == &player->actor && (player->stateFlags1 & PLAYER_STATE1_SHIELDING) != 0 &&
                std::abs(int(int16_t(actor->yawTowardsPlayer + 0x8000 - player->actor.shape.rot.y))) < 0x3000,
            opponent == &player->actor && (player->stateFlags1 & PLAYER_STATE1_READY_TO_FIRE) != 0,
            actor->colChkInfo.health * 3 < GetActorMaximumHealth(actor), now >= brain->nextAttack,
            active < 2 && now >= nextAttackSlot, brain->role };
        brain->tactic = Decide(situation);
        if (member && member->regional && CurrentRegion().population == Population::Settlement && situation.homeDistance > 450) { brain->tactic = Tactic::ReturnHome; }
        if (brain->tactic == Tactic::Evade) {
            if (Actor_TestFloorInDirection(actor, play, 100, actor->yawTowardsPlayer + 0x8000)) {
                brain->nextEvade = now + 100;
                brain->nextAttack = now + 30;
                actor->shape.rot.y = actor->world.rot.y = actor->yawTowardsPlayer;
                return 3;
            }
            brain->tactic = Tactic::Guard;
        }
        if (brain->tactic == Tactic::Attack) {
            int16_t facing = actor->yawTowardsPlayer - actor->shape.rot.y;
            if (std::abs(int(facing)) < 0x1800 && actor->xzDistToPlayer < 100 && visible) {
                int aggression = std::clamp(CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Aggression"), 100), 50, 150);
                brain->nextAttack = now + RandInt(75, 110) * 100 / aggression;
                brain->leaseEnd = now + 55;
                nextAttackSlot = now + 18;
                ++brain->attacks;
                actor->speedXZ = 0;
                actor->world.rot.y = actor->shape.rot.y;
                return 2;
            }
            brain->tactic = Tactic::Approach;
        }
    }
    int16_t yaw = actor->yawTowardsPlayer;
    float speed = 0;
    switch (brain->tactic) {
        case Tactic::Approach: speed = actor->xzDistToPlayer > 220 ? 4.5f : 2.8f; break;
        case Tactic::Flank:
            yaw += brain->flank * (actor->xzDistToPlayer > 135 ? 0x2600 : 0x4800);
            speed = 3.0f;
            break;
        case Tactic::Recover: case Tactic::Evade: yaw += 0x8000; speed = 2.5f; break;
        case Tactic::ReturnHome: yaw = Math_Vec3f_Yaw(&actor->world.pos, &actor->home.pos); speed = 3.5f; break;
        default: break;
    }
    if (!visible && brain->tactic != Tactic::ReturnHome) { speed = 0; }
    for (Actor* other = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; other != nullptr; other = other->next) {
        if (other == actor || !other->update || other->colChkInfo.health == 0 || std::abs(other->world.pos.y - actor->world.pos.y) > 70) { continue; }
        if (Distance(other->world.pos, actor->world.pos) < 62) {
            yaw = Math_Vec3f_Yaw(&other->world.pos, &actor->world.pos); speed = 2; break;
        }
    }
    Vec3f step = { actor->world.pos.x + Math_SinS(yaw) * 60, actor->world.pos.y,
                  actor->world.pos.z + Math_CosS(yaw) * 60 };
    if (speed > 0 && (!Ground(play, actor, step, actor->world.pos.y, 45) || !ClearLine(play, actor->world.pos, step))) {
        speed = 0; brain->flank = -brain->flank; brain->nextDecision = now + 8;
    }
    actor->world.rot.y = yaw;
    Math_SmoothStepToS(&actor->shape.rot.y, brain->tactic == Tactic::ReturnHome ? yaw : actor->yawTowardsPlayer, 3, 0x900, 0x100);
    actor->speedXZ = speed * Creatures::RallySpeed(actor);
    return 1;
}

void DrawSettings() {
    if (!ImGui::CollapsingHeader("Regional enemies & encounters")) { return; }
    UIWidgets::CVarCheckbox("Hidden caches and wild Gold Skulltulas", CVAR_ENHANCEMENT("ARPG.ExplorationSecrets"), { .defaultValue = true });
    UIWidgets::CVarCheckbox("Enemy names and levels overhead", CVAR_ENHANCEMENT("ARPG.EnemyNameplates"), { .defaultValue = false });
    UIWidgets::CVarCheckbox("Floating damage numbers", CVAR_ENHANCEMENT("ARPG.DamageNumbers"), { .defaultValue = true });
    UIWidgets::CVarCheckbox("Leveled regions and randomized enemy stats", CVAR_ENHANCEMENT("ARPG.Enemies"), { .defaultValue = true });
    UIWidgets::CVarCheckbox("Region populations and dungeon groups", CVAR_ENHANCEMENT("ARPG.Packs"), { .defaultValue = true });
    UIWidgets::CVarCheckbox("Neutral wildlife", CVAR_ENHANCEMENT("ARPG.Wildlife"), { .defaultValue = true });
    UIWidgets::CVarCheckbox("New creatures and pack techniques", CVAR_ENHANCEMENT("ARPG.Creatures"), { .defaultValue = true });
    UIWidgets::CVarCheckbox("Gohma broodfall", CVAR_ENHANCEMENT("ARPG.BossBroods"), { .defaultValue = true });
    UIWidgets::CVarCheckbox("Coordinated melee tactics", CVAR_ENHANCEMENT("ARPG.Tactics"), { .defaultValue = true });
    const char* labels[] = { "Pack density (%)", "Dungeon/test enemy limit", "Aggression (%)" };
    const char* keys[] = { CVAR_ENHANCEMENT("ARPG.PackDensity"), CVAR_ENHANCEMENT("ARPG.EnemyCap"), CVAR_ENHANCEMENT("ARPG.Aggression") };
    for (int i = 0; i < 3; ++i) {
        int value = CVarGetInteger(keys[i], i == 1 ? 18 : i == 0 ? 70 : 100);
        if (ImGui::SliderInt(labels[i], &value, i == 1 ? 4 : 50, i == 1 ? 20 : 150)) { CVarSetInteger(keys[i], value); CVarSave(); }
    }
    int safetyCap=CVarGetInteger(CVAR_ENHANCEMENT("ARPG.PopulationSafetyCap"),32);
    if (ImGui::SliderInt("Wilderness safety ceiling",&safetyCap,4,96)) { CVarSetInteger(CVAR_ENHANCEMENT("ARPG.PopulationSafetyCap"),safetyCap);CVarSave(); }
    ImGui::TextWrapped("Wilderness populations scale with safe ground area and density. The ceiling limits performance cost; it is not a fixed regional population.");
    ImGui::Text("Wild Gold Skulltulas: %d | Bounties earned: %u", Exploration::SkullCount(gARPGSave), gARPGSave.skullRewardTier);
    ImGui::TextWrapped("Every 5 total Skulltulas grants a gem and 5 essence. Every 20 grants a technique point, through 120. Wild tokens do not alter the original 100-token quest.");
    ImGui::TextWrapped("Fixed region levels reward exploration. Champions fight in groups; elite leaders have a better chance of rare gear. Added groups keep clear of arrivals; only some dungeon rooms receive reinforcements. Hylian watch patrols defend the field and ranch, with child-era patrols around the castle and market. Other regions retain their native inhabitants. Shops, puzzle-only rooms and scripted boss fights retain their authored encounters.");
}
void DrawTesting() {
    Creatures::DrawTesting();
    ImGui::Separator();
    static int kind = 2;
    const char* kinds[] = { "Warband", "Champion pack", "Elite and guards" };
    ImGui::Combo("Encounter", &kind, kinds, 3);
    if (ImGui::Button("Spawn test pack")) { SpawnPack(PackKind(kind), true); }
    ImGui::SameLine();
    if (ImGui::Button("Clear added enemies")) { ClearPacks(); nextEncounter = gPlayState->gameplayFrames + 600; }
    ImGui::TextWrapped("%s", lastSpawn.c_str());
    static int region = 0;
    const char* names[] = { "Hyrule Field", "Death Mountain Trail", "Desert Colossus" };
    const int entrances[] = { ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN, ENTR_DEATH_MOUNTAIN_TRAIL_BOTTOM_EXIT, ENTR_DESERT_COLOSSUS_EAST_EXIT };
    ImGui::Combo("Test region", &region, names, 3);
    if (ImGui::Button("Visit region / skip cinematics")) {
        gSaveContext.cutsceneIndex = 0; gSaveContext.sceneLayer = 0;
        gPlayState->nextEntranceIndex = entrances[region];
        gPlayState->transitionTrigger = TRANS_TRIGGER_START;
        gPlayState->transitionType = TRANS_TYPE_INSTANT;
    }
}
void DrawTargetHealth() {
    if(!Enabled() || !gPlayState) { return; }
    auto* target=GET_PLAYER(gPlayState)->focusActor;
    if(!target || !AffixSystem::GetAffixData(target) || target->colChkInfo.health<=0) { return; }
    float maximum=static_cast<float>(std::max<int>(1,GetActorMaximumHealth(target)));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram,{.74f,.38f,.24f,1});
    ImGui::ProgressBar(std::clamp(target->colChkInfo.health/maximum,0.f,1.f),{-1,5},"");
    ImGui::PopStyleColor();
}
void DrawHud() {
    if (!Enabled()) { return; }
    Region region = CurrentRegion();
    ImVec4 danger = gARPGSave.level + 5 < region.minLevel ? ImVec4(1, 0.4f, 0.3f, 1) : ImVec4(0.8f, 0.72f, 0.5f, 1);
    auto regionText=std::string(region.name)+"  Lv "+std::to_string(region.minLevel)+"-"+std::to_string(region.maxLevel);
    float labelSize = 19.0f * HUDLayout::Scale(ImGui::GetMainViewport()->WorkSize.y);
    Theme::Label(regionText, labelSize, ImGui::ColorConvertFloat4ToU32(danger));
    Actor* target = GET_PLAYER(gPlayState)->focusActor;
    auto* data = AffixSystem::GetAffixData(target);
    if (data != nullptr && target->colChkInfo.health > 0) {
        ImGui::Text("Target Lv %u  |  %s", data->level, TacticName(target));
        std::string affixes = AffixSystem::AffixNames(target);
        if (!affixes.empty()) { ImGui::TextWrapped("%s", affixes.c_str()); }

        float max = static_cast<float>(std::max<int>(1, GetActorMaximumHealth(target)));
        ImGui::ProgressBar(std::clamp(target->colChkInfo.health / max, 0.0f, 1.0f), ImVec2(-1, 5), "");
    }
}
static void Register() {
    COND_HOOK(OnDamageActor, true, [](void* targetRef, void* sourceRef, uint32_t, uint8_t alignment, float* damage) {
        auto* target=static_cast<Actor*>(targetRef); auto* source=static_cast<Actor*>(sourceRef);
        auto* member = ObjectExtension::GetInstance().Get<Member>(target);
        if (member && damage && *damage > 0 && source && !ObjectExtension::GetInstance().Has<Watchman>(source) && (alignment & AT_TYPE_PLAYER)) { member->playerEngaged = true; }
    });
    COND_HOOK(OnSceneInit, true, [](int16_t) { Reset(); });
    COND_HOOK(OnPlayDestroy, true, []() { Reset(); });
    COND_HOOK(OnPlayerUpdate, true, Director);
}
static RegisterShipInitFunc init(Register);
}
extern "C" int ARPG_EnemyTactics(Actor* actor, PlayState* play) { return ARPG::Enemies::Tactics(actor, play); }

extern "C" float ARPG_EnemyAwareness(Actor* actor) {
    const auto* data = AffixSystem::GetAffixData(actor);
    if (!ARPG::Enemies::Enabled()) { return 240.0f; }
    if (ARPG::Enemies::IsRegional(actor)) { return 360.0f; }
    return data != nullptr && data->generated ? 600.0f : 240.0f;
}

extern "C" Actor* ARPG_EnemyTarget(Actor* actor, PlayState* play) { return ARPG::Enemies::RegionalTarget(actor,play); }
extern "C" int ARPG_IsRegionalEnemy(Actor* actor) { return ARPG::Enemies::IsRegional(actor); }
