#pragma once
#include <algorithm>
#include <cstdint>

namespace ARPG::Enemies {
enum class Biome : uint8_t { Safe, Woods, Plains, Mountain, Water, Desert, Crypt, Fortress };
enum class Population : uint8_t { None, Wilderness, Dungeon, Boss, Settlement };
struct Region {
    const char* name;
    uint8_t minLevel, maxLevel, minPack, maxPack;
    Biome biome;
    Population population;
};

// [TIME] | [INFO] | Enemies.RegionFor | Scene IDs come from scene_table.h; fixed bands preserve regional danger as the player levels.
inline Region RegionFor(int scene, bool adult) {
    const auto outdoor = [adult](const char* name, int child, int mature, Biome biome, int minimum = 4, int maximum = 7) {
        int level = adult ? mature : child;
        return Region{ name, uint8_t(level), uint8_t(level + 3), uint8_t(minimum), uint8_t(maximum), biome, Population::Wilderness };
    };
    const auto town = [adult](const char* name, int child, int mature, Biome biome) {
        int level = adult ? mature : child;
        return Region{name, uint8_t(level), uint8_t(level + 3), 0, 0, biome, Population::Settlement};
    };
    switch (scene) {
        case 0x1B: case 0x1C: case 0x1D: return town("Castle town gate", 5, 16, Biome::Plains);
        case 0x1E: case 0x1F: return town("Castle town alleys", 5, 16, Biome::Plains);
        case 0x20: case 0x21: case 0x22: return town("Castle town", 5, 16, Biome::Plains);
        case 0x23: case 0x24: case 0x25: return town("Temple approach", 5, 16, Biome::Plains);
        case 0x52: return town("Kakariko Village", 6, 18, Biome::Plains);
        case 0x55: return adult ? town("Kokiri Forest", 1, 12, Biome::Woods) : Region{"Kokiri Forest",1,2,0,0,Biome::Woods,Population::Settlement};
        case 0x58: return town("Zora's Domain", 8, 20, Biome::Water);
        case 0x5F: return town("Hyrule Castle", 6, 16, Biome::Plains);
        case 0x62: return town("Goron City", 10, 24, Biome::Mountain);
        case 0x63: return town("Lon Lon Ranch", 4, 12, Biome::Plains);
        case 0x51: return outdoor("Hyrule Field", 2, 12, Biome::Plains, 5, 8);
        case 0x5B: return adult ? outdoor("Lost Woods", 1, 10, Biome::Woods, 4, 6) : Region{"Lost Woods",1,3,2,2,Biome::Woods,Population::Wilderness};
        case 0x56: return outdoor("Sacred Forest Meadow", 7, 18, Biome::Woods, 4, 6);
        case 0x60: return outdoor("Death Mountain Trail", 8, 20, Biome::Mountain);
        case 0x61: return outdoor("Death Mountain Crater", 14, 26, Biome::Mountain, 4, 6);
        case 0x54: return outdoor("Zora's River", 6, 16, Biome::Water, 4, 6);
        case 0x59: return outdoor("Zora's Fountain", 10, 22, Biome::Water, 4, 6);
        case 0x57: return outdoor("Lake Hylia", 8, 18, Biome::Water);
        case 0x5A: return outdoor("Gerudo Valley", 12, 25, Biome::Desert, 4, 6);
        case 0x5E: return outdoor("Haunted Wasteland", 22, 30, Biome::Desert, 5, 8);
        case 0x5C: return outdoor("Desert Colossus", 24, 34, Biome::Desert, 5, 8);
        case 0x64: return outdoor("Ganon's Reach", 36, 40, Biome::Fortress, 5, 8);
        case 0x00: case 0x11: return { "Deku Tree", 1, 3, 2, 2, Biome::Woods, scene == 0x00 ? Population::Dungeon : Population::Boss };
        case 0x01: case 0x12: return { "Dodongo's Cavern", 8, 11, 3, 5, Biome::Mountain, scene == 0x01 ? Population::Dungeon : Population::Boss };
        case 0x02: case 0x13: return { "Jabu-Jabu", 12, 15, 3, 5, Biome::Water, scene == 0x02 ? Population::Dungeon : Population::Boss };
        case 0x03: case 0x14: return { "Forest Temple", 18, 22, 4, 6, Biome::Woods, scene == 0x03 ? Population::Dungeon : Population::Boss };
        case 0x04: case 0x15: return { "Fire Temple", 24, 28, 4, 6, Biome::Mountain, scene == 0x04 ? Population::Dungeon : Population::Boss };
        case 0x05: case 0x16: return { "Water Temple", 27, 31, 3, 5, Biome::Water, scene == 0x05 ? Population::Dungeon : Population::Boss };
        case 0x07: case 0x18: return { "Shadow Temple", 31, 35, 4, 6, Biome::Crypt, scene == 0x07 ? Population::Dungeon : Population::Boss };
        case 0x06: case 0x17: return { "Spirit Temple", 34, 38, 4, 6, Biome::Desert, scene == 0x06 ? Population::Dungeon : Population::Boss };
        case 0x08: return { "Bottom of the Well", 14, 18, 3, 5, Biome::Crypt, Population::Dungeon };
        case 0x09: return { "Ice Cavern", 22, 26, 3, 5, Biome::Water, Population::Dungeon };
        case 0x0B: return { "Gerudo Training Ground", 32, 36, 4, 6, Biome::Fortress, Population::Dungeon };
        case 0x0D: case 0x0A: return { "Ganon's Castle", 40, 44, 4, 6, Biome::Fortress, Population::Dungeon };
        case 0x19: case 0x4F: return { "Ganon's Throne", 45, 48, 0, 0, Biome::Fortress, Population::Boss };
        case 0x3E: return { "Hidden Grotto", uint8_t(adult ? 18 : 6), uint8_t(adult ? 22 : 9), 3, 4, Biome::Crypt, Population::Dungeon };
        case 0x3F: case 0x41: return { "Royal Tombs", 12, 16, 3, 5, Biome::Crypt, Population::Dungeon };
        case 0x53: return { "Graveyard", uint8_t(adult ? 22 : 10), uint8_t(adult ? 25 : 13), 0, 0, Biome::Crypt, Population::Settlement };
        case 0x5D: return { "Gerudo's Fortress", 30, 34, 0, 0, Biome::Fortress, Population::Settlement };
        default: return { "Sanctuary", 1, 1, 0, 0, Biome::Safe, Population::None };
    }
}
}
