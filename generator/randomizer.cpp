#include "randomizer.hpp"

#include "logic/entrance_shuffle.hpp"
#include "logic/fill.hpp"
#include "logic/flatten/flatten.hpp"
#include "logic/plandomizer.hpp"
#include "logic/search.hpp"
#include "logic/spoiler_log.hpp"
#include "logic/world.hpp"
#include "seedgen/config.hpp"
#include "seedgen/settings.hpp"
#include "utility/time.hpp"

#include <iostream>

#include "SDL3/SDL_filesystem.h"
#include "dusk/app_info.hpp"
#include "dusk/logging.h"

namespace randomizer
{
    std::optional<std::string> Randomizer::Generate()
    {
        try
        {
            GenerateWorlds();
        }
        catch(const std::exception& e)
        {
            std::cout << "============================================================" << std::endl;
            std::cout << "The following exception occured: " << e.what() << std::endl;
            return e.what();
        }

        return std::nullopt;
    }

    void Randomizer::LoadConfig() {
        this->_config.LoadFromFile(GetConfigPath(), GetPrefPath());
    }

    void Randomizer::GenerateWorlds()
    {
        utility::time::ScopedTimer<"Seed generation took ", std::chrono::milliseconds> timer;
        this->_worlds.clear();
        this->_eventIdCounter = 0;
        this->_areaIdCounter = 0;
        this->_locAccIdCounter = 0;
        this->_playthroughSpheres.clear();
        this->_entranceSpheres.clear();

#if RANDOMIZER_ONLY
        const auto result = SDL_GetPrefPath(dusk::OrgName, dusk::AppName);
        if (!result)
            DuskLog.fatal("Unable to get PrefPath: {}", SDL_GetError());
        SetBaseOutputPath(result);
        LoadConfig();
#endif

        utility::platform::Log(std::string("Seed: ") + this->_config.GetSeed());

        seedgen::config::SeedRNG(this->_config, true, false);
        // Set the hash now before anything else random is decided. This allows us to show the hash for a seed
        // before generating it later
        auto hash = this->_config.GetHash();
        utility::platform::Log(std::string("Hash: ") + hash);

        // Build all worlds
        int worldId = 1;
        for (const auto& settings : this->_config.GetSettingsList())
        {
            std::unique_ptr<logic::world::World> world = std::make_unique<logic::world::World>(worldId++, this);
            world->SetSettings(settings);
            world->ResolveRandomSettings();
            world->ResolveConflictingSettings();
            world->Build();
            this->_worlds.emplace_back(std::move(world));
        }

        // Process Plando Data for all worlds
        if (this->_config.IsUsingPlandomizer())
        {
            logic::plandomizer::LoadPlandomizerData(this->_worlds, this->_config.GetPlandomizerPath());
        }

        // Pre Entrance Shuffle Tasks
        for (auto& world : this->_worlds)
        {
            world->PerformPreEntranceShuffleTasks();
        }

        utility::platform::Log("Shuffling Entrances...");
        for (auto& world : this->_worlds)
        {
            logic::entrance_shuffle::ShuffleWorldEntrances(world.get(), this->_worlds);
        }

        // Post Entrance Shuffle Tasks
        for (auto& world : this->_worlds)
        {
            world->PerformPostEntranceShuffleTasks();
        }
        logic::fill::CacheExitTimeForms(this->_worlds);

        // Flattening isn't used for anything yet, but flattens down the requirements for
        // each location and entrance into a single statement. This will be useful for hints and could potentially
        // be used to speed up the fill algorithm (but the fill algorithm is already pretty fast, so we'd only gain maybe like
        // 0.2 seconds back or something)
        utility::platform::Log("Flattening...");
        FlattenSearch search = FlattenSearch(this->_worlds.at(0).get());
        search.doSearch();

        utility::platform::Log("Filling Worlds...");
        logic::fill::FillWorlds(this->_worlds);

        // Post Fill Tasks
        for (auto& world : this->_worlds)
        {
            world->PerformPostFillTasks();
        }

        // Generate Playthrough
        logic::search::GeneratePlaythrough(this);

        // TODO: Generate Hints

        // Write Logs
        if (this->_config.IsGeneratingSpoilerLog())
        {
            logic::spoiler_log::GenerateSpoilerLog(this);
        }
        logic::spoiler_log::GenerateAntiSpoilerLog(this);
    }

    std::string Randomizer::GetSeedOutputPath()
    {
        return this->_baseOutputPath + "seeds/" + this->_config.GetHash() + "/";
    }
} // namespace randomizer
