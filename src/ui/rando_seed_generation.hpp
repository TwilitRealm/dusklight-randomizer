#pragma once

#include <mods/api.h>
#include "../archi/ap_data.hpp"
#include "../../generator/randomizer_type.hpp"

#include <string>

namespace randomizer::ui {

void GenerateRandomizerSeed(Type type = Type::DEFAULT, const archi::WorldData& apData = archi::WorldData{});
void UpdateProgressBar();
void UpdateSeedGenerationDialog();
void UpdateSeedGenProgressValue(float progress);
void UpdateGenerationStatusMsg(const std::string& str);
std::string ReadGenerationStatusMsg();

}
