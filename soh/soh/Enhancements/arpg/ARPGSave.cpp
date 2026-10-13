#include "ARPGSave.h"
#include "ARPGSaveCodec.h"
#include "ARPG.h"
#include "SkillSystem.h"
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "variables.h"
}

ARPGSaveData& gARPGSave = gSaveContext.ship.arpg;

uint32_t XpForLevel(uint8_t level) {
    return ARPG::Rules::XpThreshold(level);
}

// [TIME] | [INFO] | ARPG_InitFile | SaveManager initializes every file, including old saves without an ARPG section.
static void ARPG_InitFile(bool) {
    gARPGSave = {};
    gARPGSave.level = 1;
    for (int slot = 0; slot < ARPG_SLOT_COUNT_; ++slot) {
        gARPGSave.equipment[slot].slot = slot;
    }
    for(int i=0;i<ARPG_WEAPON_SLOT_COUNT;++i) { gARPGSave.weapons[i].slot=ARPG_SLOT_COUNT_+i; }
    ARPG::SkillRules::Initialize(gARPGSave);
    ARPG::InitFile();
    SkillSystem::ResetRuntime();
}

static void ARPG_LoadFile() {
    nlohmann::json json = nlohmann::json::object();
    const auto fields = ARPG::SaveCodec::Encode(gARPGSave);
    for (const auto& field : fields.items()) {
        nlohmann::json value;
        SaveManager::Instance->LoadData(field.key(), value);
        if (!value.is_null()) {
            json[field.key()] = std::move(value);
        }
    }
    gARPGSave = ARPG::SaveCodec::Decode(json, gARPGSave.seed);
    ARPG::OnFileLoaded();
}

// [TIME] | [INFO] | ARPG_SaveFile | This runs on the save worker; never read mutable gameplay globals here.
static void ARPG_SaveFile(const SaveContext& saveContext, int, bool) {
    const auto json = ARPG::SaveCodec::Encode(saveContext.ship.arpg);
    for (const auto& field : json.items()) {
        SaveManager::Instance->SaveData(field.key(), field.value());
    }
}

void RegisterARPGSave() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    SaveManager::Instance->AddInitFunction(ARPG_InitFile);
    SaveManager::Instance->AddLoadFunction("arpg", 1, ARPG_LoadFile);
    SaveManager::Instance->AddLoadFunction("arpg", 2, ARPG_LoadFile);
    SaveManager::Instance->AddLoadFunction("arpg", 3, ARPG_LoadFile);
    SaveManager::Instance->AddLoadFunction("arpg", 4, ARPG_LoadFile);
    SaveManager::Instance->AddLoadFunction("arpg", 5, ARPG_LoadFile);
    SaveManager::Instance->AddLoadFunction("arpg", 6, ARPG_LoadFile);
    SaveManager::Instance->AddLoadFunction("arpg", 7, ARPG_LoadFile);
    SaveManager::Instance->AddLoadFunction("arpg", 8, ARPG_LoadFile);
    SaveManager::Instance->AddLoadFunction("arpg", 9, ARPG_LoadFile);
    SaveManager::Instance->AddLoadFunction("arpg", 10, ARPG_LoadFile);
    SaveManager::Instance->AddLoadFunction("arpg", 11, ARPG_LoadFile);
    SaveManager::Instance->AddLoadFunction("arpg", 12, ARPG_LoadFile);
    SaveManager::Instance->AddLoadFunction("arpg", 13, ARPG_LoadFile);
    SaveManager::Instance->AddLoadFunction("arpg", 14, ARPG_LoadFile);
    SaveManager::Instance->AddLoadFunction("arpg", 15, ARPG_LoadFile);
    SaveManager::Instance->AddLoadFunction("arpg", 16, ARPG_LoadFile);
    SaveManager::Instance->AddSaveFunction("arpg", 16, ARPG_SaveFile, true, SECTION_PARENT_NONE);
}

static RegisterShipInitFunc initSave(RegisterARPGSave);
