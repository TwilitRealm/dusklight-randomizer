#include "config_store.hpp"

#include <mods/svc/log.hpp>

#include "../../generator/seedgen/seed.hpp"
#include "../paths.hpp"

namespace randomizer::ui {

seedgen::config::Config& GetRandomizerConfig() {
    static seedgen::config::Config s_config{paths::GetRandomizerSettingsPath(),
                                            paths::GetRandomizerPreferencesPath()};
    return s_config;
}

void SaveRandomizerConfig() {
    GetRandomizerConfig().WriteToFile(paths::GetRandomizerSettingsPath(),
                                      paths::GetRandomizerPreferencesPath());
}

bool TryCreateRandomSeed() {
    auto& config = GetRandomizerConfig();

    if (config.GetSeed().empty()) {
        config.SetSeed(seedgen::seed::GenerateSeed());
        SaveRandomizerConfig();
        return true;
    }
    return false;
}

seedgen::settings::Setting* FindSetting(const std::string& key) {
    if (key.empty()) {
        mods::log::error("Key is empty! Unable to find setting.");
    }

    auto& settings = GetRandomizerConfig().GetSettings();
    try {
        return &settings.GetMap().at(key);
    } catch (std::exception e) {
        mods::log::error("Failed to get Settings Key: {}", key);
        return nullptr;
    }
}

// Certain setting options aren't compatible with each other. We force incompatible selections
// to be compatible here.
void CheckAndSetForcedOptions() {
    bool randomizedStartingSpawn = *FindSetting("Randomize Starting Spawn") != "Off";
    bool randomizedDungeonEntrances = *FindSetting("Randomize Dungeon Entrances") != "Off";
    bool randomizedBossEntrances = *FindSetting("Randomize Boss Entrances") != "Off";
    bool randomizedGrottoEntrances = *FindSetting("Randomize Grotto Entrances") != "Off";
    bool randomizedCaveEntrances = *FindSetting("Randomize Cave Entrances") != "Off";
    bool randomizedInteriorEntrances = *FindSetting("Randomize Interior Entrances") != "Off";
    bool randomizedOverworldEntrances = *FindSetting("Randomize Overworld Entrances") != "Off";
    bool wolfStart = *FindSetting("Starting Form") == "Wolf";

    // Prologue is not compatible with wolf start and any ER
    if (wolfStart || randomizedStartingSpawn || randomizedDungeonEntrances || randomizedBossEntrances ||
        randomizedGrottoEntrances || randomizedCaveEntrances || randomizedInteriorEntrances || randomizedOverworldEntrances)
    {
        FindSetting("Skip Prologue")->SetCurrentOption("On");
    }

    // Faron Twilight isn't compatible with cave or overworld ER
    if (randomizedCaveEntrances || randomizedOverworldEntrances) {
        FindSetting("Faron Twilight Cleared")->SetCurrentOption("On");
    }

    // Eldin and Lanayru Twilights aren't compatible with overworld ER
    if (randomizedOverworldEntrances) {
        FindSetting("Eldin Twilight Cleared")->SetCurrentOption("On");
        FindSetting("Lanayru Twilight Cleared")->SetCurrentOption("On");
    }

    // MDH is not compatible with any ER
    if (randomizedStartingSpawn || randomizedDungeonEntrances || randomizedBossEntrances ||
        randomizedGrottoEntrances || randomizedCaveEntrances || randomizedInteriorEntrances || randomizedOverworldEntrances)
    {
        FindSetting("Skip Midna's Desperate Hour")->SetCurrentOption("On");
    }
}

}  // namespace randomizer::ui
