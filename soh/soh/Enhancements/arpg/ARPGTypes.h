#pragma once

// [TIME] | [INFO] | ARPGSave.h | Persistent ARPG state stored in the SOH save file
//
// One section ("arpg") holds all ARPG data. The in-RAM mirror lives in
// gARPGSave; SaveManager serializes it to/from JSON with versioned load
// handlers so future format changes stay backward compatible.

#include <stdint.h>

// ---- Item modifier system data ------------------------------------------

#define ARPG_ITEM_MOD_COUNT 3 // prefix / suffix / extra (rare tier bonus)
#define ARPG_ITEM_SOCKET_COUNT 3
#define ARPG_MATERIAL_COUNT 26
#define ARPG_MATERIAL_CAPACITY 999

typedef enum {
    /* 0x00 */ ARPG_MOD_NONE = 0,
    // offensive
    /* 0x01 */ ARPG_MOD_BRUTAL,     // +% melee damage
    /* 0x02 */ ARPG_MOD_KEEN,       // +crit chance
    /* 0x03 */ ARPG_MOD_FLAME,      // chance to apply fire on hit
    /* 0x04 */ ARPG_MOD_FROST,      // chance to apply ice on hit
    /* 0x05 */ ARPG_MOD_STORM,      // chance to apply electric on hit
    /* 0x06 */ ARPG_MOD_ARCHERY,    // +% arrow damage
    // defensive
    /* 0x07 */ ARPG_MOD_AEGIS,      // flat damage reduction (player)
    /* 0x08 */ ARPG_MOD_VITALITY,   // +% max hearts while equipped
    /* 0x09 */ ARPG_MOD_SWIFT,      // +% movement speed
    /* 0x0A */ ARPG_MOD_LUCKY,      // +% loot chance
    /* 0x0B */ ARPG_MOD_MAGICAFF,   // -% magic costs
    /* 0x0C */ ARPG_MOD_BLAST,     // +% bomb or nut radius
    /* 0x0D */ ARPG_MOD_REACH,     // +% hookshot or boomerang travel
    /* 0x0E */ ARPG_MOD_CONTROL,   // +% arsenal setup duration
    /* 0x0F */ ARPG_MOD_FOLLOWUP,  // +% setup finisher damage
    /* 0x10 */ ARPG_MOD_COUNT_
} ARPGItemModId;

// Equipment slot a modifier instance is attached to
typedef enum {
    /* 0x00 */ ARPG_SLOT_SWORD = 0,
    /* 0x01 */ ARPG_SLOT_SHIELD,
    /* 0x02 */ ARPG_SLOT_TUNIC,
    /* 0x03 */ ARPG_SLOT_BOOTS,
    /* 0x04 */ ARPG_SLOT_COUNT_
} ARPGEquipSlot;

// Native equipment remains four slots. Loot weapons use a separate append-only range.
enum { ARPG_SLOT_BOW = 4, ARPG_SLOT_HAMMER = 5, ARPG_SLOT_BOMBS = 6, ARPG_SLOT_SLINGSHOT = 7,
       ARPG_SLOT_HOOKSHOT = 8, ARPG_SLOT_BOOMERANG = 9, ARPG_SLOT_NUTS = 10,
       ARPG_SLOT_BOMBCHU = 11, ARPG_SLOT_STICK = 12, ARPG_SLOT_LENS = 13,
       ARPG_SLOT_OCARINA = 14, ARPG_SLOT_BOTTLE = 15,
       ARPG_LOOT_SLOT_COUNT = 16, ARPG_WEAPON_SLOT_COUNT = 12 };

typedef enum {
    /* 0x00 */ ARPG_RARITY_COMMON = 0,
    /* 0x01 */ ARPG_RARITY_MAGIC,
    /* 0x02 */ ARPG_RARITY_RARE,
    /* 0x03 */ ARPG_RARITY_UNIQUE,
    /* 0x04 */ ARPG_RARITY_COUNT_
} ARPGItemRarity;

typedef struct {
    uint8_t modId;   // ARPGItemModId
    int16_t power;   // rolled magnitude (meaning depends on modId)
} ARPGItemMod;

typedef struct {
    uint8_t rarity;      // ARPGItemRarity
    uint8_t slot;        // ARPGEquipSlot this instance was rolled for
    uint8_t baseEquip;   // [TIME] | [INFO] | ARPGItemInstance | equipment value 1..3; 0 migrates slot-wide v1 gear
    uint8_t modCount;    // number of valid entries in mods[]
    ARPGItemMod mods[ARPG_ITEM_MOD_COUNT];
    uint8_t itemLevel; // [TIME] | [INFO] | ARPGItemInstance.Level | Zero preserves legacy rolls; new drops record their source enemy level.
    // [TIME] | [INFO] | ARPGItemInstance.Crafting | Append-only catalog IDs and ordered sockets travel with the item.
    uint8_t uniqueId;
    uint8_t setId;
    uint8_t socketCount;
    uint8_t sockets[ARPG_ITEM_SOCKET_COUNT];
} ARPGItemInstance;

// ---- Skill system data ---------------------------------------------------

typedef enum {
    /* 0x00 */ ARPG_TREE_SWORD = 0,
    /* 0x01 */ ARPG_TREE_MAGIC,
    /* 0x02 */ ARPG_TREE_GUARDIAN,
    /* 0x03 */ ARPG_TREE_COUNT_
} ARPGSkillTree;

typedef enum {
    // Sword tree
    /* 0x00 */ ARPG_SKILL_SWORD_CRIT = 0,
    /* 0x01 */ ARPG_SKILL_SWORD_COMBO,
    /* 0x02 */ ARPG_SKILL_SWORD_SHOCKWAVE,
    /* 0x03 */ ARPG_SKILL_SWORD_QUICKSPIN,
    /* 0x04 */ ARPG_SKILL_SWORD_BEAM,
    /* 0x05 */ ARPG_SKILL_SWORD_MASTERY,
    // Magic tree
    /* 0x06 */ ARPG_SKILL_MAGIC_FLAME_BURST,
    /* 0x07 */ ARPG_SKILL_MAGIC_FROST_NOVA,
    /* 0x08 */ ARPG_SKILL_MAGIC_HOLY_AURA,
    /* 0x09 */ ARPG_SKILL_MAGIC_INFUSE,
    /* 0x0A */ ARPG_SKILL_MAGIC_ECONOMY,
    /* 0x0B */ ARPG_SKILL_MAGIC_RECOVERY,
    // Guardian tree
    /* 0x0C */ ARPG_SKILL_GUARD_TOUGHNESS,
    /* 0x0D */ ARPG_SKILL_GUARD_BULWARK,
    /* 0x0E */ ARPG_SKILL_GUARD_VENGEANCE,
    /* 0x0F */ ARPG_SKILL_GUARD_BLOODSTONE,
    /* 0x10 */ ARPG_SKILL_GUARD_PROFIT,
    /* 0x11 */ ARPG_SKILL_GUARD_FORTUNE,
    /* 0x12 */ ARPG_SKILL_SWORD_RESOLVE,
    /* 0x13 */ ARPG_SKILL_MAGIC_RECALL,
    /* 0x14 */ ARPG_SKILL_GUARD_DODGE,
    // [TIME] | [INFO] | ARPGSkillId | Append-only IDs preserve all earlier characters and learned ranks.
    ARPG_SKILL_SWORD_CLEAVE,
    ARPG_SKILL_SWORD_CROSSCUT,
    ARPG_SKILL_SWORD_CYCLONE,
    ARPG_SKILL_SWORD_LUNGE,
    ARPG_SKILL_SWORD_BLADESTORM,
    ARPG_SKILL_SWORD_EXECUTE,
    ARPG_SKILL_MAGIC_CHAIN,
    ARPG_SKILL_MAGIC_METEOR,
    ARPG_SKILL_GUARD_WARCRY,
    ARPG_SKILL_GUARD_RIPOSTE,
    ARPG_SKILL_SWORD_TEMPO,
    ARPG_SKILL_SWORD_FINESSE,
    ARPG_SKILL_MAGIC_RESONANCE,
    ARPG_SKILL_MAGIC_CHANNEL,
    ARPG_SKILL_GUARD_SECOND_WIND,
    ARPG_SKILL_GUARD_IRON_WILL,
    ARPG_SKILL_COUNT_
} ARPGSkillId;

// Skill node metadata used by the ImGui tree UI
typedef struct {
    const char* name;
    const char* desc;
    uint8_t maxRank;
    uint8_t tree;     // ARPGSkillTree
    uint8_t passive;  // 0 = active skill (needs binding/cost), 1 = passive
    uint16_t magicCost;
} ARPGSkillDef;



#define ARPG_ACTION_SLOTS 6
#define ARPG_SKILL_NONE 255
#define ARPG_MAX_LEVEL 50
#define ARPG_BACKPACK_CAPACITY 48

// [TIME] | [INFO] | ARPGExpeditionData | Snapshots contain no actor pointers; interrupted combat resumes at a fresh depth attempt.
typedef struct {
    uint32_t seed, depth, bestDepth, clears, runs;
    uint8_t phase, wave, mutator, targetSlot, baseEquip, baseLevel;
    uint8_t encounterSize, defeatedMask, objective, sealMask;
} ARPGExpeditionData;

// ---- Persistent state -----------------------------------------------------

// Append-only quest IDs. No actor pointers: save workers copy the entire campaign atomically.
#define ARPG_QUEST_COUNT 58
#define ARPG_QUEST_OBJECTIVES 3
#define ARPG_QUEST_NODES 3
typedef struct {
    float x, y, z;
    int8_t room;
    uint8_t placed, adult;
} ARPGQuestSite;
typedef struct {
    uint32_t party; // Network scope of an explicitly shared quest; zero keeps it personal.
    uint8_t partySize; // Bounty high-water mark (1..8); disconnects never shrink targets.
    uint8_t state; // 0 unaccepted, 1 active, 2 ready, 3 rewarded
    uint8_t progress[ARPG_QUEST_OBJECTIVES];
    uint8_t nodes[ARPG_QUEST_OBJECTIVES];
    ARPGQuestSite sites[ARPG_QUEST_OBJECTIVES][ARPG_QUEST_NODES];
} ARPGQuestProgress;
typedef struct {
    ARPGQuestProgress quests[ARPG_QUEST_COUNT];
    uint32_t milestones;
    uint8_t tracked; // 0 none; otherwise quest ID (1..ARPG_QUEST_COUNT)
} ARPGCampaignData;

#define ARPG_SECRET_COUNT 162
typedef struct {
    float x, y, z;
    int8_t room;
    uint8_t placed, claimed;
} ARPGSecretSite;

typedef struct {
    // identity / determinism
    uint32_t seed;
    uint32_t rngState;
    uint32_t defeatedBosses;
    uint32_t acquiredEquipment; // First native acquisitions already represented as inventory items.
    uint16_t vitalityBonus;
    // XP system
    uint32_t xp;
    uint8_t level;
    uint8_t passivePoints;  // from XP levels
    uint8_t activePoints;   // from boss defeats (hybrid progression)
    uint8_t skillRanks[ARPG_SKILL_COUNT_];
    // [TIME] | [INFO] | ARPGSaveData.Skills | Runes and action slots are part of the character snapshot.
    uint8_t skillRunes[ARPG_SKILL_COUNT_];
    uint8_t actionSlots[ARPG_ACTION_SLOTS];
    uint8_t skillRevision;
    // itemization: one modifier instance per equip slot
    ARPGItemInstance equipment[ARPG_SLOT_COUNT_];
    // [TIME] | [INFO] | ARPGSaveData.Backpack | Fixed slots preserve drag positions and keep asynchronous save snapshots self-contained.
    ARPGItemInstance backpack[ARPG_BACKPACK_CAPACITY];
    uint16_t materials[ARPG_MATERIAL_COUNT];
    // stats
    uint32_t kills;
    uint32_t eliteKills;
    uint32_t bossKills;
    uint32_t itemsFound;
    ARPGExpeditionData expedition;
    ARPGItemInstance weapons[ARPG_WEAPON_SLOT_COUNT];
    ARPGCampaignData campaign;
    ARPGSecretSite secrets[ARPG_SECRET_COUNT];
    uint8_t skullRewardTier;
} ARPGSaveData;
