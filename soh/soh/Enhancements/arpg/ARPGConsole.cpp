#include "ARPGConsole.h"

// [TIME] | [INFO] | ARPGConsole.cpp | console commands: arpg / arpggive / arpglevel /
// arpgpoints / arpgreroll (Ship console commands are single-token)

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/ConsoleWindow.h>

#include "ARPG.h"
#include "ARPGSave.h"
#include "ItemModifierSystem.h"
#include "InventorySystem.h"
#include "SkillSystem.h"
#include <charconv>
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"

extern "C" {
#include "variables.h"
#include "macros.h"
extern PlayState* gPlayState;
}


#define CMD_REGISTER Ship::Context::GetRawInstance()->GetConsole()->AddCommand
#define ERROR_MESSAGE                                                                    \
    std::reinterpret_pointer_cast<Ship::ConsoleWindow>(                                  \
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow("Console")) \
        ->SendErrorMessage
#define INFO_MESSAGE                                                                     \
    std::reinterpret_pointer_cast<Ship::ConsoleWindow>(                                  \
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow("Console")) \
        ->SendInfoMessage

// [TIME] | [INFO] | ARPGInfoHandler | prints current character state
static bool ARPGInfoHandler(std::shared_ptr<Ship::Console> console, const std::vector<std::string>& args,
                            std::string* output) {
    INFO_MESSAGE("ARPG: level %d xp %u | passive %d active %d | kills %u elites %u bosses %u items %u | seed %08X",
                 gARPGSave.level, gARPGSave.xp, gARPGSave.passivePoints, gARPGSave.activePoints, gARPGSave.kills,
                 gARPGSave.eliteKills, gARPGSave.bossKills, gARPGSave.itemsFound, gARPGSave.seed);
    return 0;
}

// [TIME] | [INFO] | ARPGGiveHandler | arpggive <tier 1-3> -> roll + collect item
static bool ARPGGiveHandler(std::shared_ptr<Ship::Console> console, const std::vector<std::string>& args,
                            std::string* output) {
    if (!ARPG::Enabled()) { ERROR_MESSAGE("Enable ARPG and load a save first."); return 1; }
    if (args.size() < 2) {
        ERROR_MESSAGE("usage: arpggive <tier 1=champion 2=elite 3=boss>");
        return 1;
    }
    int tier;
    try {
        tier = std::stoi(args[1]);
    } catch (...) {
        ERROR_MESSAGE("tier must be a number");
        return 1;
    }
    if (tier < 1 || tier > 3) {
        ERROR_MESSAGE("tier must be 1..3");
        return 1;
    }
    int slot = -1;
    if (args.size() >= 3) {
        try { slot = std::stoi(args[2]); } catch (...) { ERROR_MESSAGE("slot must be 0..5"); return 1; }
        if (slot < 0 || slot >= ARPG_LOOT_SLOT_COUNT) { ERROR_MESSAGE("slot must be 0..5"); return 1; }
    }
    if (slot >= 0 && (ARPG::Loot::WeaponSlot(slot)?!ItemModifierSystem::WeaponOwned(slot):CUR_EQUIP_VALUE(slot) == 0)) { ERROR_MESSAGE("Equip a base item in that slot first."); return 1; }
    if (!ARPG::ItemsEnabled()) { ERROR_MESSAGE("Enable Item Modifiers first."); return 1; }
    ARPGItemInstance inst = ItemModifierSystem::RollItemDrop(tier, slot);
    if (!ItemModifierSystem::GiveItemDrop(inst)) { ERROR_MESSAGE("Backpack full; no item was added."); return 1; }
    INFO_MESSAGE("added ARPG item to backpack (rarity %d, slot %d)", inst.rarity, inst.slot);
    return 0;
}

// [TIME] | [INFO] | ARPGLevelHandler | arpglevel <n> -> level up the character
static bool ARPGLevelHandlerImpl(std::shared_ptr<Ship::Console> console, const std::vector<std::string>& args,
                                 std::string* output) {
    if (!ARPG::Enabled()) { ERROR_MESSAGE("Enable ARPG and load a save first."); return 1; }
    if (args.size() < 2) {
        ERROR_MESSAGE("usage: arpglevel <1-50>");
        return 1;
    }
    int target;
    try {
        target = std::stoi(args[1]);
    } catch (...) {
        ERROR_MESSAGE("level must be a number");
        return 1;
    }
    if (target < 1 || target > ARPG_MAX_LEVEL) {
        ERROR_MESSAGE("level must be 1..%d", ARPG_MAX_LEVEL);
        return 1;
    }
    if (target <= gARPGSave.level) {
        INFO_MESSAGE("already level %d", gARPGSave.level);
        return 0;
    }
    while (gARPGSave.level < (uint8_t)target) {
        uint32_t threshold = XpForLevel(gARPGSave.level);
        uint32_t need = threshold > gARPGSave.xp ? threshold - gARPGSave.xp : 1;
        SkillSystem::GrantXp(need);
        if (gARPGSave.level >= ARPG_MAX_LEVEL) {
            break;
        }
    }
    INFO_MESSAGE("level -> %d (passive pts %d, active pts %d)", gARPGSave.level, gARPGSave.passivePoints,
                 gARPGSave.activePoints);
    return 0;
}

// [TIME] | [INFO] | ARPGRerollHandler | arpgreroll -> new RNG seed
static bool ARPGRerollHandler(std::shared_ptr<Ship::Console> console, const std::vector<std::string>& args,
                              std::string* output) {
    if (!ARPG::Enabled()) { ERROR_MESSAGE("Enable ARPG and load a save first."); return 1; }
    gARPGSave.seed = (uint32_t)std::chrono::steady_clock::now().time_since_epoch().count();
    ARPG::SeedRng(gARPGSave.seed);
    INFO_MESSAGE("ARPG seed rerolled: %08X", gARPGSave.seed);
    return 0;
}

// [TIME] | [INFO] | ARPGPointsHandler | arpgpoints <passive|active> <n>
static bool ARPGPointsHandler(std::shared_ptr<Ship::Console> console, const std::vector<std::string>& args,
                              std::string* output) {
    if (!ARPG::Enabled()) { ERROR_MESSAGE("Enable ARPG and load a save first."); return 1; }
    if (args.size() < 3) {
        ERROR_MESSAGE("usage: arpgpoints <passive|active> <n>");
        return 1;
    }
    int n;
    try {
        n = std::stoi(args[2]);
    } catch (...) {
        ERROR_MESSAGE("count must be a number");
        return 1;
    }
    n = std::clamp(n, -200, 200);
    if (args[1] == "passive") {
        gARPGSave.passivePoints = (uint8_t)std::clamp(gARPGSave.passivePoints + n, 0, 200);
    } else if (args[1] == "active") {
        gARPGSave.activePoints = (uint8_t)std::clamp(gARPGSave.activePoints + n, 0, 200);
    } else {
        ERROR_MESSAGE("pool must be 'passive' or 'active'");
        return 1;
    }
    INFO_MESSAGE("points: passive %d, active %d", gARPGSave.passivePoints, gARPGSave.activePoints);
    return 0;
}

// [TIME] | [INFO] | ARPGCommandHandler | Keep legacy aliases while exposing the documented subcommand syntax.
static bool ARPGCommandHandler(std::shared_ptr<Ship::Console> console, const std::vector<std::string>& args, std::string* output) {
    if (args.size() < 2) { return ARPGInfoHandler(console, args, output); }
    std::vector<std::string> subargs(args.begin() + 1, args.end());
    if (args[1] == "give") { return ARPGGiveHandler(console, subargs, output); }
    if (args[1] == "level") { return ARPGLevelHandlerImpl(console, subargs, output); }
    if (args[1] == "points") { return ARPGPointsHandler(console, subargs, output); }
    if (args[1] == "reroll") { return ARPGRerollHandler(console, subargs, output); }
    if (args[1] == "window") {
        if (auto window = Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow("ARPG Mod")) { window->Show(); }
        return 0;
    }
    if (args[1] == "inventory") {
        if (!ARPG::ItemsEnabled()) { ERROR_MESSAGE("Enable Item Modifiers and load a save first."); return 1; }
        ARPG::Inventory::Open();
        return 0;
    }
    if (args[1] == "cast" && args.size() == 3) {
        int id = -1;
        auto result = std::from_chars(args[2].data(), args[2].data() + args[2].size(), id);
        if (result.ec != std::errc{} || result.ptr != args[2].data() + args[2].size() || id < 0 || id >= ARPG_SKILL_COUNT_ ||
            !SkillSystem::TryCastActive(gPlayState, id)) {
            ERROR_MESSAGE("Cannot cast: check skill rank, control, magic, and cooldown."); return 1;
        }
        return 0;
    }
    ERROR_MESSAGE("arpg [inventory | window | give <tier 1..3> [slot 0..5] | level <1..50> | points <passive|active> <n> | reroll | cast <id>]");
    return 1;
}

void ARPG_RegisterConsoleCommands() {
    CMD_REGISTER("arpg", { ARPGCommandHandler, "Show ARPG character info.", {} });
    CMD_REGISTER("arpggive", { ARPGGiveHandler,
                               "Give an ARPG item.",
                               {
                                   { "tier", Ship::ArgumentType::NUMBER },
                               } });
    CMD_REGISTER("arpglevel", { ARPGLevelHandlerImpl,
                                "Level up the ARPG character.",
                                {
                                    { "level", Ship::ArgumentType::NUMBER },
                                } });
    CMD_REGISTER("arpgreroll", { ARPGRerollHandler, "Reroll the ARPG RNG seed.", {} });
    CMD_REGISTER("arpgpoints", { ARPGPointsHandler,
                                 "Add skill points.",
                                 {
                                     { "pool", Ship::ArgumentType::TEXT },
                                     { "count", Ship::ArgumentType::NUMBER },
                                 } });
}
