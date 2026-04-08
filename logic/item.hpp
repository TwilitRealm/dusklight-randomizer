#pragma once

#include <string>
#include <list>
#include <memory>

namespace tphdr::logic::world
{
    class World;
};
class Location;
namespace tphdr::logic::item
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
        Item(const int& id,
             const std::string& name,
             tphdr::logic::world::World* world,
             const Importance& importance,
             const bool& gameWinningItem,
             const bool& dungeonSmallKey,
             const bool& bigKey,
             const bool& compass,
             const bool& dungeonMap);

        int GetID() const;
        std::string GetName() const;
        tphdr::logic::world::World* GetWorld() const;
        Importance GetImportance() const;
        bool IsMajor() const;
        bool IsMinor() const;
        bool isJunk() const;
        bool IsGameWinningItem() const;
        std::list<Location*> GetChainLocations() const;
        bool IsDungeonSmallKey() const;
        bool IsBigKey() const;
        bool IsDungeonMap() const;
        bool IsCompass() const;
        bool IsGoldenBug() const;
        bool IsShadowCrystal() const;
        bool IsBottle() const;
        bool IsStamp() const;

        bool operator==(const Item& rhs) const;
        bool operator<(const Item& rhs) const;

       private:
        int _id = -1;
        std::string _name;
        tphdr::logic::world::World* _world = nullptr;
        Importance _importance = INVALID;
        bool _gameWinningItem = false;
        std::list<Location*> _chainLocations;

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
} // namespace tphdr::logic::item