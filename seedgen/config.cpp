#include "config.hpp"

#include "zlib-ng.h"
#include "seed.hpp"
#include "../utility/log.hpp"
#include "../utility/platform.hpp"
#include "../utility/random.hpp"
#include "../utility/yaml.hpp"

#include <iostream>

namespace randomizer::seedgen::config
{

    void Config::LoadFromFile(const fspath& settingsPath,
                              const fspath& preferencesPath,
                              const bool& createIfNotFound /*= true*/,
                              const bool& allowRewrite /*= true*/)
    {
        // Create files for settings/preferences if they don't exist
        std::ifstream settingsFile(settingsPath);
        std::ifstream preferencesFile(preferencesPath);

        if (settingsFile.is_open() == false)
        {
            if (createIfNotFound)
            {
                WriteDefaultSettings(settingsPath);
            }
            else
            {
                throw std::runtime_error("Could not open settings file at \"" + settingsPath.generic_string() + "\"");
            }
        }

        if (preferencesFile.is_open() == false)
        {
            if (createIfNotFound)
            {
                WriteDefaultPreferences(preferencesPath);
            }
            else
            {
                throw std::runtime_error("Could not open preferences file at \"" + preferencesPath.generic_string() + "\"");
            }
        }
        settingsFile.close();
        preferencesFile.close();

        this->_settingsList.clear();
        this->_settingsList.push_front(settings::Settings());
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
                    randomizer::utility::platform::Log(std::string("Setting \"") + settingName + "\" has no option \"" +
                                                  settingOption + "\". Reverting to default \"" +
                                                  settingInfo->GetDefaultOption() + "\"");
                    settingOption = settingInfo->GetDefaultOption();
                    rewriteSettings = true;
                }

                settings.InsertSetting(settingName, settings::Setting(settingInfo.get(), settingOption));
            }
            // Special handling for starting inventory
            else if (settingName == "Starting Inventory")
            {
                for (const auto& inventoryNode : settingNode.second)
                {
                    const auto& itemName = inventoryNode.first.as<std::string>();
                    const auto& count = inventoryNode.second.as<int>();

                    settings.AddStartingItem(itemName, count);
                }
            }
            // Special Handling for Excluded Locations
            else if (settingName == "Excluded Locations")
            {
                for (const auto& locationNode : settingNode.second)
                {
                    const auto& locationName = locationNode.as<std::string>();
                    settings.AddExcludedLocation(locationName);
                }
            }
            // Special Handling for Mixed Entrance Pools
            else if (settingName == "Mixed Entrance Pools")
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
            else if (settingName == "Seed")
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
            else if (settingName == "Plandomizer")
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
                    randomizer::utility::platform::Log(std::string("Preference \"") + preferenceName + " has no option \"" +
                                                  preferenceOption + "\". Reverting to default \"" +
                                                  preferenceInfo->GetDefaultOption() + "\"");
                    preferenceOption = preferenceInfo->GetDefaultOption();
                    rewriteSettings = true;
                }

                settings.InsertSetting(preferenceName,
                                       settings::Setting(preferenceInfo.get(), preferenceOption));
            }
            else if (preferenceName == "Game Base Path")
            {
                const auto& gameBasePath = preferenceNode.second.as<std::string>();
                this->_gameBasePath = gameBasePath;
            }
            else if (preferenceName == "Output Path")
            {
                const auto& outputPath = preferenceNode.second.as<std::string>();
                this->_outputPath = outputPath;
            }
            else if (preferenceName == "Plandomizer Path")
            {
                const auto& plandomizerPath = preferenceNode.second.as<std::string>();
                this->_plandomizerPath = plandomizerPath;
            }
        }

        // Add in any missing settings/preferences
        for (auto& [settingName, settingInfo] : *settingInfoMap)
        {
            if (!settings.GetMap().contains(settingName))
            {
                settings.InsertSetting(settingName,
                                       settings::Setting(settingInfo.get(), settingInfo->GetDefaultOption()));
                randomizer::utility::platform::Log(std::string("Added missing setting \"") + settingName + "\"");
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
        if (!settingsTree["Seed"])
        {
            this->_seed = seed::GenerateSeed();
            randomizer::utility::platform::Log("Seed is missing. Generated new seed.");
            rewriteSettings = true;
        }
        if (!settingsTree["Plandomizer"] || !settingsTree["Generate Spoiler Log"] || !settingsTree["Starting Inventory"] ||
            !settingsTree["Excluded Locations"] || !settingsTree["Mixed Entrance Pools"])
        {
            rewriteSettings = true;
        }
        if (!preferencesTree["Game Base Path"] || !preferencesTree["Output Path"] || !preferencesTree["Plandomizer Path"])
        {
            rewritePreferences = true;
        }

        // Rewrite files if deemed necessary
        if (allowRewrite && rewriteSettings)
        {
            randomizer::utility::platform::Log(std::string("Rewriting ") + settingsPath.generic_string());
            this->WriteSettingsToFile(settingsPath);
        }
        if (allowRewrite && rewritePreferences)
        {
            randomizer::utility::platform::Log(std::string("Rewriting ") + preferencesPath.generic_string());
            this->WritePreferencesToFile(preferencesPath);
        }
    }

    YAML::Node Config::SettingsToYaml()
    {
        YAML::Node out;
        for (auto& settings : this->_settingsList)
        {
            out["Seed"] = this->_seed;
            out["Plandomizer"] = this->_isUsingPlandomizer;
            out["Generate Spoiler Log"] = this->_isGeneratingSpoilerLog;

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

            out["Starting Inventory"] = std::map<std::string, int>();
            for (const auto& [itemName, count] : settings.GetStartingInventory())
            {
                out["Starting Inventory"][itemName] = count;
            }

            out["Excluded Locations"] = std::list<std::string>();
            for (const auto& locationName : settings.GetExcludedLocations())
            {
                out["Excluded Locations"].push_back(locationName);
            }

            out["Mixed Entrance Pools"] = std::list<std::list<std::string>>();
            int i = 0;
            for (const auto& pool : settings.GetMixedEntrancePools())
            {
                out["Mixed Entrance Pools"].push_back({});
                for (const auto& type : pool)
                {
                    out["Mixed Entrance Pools"][i].push_back(type);
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
            out["Game Base Path"] = this->_gameBasePath.generic_string();
            out["Output Path"] = this->_outputPath.generic_string();
            out["Plandomizer Path"] = this->_plandomizerPath.generic_string();
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

    std::string Config::GetHash()
    {
        if (this->_hash.empty())
        {
            this->_hash = seed::GenerateHash();
        }

        return this->_hash;
    }

    int WriteDefaultSettings(const fspath& settingsPath)
    {
        randomizer::utility::platform::Log("Creating Default Settings");
        std::ofstream settingsFile(settingsPath);
        if (settingsFile.is_open() == false)
        {
            LOG_TO_ERROR("Could not open file to write default settings.");
            return 1;
        }

        auto settingInfoMap = settings::GetAllSettingsInfo();

        YAML::Node root;
        root["Seed"] = seed::GenerateSeed();
        root["Plandomizer"] = false;
        root["Generate Spoiler Log"] = true;
        // TODO: root["Permalink"] = permalink::GeneratePermalink();
        for (const auto& [name, info] : *settingInfoMap)
        {
            if (info->GetType() == settings::Type::STANDARD)
            {
                root[name] = info->GetDefaultOption();
            }
        }
        root["Starting Inventory"] = std::map<std::string, int>();
        root["Excluded Locations"] = std::list<std::string>();
        root["Mixed Entrance Pools"] = std::list<std::list<std::string>>();

        settingsFile << root;
        settingsFile.close();

        return 0;
    }

    int WriteDefaultPreferences(const fspath& preferencesPath)
    {
        randomizer::utility::platform::Log("Creating Default Preferences");
        std::ofstream preferencesFile(preferencesPath);
        if (preferencesFile.is_open() == false)
        {
            LOG_TO_ERROR("Could not open file to write default preferences.");
            return 1;
        }

        auto settingInfoMap = settings::GetAllSettingsInfo();

        YAML::Node root;
        root["Game Base Path"] = "";
        root["Output Path"] = "";
        root["Plandomizer Path"] = "";
        for (const auto& [name, info] : *settingInfoMap)
        {
            if (info->GetType() == settings::Type::PREFERENCE)
            {
                root[name] = info->GetDefaultOption();
            }
        }
        preferencesFile << root;
        preferencesFile.close();

        return 0;
    }

    int SeedRNG(Config& config,
                const bool& resolveNonStandardRandom /* = false */,
                const bool& ignoreInvalidPlandomizer /* = true */)
    {
        // Seed with system time incase we have to choose random preferences during seeding
        auto seed = static_cast<uint32_t>(std::random_device {}());
        randomizer::utility::random::RandomInit(seed);

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
            auto retVal = randomizer::utility::file::GetContents(config.GetPlandomizerPath(), plandomizerContents);
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

        const size_t integerSeed = zng_crc32(0L, reinterpret_cast<const uint8_t*>(hashStr.data()), hashStr.length());
        randomizer::utility::random::RandomInit(integerSeed);

        return 0;
    }
} // namespace randomizer::seedgen::config
