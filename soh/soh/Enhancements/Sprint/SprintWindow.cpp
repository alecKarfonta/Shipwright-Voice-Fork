#include "SprintWindow.h"
#include "Sprint.h"

#include <imgui.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/cvar_prefixes.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
extern PlayState* gPlayState;
}

// [TIME] | [INFO] | SprintWindow.tunables | fixed bar geometry, scaled against the game window
static constexpr float BAR_WIDTH = 140.0f;
static constexpr float BAR_HEIGHT = 8.0f;

// [TIME] | [INFO] | SprintWindow.Draw | overlay anchored above the heart row; only visible while the
// enhancement is on, a play state exists, and stamina is not full (or the player is sprinting)
void SprintWindow::Draw() {
    // [TIME] | [INFO] | SprintWindow.Draw | visibility driven by the window cvar directly (default on),
    // since this override bypasses GuiWindow::Draw()'s IsVisible() gate
    if (!CVarGetInteger(CVAR_WINDOW("SprintBarEnabled"), 1)) {
        return;
    }
    if (!Sprint_Enabled()) {
        return;
    }
    if (gPlayState == NULL) {
        return;
    }

    bool show = Sprint_IsSprinting() || Sprint_GetStamina01() < 0.999f;
    if (!show) {
        return;
    }

    // [TIME] | [INFO] | SprintWindow.Draw | hide alongside the vanilla HUD (cutscenes, minigames, etc.)
    if (gSaveContext.hudVisibilityMode == HUD_VISIBILITY_NOTHING ||
        gSaveContext.hudVisibilityMode == HUD_VISIBILITY_NOTHING_ALT ||
        gSaveContext.hudVisibilityMode == HUD_VISIBILITY_NOTHING_INSTANT) {
        return;
    }

    float stamina = Sprint_GetStamina01();

    // [TIME] | [INFO] | SprintWindow.Draw | color states: cyan while sprinting, green regen, red exhausted
    ImVec4 fillColor = ImVec4(0.31f, 0.89f, 1.00f, 1.0f);
    if (Sprint_IsExhausted()) {
        fillColor = ImVec4(1.00f, 0.27f, 0.15f, 1.0f);
    } else if (!Sprint_IsSprinting()) {
        fillColor = ImVec4(0.40f, 0.86f, 0.38f, 1.0f);
    }

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 pos = ImVec2(io.DisplaySize.x * 0.5f - BAR_WIDTH * 0.5f, io.DisplaySize.y - 120.0f);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(BAR_WIDTH, BAR_HEIGHT + 4.0f), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));

    ImGui::Begin("SprintStaminaBar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoDocking |
                     ImGuiWindowFlags_AlwaysAutoResize);
    {
        // [TIME] | [INFO] | SprintWindow.Draw | raw ImDrawList rect fill so color tracks sprint state
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 min = p;
        ImVec2 max = ImVec2(p.x + BAR_WIDTH, p.y + BAR_HEIGHT);

        dl->AddRectFilled(min, max, IM_COL32(0, 0, 0, 140), 3.0f);
        dl->AddRectFilled(min, ImVec2(min.x + BAR_WIDTH * stamina, max.y),
                          ImGui::ColorConvertFloat4ToU32(fillColor), 3.0f);
        dl->AddRect(min, max, IM_COL32(0, 0, 0, 200), 3.0f, 0, 1.0f);
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void SprintWindow::InitElement() {
    // [TIME] | [INFO] | SprintWindow.InitElement | nothing to load; textures would be registered here
}
