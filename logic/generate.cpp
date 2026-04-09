#include "generate.hpp"

#include "entrance_shuffle.hpp"
#include "fill.hpp"
#include "flatten/flatten.hpp"
#include "plandomizer.hpp"
#include "search.hpp"
#include "spoiler_log.hpp"
#include "../seedgen/config.hpp"
#include "../seedgen/settings.hpp"
#include "../utility/log.hpp"
#include "../utility/time.hpp"

#include <iostream>

namespace randomizer::logic::generate
{
    randomizer::logic::world::WorldPool GenerateWorlds()
    {
        randomizer::utility::time::ScopedTimer<"Seed generation took ", std::chrono::milliseconds> timer;
        randomizer::seedgen::config::Config config;
        config.LoadFromFile(SETTINGS_PATH, PREFERENCES_PATH);

        randomizer::utility::platform::Log(std::string("Seed: ") + config.GetSeed());

        randomizer::logic::world::WorldPool worlds = {};
        GenerateRandomizer(config, worlds);

        return std::move(worlds);
    }

    void GenerateRandomizer(randomizer::seedgen::config::Config& config, randomizer::logic::world::WorldPool& worlds)
    {
        randomizer::seedgen::config::SeedRNG(config, true, false);
        // Set the hash now before anything else random is decided. This allows us to show the hash for a seed
        // before generating it later
        auto hash = config.GetHash();
        randomizer::utility::platform::Log(std::string("Hash: ") + hash);

        // Build all worlds
        int worldId = 1;
        for (const auto& settings : config.GetSettingsList())
        {
            std::unique_ptr<randomizer::logic::world::World> world = std::make_unique<randomizer::logic::world::World>(worldId++);
            world->SetSettings(settings);
            world->ResolveRandomSettings();
            world->ResolveConflictingSettings();
            world->Build();
            worlds.emplace_back(std::move(world));
        }

        // Give each world a pointer to the world pool
        for (auto& world : worlds)
        {
            world->SetWorlds(&worlds);
        }

        // Process Plando Data for all worlds
        if (config.IsUsingPlandomizer())
        {
            randomizer::logic::plandomizer::LoadPlandomizerData(worlds, config.GetPlandomizerPath());
        }

        // Pre Entrance Shuffle Tasks
        for (auto& world : worlds)
        {
            world->PerformPreEntranceShuffleTasks();
        }

        randomizer::utility::platform::Log("Shuffling Entrances...");
        for (auto& world : worlds)
        {
            randomizer::logic::entrance_shuffle::ShuffleWorldEntrances(world.get(), worlds);
        }

        // Post Entrance Shuffle Tasks
        for (auto& world : worlds)
        {
            world->PerformPostEntranceShuffleTasks();
        }
        randomizer::logic::fill::CacheExitTimeForms(worlds);

        // Flattening isn't used for anything yet, but flattens down the requirements for
        // each location and entrance into a single statement. This will be useful for hints and could potentially
        // be used to speed up the fill algorithm (but the fill algorithm is already pretty fast, so we'd only gain maybe like
        // 0.2 seconds back or something)
        randomizer::utility::platform::Log("Flattening...");
        FlattenSearch search = FlattenSearch(worlds.at(0).get());
        search.doSearch();

        randomizer::utility::platform::Log("Filling Worlds...");
        randomizer::logic::fill::FillWorlds(worlds);

        // Post Fill Tasks
        for (auto& world : worlds)
        {
            world->PerformPostFillTasks();
        }

        // Generate Playthrough
        randomizer::logic::search::GeneratePlaythrough(&worlds);

        // TODO: Generate Hints

        // Write Logs
        if (config.IsGeneratingSpoilerLog())
        {
            randomizer::logic::spoiler_log::GenerateSpoilerLog(worlds, config);
        }
        randomizer::logic::spoiler_log::GenerateAntiSpoilerLog(worlds, config);
    }
} // namespace randomizer::logic::generate
