#pragma once

#include "ARPGTypes.h"
#include "SkillDefinitions.h"

// [TIME] | [INFO] | ARPGSave | Alias into SaveContext so asynchronous saves use their immutable snapshot.
extern ARPGSaveData& gARPGSave;
uint32_t XpForLevel(uint8_t level);
void RegisterARPGSave();
