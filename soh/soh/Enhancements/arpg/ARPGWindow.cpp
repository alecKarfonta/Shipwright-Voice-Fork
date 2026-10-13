#include "ARPGWindow.h"
#include "QuestSystem.h"
#include "HUDLayout.h"
#include "ARPGTheme.h"

// [TIME] | [INFO] | ARPGWindow.cpp | Config + debug UI: toggles, skill tree, gear, tuning

#include <imgui.h>
#include <algorithm>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/SohGui/UIWidgets.hpp"
#include "soh/cvar_prefixes.h"
#include "ARPG.h"
#include "ARPGSave.h"
#include "AffixSystem.h"
#include "ItemModifierSystem.h"
#include "SkillSystem.h"
#include "SkillUI.h"
#include "ARPGTesting.h"
#include "InventorySystem.h"
#include "InventoryRules.h"
#include "ControllerConfig.h"
#include "EnemySystem.h"
#include "ExpeditionSystem.h"

extern "C" {
#include "z64.h"
#include "variables.h"
extern PlayState* gPlayState;
}

void ARPGWindow::DrawElement() {
    // SECTION: master toggles
    if (ImGui::Button("Open Loot Inventory")) { ARPG::Inventory::Open(); }
    ImGui::Text("ARPG Mod (Diablo-2-style layer)");
    UIWidgets::PaddedSeparator();
    UIWidgets::CVarCheckbox("Enable ARPG systems", CVAR_ENHANCEMENT("ARPG.Enabled"),
                            { .defaultValue = true, .color = UIWidgets::Colors::LightBlue });
    UIWidgets::CVarCheckbox("Monster Affixes", CVAR_ENHANCEMENT("ARPG.Affixes"),
                            { .defaultValue = true, .color = UIWidgets::Colors::LightBlue });
    UIWidgets::CVarCheckbox("Item Modifiers", CVAR_ENHANCEMENT("ARPG.Items"),
                            { .color = UIWidgets::Colors::LightBlue });
    UIWidgets::CVarCheckbox("Skill Trees & XP", CVAR_ENHANCEMENT("ARPG.Skills"),
                            { .color = UIWidgets::Colors::LightBlue });
    ImGui::TextWrapped("The adventure journal replaces the pause menu while ARPG is enabled.");
    UIWidgets::CVarCheckbox("Hyrule Reborn quests", CVAR_ENHANCEMENT("ARPG.Quests"), { .defaultValue = true });
    UIWidgets::CVarCheckbox("Quest objective tracker", CVAR_ENHANCEMENT("ARPG.QuestTracker"), { .defaultValue = true });
    UIWidgets::CVarCheckbox("Remastered equipment models", CVAR_ENHANCEMENT("ARPG.RemasteredGear"), { .defaultValue = true });
    UIWidgets::CVarCheckbox("Remastered enemy models", CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"), { .defaultValue = true });
    UIWidgets::CVarCheckbox("Show gear pickup textboxes", CVAR_ENHANCEMENT("ARPG.PickupText"));
    UIWidgets::CVarCheckbox("Show character and region HUD", CVAR_ENHANCEMENT("ARPG.HUD"), { .defaultValue = true });
    UIWidgets::CVarCheckbox("Floating damage numbers", CVAR_ENHANCEMENT("ARPG.DamageNumbers"), { .defaultValue = true });
    UIWidgets::CVarCheckbox("Enchanted elite auras (Fire/Frost/Storm)", CVAR_ENHANCEMENT("ARPG.Enchanted"), { .defaultValue = true });
    UIWidgets::CVarCheckbox("Allow ARPG in randomizer saves", CVAR_ENHANCEMENT("ARPG.AllowRandomizer"));
    UIWidgets::PaddedSeparator();

    ARPG::Enemies::DrawSettings();
    LocalNpc::DrawSettings();
    if (ImGui::CollapsingHeader("Combat controller")) {
        ImGui::TextWrapped("X / Y / RB: C items. D-pad Up / Right / Down / Left: abilities 1-4. Stick clicks: abilities 5-6.");
        ImGui::TextWrapped("A: action. B: sword. LT: target. RT: shield. Hold LB: sprint. Menu: backpack. View: settings.");
        if (ImGui::Button("Apply combat bindings")) { ARPG::ControllerConfig::ApplyCombatBindings(); }
    }
    ARPG::Testing::DrawControls();

    if (!ARPG::Enabled()) {
        ImGui::TextDisabled("Enable the master switch and load an allowed save to view your character.");
        return;
    }

    // SECTION: character sheet
    if (ImGui::CollapsingHeader("Character")) {
        uint32_t next = XpForLevel(gARPGSave.level);
        std::string nextStr = gARPGSave.level >= ARPG_MAX_LEVEL ? ", MAX"
                                                                : spdlog::fmt_lib::format(" / next: {}", next);
        ImGui::Text("Level %d  (XP: %u%s)", gARPGSave.level, gARPGSave.xp, nextStr.c_str());
        ImGui::Text("Points: %d passive, %d active", gARPGSave.passivePoints, gARPGSave.activePoints);
        ImGui::Text("Kills: %u  Elites: %u  Bosses: %u", gARPGSave.kills, gARPGSave.eliteKills,
                    gARPGSave.bossKills);
        ImGui::Text("Items found: %u   Seed: %08X", gARPGSave.itemsFound, gARPGSave.seed);
    }

    if (ImGui::Button("Open Skill Journal [K]", ImVec2(0, 32))) { ARPG::SkillUI::OpenJournal(); }
    ImGui::TextWrapped("Assign six techniques in Skills. Keyboard 1-6; D-pad Up/Right/Down/Left for 1-4; left/right stick click for 5-6. LB + D-pad Left/Right also casts 5-6.");
    const char* elements[] = { "Fire", "Ice", "Light" };
    int element = std::clamp(CVarGetInteger(CVAR_ENHANCEMENT("ARPG.InfusionElement"), 0), 0, 2);
    if (ImGui::Combo("Infusion element", &element, elements, 3)) {
        CVarSetInteger(CVAR_ENHANCEMENT("ARPG.InfusionElement"), element); CVarSave();
    }

    // SECTION: equipped gear
    if (ImGui::CollapsingHeader("Equipped Gear")) {
        const auto& slotNames = ARPG::Loot::SlotNames;
        const char* rarityNames[] = { "Common", "Magic", "Rare", "Unique" };
        for (int slot = 0; slot < ARPG_LOOT_SLOT_COUNT; slot++) {
            const ARPGItemInstance& inst = ARPG::InventoryRules::Equipped(gARPGSave,slot);
            ImGui::PushID(slot);
            ImGui::Text("%s:", slotNames[slot]);
            ImGui::SameLine();
            if (inst.modCount == 0) {
                ImGui::TextDisabled("no modifiers");
            } else {
                ImGui::Text("%s: %s", rarityNames[inst.rarity % ARPG_RARITY_COUNT_], ItemModifierSystem::ItemName(inst).c_str());
                if (!ItemModifierSystem::IsEquipped(inst)) { ImGui::TextDisabled("Inactive: equip the matching base item."); }
                for (int m = 0; m < inst.modCount && m < ARPG_ITEM_MOD_COUNT; m++) {
                    ImGui::BulletText("%s", ItemModifierSystem::DescribeModifier(inst.mods[m]).c_str());
                }
            }
            ImGui::PopID();
        }
    }

    // SECTION: tuning
    if (ImGui::CollapsingHeader("Tuning (dev)")) {
        // [TIME] | [INFO] | ARPGWindow.Tuning | runtime-tunable rates saved to cvars
        int eliteChance = CVarGetInteger(CVAR_ENHANCEMENT("ARPG.EliteChance"), AffixSystem::ELITE_CHANCE_DEFAULT);
        if (UIWidgets::SliderInt("Champion/Elite base spawn %", &eliteChance,
                                 { .min = 0, .max = 60, .defaultValue = AffixSystem::ELITE_CHANCE_DEFAULT })) {
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.EliteChance"), eliteChance);
            CVarSave();
        }
        int championRatio = CVarGetInteger(CVAR_ENHANCEMENT("ARPG.ChampionRatio"), AffixSystem::CHAMPION_RATIO_DEFAULT);
        if (ImGui::SliderInt("Champion share of special spawns %", &championRatio, 0, 100)) {
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.ChampionRatio"), championRatio);
            CVarSave();
        }
        int dropChance = CVarGetInteger(CVAR_ENHANCEMENT("ARPG.DropChance"), ItemModifierSystem::DROP_CHANCE_DEFAULT);
        if (UIWidgets::SliderInt("Base gear drop rate", &dropChance,
                                 { .min = 0, .max = 100, .defaultValue = ItemModifierSystem::DROP_CHANCE_DEFAULT })) {
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.DropChance"), dropChance);
            CVarSave();
        }
        int xpRate = CVarGetInteger(CVAR_ENHANCEMENT("ARPG.XpRate"), 100);
        if (UIWidgets::SliderInt("XP rate %", &xpRate,
                                 { .min = 25, .max = 400, .defaultValue = 100 })) {
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.XpRate"), xpRate);
            CVarSave();
        }
        if (ImGui::TreeNode("Equipment roll ranges")) {
            static int mod = ARPG_MOD_BRUTAL;
            static int rarity = ARPG_RARITY_MAGIC;
            if (ImGui::BeginCombo("Modifier", ItemModifierSystem::ModName(mod))) {
                for (int id = 1; id < ARPG_MOD_COUNT_; ++id) {
                    if (ImGui::Selectable(ItemModifierSystem::ModName(id), id == mod)) { mod = id; }
                }
                ImGui::EndCombo();
            }
            const char* rarities[] = { "Magic", "Rare", "Unique" };
            int selectedRarity = rarity - 1;
            if (ImGui::Combo("Rarity", &selectedRarity, rarities, 3)) { rarity = selectedRarity + 1; }
            auto range = ItemModifierSystem::GetPowerRange(mod, rarity);
            bool changed = ImGui::SliderInt("Minimum power", &range.min, 1, 100);
            changed |= ImGui::SliderInt("Maximum power", &range.max, range.min, 100);
            if (ImGui::Button("Restore default range")) {
                range = ItemModifierSystem::GetPowerRange(mod, rarity, true);
                changed = true;
            }
            if (changed) {
                ItemModifierSystem::SetPowerRange(mod, rarity, range);
                CVarSave();
            }
            ImGui::TextWrapped("Low: %s", ItemModifierSystem::DescribeModifier({ uint8_t(mod), int16_t(range.min) }).c_str());
            ImGui::TextWrapped("High: %s", ItemModifierSystem::DescribeModifier({ uint8_t(mod), int16_t(range.max) }).c_str());
            ImGui::TextDisabled("Changes apply to future drops.");
            ImGui::TreePop();
        }
    }
}

// [TIME] | [INFO] | ARPGWindow.Draw | The character HUD remains available when the configuration window is closed.
void ARPGWindow::Draw() {
    Ship::GuiWindow::Draw();
    LocalNpc::Draw();
    ARPG::SkillUI::DrawHotbar();
    ARPG::Quests::DrawHud();
    if (!ARPG::Enabled() || !CVarGetInteger(CVAR_ENHANCEMENT("ARPG.HUD"), 1) ||
        GameInteractor::IsGameplayPaused()) { return; }
    if (!GameInteractor::IsPlayerInControl()) { return; }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float hudScale = ARPG::HUDLayout::Scale(viewport->WorkSize.y);
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * (ARPG::Expeditions::Busy()?.46f:.5f), viewport->WorkPos.y + 18 * hudScale),
                           ImGuiCond_Always, ImVec2(.5f, 0));
    ImGui::SetNextWindowSize(ImVec2(ARPG::Expeditions::Busy()?std::min(380.f * hudScale,viewport->WorkSize.x*.30f):std::min(300.f * hudScale, viewport->WorkSize.x - 32.f),0),ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    if (ImGui::Begin("##ARPGExperience", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs)) {
        if(ARPG::Expeditions::Busy()) { ARPG::Expeditions::DrawHud(); ARPG::Enemies::DrawTargetHealth(); }
        else { ARPG::Enemies::DrawHud(); }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
