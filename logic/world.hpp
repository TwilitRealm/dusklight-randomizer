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
namespace tphdr::logic::search
{
    class Search;
}

namespace tphdr::logic::world
{
    class World;
    using WorldPool = std::vector<std::unique_ptr<World>>;

    class World
    {
       public:
        World(const int& id);

        int GetID() const;
        void SetSettings(const tphdr::seedgen::settings::Settings& settings);
        const tphdr::seedgen::settings::Settings& GetSettings() const;
        void SetWorlds(WorldPool* worlds);

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
        void SetSearchStartingProperties(tphdr::logic::search::Search* search) const;
        void PerformPostFillTasks();
        void FinalizeBottleContents();
        void AddPlandomizedLocation(tphdr::logic::location::Location* location, tphdr::logic::item::Item* item);
        void AddPlandomizedEntrance(tphdr::logic::entrance::Entrance* entrance, tphdr::logic::entrance::Entrance* target);
        std::unordered_map<tphdr::logic::entrance::Entrance*, tphdr::logic::entrance::Entrance*> GetPlandomizerEntrances();

        tphdr::logic::dungeon::Dungeon* GetDungeon(const std::string& name);
        const std::map<std::string, std::unique_ptr<tphdr::logic::dungeon::Dungeon>>& GetDungeonTable() const;
        tphdr::logic::item::Item* GetItem(const std::string& name, const bool& ignoreError = false);
        tphdr::logic::item::Item* GetShadowCrystal();
        tphdr::logic::item::Item* GetGameWinningItem() const;
        tphdr::logic::item_pool::ItemPool& GetItemPool();
        tphdr::logic::item_pool::ItemPool& GetStartingItemPool();
        tphdr::logic::location::Location* GetLocation(const std::string& name);
        tphdr::logic::location::LocationPool GetAllLocations(const bool& includeNonItemLocations = false);
        tphdr::logic::area::Area* GetArea(const std::string& name, const bool& createIfNotFound = false);
        tphdr::logic::area::Area* GetRootArea() const;
        const std::map<std::string, std::unique_ptr<tphdr::logic::area::Area>>& GetAreaTable() const;
        tphdr::logic::entrance::Entrance* GetEntrance(const std::string& originalName);
        int GetNewEntranceID();
        tphdr::logic::entrance::EntrancePool GetShuffleableEntrances(const tphdr::logic::entrance::Type& type,
                                                                     const bool& onlyPrimary = false);
        tphdr::logic::entrance::EntrancePool GetShuffledEntrances(
            const tphdr::logic::entrance::Type& type = tphdr::logic::entrance::Type::ALL,
            const bool& onlyPrimary = false);
        std::unordered_map<tphdr::logic::entrance::Entrance*, int>& GetExitTimeFormCache();

        int GetMacroIndex(const std::string& macroName) const;
        const tphdr::logic::requirement::Requirement& GetMacro(const int& macroIndex);
        int GetEventIndex(const std::string& eventName);
        std::string GetEventName(const int& eventIndex);

        tphdr::seedgen::settings::Setting& Setting(const std::string& settingName);
        void SetPlaythroughSpheres(const std::list<std::list<tphdr::logic::location::Location*>>& playthroughSpheres);
        std::list<std::list<tphdr::logic::location::Location*>> GetPlaythroughSpheres() const;
        void SetEntranceSpheres(const std::list<std::list<tphdr::logic::entrance::Entrance*>>& entranceSpheres);
        std::list<std::list<tphdr::logic::entrance::Entrance*>> GetEntranceSpheres() const;

       private:
        int _id = -1;

        static int _eventIdCounter; // Needs to be shared for events across all worlds
        int _entranceIdCounter = 0; // Specific for this world

        tphdr::seedgen::settings::Settings _settings;
        std::map<std::string, std::unique_ptr<tphdr::logic::item::Item>> _itemTable = {};
        std::map<std::string, std::unique_ptr<tphdr::logic::location::Location>> _locationTable = {};
        std::unordered_set<std::string> _intentionallyRemovedLocations = {};
        std::unordered_set<std::string> _registeredLocationCategories = {};
        std::map<std::string, std::unique_ptr<tphdr::logic::area::Area>> _areaTable = {};
        std::map<std::string, std::unique_ptr<tphdr::logic::dungeon::Dungeon>> _dungeons = {};
        std::map<int, tphdr::logic::requirement::Requirement> _macros = {};
        std::unordered_map<std::string, int> _macroIndexes = {};
        std::unordered_map<std::string, int> _eventIndexes = {};
        std::unordered_map<int, std::string> _eventNames = {};
        tphdr::logic::item_pool::ItemPool _itemPool = {};
        tphdr::logic::item_pool::ItemPool _startingItemPool = {};
        std::unordered_map<tphdr::logic::entrance::Entrance*, int> _exitTimeFormCache = {};

        // Playthroughs will be stored in world 0 for convenience
        std::list<std::list<tphdr::logic::location::Location*>> _playthroughSpheres = {};
        std::list<std::list<tphdr::logic::entrance::Entrance*>> _entranceSpheres = {};

        // Plandomizer Data
        std::unordered_map<tphdr::logic::location::Location*, tphdr::logic::item::Item*> _plandomizerLocations = {};
        std::unordered_map<tphdr::logic::entrance::Entrance*, tphdr::logic::entrance::Entrance*> _plandomizerEntrances = {};

        WorldPool* _worlds = nullptr;
    };
} // namespace tphdr::logic::world
