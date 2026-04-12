#pragma once

#include "area.hpp"
#include "dungeon.hpp"
#include "item.hpp"
#include "item_pool.hpp"
#include "location.hpp"
#include "requirement.hpp"

#include "../seedgen/settings.hpp"
#include "../utility/container.hpp"
#include "../utility/log.hpp"

#include <unordered_map>
#include <map>
#include <vector>
#include <memory>

#include <iostream>

// Forward Declarations
namespace randomizer
{
    class Randomizer;
}

namespace randomizer::logic::search
{
    class Search;
}

namespace randomizer::logic::world
{
    class World;
    using WorldPool = std::vector<std::unique_ptr<World>>;

    class World
    {
       public:
        World(const int& id, randomizer::Randomizer* randomizer);

        int GetID() const;
        void SetSettings(const randomizer::seedgen::settings::Settings& settings);
        const randomizer::seedgen::settings::Settings& GetSettings() const;
        void SetRandomizer(Randomizer* randomizer);
        Randomizer* GetRandomizer() const;

        /**
         * @brief Resolves all remaining random settings within a specific world
         */
        void ResolveRandomSettings();
        
        /**
         * @brief Resolves settings that conflict with each other. Ideally will only resolve settings that conflict due to
         * having their current option randomly chosen.
         */
        void ResolveConflictingSettings();
        void Build();
        void BuildItemTable();
        void BuildLocationTable();
        void LoadLogicMacros();
        void LoadWorldGraph();
        bool EvaluateSettingCondition(const std::string& condition);

        /**
         * @brief Generate the main item pool and starting item pool for this world.
         */
        void GenerateItemPools();

        /**
         * @brief Perform all tasks which must be complete before shuffling entrances.
         */
        void PerformPreEntranceShuffleTasks();
        void PlaceVanillaItems();
        void PlacePlandomizerItems();
        void SetNonProgressLocations();

        /**
         * @brief Perform all tasks which require shuffled entrances to be set, but before running the main item placement
         * algorithm.
         */
        void PerformPostEntranceShuffleTasks();
        void AssignAreaProperties();
        void AssignGoalLocations();

        /**
         * @brief Forbid items from being in certain locations depending on settings
         */
        void SetForbiddenItems();

        /**
         *  @brief STUB: Would choose required dungeons ahead of placing any non-vanilla and non-plandomized items. Not really
         *  required unless we let users choose a specific amount of directly required dungeons
         */
        void ChooseRequiredDungeons();

        /**
         *  @brief Determines which dungeons are required based on placed items. Sets required dungeons as such in their
         *  properties. If "Unrequired Dungeons Are Barren" is "On", then unrequired dungeons will have all their locations
         *  progression status set to false.
         */
        void DetermineRequiredDungeons();

        /**
         *  @brief Adds junk to the main pool until the number of items in the pool matches the total number of
         * currently empty locations.
         */
        void SanitizeItemPool();
        void SetSearchStartingProperties(randomizer::logic::search::Search* search) const;
        void PerformPostFillTasks();
        void FinalizeBottleContents();
        void AddPlandomizedLocation(randomizer::logic::location::Location* location, randomizer::logic::item::Item* item);
        void AddPlandomizedEntrance(randomizer::logic::entrance::Entrance* entrance, randomizer::logic::entrance::Entrance* target);
        std::unordered_map<randomizer::logic::entrance::Entrance*, randomizer::logic::entrance::Entrance*> GetPlandomizerEntrances();

        randomizer::logic::dungeon::Dungeon* GetDungeon(const std::string& name);
        const std::map<std::string, std::unique_ptr<randomizer::logic::dungeon::Dungeon>>& GetDungeonTable() const;
        randomizer::logic::item::Item* GetItem(const std::string& name, const bool& ignoreError = false);
        randomizer::logic::item::Item* GetShadowCrystal();
        randomizer::logic::item::Item* GetGameWinningItem() const;
        randomizer::logic::item_pool::ItemPool& GetItemPool();
        randomizer::logic::item_pool::ItemPool& GetStartingItemPool();
        randomizer::logic::location::Location* GetLocation(const std::string& name);
        randomizer::logic::location::LocationPool GetAllLocations(const bool& includeNonItemLocations = false);
        randomizer::logic::area::Area* GetArea(const std::string& name, const bool& createIfNotFound = false);
        randomizer::logic::area::Area* GetRootArea() const;
        const std::map<std::string, std::unique_ptr<randomizer::logic::area::Area>>& GetAreaTable() const;
        randomizer::logic::entrance::Entrance* GetEntrance(const std::string& originalName);
        int GetNewEntranceID();
        randomizer::logic::entrance::EntrancePool GetShuffleableEntrances(const randomizer::logic::entrance::Type& type,
                                                                     const bool& onlyPrimary = false);
        randomizer::logic::entrance::EntrancePool GetShuffledEntrances(
            const randomizer::logic::entrance::Type& type = randomizer::logic::entrance::Type::ALL,
            const bool& onlyPrimary = false);
        std::unordered_map<randomizer::logic::entrance::Entrance*, int>& GetExitTimeFormCache();

        int GetMacroIndex(const std::string& macroName) const;
        const randomizer::logic::requirement::Requirement& GetMacro(const int& macroIndex);
        int GetEventIndex(const std::string& eventName, bool addIfNone = true);
        std::string GetEventName(const int& eventIndex);

        randomizer::seedgen::settings::Setting& Setting(const std::string& settingName);

       private:
        int _id = -1;
        int _entranceIdCounter = 0;

        randomizer::seedgen::settings::Settings _settings;
        std::map<std::string, std::unique_ptr<randomizer::logic::item::Item>> _itemTable = {};
        std::map<std::string, std::unique_ptr<randomizer::logic::location::Location>> _locationTable = {};
        std::unordered_set<std::string> _intentionallyRemovedLocations = {};
        std::unordered_set<std::string> _registeredLocationCategories = {};
        std::map<std::string, std::unique_ptr<randomizer::logic::area::Area>> _areaTable = {};
        std::map<std::string, std::unique_ptr<randomizer::logic::dungeon::Dungeon>> _dungeons = {};
        std::map<int, randomizer::logic::requirement::Requirement> _macros = {};
        std::unordered_map<std::string, int> _macroIndexes = {};
        std::unordered_map<std::string, int> _eventIndexes = {};
        std::unordered_map<int, std::string> _eventNames = {};
        randomizer::logic::item_pool::ItemPool _itemPool = {};
        randomizer::logic::item_pool::ItemPool _startingItemPool = {};
        std::unordered_map<randomizer::logic::entrance::Entrance*, int> _exitTimeFormCache = {};

        // Plandomizer Data
        std::unordered_map<randomizer::logic::location::Location*, randomizer::logic::item::Item*> _plandomizerLocations = {};
        std::unordered_map<randomizer::logic::entrance::Entrance*, randomizer::logic::entrance::Entrance*> _plandomizerEntrances = {};

        Randomizer* _randomizer = nullptr;
    };
} // namespace randomizer::logic::world
