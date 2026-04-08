#pragma once

#include "location.hpp"

#include <unordered_set>

// Forward declarations
namespace randomizer::logic::item
{
    class Item;
}

namespace randomizer::logic::area
{
    class Area;
}

namespace randomizer::logic::entrance
{
    class Entrance;
}
namespace randomizer::logic::world
{
    class World;
}

namespace randomizer::logic::dungeon
{
    /**
     *  @brief Holds dungeon specific data
     */
    class Dungeon
    {
       public:
        Dungeon(const std::string& name, randomizer::logic::world::World* world);

        std::string GetName() const;
        void SetSmallKey(randomizer::logic::item::Item* item);
        randomizer::logic::item::Item* GetSmallKey() const;
        void SetBigKey(randomizer::logic::item::Item* item);
        randomizer::logic::item::Item* GetBigKey() const;
        void SetCompass(randomizer::logic::item::Item* item);
        randomizer::logic::item::Item* GetCompass() const;
        void SetDungeonMap(randomizer::logic::item::Item* item);
        randomizer::logic::item::Item* GetDungeonMap() const;
        void SetStartingArea(randomizer::logic::area::Area* startingArea);
        randomizer::logic::area::Area* GetStartingAreas();
        void AddStartingEntrance(randomizer::logic::entrance::Entrance* startingEntrance);
        std::unordered_set<randomizer::logic::entrance::Entrance*> GetStartingEntrances() const;
        void AddLocation(randomizer::logic::location::Location* location);
        randomizer::logic::location::LocationPool GetLocations();
        void SetGoalLocation(randomizer::logic::location::Location* goalLocation);
        randomizer::logic::location::Location* GetGoalLocation();
        void SetRequired(const bool& required);
        bool IsRequired() const;

        /**
         *  @brief Returns whether or not the dungeon should be barren given the current settings and placement of dungeon
         * rewards and/or plandomized items
         */
        bool ShouldBeBarren() const;

       private:
        std::string _name = "";
        randomizer::logic::world::World* _world;
        randomizer::logic::item::Item* _smallKey;
        randomizer::logic::item::Item* _bigKey;
        randomizer::logic::item::Item* _compass;
        randomizer::logic::item::Item* _dungeonMap;
        randomizer::logic::area::Area* _startingArea;
        std::unordered_set<randomizer::logic::entrance::Entrance*> _startingEntrances;
        randomizer::logic::location::Location* _goalLocation;
        randomizer::logic::location::LocationPool _locations = {};
        bool _required = false;
    };
} // namespace randomizer::logic::dungeon
