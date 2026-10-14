#pragma once

#include <string>
#include <list>
#include <memory>

namespace randomizer::logic::world
{
    class World;
};
namespace randomizer::logic::location {
    class Location;
}
namespace randomizer::logic::item
{
    enum Importance
    {
        INVALID,
        MAJOR,
        MINOR,
        JUNK,
    };

    Importance ImportanceFromStr(const std::string& str);

    class Item
    {
       public:
        Item() = default;
        Item(int id,
             const std::string& name,
             int apId,
             world::World* world,
             Importance importance,
             bool gameWinningItem,
             bool dungeonSmallKey,
             bool bigKey,
             bool compass,
             bool dungeonMap);

        int GetID() const;
        int GetAPID() const;
        std::string GetName() const;
        world::World* GetWorld() const;
        Importance GetImportance() const;
        bool IsMajor() const;
        bool IsMinor() const;
        bool IsJunk() const;
        bool IsGameWinningItem() const;
        void AddChainLocation(location::Location* location);
        std::list<location::Location*> GetChainLocations() const;
        bool IsDungeonSmallKey() const;
        bool IsBigKey() const;
        bool IsDungeonMap() const;
        bool IsCompass() const;
        bool IsGoldenBug() const;
        bool IsShadowCrystal() const;
        bool IsBottle() const;
        bool IsStamp() const;
        bool CanBeInBarrenRegion() const;
        bool IsSameOrSimilarItem(Item* otherItem) const;

        bool operator==(const Item& rhs) const;
        bool operator<(const Item& rhs) const;

       private:
        int _id = -1;
        int _apId = -1;
        std::string _name;
        world::World* _world = nullptr;
        Importance _importance = INVALID;
        bool _gameWinningItem = false;
        std::list<location::Location*> _chainLocations;

        bool _dungeonSmallKey = false;
        bool _bigKey = false;
        bool _dungeonMap = false;
        bool _compass = false;
        bool _goldenBug = false;
        bool _bottle = false;
        bool _shadowCrystal = false;
        bool _stamp = false;
    };

    extern std::unique_ptr<Item> Nothing;
} // namespace randomizer::logic::item