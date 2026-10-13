#pragma once
#include <cstdint>
#include <string>

namespace ARPG {
// [TIME] | [INFO] | ARPG.Gates | CVARs default off; require a loaded save, with randomizer opt-in.
bool Enabled();
bool AffixesEnabled();
bool ItemsEnabled();
bool SkillsEnabled();
// [TIME] | [INFO] | ARPG.Random | Seed plus current stream state are saved; reproducible for identical call order.
uint32_t RandInt(uint32_t min, uint32_t max);
float RandFloat(float min, float max);
void SeedRng(uint32_t seed);
uint32_t CurrentSeed();
void Log(const std::string& level, const std::string& msg);
void InitFile();
void OnFileLoaded();
}
