#pragma once

// [TIME] | [INFO] | AffixData.h | Per-actor affix state attached via ObjectExtension

#include <stdint.h>

typedef enum {
    /* 0x00 */ ARPG_TIER_NORMAL = 0,
    /* 0x01 */ ARPG_TIER_CHAMPION,
    /* 0x02 */ ARPG_TIER_ELITE,
    /* 0x03 */ ARPG_TIER_BOSS, // reserved for boss-category actors
    /* 0x04 */ ARPG_TIER_COUNT_
} ARPGAffixTier;

// Elemental / attack-type groups used for resist/vuln rolls. These map onto
// the engine's DMG_* aggregate masks (z64collision_check.h).
typedef enum {
    /* 0x00 */ ARPG_ELEM_SWORD = 0,   // DMG_SWORD (slash/spin/jump)
    /* 0x01 */ ARPG_ELEM_HAMMER,      // DMG_HAMMER
    /* 0x02 */ ARPG_ELEM_RANGED,      // DMG_RANGED (arrows/slingshot/hookshot)
    /* 0x03 */ ARPG_ELEM_FIRE,        // DMG_FIRE
    /* 0x04 */ ARPG_ELEM_ICE,         // DMG_ARROW_ICE | DMG_MAGIC_ICE
    /* 0x05 */ ARPG_ELEM_LIGHT,       // DMG_ARROW_LIGHT | DMG_MAGIC_LIGHT
    /* 0x06 */ ARPG_ELEM_MAGIC,       // DMG_MAGIC_FIRE | other magic
    /* 0x07 */ ARPG_ELEM_EXPLOSIVE,   // DMG_EXPLOSIVE
    /* 0x08 */ ARPG_ELEM_COUNT_
} ARPGElementGroup;

#define ARPG_AFFIX_SLOTS 3

// Rolled affix record attached to an actor
typedef struct {
    uint8_t tier;          // ARPGAffixTier
    uint8_t level;
    uint16_t packId;
    uint8_t role;
    uint8_t packMember;
    uint8_t generated;
    // x256 fixed-point multipliers (256 = 1.0x) so state stays POD/simple
    float appliedHpMult; // [TIME] | [INFO] | AffixData | Actual ratio after the u8 HP cap, for removing the enhancement.
    uint16_t hpMult;       // applied at init to colChkInfo.health
    uint16_t dmgMult;      // applied to outgoing AT damage
    uint16_t speedMult;    // applied per-update to speedXZ (curated actors)
    // per-element-group modifiers: 256 = neutral; >256 resistant; 65535 immune
    uint16_t elemMult[ARPG_ELEM_COUNT_];
    // rolled affix name indices for the nametag
    uint8_t affixIds[ARPG_AFFIX_SLOTS];
    uint8_t affixCount;
    // loot budget: 0 = normal drops, higher = better chance/more items
    uint8_t lootBudget;
} ARPGAffixData;
