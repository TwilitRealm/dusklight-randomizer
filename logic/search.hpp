#pragma once

#include "item_pool.hpp"
#include "../utility/log.hpp"

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <list>
#include <memory>
#include <iostream>
#include <optional>

// Forward Declarations (we have a lot here)
namespace tphdr::logic::world
{
    class World;
    using WorldPool = std::vector<std::unique_ptr<World>>;
} // namespace tphdr::logic::world

namespace tphdr::logic::item
{
    class Item;
}

namespace tphdr::logic::location
{
    class Location;
}

namespace tphdr::logic::area
{
    class EventAccess;
    class LocationAccess;
    class Area;
} // namespace tphdr::logic::area

namespace tphdr::logic::entrance
{
    class Entrance;
}

namespace tphdr::logic::search
{
    enum class SearchMode
    {
        ACCESSIBLE_LOCATIONS,
        GAME_BEATABLE,
        ALL_LOCATIONS_REACHABLE,
        GENERATE_PLAYTHROUGH,
        SPHERE_ZERO,
        TRACKER_SPHERES
    };

    class Search
    {
       public:
        Search(const SearchMode& searchMode,
               tphdr::logic::world::WorldPool* worlds,
               const tphdr::logic::item_pool::ItemPool& items = {},
               const int& worldToSearch = -1);

        static auto Accessible(tphdr::logic::world::WorldPool* worlds,
                               const tphdr::logic::item_pool::ItemPool& items = {},
                               const int& worldToSearch = -1)
        {
            return Search(SearchMode::ACCESSIBLE_LOCATIONS, worlds, items, worldToSearch);
        }

        static auto AllLocationsReachable(tphdr::logic::world::WorldPool* worlds,
                                          const tphdr::logic::item_pool::ItemPool& items = {},
                                          const int& worldToSearch = -1)
        {
            return Search(SearchMode::ALL_LOCATIONS_REACHABLE, worlds, items, worldToSearch);
        }

        static auto Playthrough(tphdr::logic::world::WorldPool* worlds,
                                const tphdr::logic::item_pool::ItemPool& items = {},
                                const int& worldToSearch = -1)
        {
            return Search(SearchMode::GENERATE_PLAYTHROUGH, worlds, items, worldToSearch);
        }

        static auto Beatable(tphdr::logic::world::WorldPool* worlds,
                             const tphdr::logic::item_pool::ItemPool& items = {},
                             const int& worldToSearch = -1)
        {
            return Search(SearchMode::GAME_BEATABLE, worlds, items, worldToSearch);
        }

        static auto SphereZero(tphdr::logic::world::WorldPool* worlds,
                             const tphdr::logic::item_pool::ItemPool& items = {},
                             const int& worldToSearch = -1)
        {
            return Search(SearchMode::SPHERE_ZERO, worlds, items, worldToSearch);
        }

        void SearchWorlds();

        /**
         *  @brief Loop through and see if there are any events that are now accessible. Add them to the ownedEvents list if
         * they are.
         *
         */
        void ProcessEvents();
        void ProcessExits();
        void ProcessLocations(std::list<tphdr::logic::area::LocationAccess*>& itemLocations);
        void ProcessLocation(tphdr::logic::location::Location* location);
        void Explore(tphdr::logic::area::Area* area);
        void ExpandFormTimes(tphdr::logic::area::Area* area);

        void AddExitToEntranceSpheres(tphdr::logic::entrance::Entrance*);
        void RemoveEmptySpheres();

        /**
         *  @brief Will dump a file which can be turned into a visual graph using graphviz
         *  https://graphviz.org/download/
         *  Use this command to generate the graph: "dot -Tsvg <filename> -o world.svg"
         *  Then, open world.svg in a browser and CTRL + F to find the area of interest
         */
        void DumpWorldGraph(const int& world = 0);

        SearchMode _searchMode;
        tphdr::logic::world::WorldPool* _worlds;
        int _worldToSearch = -1;

        // Search variables
        int _sphereNum = 0;
        bool _newThingsFound = true;
        bool _isBeatable = false;
        bool _collectItems = true;
        std::unordered_set<int> _ownedEvents;
        std::unordered_multiset<tphdr::logic::item::Item*> _ownedItems;

        std::list<tphdr::logic::area::EventAccess*> _eventsToTry;
        std::list<tphdr::logic::entrance::Entrance*> _exitsToTry;
        std::unordered_set<tphdr::logic::location::Location*> _visitedLocations;
        std::unordered_set<tphdr::logic::area::Area*> _visitedAreas;
        std::unordered_set<tphdr::logic::entrance::Entrance*> _successfulExits;
        std::unordered_set<tphdr::logic::entrance::Entrance*> _playthroughEntrances;
        bool _foundDisconnectedExit = false;

        std::list<std::list<tphdr::logic::location::Location*>> _playthroughSpheres;
        std::list<std::list<tphdr::logic::entrance::Entrance*>> _entranceSpheres;

        std::unordered_map<tphdr::logic::area::Area*, int> _areaFormTime;
    };

    /**
     * @brief Verifies that necessary logic for all worlds is satisfied.
     * 
     * @param worlds The worlds to verify logic for
     * @param items The pool of items that haven't been placed yet
     * 
     * @return An optional value that holds a string explaining why the logic was not satisfied if validation failed
     */
    std::optional<std::string> VerifyLogic(tphdr::logic::world::WorldPool* worlds,
                                           const tphdr::logic::item_pool::ItemPool& items = {});
    void GeneratePlaythrough(tphdr::logic::world::WorldPool* worlds);
    bool GameBeatable(tphdr::logic::world::WorldPool* worlds, const tphdr::logic::item_pool::ItemPool& items = {});
} // namespace tphdr::logic::search
