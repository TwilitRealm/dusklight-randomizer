#pragma once

#include "entrance.hpp"
#include "requirement.hpp"

#include <set>
#include <list>

// Forward Declarations
namespace tphdr::logic::location
{
    class Location;
}

namespace tphdr::logic::search
{
    class Search;
}

namespace tphdr::logic::world
{
    class World;
}

namespace tphdr::logic::area
{
    class Area;
    class LocationAccess
    {
       public:
        LocationAccess(tphdr::logic::location::Location* loc, const tphdr::logic::requirement::Requirement& req, Area* area);

        tphdr::logic::location::Location* GetLocation() const;
        const tphdr::logic::requirement::Requirement& GetRequirement();
        Area* GetArea() const;
        int GetID() const;

       private:
        static int _idCounter;

        int _id = -1;
        tphdr::logic::location::Location* _loc = nullptr;
        tphdr::logic::requirement::Requirement _req;
        Area* _area = nullptr;
    };

    class EventAccess
    {
       public:
        EventAccess(const tphdr::logic::requirement::Requirement& req, Area* area, const int& eventIndex);

        const tphdr::logic::requirement::Requirement& GetRequirement();
        Area* GetArea() const;
        int GetEventIndex() const;
        std::string GetName() const;

       private:
        tphdr::logic::requirement::Requirement _req;
        Area* _area = nullptr;
        int _eventIndex = -1;
    };

    class Area
    {
       public:
        Area(const std::string& name, tphdr::logic::world::World* world);

        std::string GetName() const;
        void SetHardAssignedRegion(const std::string& _hardAssignedRegion);
        std::string GetHardAssignRegion() const;
        void SetEvents(std::list<std::unique_ptr<EventAccess>>& events);
        std::list<EventAccess*> GetEvents() const;
        void SetLocations(std::list<std::unique_ptr<LocationAccess>>& locations);
        std::list<LocationAccess*> GetLocations() const;
        void SetExits(std::list<std::unique_ptr<tphdr::logic::entrance::Entrance>>& exits);
        std::list<tphdr::logic::entrance::Entrance*> GetExits() const;
        void AddExit(std::unique_ptr<tphdr::logic::entrance::Entrance>& exit);
        void RemoveExit(tphdr::logic::entrance::Entrance* exit);
        void AddEntrance(tphdr::logic::entrance::Entrance* entrance);
        void RemoveEntrance(tphdr::logic::entrance::Entrance* entrance);
        std::list<tphdr::logic::entrance::Entrance*> GetEntrances() const;
        tphdr::logic::world::World* GetWorld() const;
        void SetCanChangeTime(const bool& canChangeTime);
        bool CanChangeTime() const;
        void SetCanTransform(const bool& canTransform);
        bool CanTransform() const;
        void AddHintRegion(const std::string& region);
        std::set<std::string> GetHintRegions();
        void SetTwilightCompletedMacroIndex(const int& macroIndex);
        int GetTwilightCompletedMacroIndex() const;
        bool TwilightCleared(tphdr::logic::search::Search* search) const;

        /**
         *  @brief Assigns this area's hint regions(s) as well as assigns any locations within the area to a dungeon if the
         * area's hint region is a dungeon
         */
        void AssignHintRegionsAndDungeonLocations();

       private:
        static int _idCounter;

        int _id = -1;
        std::string _name = "";
        std::string _hardAssignedRegion = "";
        std::set<std::string> _hintRegions = {};
        std::list<std::unique_ptr<EventAccess>> _events = {};
        std::list<std::unique_ptr<LocationAccess>> _locations = {};
        std::list<std::unique_ptr<tphdr::logic::entrance::Entrance>> _exits = {};
        std::list<tphdr::logic::entrance::Entrance*> _entrances = {};
        tphdr::logic::world::World* _world;
        bool _canChangeTime = false;
        bool _canTransform = false;
        int _twilightCompletedMacroIndex = -1;
    };

} // namespace tphdr::logic::area
