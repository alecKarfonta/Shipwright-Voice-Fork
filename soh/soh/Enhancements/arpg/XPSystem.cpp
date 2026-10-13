#include "XPSystem.h"
#include "QuestSystem.h"
#include "ARPG.h"
#include "ARPGSave.h"
#include "ARPGRules.h"
#include "AffixSystem.h"
#include "EnemyRules.h"
#include "EnemySystem.h"
#include "SkillSystem.h"
#include <spdlog/spdlog.h>
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/ObjectExtension/ActorMaximumHealth.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"
#include "soh/Network/Anchor/Anchor.h"
#include "soh/Notification/Notification.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

namespace XPSystem {

struct Rewarded {};
static ObjectExtension::Register<Rewarded> rewardedRegister;

void ResetDefeatCredit(Actor* actor) {
    if (actor != nullptr) { ObjectExtension::GetInstance().Remove<Rewarded>(actor); }
}

// [TIME] | [INFO] | XPSystem.ExperienceFor | Explicit enemy weights keep rewards tied to combat difficulty rather than actor-ID order.
uint32_t ExperienceFor(const Actor* actor) {
    switch (actor->id) {
        case ACTOR_EN_FIRE_ROCK:
        case ACTOR_EN_ENCOUNT2: return 0;
        case ACTOR_EN_FIREFLY:
        case ACTOR_EN_CROW: return 10;
        case ACTOR_EN_DEKUBABA:
        case ACTOR_EN_DEKUNUTS:
        case ACTOR_EN_TITE: return 20;
        case ACTOR_EN_ST:
        case ACTOR_EN_RD: return 35;
        case ACTOR_EN_ZF:
        case ACTOR_EN_WF: return 60;
        case ACTOR_EN_TEST: return 80;
        case ACTOR_EN_IK: return 140;
        case ACTOR_EN_TORCH2: return 200;
        default: return std::clamp<uint32_t>(10 + 5 * GetActorMaximumHealth(actor), 15, 150);
    }
}

static const int16_t bosses[] = {
    ACTOR_BOSS_GOMA, ACTOR_BOSS_DODONGO, ACTOR_BOSS_VA, ACTOR_BOSS_GANONDROF, ACTOR_BOSS_FD,
    ACTOR_BOSS_MO, ACTOR_BOSS_SST, ACTOR_BOSS_TW, ACTOR_BOSS_GANON, ACTOR_BOSS_GANON2
};

static void Reward(void* ref, bool boss) {
    Actor* actor = static_cast<Actor*>(ref);
    if (actor && !ARPG::Enemies::RewardEligible(actor)) { return; }
    auto& extensions = ObjectExtension::GetInstance();
    if (!ARPG::Enabled() || actor == nullptr || extensions.Has<Rewarded>(actor)) { return; }
    if (!boss && (!EnemyDifficulty::IsEnemyActor(actor) || actor->category == ACTORCAT_BOSS)) { return; }
    uint32_t xp = boss ? 1500 : ExperienceFor(actor);
    if (xp == 0) { return; }
    extensions.Set<Rewarded>(actor, Rewarded{});
    ARPG::Quests::OnDefeat(actor,boss);
    if (boss) {
        gARPGSave.bossKills = ARPG::Rules::SaturatingAdd(gARPGSave.bossKills, 1);
        if (ARPG::SkillsEnabled()) {
            for (int i = 0; i < 10; ++i) {
                if (actor->id == bosses[i] && !(gARPGSave.defeatedBosses & (1u << i))) {
                    gARPGSave.defeatedBosses |= 1u << i;
                    SkillSystem::OnBossDefeat();
                    break;
                }
            }
        }
    } else {
        gARPGSave.kills = ARPG::Rules::SaturatingAdd(gARPGSave.kills, 1);
        const auto* affix = ARPG::AffixesEnabled() ? AffixSystem::GetAffixData(actor) : nullptr;
        if (affix != nullptr && affix->tier >= ARPG_TIER_CHAMPION) {
            xp *= affix->tier == ARPG_TIER_ELITE ? 4 : 2;
            gARPGSave.eliteKills = ARPG::Rules::SaturatingAdd(gARPGSave.eliteKills, 1);
        }
    }
    const auto* leveled = ARPG::AffixesEnabled() ? AffixSystem::GetAffixData(actor) : nullptr;
    if (leveled != nullptr && leveled->level > 0) { xp = ARPG::Enemies::LevelExperience(xp, leveled->level); }

    // [TIME] | [INFO] | Reward.Share | Broadcast the rolled reward so teammates get D2-style party XP.
    // Gated on the shared ARPG seed: party kill-share pairs with deterministic co-op rolls.
    {
        auto* anchor = Anchor::Instance;
        if (anchor != nullptr && anchor->isEnabled && anchor->isConnected && anchor->roomState.arpgSeed != 0 &&
            ARPG::SkillsEnabled()) {
            u8 tier = boss ? ARPG_TIER_BOSS : (leveled != nullptr ? leveled->tier : ARPG_TIER_NORMAL);
            u8 level = leveled != nullptr ? leveled->level : 0;
            anchor->SendPacket_ArpgEnemyKill(xp, tier, level, u16(actor->id));
        }
    }

    if (!ARPG::SkillsEnabled()) { return; }
    int rate = std::clamp(CVarGetInteger(CVAR_ENHANCEMENT("ARPG.XpRate"), 100), 0, 1000);
    SkillSystem::GrantXp(xp * rate / 100);
    uint32_t healing = SkillSystem::GetHpOnKill();
    if (healing > 0) { Health_ChangeBy(gPlayState, healing); }
    uint32_t rupees = SkillSystem::GetRupeesOnKill();
    if (rupees > 0) { Rupees_ChangeBy(rupees); }
}

// [TIME] | [INFO] | XPSystem.RemoteKill | Teammate defeated an enemy: grant half XP (party share) and
// surface elite/boss slays. Called from the Anchor game-thread packet queue (ArpgEnemyKill.cpp).
void RemoteKill(uint32_t xp, uint8_t tier, uint8_t level, const char* name) {
    if (!ARPG::Enabled() || !ARPG::SkillsEnabled() || xp == 0) {
        return;
    }
    auto* anchor = Anchor::Instance;
    if (anchor == nullptr || !anchor->isEnabled || anchor->roomState.arpgSeed == 0) {
        return; // kill-share rides along with shared-roll co-op only
    }
    int rate = std::clamp(CVarGetInteger(CVAR_ENHANCEMENT("ARPG.XpRate"), 100), 0, 1000);
    uint32_t share = xp / 2; // Diablo-style: full for the killer, half for party members
    SkillSystem::GrantXp(share * rate / 100);
    SPDLOG_INFO("[ARPG][coop] remote kill share +{} XP from {} (tier {})", share, name != nullptr ? name : "?", tier);

    if (tier >= ARPG_TIER_CHAMPION && name != nullptr) {
        const char* tierName = tier == ARPG_TIER_BOSS ? "Boss" : tier == ARPG_TIER_ELITE ? "Elite" : "Champion";
        Notification::Emit({
            .message = std::string(name) + " slain a " + (level > 0 ? "Lv " + std::to_string(level) + " " : "") + tierName + " (+" + std::to_string(share) + " XP)",
        });
    }
}

static void RegisterXpHooks() {
    COND_HOOK(OnEnemyDefeat, true, [](void* ref) { Reward(ref, false); });
    COND_HOOK(OnBossDefeat, true, [](void* ref) { Reward(ref, true); });
}
static RegisterShipInitFunc initXp(RegisterXpHooks);

} // namespace XPSystem

// [TIME] | [INFO] | ARPG_OnRemoteKill | Global-scope trampoline into XPSystem; declared in XPSystem.h,
// called from the Anchor packet handler (ArpgEnemyKill.cpp).
void ARPG_OnRemoteKill(uint32_t xp, uint8_t tier, uint8_t level, const char* name) {
    XPSystem::RemoteKill(xp, tier, level, name);
}
