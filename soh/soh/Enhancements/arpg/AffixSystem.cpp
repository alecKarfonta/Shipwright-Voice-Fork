#include "AffixSystem.h"
#include "ArsenalSystem.h"
#include "AdventureRelics.h"
#include "CreatureSystem.h"
#include "ExpeditionSystem.h"

// [TIME] | [INFO] | AffixSystem.cpp | Tier rolls, stat application, damage mults, nametags
//
// Flow: OnActorInit (enemy) -> roll tier+affixes -> scale HP -> nametag.
// Damage: OnDamageActor hook multiplies incoming damage by target resist/vuln
// and by player item/skill bonuses. Composes with EnemyDifficulty (both are
// multiplicative, so ordering does not matter).

#include <algorithm>
#include <cmath>
#include <string>

#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/ObjectExtension/ActorMaximumHealth.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/nametag.h"
#include "ARPG.h"
#include "ARPGSave.h"
#include "ItemModifierSystem.h"
#include "SkillSystem.h"
#include "ARPGRules.h"
#include "ARPGSpells.h"
#include "EnemySystem.h"
#include "CoopRules.h"
#include "SkillCombat.h"
#include "EnchantedSystem.h"
#include "DamageNumbers.h"
#include "soh/ActorDB.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"
#include "soh/Network/Anchor/Anchor.h"

extern "C" {
#include "z64.h"
#include "variables.h"
#include "functions.h"
#include "macros.h"
extern PlayState* gPlayState;
}

// SECTION: affix name pool

// [TIME] | [INFO] | sAffixNames | display names indexed by rolled affixId
// 11..13 are Enchanted elemental auras (see EnchantedSystem)
static const char* sAffixNames[] = {
    "Empowered",        // +damage out
    "Fortified",        // +HP
    "Swift",            // +speed (curated enemies)
    "Fireproof",        // resist fire
    "Frostproof",       // resist ice
    "Lightward",        // resist light
    "Spellwoven",       // resist magic
    "Blastproof",       // resist explosive
    "Ironhide",         // resist swords
    "Arrowward",        // resist ranged
    "Frail",            // takes +50% damage (drops extra loot)
    "Fire Enchanted",   // burns on hit + fire nova on death
    "Frost Enchanted",  // chills on hit + frost nova on death
    "Storm Enchanted",  // chance to zap on hit + storm nova on death
};
constexpr int AFFIX_COUNT = sizeof(sAffixNames) / sizeof(sAffixNames[0]);

// AffixId -> element group + resist multiplier semantics
// (elemMult stores DAMAGE DIVISOR x256: 256=neutral, 512=takes half, 384=takes 2/3)
struct AffixDef {
    int elemGroup;   // -1 = no element interaction
    uint16_t resist; // divisor semantics, 256 = none
};

static const AffixDef sAffixDefs[AFFIX_COUNT] = {
    { -1, 256 },              // Empowered
    { -1, 256 },              // Fortified
    { -1, 256 },              // Swift
    { ARPG_ELEM_FIRE, 512 },      // Fireproof
    { ARPG_ELEM_ICE, 512 },       // Frostproof
    { ARPG_ELEM_LIGHT, 512 },     // Lightward
    { ARPG_ELEM_MAGIC, 512 },     // Spellwoven
    { ARPG_ELEM_EXPLOSIVE, 512 }, // Blastproof
    { ARPG_ELEM_SWORD, 384 },     // Ironhide
    { ARPG_ELEM_RANGED, 512 },    // Arrowward
    { -1, 256 },              // Frail
    { -1, 256 },              // Fire Enchanted (aura handled by EnchantedSystem)
    { -1, 256 },              // Frost Enchanted
    { -1, 256 },              // Storm Enchanted
};

// SECTION: ObjectExtension registration

static ObjectExtension::Register<ARPGAffixData> AffixDataRegister;

ARPGAffixData* AffixSystem::GetAffixData(Actor* actor) {
    return ObjectExtension::GetInstance().Get<ARPGAffixData>(actor);
}

// Bosses with private/manual health fields: no HP affixes in v1 (damage-out only)
static bool IsManualHealthBoss(s16 actorId) {
    switch (actorId) {
        case ACTOR_BOSS_DODONGO:   // King Dodongo (private s16 health)
        case ACTOR_BOSS_GANONDROF: // Phantom Ganon
        case ACTOR_BOSS_GANON:
        case ACTOR_BOSS_GANON2:
            return true;
        default:
            return false;
    }
}

// SECTION: element group mapping

// [TIME] | [INFO] | ElementGroupForFlags | DMG_* bit -> ARPGElementGroup
int AffixSystem::ElementGroupForFlags(uint32_t dmgFlags) {
    if (dmgFlags & DMG_ARROW_ICE) {
        return ARPG_ELEM_ICE;
    }
    if (dmgFlags & DMG_ARROW_LIGHT) {
        return ARPG_ELEM_LIGHT;
    }
    if (dmgFlags & (DMG_ARROW_FIRE | DMG_MAGIC_FIRE)) {
        return ARPG_ELEM_FIRE;
    }
    if (dmgFlags & DMG_MAGIC_ICE) {
        return ARPG_ELEM_ICE;
    }
    if (dmgFlags & DMG_MAGIC_LIGHT) {
        return ARPG_ELEM_LIGHT;
    }
    if (dmgFlags & DMG_EXPLOSIVE) {
        return ARPG_ELEM_EXPLOSIVE;
    }
    if (dmgFlags & DMG_HAMMER) {
        return ARPG_ELEM_HAMMER;
    }
    if (dmgFlags & (DMG_ARROW | DMG_HOOKSHOT | DMG_SLINGSHOT)) {
        return ARPG_ELEM_RANGED;
    }
    return (dmgFlags & (DMG_SWORD | DMG_DEKU_STICK)) ? ARPG_ELEM_SWORD : -1;
}

// SECTION: roll + apply

// [TIME] | [INFO] | sCoopOccurrences | Co-op determinism bookkeeping: nth identical (actorId, params)
// actor configured in the current scene. Reset on scene change; stable because
// scene actor init order is deterministic for a shared seed.
struct OccurrenceKey {
    uint32_t id, params;
    bool operator==(const OccurrenceKey& o) const { return id == o.id && params == o.params; }
};
struct OccurrenceHash {
    size_t operator()(const OccurrenceKey& k) const {
        return (size_t(k.id) * 1099511628211ull) ^ size_t(k.params);
    }
};
static std::unordered_map<OccurrenceKey, uint32_t, OccurrenceHash> sCoopOccurrences;
static uint32_t sCoopScene = 0xFFFFFFFF;

// Active shared seed: room seed when Anchor is connected and the room published one.
static uint32_t ActiveSharedSeed() {
    auto* anchor = Anchor::Instance;
    if (anchor == nullptr || !anchor->isEnabled || !anchor->isConnected || anchor->roomState.arpgSeed == 0) {
        return 0;
    }
    return anchor->roomState.arpgSeed;
}

// [TIME] | [INFO] | AffixSystem.Configure | Seeded regional rolls compose with existing difficulty and preserve damage already taken.
void AffixSystem::Configure(Actor* actor, int forcedTier, uint16_t pack, uint8_t member) {
    if (!ARPG::AffixesEnabled() || actor == nullptr) { return; }
    float healthFraction = 1.0f;
    if (auto* old = GetAffixData(actor); old != nullptr && old->appliedHpMult > 0) {
        int maximum = std::max<int>(actor->colChkInfo.health, GetActorMaximumHealth(actor));
        healthFraction = maximum > 0 ? actor->colChkInfo.health / float(maximum) : 0.0f;
        actor->colChkInfo.health = maximum;
        EnemyDifficulty::ApplyHealthMultiplier(actor, 1.0f / old->appliedHpMult);
    }
    NameTag_RemoveForActorByTag(actor, "ARPGAffix");
    bool isBoss = actor->category == ACTORCAT_BOSS;

    int medallions = 0;
    for (int q = QUEST_MEDALLION_FOREST; q <= QUEST_MEDALLION_LIGHT; q++) {
        if (CHECK_QUEST_ITEM(q)) {
            medallions++;
        }
    }

    ARPGAffixData data{};
    std::fill(std::begin(data.elemMult), std::end(data.elemMult), 256);

    // [TIME] | [INFO] | Configure.Coop | Shared room seed -> deterministic rolls identical on every client.
    uint32_t sharedSeed = ActiveSharedSeed();
    ARPG::Coop::RollContext coop{};
    if (sharedSeed != 0) {
        uint32_t scene = gPlayState != nullptr ? uint32_t(gPlayState->sceneNum) : 0;
        if (sCoopScene != scene) {
            sCoopOccurrences.clear();
            sCoopScene = scene;
        }
        OccurrenceKey key{ uint32_t(uint16_t(actor->id)), uint32_t(uint16_t(actor->params)) };
        uint32_t occurrence = sCoopOccurrences[key]++;
        coop = { sharedSeed, scene, key.id, key.params, occurrence };
    }

    int chance = std::clamp(CVarGetInteger(CVAR_ENHANCEMENT("ARPG.EliteChance"), ELITE_CHANCE_DEFAULT), 0, 100);
    int championRatio = std::clamp(CVarGetInteger(CVAR_ENHANCEMENT("ARPG.ChampionRatio"), CHAMPION_RATIO_DEFAULT), 0, 100);
    if (sharedSeed != 0) {
        data.tier = isBoss ? ARPG_TIER_BOSS : forcedTier >= 0 ? std::clamp(forcedTier, 0, 2) :
                    ARPG::Coop::RollTier(coop, chance, championRatio, isBoss);
    } else {
        data.tier = isBoss ? ARPG_TIER_BOSS : forcedTier >= 0 ? std::clamp(forcedTier, 0, 2) :
                    int(ARPG::RandInt(1, 100)) <= chance ? (int(ARPG::RandInt(1, 100)) <= championRatio ? ARPG_TIER_CHAMPION : ARPG_TIER_ELITE) : ARPG_TIER_NORMAL;
    }
    const auto region = ARPG::Enemies::CurrentRegion();
    if (!isBoss && region.minLevel == 1 && region.maxLevel <= 3) { data.tier = ARPG_TIER_NORMAL; }
    data.packId = pack;
    data.packMember = member;
    data.generated = pack != 0;
    data.role = member % 3;
    data.hpMult = data.dmgMult = data.speedMult = 256;
    if (ARPG::Enemies::Enabled()) {
        const auto stats = sharedSeed != 0 ? ARPG::Coop::RollStatsFor(coop, ARPG::Enemies::CurrentRegion(), data.tier)
                                           : ARPG::Enemies::RollStats(gARPGSave.rngState, ARPG::Enemies::CurrentRegion(), data.tier);
        data.level = stats.level;
        data.hpMult = stats.hp;
        data.dmgMult = stats.damage;
        data.speedMult = EnemyDifficulty::IsCurated(actor->id) ? stats.speed : 256;
    } else if (data.tier != ARPG_TIER_NORMAL) {
        data.hpMult = isBoss ? 256 + medallions * 26 : data.tier == ARPG_TIER_ELITE ? 560 : 400;
        data.dmgMult = isBoss ? 256 + medallions * 13 : data.tier == ARPG_TIER_ELITE ? 340 : 290;
    }
    if (IsManualHealthBoss(actor->id)) { data.hpMult = 256; }
    data.lootBudget = data.tier;
    data.affixCount = data.tier == ARPG_TIER_NORMAL ? 0 : data.tier == ARPG_TIER_CHAMPION ? 1 : 2;

    uint8_t candidates[AFFIX_COUNT];
    uint8_t candidateCount = 0;
    for (uint8_t id = 0; id < AFFIX_COUNT; ++id) {
        if (IsManualHealthBoss(actor->id) && id != 0 && id != 2) { continue; }
        if (id == 2 && !EnemyDifficulty::IsCurated(actor->id)) { continue; }
        // [TIME] | [INFO] | RollAffixes | Enchanted elemental auras are Champion/Elite signature mods.
        if (id >= 11 && id <= 13 && (data.tier == ARPG_TIER_NORMAL || !ARPG::Enchanted::EnabledGate())) { continue; }        candidates[candidateCount++] = id;
    }
    data.affixCount = std::min(data.affixCount, candidateCount);
    for (uint8_t slot = 0; slot < data.affixCount; ++slot) {
        data.affixIds[slot] = sharedSeed != 0 ? ARPG::Coop::TakeAffix(coop, candidates, candidateCount, slot)
                                              : ARPG::Rules::TakeDistinct(gARPGSave.rngState, candidates, candidateCount, slot);
    }

    // [TIME] | [INFO] | RollAffixes | Named stat affixes have real effects; exclude unsupported boss HP and movement rolls.
    for (int a = 0; a < data.affixCount; ++a) {
        if (data.affixIds[a] == 0) {
            data.dmgMult = std::min<int>(832, data.dmgMult + 64);
        } else if (data.affixIds[a] == 1) {
            data.hpMult = std::min<int>(1920, data.hpMult + 128);
        } else if (data.affixIds[a] == 2) {
            data.speedMult = std::min<int>(350, data.speedMult + 64);
        } else if (data.affixIds[a] == 10) {
            ++data.lootBudget;
        }
    }
    // Enchanted auras imply their element resist (Fire Enchanted is fire-immune, etc.)
    for (int a = 0; a < data.affixCount; ++a) {
        if (data.affixIds[a] == 11) { data.elemMult[ARPG_ELEM_FIRE] = 512; }
        if (data.affixIds[a] == 12) { data.elemMult[ARPG_ELEM_ICE] =  512; }
        if (data.affixIds[a] == 13) { data.elemMult[ARPG_ELEM_LIGHT] = 512; }
    }

    // Elemental resistances from affix names; Elites get hard immunity on one group
    for (int a = 0; a < data.affixCount; a++) {
        const AffixDef& def = sAffixDefs[data.affixIds[a] % AFFIX_COUNT];
        if (def.elemGroup == -1) {
            continue;
        }
        // [TIME] | [INFO] | RollAffixes | Keep physical attacks viable before the player owns alternate elemental weapons.
        bool immunity = data.tier == ARPG_TIER_ELITE && def.elemGroup >= ARPG_ELEM_FIRE && def.elemGroup <= ARPG_ELEM_MAGIC;
        data.elemMult[def.elemGroup] = immunity ? 0xFFFF : def.resist;
    }

    ObjectExtension::GetInstance().Set<ARPGAffixData>(actor, std::move(data));

    // Scale HP immediately (Init already assigned colChkInfo.health)
    ARPGAffixData* affix = AffixSystem::GetAffixData(actor);
    if (affix != nullptr && !IsManualHealthBoss(actor->id)) {
        uint8_t baseHealth = actor->colChkInfo.health;
        EnemyDifficulty::ApplyHealthMultiplier(actor, affix->hpMult / 256.0f);
        affix->appliedHpMult = baseHealth > 0 ? actor->colChkInfo.health / float(baseHealth) : 1.0f;
    }

    if (healthFraction < 1.0f && !IsManualHealthBoss(actor->id)) {
        actor->colChkInfo.health = healthFraction > 0 ? std::max(1, int(std::lround(actor->colChkInfo.health * healthFraction))) : 0;
    }

    // Nametag label (tier + affixes)
    static const char* tierNames[] = { "", "Champion", "Elite", "Boss" };
    static const char* eliteNames[] = { "Ashfang", "Dreadhowl", "Grimscar", "Nightmaw", "Stormclaw", "Bonebreaker", "Ironspite", "Frostbane" };
    std::string species = ActorDB::Instance->RetrieveEntry(actor->id).desc;
    if (actor->id == ACTOR_EN_WF) { species = "Wolfos"; }
    if (actor->id == ACTOR_EN_TEST) { species = "Stalfos"; }
    if (actor->id == ACTOR_EN_ZF) { species = actor->params == -2 ? "Dinolfos" : "Lizalfos"; }
    if (actor->id == ACTOR_EN_TITE) { species = "Tektite"; }
    std::string label = affix->level > 0 ? "Lv " + std::to_string(affix->level) + " " : "";
    if (affix->tier == ARPG_TIER_ELITE) {
        // [TIME] | [INFO] | Configure.EliteName | Deterministic name from the shared seed so both screens show the same Elite.
        label += sharedSeed != 0 ? ARPG::Coop::EliteName(coop) : eliteNames[ARPG::RandInt(0, 7)];
    }
    else {
        if (affix->tier != ARPG_TIER_NORMAL) { label += std::string(tierNames[affix->tier]) + " "; }
        label += species;
    }
    // [TIME] | [INFO] | Configure.CoopLog | One-line roll fingerprint for two-client parity checks.
    if (sharedSeed != 0 && affix->tier != ARPG_TIER_NORMAL) {
        SPDLOG_INFO("[ARPG][coop] seed={:08X} scene={} actor={:04X} params={:04X} occ={} tier={} lvl={} hp={} dmg={} spd={} affixes=[{},{}] \"{}\"",
                    sharedSeed, coop.scene, coop.actorId, coop.params, coop.occurrence, affix->tier, affix->level,
                    affix->hpMult, affix->dmgMult, affix->speedMult,
                    affix->affixCount > 0 ? affix->affixIds[0] : -1,
                    affix->affixCount > 1 ? affix->affixIds[1] : -1, label.c_str());
    }
    NameTagOptions options{};
    options.tag = "ARPGAffix";
    options.yOffset = 12;
    options.scale = affix->tier == ARPG_TIER_NORMAL ? 0.24f : 0.30f;
    options.textColor = affix->tier == ARPG_TIER_NORMAL ? Color_RGBA8{ 215, 210, 190, 230 } : affix->tier == ARPG_TIER_ELITE ? Color_RGBA8{ 255, 120, 40, 255 } // orange
                                                       : Color_RGBA8{ 120, 220, 255, 255 }; // cyan
    NameTag_RegisterForActorWithOptions(actor, label.c_str(), options);
}

std::string AffixSystem::AffixNames(Actor* actor) {
    const auto* data = GetAffixData(actor);
    std::string names;
    if (data != nullptr) {
        for (int i = 0; i < data->affixCount; ++i) {
            if (i > 0) { names += ", "; }
            names += sAffixNames[data->affixIds[i] % AFFIX_COUNT];
        }
    }
    return names;
}

// SECTION: damage pipeline

// [TIME] | [INFO] | ComputeDamageMultiplier | target resist/vuln x player bonuses.
// Each subsystem contributes only when its own CVAR gate is on.
float AffixSystem::ResistanceForFlags(Actor* actor, uint32_t dmgFlags) {
    const auto* affix = ARPG::AffixesEnabled() ? GetAffixData(actor) : nullptr;
    if (affix == nullptr) { return 1.0f; }
    int group = ElementGroupForFlags(dmgFlags);
    float resistance = group >= 0 ? ARPG::Rules::ResistanceMultiplier(affix->elemMult[group]) : 1.0f;
    if (dmgFlags & (DMG_MAGIC_FIRE | DMG_MAGIC_ICE | DMG_MAGIC_LIGHT)) {
        resistance *= ARPG::Rules::ResistanceMultiplier(affix->elemMult[ARPG_ELEM_MAGIC]);
    }
    // [TIME] | [INFO] | AffixSystem.ResistanceForFlags | Frail applies once, including elemental attacks that also count as magic.
    for (int i = 0; i < affix->affixCount; ++i) {
        if (affix->affixIds[i] == 10) { resistance *= 1.5f; break; }
    }
    return resistance;
}

float AffixSystem::ComputeDamageMultiplier(Actor* target, uint32_t dmgFlags, bool* wasCrit, float* resistanceOut) {
    float mult = 1.0f;
    float resistance = ResistanceForFlags(target, dmgFlags);
    float elementalBonus = 0.0f;
    bool crit = false;

    // Player-side bonuses apply only to player attack types
    bool melee = (dmgFlags & (DMG_SWORD | DMG_HAMMER | DMG_DEKU_STICK)) != 0;
    bool ranged = (dmgFlags & (DMG_ARROW | DMG_HOOKSHOT | DMG_SLINGSHOT)) != 0;
    if (melee) {
        if (ARPG::ItemsEnabled()) {
            mult *= ItemModifierSystem::GetMeleeDamageMult();
        }
        if (ARPG::SkillsEnabled()) {
            mult *= SkillSystem::GetMeleeDamageMult();
            // Elemental Infusion: imbued blade deals +50% while active
            if (SkillSystem::IsInfuseActive()) {
                float resist = ResistanceForFlags(target, SkillSystem::InfusionFlags());
                elementalBonus += SkillSystem::InfusionPower() * resist;
                if (target->colChkInfo.damageTable != nullptr && resist > 0) {
                    int bit = 0;
                    for (uint32_t flags = SkillSystem::InfusionFlags(); flags > 1; flags >>= 1) { ++bit; }
                    uint8_t entry = target->colChkInfo.damageTable->table[bit];
                    if ((entry & 15) > 0) { target->colChkInfo.damageEffect = entry >> 4; }
                }
            }
        }
        if (SkillSystem::RollCrit()) {
            mult *= 2.0f;
            crit = true;
        }
    } else if (ranged) {
        if (ARPG::ItemsEnabled()) {
            mult *= ItemModifierSystem::GetRangedDamageMult();
        }
    }

    // Gear elemental on-hit procs
    if (ARPG::ItemsEnabled()) {
        elementalBonus += ItemModifierSystem::RollElementalProcMult(target, dmgFlags) - 1.0f;
    }

    // [TIME] | [INFO] | ComputeDamageMultiplier.Out | Crit/resist readouts power the floating damage numbers.
    if (wasCrit != nullptr) { *wasCrit = crit; }
    if (resistanceOut != nullptr) { *resistanceOut = resistance; }
    return mult * (resistance + elementalBonus);
}

// SECTION: hooks

static void RegisterAffixHooks() {
    COND_HOOK(OnActorInit, true, [](void* actorRef) {
        Actor* actor = static_cast<Actor*>(actorRef);
        if (!ARPG::AffixesEnabled() || !EnemyDifficulty::IsEnemyActor(actor)) {
            return;
        }
        if (actor->colChkInfo.health == 0 && actor->category != ACTORCAT_BOSS) {
            return;
        }
        AffixSystem::Configure(actor);
    });

    // [TIME] | [INFO] | AffixSystem.Disable | Remove already-applied HP and labels when a live toggle is switched off.
    COND_HOOK(OnActorUpdate, true, [](void* ref) {
        auto* actor = static_cast<Actor*>(ref);
        auto* affix = AffixSystem::GetAffixData(actor);
        if (affix == nullptr) {
            if (ARPG::AffixesEnabled() && EnemyDifficulty::IsEnemyActor(actor) && actor->colChkInfo.health > 0) {
                AffixSystem::Configure(actor);
            }
            return;
        }
        if (ARPG::AffixesEnabled()) {
            if ((affix->level > 0) != ARPG::Enemies::Enabled() && actor->colChkInfo.health > 0) {
                AffixSystem::Configure(actor, affix->tier, affix->packId, affix->packMember);
            }
            return;
        }
        if (affix->appliedHpMult > 0.0f) {
            int maximum = std::max<int>(actor->colChkInfo.health, GetActorMaximumHealth(actor));
            float fraction = maximum > 0 ? actor->colChkInfo.health / float(maximum) : 0;
            actor->colChkInfo.health = maximum;
            EnemyDifficulty::ApplyHealthMultiplier(actor, 1.0f / affix->appliedHpMult);
            actor->colChkInfo.health = fraction > 0 ? std::max(1, int(std::lround(actor->colChkInfo.health * fraction))) : 0;
        }
        ObjectExtension::GetInstance().Remove<ARPGAffixData>(actor);
        NameTag_RemoveForActorByTag(actor, "ARPGAffix");
    });

    // [TIME] | [INFO] | AffixSystem.OnDamageActor | Alignment comes from the attacking collider, including player projectiles.
    COND_HOOK(OnDamageActor, true, [](void* actorRef, void* attackerRef, u32 dmgFlags, uint8_t attackType, float* damage) {
        if (!ARPG::Enabled()) {
            return;
        }
        Actor* target = static_cast<Actor*>(actorRef);
        Actor* attacker = static_cast<Actor*>(attackerRef);
        if (ARPG::Enemies::IsWatchman(attacker)) { return; }
        *damage *= ARPG::Creatures::OnContact(target,attacker,dmgFlags,attackType,*damage);
        *damage *= ARPG::Relics::OnContact(target,attacker,dmgFlags,attackType,*damage);
        *damage *= ARPG::Arsenal::OnContact(target,attacker,dmgFlags,attackType,*damage);
        if(*damage<=0) { return; }
        if (target->id == ACTOR_PLAYER) {
            const auto* affix = AffixSystem::GetAffixData(attacker);
            if (affix == nullptr && attacker != nullptr) { affix = AffixSystem::GetAffixData(attacker->parent); }
            if (ARPG::AffixesEnabled() && affix != nullptr) {
                *damage *= affix->dmgMult / 256.0f;
                // [TIME] | [INFO] | OnDamageActor.Enchanted | Fire/Frost/Storm auras fold their bonus into the same hit.
                ARPG::Enchanted::OnPlayerStruck(affix, damage);
            }
            if (SkillSystem::IsAuraActive()) { SkillSystem::OnIncomingHit(*damage); }
            return;
        }
        if (EnemyDifficulty::IsEnemyActor(target) && (attackType & AT_TYPE_PLAYER)) {
            bool crit = false;
            float resist = 1.0f;
            *damage *= AffixSystem::ComputeDamageMultiplier(target, dmgFlags, &crit, &resist) * ARPG::Spells::DamageMultiplier(attacker) * ARPG::SkillCombat::DamageMultiplier(attacker, target) * ARPG::Expeditions::DamageMultiplier(target,dmgFlags);
            *damage *= ItemModifierSystem::WeaponDamageMultiplier(target,attacker,dmgFlags,&crit);
            ARPG::SkillCombat::OnHit(attacker, target, *damage);
            SkillSystem::OnMeleeHit(attacker, target, dmgFlags, *damage);
            if (*damage <= 0) { target->colChkInfo.damageEffect = 0; }
            // [TIME] | [INFO] | OnDamageActor.Numbers | World-space combat text: white hits, yellow crits, grey resists.
            ARPG::DamageNumbers::Spawn(target, *damage, crit, resist < 0.75f);
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterAffixHooks);
