#pragma once

#include <filesystem>
#include <string>

namespace randomizer::paths {

std::filesystem::path GetRandomizerPath(const std::string& subfolder = "");
std::filesystem::path GetRandomizerSettingsPath(const std::string& subfolder = "");
std::filesystem::path GetRandomizerPreferencesPath(const std::string& subfolder = "");
std::filesystem::path GetRandomizerPresetsPath(const std::string& subfolder = "");
std::filesystem::path GetRandomizerSeedsPath(const std::string& subfolder = "");

}  // namespace randomizer::paths
