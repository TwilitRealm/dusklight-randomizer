#include "spoiler_log.hpp"

#include "entrance_shuffle.hpp"
#include "../utility/file.hpp"
#include "../utility/platform.hpp"
#include "../utility/yaml.hpp"

#include <algorithm>
#include <iostream>
#include <fstream>

namespace tphdr::logic::spoiler_log
{
    std::string SpoilerFormatLocation(tphdr::logic::location::Location* location, const size_t& longestNameLength)
    {
        auto numSpaces = longestNameLength - location->GetName().length();
        std::string spaces(numSpaces, ' ');

        return location->GetName() + ": " + spaces + location->GetCurrentItem()->GetName();
    }

    std::string SpoilerFormatEntrance(tphdr::logic::entrance::Entrance* entrance, const size_t& longestNameLength)
    {
        auto numSpaces = longestNameLength - entrance->GetOriginalName().length();
        std::string spaces(numSpaces, ' ');
        auto replacement = entrance->GetReplaces();
        auto parent = replacement->GetParentArea()->GetName();
        auto connected = replacement->GetOriginalConnectedArea()->GetName();

        return entrance->GetOriginalName() + ": " + spaces + connected + " from " + parent;
    }

    void LogBasicInfo(std::ofstream& log, tphdr::seedgen::config::Config& config, tphdr::logic::world::WorldPool& worlds)
    {
        log << "Twilight Princess HD Randomizer Version: " << "1.0.0" << std::endl;
        log << "Seed: " << config.GetSeed() << std::endl;

        // TODO: Setting string

        log << "Hash: " << config.GetHash() << std::endl;
    }

    void LogSettings(std::ofstream& log, tphdr::seedgen::config::Config& config, tphdr::logic::world::WorldPool& worlds)
    {
        log << std::endl << "# Settings" << std::endl;
        log << YAML::Dump(config.SettingsToYaml()) << std::endl;
    }

    void GenerateSpoilerLog(tphdr::logic::world::WorldPool& worlds, tphdr::seedgen::config::Config& config)
    {
        tphdr::utility::platform::Log("Generating Spoiler Log");

        // Create logs folder if it doesn't exist
        if (!tphdr::utility::file::dirExists(LOGS_PATH))
        {
            tphdr::utility::file::create_directories(LOGS_PATH);
        }

        std::string filepath = std::string(LOGS_PATH) + config.GetHash() + " Spoiler Log.txt";
        std::ofstream spoilerLog;
        spoilerLog.open(filepath);

        LogBasicInfo(spoilerLog, config, worlds);

        // Gather worlds with starting inventories
        std::list<tphdr::logic::world::World*> worldswithStartingInventories = {};
        for (const auto& world : worlds)
        {
            if (!world->GetStartingItemPool().empty())
            {
                worldswithStartingInventories.push_back(world.get());
            }
        }
        // Print starting inventories if there are any
        if (!worldswithStartingInventories.empty())
        {
            spoilerLog << std::endl << "Starting Inventory:" << std::endl;
            for (const auto& world : worldswithStartingInventories)
            {
                spoilerLog << "    World " << world->GetID() << ":" << std::endl;
                for (const auto& item : world->GetStartingItemPool())
                {
                    spoilerLog << "      - " << item->GetName() << std::endl;
                }
            }
        }

        // TODO: Print required dungeons

        // Get name lengths for pretty formatting
        size_t longestNameLength = 0;
        for (const auto& sphere : worlds.at(0)->GetPlaythroughSpheres())
        {
            for (const auto& location : sphere)
            {
                longestNameLength = std::max(location->GetName().length(), longestNameLength);
            }
        }

        // Print playthrough
        int sphereNum = 0;
        spoilerLog << std::endl << "Playthrough:" << std::endl;
        for (auto& sphere : worlds.at(0)->GetPlaythroughSpheres())
        {
            sphereNum += 1;
            spoilerLog << "    Sphere " << sphereNum << ":" << std::endl;
            sphere.sort([](const auto& a, const auto& b) { return a->GetName()[0] < b->GetName()[0]; });
            for (const auto& location : sphere)
            {
                spoilerLog << "        " << SpoilerFormatLocation(location, longestNameLength) << std::endl;
            }
        }

        // Get name lengths for pretty formatting
        longestNameLength = 0;
        for (const auto& sphere : worlds.at(0)->GetEntranceSpheres())
        {
            for (const auto& entrance : sphere)
            {
                longestNameLength = std::max(entrance->GetOriginalName().length(), longestNameLength);
            }
        }

        // Print entrance playthrough
        sphereNum = 0;
        if (longestNameLength != 0)
        {
            spoilerLog << std::endl << "Entrance Playthrough:" << std::endl;
        }
        for (auto& sphere : worlds.at(0)->GetEntranceSpheres())
        {
            sphereNum += 1;
            if (sphere.empty())
            {
                continue;
            }
            spoilerLog << "    Sphere " << sphereNum << ":" << std::endl;
            sphere.sort([](auto& e1, auto& e2) { return e1->GetID() < e2->GetID(); });
            for (const auto& entrance : sphere)
            {
                spoilerLog << "        " << SpoilerFormatEntrance(entrance, longestNameLength) << std::endl;
            }
        }

        // Recalculate longest name length for all locations
        longestNameLength = 0;
        for (const auto& world : worlds)
        {
            for (const auto& location : world->GetAllLocations())
            {
                longestNameLength = std::max(location->GetName().length(), longestNameLength);
            }
        }

        // Print All Locations
        spoilerLog << std::endl << "All Locations:" << std::endl;
        for (const auto& world : worlds)
        {
            spoilerLog << "    World " << world->GetID() << ":" << std::endl;
            for (const auto& location : world->GetAllLocations())
            {
                spoilerLog << "        " << SpoilerFormatLocation(location, longestNameLength) << std::endl;
            }
        }

        // Recalculate longest name length for all shuffled entrances
        longestNameLength = 0;
        for (const auto& world : worlds)
        {
            for (const auto& entrance : world->GetShuffledEntrances())
            {
                longestNameLength = std::max(entrance->GetOriginalName().length(), longestNameLength);
            }
        }
        // Print all randomized entrances
        if (longestNameLength != 0)
        {
            spoilerLog << std::endl << "All Entrances:" << std::endl;
        }
        for (const auto& world : worlds)
        {
            auto entrances = world->GetShuffledEntrances();
            if (!entrances.empty())
            {
                spoilerLog << "    World " << world->GetID() << ":" << std::endl;
                // Create entrance pools to easily separate the entrances by type
                auto entrancePools = tphdr::logic::entrance_shuffle::CreateEntrancePools(world.get());
                auto mixedPools = world->GetSettings().GetMixedEntrancePools();
                for (auto& [entranceType, entrancePool] : entrancePools)
                {
                    auto typeStr = tphdr::logic::entrance::TypeToStr(entranceType);
                    // If this is a mixed pool, display the types it mixed
                    if (typeStr.starts_with("Mixed Pool"))
                    {
                        typeStr += " (";
                        auto& pool = mixedPools.front();
                        for (const auto& type : pool)
                        {
                            typeStr += type + " + ";
                        }
                        typeStr.erase(typeStr.end() - 3, typeStr.end()); // Remove the last " + "
                        typeStr += ")";
                        mixedPools.pop_front();
                    }
                    spoilerLog << "        " << typeStr << ":" << std::endl;
                    std::sort(entrancePool.begin(),
                              entrancePool.end(),
                              [](auto& e1, auto& e2) { return e1->GetID() < e2->GetID(); });
                    for (const auto& entrance : entrancePool)
                    {
                        // Ignore entrances that are impossible
                        if (entrance->GetRequirement()._type == tphdr::logic::requirement::Type::IMPOSSIBLE)
                        {
                            continue;
                        }
                        spoilerLog << "            " << SpoilerFormatEntrance(entrance, longestNameLength) << std::endl;
                    }
                }
            }
        }

        // TODO: Hints

        // Log Settings
        LogSettings(spoilerLog, config, worlds);

        spoilerLog.close();

        tphdr::utility::platform::Log("Wrote spoiler log to " + filepath);
    }

    void GenerateAntiSpoilerLog(tphdr::logic::world::WorldPool& worlds, tphdr::seedgen::config::Config& config)
    {
        // Create logs folder if it doesn't exist
        if (!tphdr::utility::file::dirExists(LOGS_PATH))
        {
            tphdr::utility::file::create_directories(LOGS_PATH);
        }

        std::string filepath = std::string(LOGS_PATH) + config.GetHash() + " Anti-Spoiler Log.txt";
        std::ofstream antiSpoilerLog;
        antiSpoilerLog.open(filepath);

        LogBasicInfo(antiSpoilerLog, config, worlds);
        LogSettings(antiSpoilerLog, config, worlds);
    }
} // namespace tphdr::logic::spoiler_log
