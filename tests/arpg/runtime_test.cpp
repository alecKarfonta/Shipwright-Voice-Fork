#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGBridge.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/ARPGSaveCodec.h"
#include "soh/Enhancements/arpg/AffixSystem.h"
#include "soh/Enhancements/arpg/SkillSystem.h"
#include "soh/Enhancements/arpg/ItemModifierSystem.h"
#include "soh/Enhancements/arpg/InventoryRules.h"
#include "soh/Enhancements/arpg/CraftingRules.h"
#include "soh/Enhancements/arpg/InventorySystem.h"
#include "soh/Enhancements/arpg/EnemySystem.h"
#include "soh/Enhancements/arpg/SkillCombat.h"
#include "soh/Enhancements/arpg/SkillRules.h"
#include "soh/ObjectExtension/ActorMaximumHealth.h"
#include <vector>
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"
#include "soh/Network/Anchor/Anchor.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/ShipInit.hpp"
#include "soh/SaveManager.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "overlays/actors/ovl_En_M_Thunder/z_en_m_thunder.h"
void Sram_InitDebugSave(void);
void CollisionCheck_ApplyDamage(PlayState*, CollisionCheckContext*, Collider*, ColliderInfo*);
extern PlayState* gPlayState;
extern GameState* gGameState;
}

namespace {
int phase = 0;
int frames = 0;
std::filesystem::path resultPath;
nlohmann::json checks = nlohmann::json::array();
Actor target{};
Actor attacker{};
Actor* groundLoot = nullptr;
Actor* materialLoot = nullptr;
uint16_t materialsBefore = 0;
uint32_t itemsBefore = 0;
int bagBefore = 0;
std::vector<Actor*> packActors;
std::vector<Vec3f> packPositions;
int initialPack = 0;
uint32_t combatAttacks = 0;
bool dungeonFixturePlaced = false;
Vec3f dungeonFixturePosition{};

void Check(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
    checks.push_back(message);
    SPDLOG_INFO("[ARPG QA] PASS: {}", message);
}

void Result(const char* status, const std::string& error = "") {
    std::ofstream(resultPath) << nlohmann::json({ { "status", status }, { "checks", checks }, { "error", error } }).dump(2);
    phase = 99;
}

void RunChecks() {
    if (phase == 99) { return; }
    try {
        ++frames;
        if (phase >= 10) { gSaveContext.health = gSaveContext.healthCapacity; }
        // [TIME] | [INFO] | RuntimeTests.DungeonFixture | Hold the idle population fixture at its valid entrance spawn; enemy body collisions can push even an invulnerable player through the exit.
        if (phase == 15 && gPlayState && gPlayState->sceneNum == SCENE_DEKU_TREE && GameInteractor::IsPlayerInControl()) {
            auto* player = GET_PLAYER(gPlayState);
            if (!dungeonFixturePlaced) {
                dungeonFixturePosition = player->actor.world.pos;
                dungeonFixturePlaced = true;
                SPDLOG_INFO("[ARPG QA] Dungeon fixture position: x={} y={} z={}", dungeonFixturePosition.x,
                            dungeonFixturePosition.y, dungeonFixturePosition.z);
            }
            player->actor.world.pos = dungeonFixturePosition;
            player->actor.prevPos = dungeonFixturePosition;
            player->actor.velocity = {};
            player->actor.speedXZ = 0;
            player->invincibilityTimer = 20;
        }

        if (phase == 0 && frames > 60 && gGameState != nullptr) {
            CVarSetInteger(CVAR_DEVELOPER_TOOLS("DebugSaveFileMode"), 2);
            gSaveContext.gameMode = GAMEMODE_NORMAL;
            gSaveContext.fileNum = 0xFE;
            Sram_InitDebugSave();
            gSaveContext.gameMode = GAMEMODE_NORMAL;
            gSaveContext.fileNum = 0xFF;
            gSaveContext.linkAge = 0;
            gSaveContext.cutsceneIndex = 0;
            gSaveContext.sceneLayer = 0;
            gSaveContext.entranceIndex = ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;
            gSaveContext.respawnFlag = 0;
            gSaveContext.dayTime = gSaveContext.skyboxTime = 0x8000;
            gGameState->running = false;
            SET_NEXT_GAMESTATE(gGameState, Play_Init, PlayState);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"), 1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Items"), 1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Skills"), 1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Affixes"), 1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.PickupText"), 0);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"), 0);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enemies"), 1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Tactics"), 1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.InventoryOnPause"), 0);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.AllowRandomizer"), 0);
            ARPG::SeedRng(0xD1AB102);
            CVarSetInteger(CVAR_WINDOW("ARPG"), 1);
            phase = 1;
            frames = 0;
            return;
        }
        if (phase == 1 && frames > 60 && GameInteractor::IsPlayerInControl()) {
            Check(ARPG::Enabled() && gARPGSave.level == 1, "Debug save initializes isolated ARPG state");
            SkillSystem::GrantXp(560);
            Check(gARPGSave.level == 3 && gARPGSave.passivePoints == 2, "Runtime XP grants levels and points");
            Check(SkillSystem::SpendPoint(ARPG_SKILL_MAGIC_RECOVERY), "Spend a passive point");
            gARPGSave.activePoints = 10;
            Check(SkillSystem::SpendPoint(ARPG_SKILL_MAGIC_FROST_NOVA), "Learn an advanced active after its prerequisite");
            gSaveContext.magic = gSaveContext.magicCapacity = 96;
            gSaveContext.isMagicAcquired = 1;
            gSaveContext.magicState = MAGIC_STATE_IDLE;
            Check(!SkillSystem::TryCastActive(gPlayState, ARPG_SKILL_MAGIC_RECOVERY), "Passive cannot be cast");
            Check(SkillSystem::TryCastActive(gPlayState, ARPG_SKILL_MAGIC_FROST_NOVA), "Frost Nova spawns a native area collider");
            int magicTarget = gSaveContext.magicTarget;
            Check(!SkillSystem::TryCastActive(gPlayState, ARPG_SKILL_MAGIC_FROST_NOVA) && gSaveContext.magicTarget == magicTarget,
                  "Cooldown rejection does not spend magic");

            target.id = ACTOR_EN_TEST;
            target.category = ACTORCAT_ENEMY;
            target.colChkInfo.health = 10;
            attacker.id = ACTOR_PLAYER;
            attacker.category = ACTORCAT_PLAYER;
            ARPGAffixData affix{};
            affix.tier = ARPG_TIER_ELITE;
            affix.hpMult = 256;
            affix.dmgMult = 384;
            affix.speedMult = 320;
            std::fill(std::begin(affix.elemMult), std::end(affix.elemMult), 256);
            affix.elemMult[ARPG_ELEM_FIRE] = UINT16_MAX;
            affix.elemMult[ARPG_ELEM_SWORD] = 384;
            ObjectExtension::GetInstance().Set<ARPGAffixData>(&target, std::move(affix));
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Skills"), 0);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Items"), 0);
            float damage = 6;
            GameInteractor_ExecuteOnDamageActor(&target, &attacker, DMG_SLASH_MASTER, AT_TYPE_PLAYER, &damage);
            Check(std::abs(damage - 4.0f) < 0.001f, "Enemy resistance scales player damage");
            damage = 6;
            GameInteractor_ExecuteOnDamageActor(&target, &attacker, DMG_MAGIC_FIRE, AT_TYPE_PLAYER, &damage);
            Check(damage == 0, "Fire immunity suppresses damage");
            auto* testAffix = AffixSystem::GetAffixData(&target);
            testAffix->elemMult[ARPG_ELEM_FIRE] = 256;
            testAffix->affixIds[0] = 10;
            testAffix->affixCount = 1;
            damage = 6;
            GameInteractor_ExecuteOnDamageActor(&target, &attacker, DMG_MAGIC_FIRE, AT_TYPE_PLAYER, &damage);
            Check(damage == 9, "Frail amplifies elemental magic exactly once");
            testAffix->affixCount = 0;
            testAffix->elemMult[ARPG_ELEM_FIRE] = UINT16_MAX;
            damage = 4;
            GameInteractor_ExecuteOnDamageActor(&attacker, &target, DMG_SLASH_MASTER, AT_TYPE_ENEMY, &damage);
            Check(damage == 6, "Outgoing enemy affix scales incoming player damage");
            target.speedXZ = 3;
            bool constantSpeed = true;
            for (int i = 0; i < 100; ++i) { constantSpeed &= ARPG_ActorMovementMultiplier(&target) == 1.25f; }
            Check(constantSpeed, "Swift velocity multiplier stays constant");
            Check(target.speedXZ == 3, "Swift does not mutate persistent AI speed");
            auto anchor = Anchor::Instance;
            Check(anchor != nullptr, "EnemyDifficulty integration is available");
            auto difficulty = anchor->roomState.difficultyMode;
            anchor->roomState.difficultyMode = 2;
            EnemyDifficulty::UpdatePlayerCount(5);
            EnemyDifficulty::RegisterHooks(true);
            Check(EnemyDifficulty_MovementMultiplier(&target) * ARPG_ActorMovementMultiplier(&target) == 2.25f,
                  "ARPG and five-player difficulty compose within the movement cap");
            Check(GameInteractor::State::DefenseModifier < 0, "Full co-op difficulty increases incoming damage");
            anchor->roomState.difficultyMode = 1;
            EnemyDifficulty::RegisterHooks(true);
            Check(EnemyDifficulty_MovementMultiplier(&target) == 1 && GameInteractor::State::DefenseModifier == 0,
                  "HP-only co-op mode leaves speed and defense unchanged");
            EnemyDifficulty::RegisterHooks(false);
            EnemyDifficulty::UpdatePlayerCount(1);
            anchor->roomState.difficultyMode = difficulty;
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"), 0);
            damage = 6;
            GameInteractor_ExecuteOnDamageActor(&target, &attacker, DMG_MAGIC_FIRE, AT_TYPE_PLAYER, &damage);
            Check(damage == 6, "Master-off restores unmodified damage");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"), 1);
            auto quest = gSaveContext.ship.quest.id;
            gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
            Check(!ARPG::Enabled(), "Randomizer stays gated without opt-in");
            gSaveContext.ship.quest.id = quest;
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Skills"), 1);
            auto before = gARPGSave.xp;
            GameInteractor_ExecuteOnEnemyDefeat(&target);
            auto after = gARPGSave.xp;
            GameInteractor_ExecuteOnEnemyDefeat(&target);
            Check(after > before && gARPGSave.xp == after, "Defeat hooks grant XP once per actor");
            ObjectExtension::GetInstance().Free(&target);
            target.id = ACTOR_BOSS_GOMA;
            target.category = ACTORCAT_BOSS;
            auto activePoints = gARPGSave.activePoints;
            auto milestone = gARPGSave.level / 2;
            GameInteractor_ExecuteOnBossDefeat(&target);
            Check(gARPGSave.activePoints == activePoints + 2 + gARPGSave.level / 2 - milestone, "A first boss victory grants two technique points plus level milestones");
            ObjectExtension::GetInstance().Free(&target);
            activePoints = gARPGSave.activePoints; milestone = gARPGSave.level / 2;
            GameInteractor_ExecuteOnBossDefeat(&target);
            Check(gARPGSave.activePoints == activePoints + gARPGSave.level / 2 - milestone, "Repeated victories cannot farm the distinct-boss technique reward");
            ObjectExtension::GetInstance().Free(&target);
            phase = 2;
            frames = 0;
            return;
        }
        if (phase == 2 && frames > 100 && GameInteractor::IsPlayerInControl()) {
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Items"), 1);
            for (int mod = ARPG_MOD_BRUTAL; mod <= ARPG_MOD_ARCHERY; ++mod) {
                for (int rarity = ARPG_RARITY_RARE; rarity <= ARPG_RARITY_UNIQUE; ++rarity) {
                    ItemModifierSystem::SetPowerRange(mod, rarity, { 17, 17 });
                }
            }
            bool validRolls = true;
            for (int sample = 0; sample < 100; ++sample) {
                const auto item = ItemModifierSystem::RollItemDrop(ARPG_TIER_BOSS, ARPG_SLOT_SWORD);
                validRolls &= ARPG::InventoryRules::Valid(item) && item.modCount >= 2;
                const bool catalog=ARPG::Loot::Unique(item)||ARPG::Loot::Set(item);
                validRolls &= item.rarity!=ARPG_RARITY_UNIQUE || catalog;
                for (int m = 0; m < item.modCount; ++m) {
                    validRolls &= catalog || item.mods[m].power == ARPG::Enemies::LootPower(17, item.itemLevel);
                    for (int n = 0; n < m; ++n) { validRolls &= item.mods[m].modId != item.mods[n].modId; }
                }
            }
            Check(validRolls, "Production rare rolls honor tuned ranges; uniques and sets resolve catalog powers with distinct modifiers");
            for (int mod = ARPG_MOD_BRUTAL; mod <= ARPG_MOD_ARCHERY; ++mod) {
                for (int rarity = ARPG_RARITY_RARE; rarity <= ARPG_RARITY_UNIQUE; ++rarity) {
                    ItemModifierSystem::SetPowerRange(mod, rarity, ItemModifierSystem::GetPowerRange(mod, rarity, true));
                }
            }
            {
                const auto original=gARPGSave;
                for(auto& item:gARPGSave.equipment) { item={}; }
                ARPGItemInstance base{}; base.slot=ARPG_SLOT_SWORD; base.baseEquip=CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD); base.itemLevel=35; base.socketCount=2;
                gARPGSave.backpack[47]=base; gARPGSave.materials[19]=gARPGSave.materials[20]=1;
                Check(ARPG::Crafting::Socket(gARPGSave,47,19) && ARPG::Crafting::Socket(gARPGSave,47,20),"Native character consumes ordered runes exactly once");
                Check(ARPG::Inventory::Equip(47) && ARPG::Loot::Runework(gARPGSave.equipment[0])==1,"Common sword awakens Kindling and equips through the native inventory");
                Check(ItemModifierSystem::GetModPower(ARPG_MOD_FLAME)==31 && ItemModifierSystem::GetStyleBonuses().power[ARPG_TREE_SWORD]==15,"Runework and socket bonuses reach live combat queries");
                for(int slot=0;slot<4;++slot) {
                    base={};base.slot=slot;base.baseEquip=CUR_EQUIP_VALUE(slot);base.itemLevel=35;
                    ARPG::Loot::IdentifySet(base,1);gARPGSave.equipment[slot]=base;
                }
                Check(ARPG::Loot::SetPieces(gARPGSave,1,ItemModifierSystem::EquippedMask())==4 && ItemModifierSystem::GetStyleBonuses().recovery[ARPG_TREE_GUARDIAN]==20,"Four active Briarwatch pieces grant the full Guardian style");
                gARPGSave.equipment[3]={};
                Check(ItemModifierSystem::GetStyleBonuses().recovery[ARPG_TREE_GUARDIAN]==0 && ItemModifierSystem::GetModPower(ARPG_MOD_AEGIS)==8,"Breaking the set removes four-piece effects but keeps the two-piece bonus");
                base={};base.slot=0;base.baseEquip=CUR_EQUIP_VALUE(0);base.itemLevel=35;ARPG::Loot::IdentifyUnique(base,2);gARPGSave.equipment[0]=base;
                gSaveContext.magic=20;gSaveContext.magicState=MAGIC_STATE_IDLE;gARPGSave.skillRanks[ARPG_SKILL_MAGIC_CHANNEL]=0;
                SkillSystem::ResetRuntime();
                SkillSystem::OnMeleeHit(&GET_PLAYER(gPlayState)->actor,&target,DMG_SWORD,1);
                Check(gSaveContext.magic==22,"Moonwell Fang restores magic on a valid basic sword hit");
                SkillSystem::OnMeleeHit(&GET_PLAYER(gPlayState)->actor,&target,DMG_SWORD,1);
                Check(gSaveContext.magic==22,"Repeated callbacks in the same hit window cannot duplicate magic recovery");
                base={};base.slot=1;base.baseEquip=CUR_EQUIP_VALUE(1);base.itemLevel=35;ARPG::Loot::IdentifyUnique(base,3);gARPGSave.equipment[1]=base;
                gARPGSave.skillRanks[ARPG_SKILL_GUARD_RIPOSTE]=1;
                SkillSystem::ResetRuntime();
                const int recharge=std::lround(ARPG::SkillRules::Cooldown(gARPGSave,ARPG_SKILL_GUARD_RIPOSTE)*.85f);
                Check(SkillSystem::TryCastActive(gPlayState,ARPG_SKILL_GUARD_RIPOSTE) && SkillSystem::CooldownTotal(ARPG_SKILL_GUARD_RIPOSTE)==recharge,"Unique recharge reduction changes the actual skill timer");
                SkillSystem::ResetRuntime();
                CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Items"),0);
                Check(ItemModifierSystem::GetStyleBonuses().manaOnHit==0 && ItemModifierSystem::GetStyleBonuses().recovery[2]==0,"Items-off removes catalog style effects");
                CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Items"),1);
                gARPGSave=original;
            }
            gARPGSave.equipment[ARPG_SLOT_SWORD] = { ARPG_RARITY_RARE, ARPG_SLOT_SWORD,
                static_cast<uint8_t>(CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD)), 2,
                { { ARPG_MOD_BRUTAL, 30 }, { ARPG_MOD_KEEN, 10 }, {} } };
            Check(ItemModifierSystem::GetMeleeDamageMult() == 1.3f, "Equipped base item applies its damage modifier");
            auto improved = gARPGSave.equipment[0];
            improved.mods[0].power = 45;
            int bagIndex = ARPG::InventoryRules::FreeSlot(gARPGSave);
            Check(ItemModifierSystem::GiveItemDrop(improved), "Loot collection adds a backpack item");
            Check(ItemModifierSystem::GetMeleeDamageMult() == 1.3f, "Collecting loot preserves equipped modifiers");
            Check(ARPG::Inventory::Equip(bagIndex), "Inventory equip accepts an owned age-compatible item");
            Check(ItemModifierSystem::GetMeleeDamageMult() == 1.45f && gARPGSave.backpack[bagIndex].mods[0].power == 30,
                  "Inventory swap updates combat effects and retains the previous roll");
            Check(CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == improved.baseEquip && gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER,
                  "Inventory equip updates native sword and B-button item");
            auto childSword = improved;
            childSword.baseEquip = EQUIP_VALUE_SWORD_KOKIRI;
            Check(!ARPG::Inventory::EquipRestriction(childSword).empty(), "Adult Link cannot equip child-only loot");
            Check(ARPG::Inventory::Unequip(0), "Equipped modifiers can return to an empty backpack slot");
            Check(ItemModifierSystem::GetMeleeDamageMult() == 1.0f, "Unequipping removes the modifier from combat queries");
            Check(ARPG::Inventory::Equip(bagIndex) && ItemModifierSystem::GetMeleeDamageMult() == 1.3f,
                  "Backpack gear can be equipped again without losing either roll");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Items"), 0);
            Check(ItemModifierSystem::GetMeleeDamageMult() == 1.0f, "Items-off removes saved gear effects");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Items"), 1);
            SkillSystem::ResetRuntime();
            gSaveContext.magicState = MAGIC_STATE_IDLE;
            gSaveContext.magic = 96;
            gARPGSave.skillRanks[ARPG_SKILL_MAGIC_FLAME_BURST] = 1;
            Check(SkillSystem::TryCastActive(gPlayState, ARPG_SKILL_MAGIC_FLAME_BURST), "Flame Burst spawns an independent native fire collider");
            phase = 3;
            frames = 0;
            return;
        }
        if (phase == 3 && frames > 180 && GameInteractor::IsPlayerInControl()) {
            SkillSystem::ResetRuntime();
            gSaveContext.magicState = MAGIC_STATE_IDLE;
            gSaveContext.magic = 96;
            gARPGSave.skillRanks[ARPG_SKILL_MAGIC_HOLY_AURA] = 1;
            Check(SkillSystem::TryCastActive(gPlayState, ARPG_SKILL_MAGIC_HOLY_AURA), "Holy Aura spawns native Nayru's Love");
            phase = 4;
            frames = 0;
            return;
        }
        if (phase == 4 && frames > 80 && GameInteractor::IsPlayerInControl()) {
            Check(SkillSystem::IsAuraActive(), "Holy Aura remains active across gameplay updates");
            SkillSystem::ResetRuntime();
            gSaveContext.magicState = MAGIC_STATE_IDLE;
            gSaveContext.magic = 96;
            gSaveContext.health = gSaveContext.healthCapacity;
            gARPGSave.skillRanks[ARPG_SKILL_SWORD_BEAM] = 1;
            Check(SkillSystem::TryCastActive(gPlayState, ARPG_SKILL_SWORD_BEAM), "Sword Beam spawns a traveling collider");
            EnMThunder* beam = nullptr;
            for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_ITEMACTION].head; actor != nullptr; actor = actor->next) {
                if (actor->id == ACTOR_EN_M_THUNDER && actor->update != nullptr) { beam = reinterpret_cast<EnMThunder*>(actor); break; }
            }
            Check(beam != nullptr && (beam->collider.info.toucher.dmgFlags & DMG_SWORD), "Sword Beam carries the equipped sword's damage type");
            auto pos = GET_PLAYER(gPlayState)->actor.world.pos;
            bool nativeHits = true;
            for (int id : { ACTOR_EN_DEKUBABA, ACTOR_EN_TEST, ACTOR_EN_IK }) {
                Actor* enemy = Actor_Spawn(&gPlayState->actorCtx, gPlayState, id, pos.x + 500, pos.y, pos.z, 0, 0, 0, 0);
                Check(enemy != nullptr && enemy->update != nullptr, "Native enemy initializes for beam collision testing");
                Collider receiver{};
                receiver.actor = enemy;
                receiver.acFlags = AC_HIT;
                ColliderInfo hit{};
                hit.bumperFlags = BUMP_HIT;
                hit.acHitInfo = &beam->collider.info;
                hit.acHit = &beam->collider.base;
                enemy->colChkInfo.damage = 0;
                CollisionCheck_ApplyDamage(gPlayState, &gPlayState->colChkCtx, &receiver, &hit);
                nativeHits &= enemy->colChkInfo.damage > 0;
                Actor_Kill(enemy);
            }
            Check(nativeHits, "Sword Beam damages native Deku Baba, Stalfos, and Iron Knuckle damage tables");
            phase = 5;
            frames = 0;
            return;
        }
        if (phase == 5 && frames > 100 && GameInteractor::IsPlayerInControl()) {
            ARPGItemInstance craft{};craft.slot=0;craft.baseEquip=2;craft.itemLevel=35;craft.socketCount=3;
            craft.sockets[0]=19;craft.sockets[1]=23;craft.sockets[2]=20;
            gARPGSave.backpack[43]=craft;
            ARPG::Loot::IdentifyUnique(craft,1);craft.socketCount=1;craft.sockets[0]=13;craft.sockets[1]=craft.sockets[2]=0;gARPGSave.backpack[44]=craft;
            ARPG::Loot::IdentifySet(craft,2);gARPGSave.backpack[45]=craft;
            gARPGSave.materials[13]=3;gARPGSave.materials[19]=4;gARPGSave.materials[25]=50;
            gSaveContext.fileNum = 0;
            const auto expected = ARPG::SaveCodec::Encode(gARPGSave);
            SaveManager::Instance->SaveFile(0);
            gARPGSave.xp = 0;
            gARPGSave.equipment[0] = {};
            for (auto& item : gARPGSave.backpack) { item = {}; }
            SaveManager::Instance->ThreadPoolWait();
            SaveManager::Instance->LoadFile(0);
            Check(ARPG::SaveCodec::Encode(gARPGSave) == expected, "Asynchronous SaveManager snapshot survives mutation and reload, including uniques, sets, sockets and material counts");
            Check(ARPG::Loot::Runework(gARPGSave.backpack[43])==5 && gARPGSave.backpack[44].uniqueId==1 && gARPGSave.backpack[45].setId==2,"Saved item identities and ordered runeworks survive native reload");
            auto saved = gSaveContext;
            SaveManager::Instance->InitFile(false);
            Check(gARPGSave.level == 1 && gARPGSave.xp == 0 && gARPGSave.equipment[0].modCount == 0 && ARPG::InventoryRules::Count(gARPGSave) == 0,
                  "Switching to a new save clears prior ARPG progress");
            gSaveContext = saved;
            ARPG::OnFileLoaded();
            target.category = ACTORCAT_BOSS;
            auto item = ItemModifierSystem::RollItemDrop(ARPG_TIER_BOSS);
            itemsBefore = gARPGSave.itemsFound;
            bagBefore = ARPG::InventoryRules::Count(gARPGSave);
            ItemModifierSystem::SpawnItemDrop(&target, item);
            for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_MISC].head; actor != nullptr; actor = actor->next) {
                if (actor->id == ACTOR_EN_ITEM00) { groundLoot = actor; break; }
            }
            Check(groundLoot != nullptr && gARPGSave.itemsFound == itemsBefore, "Boss gear exists as a collectible before it is owned");
            groundLoot->world.pos.x += 150.0f;
            materialsBefore=gARPGSave.materials[13];
            ItemModifierSystem::SpawnMaterialDrop(&target,13);
            for(Actor* actor=gPlayState->actorCtx.actorLists[ACTORCAT_MISC].head;actor;actor=actor->next) { if(actor->id==ACTOR_EN_ITEM00 && actor!=groundLoot) { materialLoot=actor;break; } }
            Check(materialLoot!=nullptr && gARPGSave.materials[13]==materialsBefore,"A gem spawns physically before entering the saved material satchel");
            materialLoot->world.pos.x+=200.f;
            phase = 6;
            frames = 0;
            return;
        }
        if (phase == 6 && frames > 30 && GameInteractor::IsPlayerInControl()) {
            bool present = false;
            for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_MISC].head; actor != nullptr; actor = actor->next) {
                if (actor == groundLoot) { present = true; break; }
            }
            Check(present, "Named ground gear survives actor updates and rendering");
            Check(materialLoot && materialLoot->update && materialLoot->draw,"Gem collectible survives native updates and drawing");
            gARPGSave.materials[13]=999;
            Check(!GameInteractor_Should(VB_GIVE_ITEM_FROM_ITEM_00,true,reinterpret_cast<EnItem00*>(materialLoot)) && materialLoot->update,"A full material stack keeps the gem on the ground");
            gARPGSave.materials[13]=materialsBefore;
            Check(!GameInteractor_Should(VB_GIVE_ITEM_FROM_ITEM_00,true,reinterpret_cast<EnItem00*>(materialLoot)) && !materialLoot->update && gARPGSave.materials[13]==materialsBefore+1,"Picking up a gem consumes the collectible and grants one saved ingredient");
            auto savedInventory = gARPGSave;
            auto filler = ItemModifierSystem::RollItemDrop(ARPG_TIER_BOSS, 0);
            filler.baseEquip = CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
            while (ARPG::InventoryRules::FreeSlot(gARPGSave) >= 0) { ARPG::InventoryRules::Collect(gARPGSave, filler); }
            auto fullCount = gARPGSave.itemsFound;
            Check(!GameInteractor_Should(VB_GIVE_ITEM_FROM_ITEM_00, true, reinterpret_cast<EnItem00*>(groundLoot)),
                  "Full backpack suppresses vanilla collectible consumption");
            Check(groundLoot->update != nullptr && gARPGSave.itemsFound == fullCount,
                  "Full backpack leaves ground loot alive without awarding an item");
            Check(ARPG::Inventory::Equip(0) && ARPG::InventoryRules::Count(gARPGSave) == ARPG_BACKPACK_CAPACITY,
                  "Native inventory swap succeeds with all backpack slots occupied");
            gARPGSave = savedInventory;
            groundLoot->world.pos = GET_PLAYER(gPlayState)->actor.world.pos;
            groundLoot->velocity = {};
            groundLoot->speedXZ = 0;
            groundLoot->gravity = 0;
            auto* collectible = reinterpret_cast<EnItem00*>(groundLoot);
            collectible->unk_154 = -1;
            phase = 7;
            frames = 0;
            return;
        }
        if (phase == 7 && frames > 10 && GameInteractor::IsPlayerInControl()) {
            Check(gARPGSave.itemsFound == itemsBefore + 1 && ARPG::InventoryRules::Count(gARPGSave) == bagBefore + 1,
                  "Native collectible proximity adds loot to the backpack exactly once");
            auto* player = GET_PLAYER(gPlayState);
            for (auto& item : gARPGSave.equipment) { item = {}; }
            gARPGSave.equipment[ARPG_SLOT_TUNIC] = { ARPG_RARITY_RARE, ARPG_SLOT_TUNIC,
                static_cast<uint8_t>(CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC)), 1, { { ARPG_MOD_VITALITY, 20 }, {}, {} } };
            gSaveContext.healthCapacity = 10 * FULL_HEART_HEALTH;
            gARPGSave.vitalityBonus = 0;
            GameInteractor_ExecuteOnPlayerUpdate();
            Check(gSaveContext.healthCapacity == 12 * FULL_HEART_HEALTH, "Vitality adds temporary capacity");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Items"), 0);
            GameInteractor_ExecuteOnPlayerUpdate();
            Check(gSaveContext.healthCapacity == 10 * FULL_HEART_HEALTH && gARPGSave.vitalityBonus == 0,
                  "Disabling gear restores earned heart capacity");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Items"), 1);
            gARPGSave.skillRanks[ARPG_SKILL_MAGIC_RECALL] = 1;
            SkillSystem::ResetRuntime();
            gSaveContext.magicState = MAGIC_STATE_IDLE;
            gSaveContext.magic = 96;
            gSaveContext.fw.set = 0;
            Check(!SkillSystem::TryCastActive(gPlayState, ARPG_SKILL_MAGIC_RECALL), "Wind Recall rejects a missing anchor without spending magic");
            gSaveContext.fw.set = 1;
            gSaveContext.fw.entranceIndex = gSaveContext.entranceIndex;
            gSaveContext.fw.roomIndex = gPlayState->roomCtx.curRoom.num;
            gSaveContext.fw.pos.x = player->actor.world.pos.x + 20;
            gSaveContext.fw.pos.y = player->actor.world.pos.y;
            gSaveContext.fw.pos.z = player->actor.world.pos.z;
            gSaveContext.fw.yaw = player->actor.shape.rot.y;
            Check(SkillSystem::TryCastActive(gPlayState, ARPG_SKILL_MAGIC_RECALL), "Wind Recall returns to the existing same-room anchor");
            Check(std::abs(player->actor.world.pos.x - gSaveContext.fw.pos.x) < 0.1f, "Recall moves the player to the saved position");
            phase = 8;
            frames = 0;
            return;
        }
        if (phase == 8 && frames > 100 && GameInteractor::IsPlayerInControl()) {
            gPlayState->linkAgeOnLoad = LINK_AGE_CHILD;
            gPlayState->nextEntranceIndex = ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;
            gPlayState->transitionTrigger = TRANS_TRIGGER_START;
            gPlayState->transitionType = TRANS_TYPE_INSTANT;
            phase = 9;
            frames = 0;
            return;
        }
        if (phase == 9 && frames > 100 && GameInteractor::IsPlayerInControl()) {
            Check(gSaveContext.linkAge == 1 && GET_PLAYER(gPlayState)->ageProperties != nullptr, "Child timeline loads after the adult spell checks");
            SkillSystem::ResetRuntime();
            gSaveContext.magicState = MAGIC_STATE_IDLE;
            gSaveContext.magic = 96;
            Check(SkillSystem::TryCastActive(gPlayState, ARPG_SKILL_MAGIC_FROST_NOVA), "Child Link can cast a native frost pulse");
            phase = 10;
            frames = 0;
            return;
        }
        if (phase == 10 && frames > 100 && GameInteractor::IsPlayerInControl()) {
            using namespace ARPG::Enemies;
            Check(CurrentRegion().minLevel == 4, "Child Hyrule Field uses its fixed low-level region band");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"), 1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.EnemyCap"), 12);
            initialPack = SpawnPack(PackKind::Champions, true);
            Check(initialPack >= 3 && initialPack <= 8, "Native spawn director creates a bounded champion encounter on valid ground");
            int champions = 0;
            bool valid = true;
            for (Actor* a = gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head; a != nullptr; a = a->next) {
                const auto* data = AffixSystem::GetAffixData(a);
                if (data == nullptr || !data->generated || !a->update) { continue; }
                champions += data->tier == ARPG_TIER_CHAMPION;
                valid &= data->level >= 4 && data->level <= 8 && data->hpMult >= 256 && data->packId != 0;
                valid &= a->parent == nullptr && a->colChkInfo.health == GetActorMaximumHealth(a);
                packActors.push_back(a); packPositions.push_back(a->world.pos);
            }
            Check(champions == 3 && valid, "Champion pack has three champions, leveled guards, composed HP, and safe ownership");
            phase = 11; frames = 0; return;
        }
        if (phase == 11 && frames > 220 && GameInteractor::IsPlayerInControl()) {
            bool moved = false;
            for (size_t i = 0; i < packActors.size(); ++i) {
                moved |= Math_Vec3f_DistXZ(&packActors[i]->world.pos, &packPositions[i]) > 60;
                combatAttacks += ARPG::Enemies::AttackCount(packActors[i]);
            }
            Check(moved, "Real pack actors close distance and flank through native physics");
            Check(combatAttacks > 0, "Coordinated AI commits native melee attacks during live gameplay");
            ARPG::Enemies::ClearPacks();
            Check(ARPG::Enemies::LiveCount() == 0, "Clearing generated packs immediately removes their active population");
            phase = 12; frames = 0; return;
        }
        if (phase == 12 && frames > 20 && GameInteractor::IsPlayerInControl()) {
            using namespace ARPG::Enemies;
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.EnemyCap"), 4);
            int spawned = SpawnPack(PackKind::EliteGuard, true);
            Check(spawned >= 3, "Elite encounter spawns after previous actors are destroyed");
            int extra = SpawnPack(PackKind::Warband, true);
            Check(LiveCount() <= 4 && extra == 0, "Live population cap rejects additional encounters");
            Actor* elite = nullptr;
            for (Actor* a = gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head; a != nullptr; a = a->next) {
                const auto* data = AffixSystem::GetAffixData(a);
                if (data != nullptr && data->generated && data->tier == ARPG_TIER_ELITE) { elite = a; break; }
            }
            Check(elite != nullptr, "Elite pack has a distinct leader");
            const auto* data = AffixSystem::GetAffixData(elite);
            auto gear = ItemModifierSystem::RollItemDrop(data->tier, 0, data->level);
            Check(gear.rarity >= ARPG_RARITY_RARE && gear.itemLevel == data->level, "Elite rewards guarantee rare-or-better gear at the enemy level");
            auto beforeXp = gARPGSave.xp;
            int beforeLoot = gPlayState->actorCtx.actorLists[ACTORCAT_MISC].length;
            GameInteractor_ExecuteOnEnemyDefeat(elite);
            Check(gARPGSave.xp > beforeXp, "Leveled elite defeat awards scaled XP");
            Check(gPlayState->actorCtx.actorLists[ACTORCAT_MISC].length > beforeLoot, "Elite defeat spawns guaranteed physical loot");
            Check(ARPG::SaveCodec::DecodeItem(ARPG::SaveCodec::EncodeItem(gear), 0).itemLevel == gear.itemLevel, "Enemy item level survives the production save codec");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"), 0);
            GameInteractor_ExecuteOnPlayerUpdate();
            Check(LiveCount() == 0, "Disabling packs cleans generated enemies without rewards");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Tactics"), 0);
            Check(ARPG_EnemyTactics(&target, gPlayState) == 0, "Tactics-off returns control to native AI");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Tactics"), 1);
            gPlayState->linkAgeOnLoad = LINK_AGE_ADULT;
            gPlayState->nextEntranceIndex = ENTR_DESERT_COLOSSUS_EAST_EXIT;
            gPlayState->transitionTrigger = TRANS_TRIGGER_START;
            gPlayState->transitionType = TRANS_TYPE_INSTANT;
            phase = 13; frames = 0; return;
        }
        if (phase == 13 && frames > 100 && GameInteractor::IsPlayerInControl()) {
            Check(ARPG::Enemies::CurrentRegion().minLevel == 34 && ARPG::Enemies::LiveCount() == 0,
                  "Adult desert region loads its higher level band and clears previous encounters");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"), 1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.EnemyCap"), 12);
            auto* regionPlayer = GET_PLAYER(gPlayState);
            SPDLOG_INFO("[ARPG QA] Desert placement: x={} y={} z={} floor={} type={} clear={} enemies={} actors={}",
                regionPlayer->actor.world.pos.x, regionPlayer->actor.world.pos.y, regionPlayer->actor.world.pos.z,
                regionPlayer->actor.floorHeight, SurfaceType_GetFloorType(&gPlayState->colCtx, regionPlayer->actor.floorPoly, regionPlayer->actor.floorBgId),
                Flags_GetClear(gPlayState, gPlayState->roomCtx.curRoom.num), gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].length, gPlayState->actorCtx.total);
            Check(ARPG::Enemies::SpawnPack(ARPG::Enemies::PackKind::EliteGuard, true) >= 3,
                  "High-level desert encounter spawns a mixed Dinolfos and Stalfos pack");
            phase = 14; frames = 0; return;
        }
        if (phase == 14 && frames > 250 && GameInteractor::IsPlayerInControl()) {
            uint32_t attacks = 0;
            for (Actor* a = gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head; a != nullptr; a = a->next) {
                attacks += ARPG::Enemies::AttackCount(a);
            }
            Check(attacks > 0, "Adult desert melee families execute coordinated native attacks");
            ARPG::Enemies::ClearPacks();
            gPlayState->linkAgeOnLoad = LINK_AGE_CHILD;
            gPlayState->nextEntranceIndex = ENTR_DEKU_TREE_ENTRANCE;
            gPlayState->transitionTrigger = TRANS_TRIGGER_START;
            gPlayState->transitionType = TRANS_TYPE_INSTANT;
            phase = 15; frames = 0; return;
        }
        if (phase == 15 && frames > 350 && GameInteractor::IsPlayerInControl()) {
            Check(ARPG::Enemies::CurrentRegion().population == ARPG::Enemies::Population::Dungeon,
                  "Dungeon entrance uses the combat-room population policy");
            Check(ARPG::Enemies::LiveCount() >= 2, "Native dungeon enemy group receives additional regional pack members");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"), 0);
            GameInteractor_ExecuteOnPlayerUpdate();
            Check(ARPG::Enemies::LiveCount() == 0, "Master-off removes dungeon reinforcements");
            Result("passed"); return;
        }
        if (frames > 1500) { throw std::runtime_error("Timed out waiting for gameplay phase " + std::to_string(phase)); }
    } catch (const std::exception& exception) {
        SPDLOG_ERROR("[ARPG QA] {}", exception.what());
        Result("failed", exception.what());
    }
}

// [TIME] | [INFO] | ARPG.RuntimeTests | Never run against ordinary user saves; both opt-in environment and isolated home are required.
void RegisterRuntimeTests() {
    const char* enabled = std::getenv("SOH_ARPG_QA");
    const char* home = std::getenv("SHIP_HOME");
    if (enabled == nullptr || std::string(enabled) != "1" || home == nullptr ||
        !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-")) { return; }
    resultPath = std::filesystem::path(home) / "runtime-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(RunChecks);
}
static RegisterShipInitFunc runtimeTests(RegisterRuntimeTests);
}
