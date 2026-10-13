#include "CharacterTravel.h"
#include "Anchor.h"
#include "soh/SaveManager.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
extern PlayState* gPlayState;
}

char z2ASCII(int code);
std::string decodeNTSCPlayerNameChar(int code);

namespace CharacterTravel {
bool Enabled() {
    return CVarGetInteger(CVAR_REMOTE_ANCHOR("PortableCharacters"), 1) != 0;
}

bool Checkpoint(std::string& error) {
    error.clear();
    if (!gPlayState)
        return true; // The character will be chosen in native file select.
    if (!Anchor::Instance->IsSaveLoaded() || !SaveManager::Instance->SaveFile_Exist(gSaveContext.fileNum)) {
        error = "Load a saved character before changing servers.";
        return false;
    }
    if (gSaveContext.health <= 0 || gPlayState->gameplayFrames < 60 ||
        gPlayState->sceneNum == SCENE_CHAMBER_OF_THE_SAGES || gPlayState->sceneNum == SCENE_CUTSCENE_MAP ||
        (!CHECK_QUEST_ITEM(QUEST_SONG_TIME) && INV_CONTENT(ITEM_OCARINA_TIME) == ITEM_OCARINA_TIME) ||
        !GameInteractor::IsPlayerInControl() || !GameInteractor_Should(VB_BE_ABLE_TO_SAVE, true)) {
        error = "Return to normal gameplay before saving and changing servers.";
        return false;
    }
    // Use the native save preparation (scene flags and temporary B-button items),
    // then wait for this exact immutable snapshot to be durably written. A failed
    // save leaves the current connection intact and can be retried.
    try {
        Play_PerformSave(gPlayState);
        auto result = SaveManager::Instance->LastSaveResult();
        if (result.valid() && result.get())
            return true;
    } catch (const std::exception& e) { SPDLOG_ERROR("Character checkpoint failed: {}", e.what()); }
    error = "Character could not be saved. Check free space and save-folder access, then try again.";
    return false;
}

std::string Summary() {
    if (!Anchor::Instance->IsSaveLoaded())
        return "Characters are your local save slots. Choose the same save in any party.";
    std::string name;
    for (auto code : gSaveContext.playerName) {
        if (gSaveContext.ship.filenameLanguage == NAME_LANGUAGE_PAL)
            name += z2ASCII(code);
        else
            name += decodeNTSCPlayerNameChar(code);
    }
    while (!name.empty() && name.back() == ' ')
        name.pop_back();
    return name + " / Slot " + std::to_string(gSaveContext.fileNum + 1) + " / Level " +
           std::to_string(gARPGSave.level) + " / " + std::to_string(gSaveContext.rupees) + " Rupees";
}
} // namespace CharacterTravel
