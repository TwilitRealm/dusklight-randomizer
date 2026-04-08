#pragma once

#include "location.hpp"

#include <unordered_set>

// Forward declarations
namespace tphdr::logic::item
{
    class Item;
}

namespace tphdr::logic::area
{
    class Area;
}

namespace tphdr::logic::entrance
{
    class Entrance;
}
namespace tphdr::logic::world
{
    class World;
}

namespace tphdr::logic::dungeon
{
    /**
     *  @brief Holds dungeon specific data
     */
    class Dungeon
    {
       public:
        Dungeon(const std::string& name, tphdr::logic::world::World* world);

        std::string GetName() const;
        void SetSmallKey(tphdr::logic::item::Item* item);
        tphdr::logic::item::Item* GetSmallKey() const;
        void SetBigKey(tphdr::logic::item::Item* item);
        tphdr::logic::item::Item* GetBigKey() const;
        void SetCompass(tphdr::logic::item::Item* item);
        tphdr::logic::item::Item* GetCompass() const;
        void SetDungeonMap(tphdr::logic::item::Item* item);
        tphdr::logic::item::Item* GetDungeonMap() const;
        void SetStartingArea(tphdr::logic::area::Area* startingArea);
        tphdr::logic::area::Area* GetStartingAreas();
        void AddStartingEntrance(tphdr::logic::entrance::Entrance* startingEntrance);
        std::unordered_set<tphdr::logic::entrance::Entrance*> GetStartingEntrances() const;
        void AddLocation(tphdr::logic::location::Location* location);
        tphdr::logic::location::LocationPool GetLocations();
        void SetGoalLocation(tphdr::logic::location::Location* goalLocation);
        tphdr::logic::location::Location* GetGoalLocation();
        void SetRequired(const bool& required);
        bool IsRequired() const;

        /**
         *  @brief Returns whether or not the dungeon should be barren given the current settings and placement of dungeon
         * rewards and/or plandomized items
         */
        bool ShouldBeBarren() const;

       private:
        std::string _name = "";
        tphdr::logic::world::World* _world;
        tphdr::logic::item::Item* _smallKey;
        tphdr::logic::item::Item* _bigKey;
        tphdr::logic::item::Item* _compass;
        tphdr::logic::item::Item* _dungeonMap;
        tphdr::logic::area::Area* _startingArea;
        std::unordered_set<tphdr::logic::entrance::Entrance*> _startingEntrances;
        tphdr::logic::location::Location* _goalLocation;
        tphdr::logic::location::LocationPool _locations = {};
        bool _required = false;
    };
} // namespace tphdr::logic::dungeon
