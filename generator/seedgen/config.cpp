#include "config.hpp"

#include "seed.hpp"
#include "../utility/crc32.hpp"
#include "../utility/log.hpp"
#include "../utility/platform.hpp"
#include "../utility/random.hpp"
#include "../utility/yaml.hpp"

#include <iostream>

// Fields which aren't part of settings_list.yaml
constexpr std::string_view SEED = "Seed";
constexpr std::string_view PLANDOMIZER = "Plandomizer";
constexpr std::string_view PLANDOMIZER_PATH = "Plandomizer Path";
constexpr std::string_view STARTING_INVENTORY = "Starting Inventory";
constexpr std::string_view EXCLUDED_LOCATIONS = "Excluded Locations";
constexpr std::string_view MIXED_ENTRANCE_POOLS = "Mixed Entrance Pools";
constexpr std::string_view GENERATE_SPOILER_LOG = "Generate Spoiler Log";

namespace randomizer::seedgen::config
{
    Config::Config() {
        // Create at least one player's settings
        this->_settingsList.push_front(settings::Settings());
    }

    Config::Config(const fspath& settingsPath, const fspath& preferencesPath) {
        // Create at least one player's settings
        this->_settingsList.push_front(settings::Settings());
        LoadFromFile(settingsPath, preferencesPath);
    }

    void Config::ResetSettingsToDefault() {
        for (auto& settings : this->_settingsList) {
            for (auto& [settingName, setting] : settings.GetMap()) {
                if (setting.GetInfo()->GetType() == settings::Type::STANDARD) {
                    setting.SetCurrentOption(setting.GetInfo()->GetDefaultOption());
                }
            }
            settings.GetModifiableExcludedLocations().clear();
            settings.GetModifiableStartingInventory().clear();
            settings.GetModifiableMixedEntrancePools().clear();
        }
    }

    void Config::ResetPreferencesToDefault() {
        for (auto& settings : this->_settingsList) {
            for (auto& [settingName, setting] : settings.GetMap()) {
                if (setting.GetInfo()->GetType() == settings::Type::PREFERENCE) {
                    setting.SetCurrentOption(setting.GetInfo()->GetDefaultOption());
                }
            }
            this->_plandomizerPath = "";
        }
    }

    void Config::LoadFromFile(const fspath& settingsPath,
                              const fspath& preferencesPath,
                              bool createIfNotFound /*= true*/,
                              bool allowRewrite /*= true*/)
    {
        // Create files for settings/preferences if they don't exist
        if (!std::filesystem::exists(settingsPath))
        {
            if (createIfNotFound)
            {
                WriteSettingsToFile(settingsPath);
            }
            else
            {
                throw std::runtime_error("Could not open settings file at \"" + settingsPath.generic_string() + "\"");
            }
        }

        if (!std::filesystem::exists(preferencesPath))
        {
            if (createIfNotFound)
            {
                WritePreferencesToFile(preferencesPath);
            }
            else
            {
                throw std::runtime_error("Could not open preferences file at \"" + preferencesPath.generic_string() + "\"");
            }
        }

        auto& settings = this->_settingsList.front();

        // Load settings info
        auto settingInfoMap = settings::GetAllSettingsInfo();

        // Read in settings and preferences. If we have to change anything,
        // rewrite the appropriate file if allowed.
        bool rewriteSettings = false;
        auto settingsTree = LoadYAML(settingsPath);

        // Loop through all setting fields
        for (const auto& settingNode : settingsTree)
        {
            const auto& settingName = settingNode.first.as<std::string>();
            // Insert the setting if it's in the info map
            if (settingInfoMap->contains(settingName))
            {
                auto& settingInfo = settingInfoMap->at(settingName);
                auto settingOption = settingNode.second.as<std::string>();

                // If the option doesn't exist, revert to default and rewrite later if necessary
                if (settingInfo->GetIndexOfOption(settingOption) == -1)
                {
                    utility::platform::Log(std::string("Setting \"") + settingName + "\" has no option \"" +
                                                  settingOption + "\". Reverting to default \"" +
                                                  settingInfo->GetDefaultOption() + "\"");
                    settingOption = settingInfo->GetDefaultOption();
                    rewriteSettings = true;
                }

                settings.GetMap().at(settingName).SetCurrentOption(settingOption);
            }
            // Special handling for starting inventory
            else if (settingName == STARTING_INVENTORY)
            {
                for (const auto& inventoryNode : settingNode.second)
                {
                    const auto& itemName = inventoryNode.first.as<std::string>();
                    const auto& count = inventoryNode.second.as<int>();

                    settings.AddStartingItem(itemName, count);
                }
            }
            // Special Handling for Excluded Locations
            else if (settingName == EXCLUDED_LOCATIONS)
            {
                for (const auto& locationNode : settingNode.second)
                {
                    const auto& locationName = locationNode.as<std::string>();
                    settings.AddExcludedLocation(locationName);
                }
            }
            // Special Handling for Mixed Entrance Pools
            else if (settingName == MIXED_ENTRANCE_POOLS)
            {
                for (const auto& poolNode : settingNode.second)
                {
                    if (!poolNode.IsSequence())
                    {
                        throw std::runtime_error("Mixed Entrance Pools is not a nested sequence of strings");
                    }
                    settings.AddMixedPool(poolNode.as<std::list<std::string>>());
                }
            }
            // Special handling for Seed
            else if (settingName == SEED)
            {
                const auto& seed = settingNode.second.as<std::string>();
                this->_seed = seed;

                // If seed is empty string, generate a new one
                if (this->_seed.empty())
                {
                    this->_seed = seed::GenerateSeed();
                }
            }
            // Special handling for Plandomizer
            else if (settingName == PLANDOMIZER)
            {
                const auto& plandomizer = settingNode.second.as<bool>(false);
                this->_isUsingPlandomizer = plandomizer;
            }
        }

        // Loop through all preference fields
        bool rewritePreferences = false;
        auto preferencesTree = LoadYAML(preferencesPath);
        for (const auto& preferenceNode : preferencesTree)
        {
            const auto& preferenceName = preferenceNode.first.as<std::string>();
            // Insert the preference if it's in the info map
            if (settingInfoMap->contains(preferenceName))
            {
                auto& preferenceInfo = settingInfoMap->at(preferenceName);
                auto preferenceOption = preferenceNode.second.as<std::string>();

                // If the option doesn't exist, revert to default and rewrite later if necessary
                if (preferenceInfo->GetIndexOfOption(preferenceOption) == -1)
                {
                    utility::platform::Log(std::string("Preference \"") + preferenceName + " has no option \"" +
                                                  preferenceOption + "\". Reverting to default \"" +
                                                  preferenceInfo->GetDefaultOption() + "\"");
                    preferenceOption = preferenceInfo->GetDefaultOption();
                    rewritePreferences = true;
                }

                settings.GetMap().at(preferenceName).SetCurrentOption(preferenceOption);
            }
            else if (preferenceName == PLANDOMIZER_PATH)
            {
                const auto& plandomizerPath = preferenceNode.second.as<std::string>();
                this->_plandomizerPath = plandomizerPath;
            }
        }

        // Rewrite the file(s) if any settings or preferences are missing
        for (auto& [settingName, settingInfo] : *settingInfoMap)
        {
            if (!settingsTree[settingName])
            {
                utility::platform::Log(std::string("Added missing setting \"") + settingName + "\"");
                if (settingInfo->GetType() == settings::Type::STANDARD)
                {
                    rewriteSettings = true;
                }
                else if (settingInfo->GetType() == settings::Type::PREFERENCE)
                {
                    rewritePreferences = true;
                }
            }
        }
        if (!settingsTree[SEED])
        {
            this->_seed = seed::GenerateSeed();
            utility::platform::Log("Seed is missing. Generated new seed.");
            rewriteSettings = true;
        }
        if (!settingsTree[PLANDOMIZER] || !settingsTree[GENERATE_SPOILER_LOG] || !settingsTree[STARTING_INVENTORY] ||
            !settingsTree[EXCLUDED_LOCATIONS] || !settingsTree[MIXED_ENTRANCE_POOLS])
        {
            rewriteSettings = true;
        }
        if (!preferencesTree[PLANDOMIZER_PATH])
        {
            rewritePreferences = true;
        }

        // Rewrite files if deemed necessary
        if (allowRewrite && rewriteSettings)
        {
            utility::platform::Log(std::string("Rewriting ") + settingsPath.generic_string());
            this->WriteSettingsToFile(settingsPath);
        }
        if (allowRewrite && rewritePreferences)
        {
            utility::platform::Log(std::string("Rewriting ") + preferencesPath.generic_string());
            this->WritePreferencesToFile(preferencesPath);
        }
    }

    YAML::Node Config::SettingsToYaml()
    {
        YAML::Node out;
        for (auto& settings : this->_settingsList)
        {
            out[SEED] = this->_seed;
            out[PLANDOMIZER] = this->_isUsingPlandomizer;
            out[GENERATE_SPOILER_LOG] = this->_isGeneratingSpoilerLog;

            // Sort settings by id to keep relevant settings close together in the settings file
            std::list<std::string> sortedNames = {};
            for (auto& [settingName, setting] : settings.GetMap())
            {
                sortedNames.push_back(settingName);
            }
            sortedNames.sort(
                [&](const auto& a, const auto& b)
                { return settings.GetMap().at(a).GetInfo()->GetID() < settings.GetMap().at(b).GetInfo()->GetID(); });

            for (const auto& settingName : sortedNames)
            {
                auto& setting = settings.GetMap().at(settingName);
                if (setting.GetInfo()->GetType() == settings::Type::STANDARD)
                {
                    out[settingName] = setting.GetCurrentOption();
                }
            }

            out[STARTING_INVENTORY] = std::map<std::string, int>();
            for (const auto& [itemName, count] : settings.GetStartingInventory())
            {
                out[STARTING_INVENTORY][itemName] = count;
            }

            out[EXCLUDED_LOCATIONS] = std::list<std::string>();
            for (const auto& locationName : settings.GetExcludedLocations())
            {
                out[EXCLUDED_LOCATIONS].push_back(locationName);
            }

            out[MIXED_ENTRANCE_POOLS] = std::list<std::list<std::string>>();
            int i = 0;
            for (const auto& pool : settings.GetMixedEntrancePools())
            {
                out[MIXED_ENTRANCE_POOLS].push_back({});
                for (const auto& type : pool)
                {
                    out[MIXED_ENTRANCE_POOLS][i].push_back(type);
                }
                i += 1;
            }
        }

        return out;
    }

    YAML::Node Config::PreferencesToYaml()
    {
        YAML::Node out;
        for (auto& settings : this->_settingsList)
        {
            out[PLANDOMIZER_PATH] = this->_plandomizerPath.generic_string();
            for (auto& [settingName, setting] : settings.GetMap())
            {
                if (setting.GetInfo()->GetType() == settings::Type::PREFERENCE)
                {
                    out[settingName] = setting.GetCurrentOption();
                }
            }
        }

        return out;
    }

    void Config::WriteSettingsToFile(const fspath& settingsPath)
    {
        std::ofstream outputFile(settingsPath);
        if (outputFile.is_open() == false)
        {
            throw std::runtime_error("Unable to open settings file \"" + settingsPath.generic_string() + "\" for writing.");
        }

        outputFile << this->SettingsToYaml();
        outputFile.close();
    }

    void Config::WritePreferencesToFile(const fspath& preferencesPath)
    {
        std::ofstream outputFile(preferencesPath);
        if (outputFile.is_open() == false)
        {
            throw std::runtime_error("Unable to open preferences file \"" + preferencesPath.generic_string() +
                                     "\" for writing.");
        }

        outputFile << this->PreferencesToYaml();
        outputFile.close();
    }

    void Config::WriteToFile(const fspath& settingsPath, const fspath& preferencesPath) {
        WriteSettingsToFile(settingsPath);
        WritePreferencesToFile(preferencesPath);
    }

    std::string Config::GetHash(bool generateIfEmpty)
    {
        if (this->_hash.empty() && generateIfEmpty)
        {
            this->_hash = seed::GenerateHash();
        }

        return this->_hash;
    }

    int SeedRNG(Config& config,
                bool resolveNonStandardRandom /* = false */,
                bool ignoreInvalidPlandomizer /* = true */)
    {
        // Seed with system time incase we have to choose random preferences during seeding
        auto seed = static_cast<uint32_t>(std::random_device {}());
        utility::random::RandomInit(seed);

        // Seed the rng using a combination of the seed and standard settings
        std::string hashStr = config.GetSeed();
        for (auto& settings : config.GetSettingsList())
        {
            for (auto& [settingName, setting] : settings.GetMap())
            {
                if (setting.GetInfo()->GetType() == settings::Type::STANDARD)
                {
                    hashStr += settingName + setting.GetCurrentOption();
                }
                else if (resolveNonStandardRandom)
                {
                    setting.ResolveIfRandom();
                }
            }

            // Special handling for other settings
            for (const auto& [itemName, count] : settings.GetStartingInventory())
            {
                hashStr += itemName + std::to_string(count);
            }

            for (const auto& locationName : settings.GetExcludedLocations())
            {
                hashStr += locationName;
            }

            for (const auto& pool : settings.GetMixedEntrancePools())
            {
                for (const auto& type : pool)
                {
                    hashStr += type;
                }
            }
        }

        // Change the seed if we're using plandomizer
        if (config.IsUsingPlandomizer())
        {
            std::string plandomizerContents;
            auto retVal = utility::file::GetContents(config.GetPlandomizerPath(), plandomizerContents);
            if (!ignoreInvalidPlandomizer && retVal != 0)
            {
                LOG_TO_ERROR("Could not read plandomizer file at \"" + config.GetPlandomizerPath().generic_string() + "\"");
                return 1;
            }
            hashStr += plandomizerContents;
        }

        // Change the seed if we're generating a spoiler log
        if (config.IsGeneratingSpoilerLog())
        {
            hashStr += "Spoiler Log: True";
        }

        const size_t integerSeed = utility::crc32(hashStr.data(), hashStr.length());
        utility::random::RandomInit(integerSeed);

        return 0;
    }
} // namespace randomizer::seedgen::config
