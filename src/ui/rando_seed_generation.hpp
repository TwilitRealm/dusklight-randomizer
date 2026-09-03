#pragma once

#include <mods/api.h>

#include <string>

namespace randomizer::ui {

void GenerateRandomizerSeed();
ModResult UpdateSeedGenerationDialog();
void UpdateSeedGenProgressValue(float progress);
void UpdateGenerationStatusMsg(const std::string& str);
std::string ReadGenerationStatusMsg();

}
