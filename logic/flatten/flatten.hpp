#pragma once

#include "../entrance.hpp"
#include "simplify_algebraic.hpp"
#include "../../utility/log.hpp"

#include <functional>
#include <iostream>
#include <map>

namespace tphdr::logic::area
{
    class EventAccess;
    class Area;
} // namespace tphdr::logic::area

namespace tphdr::logic::world
{
    class World;
}

class FlattenSearch
{
   public:
    FlattenSearch() = default;
    FlattenSearch(tphdr::logic::world::World* world_);

    tphdr::logic::world::World* world = nullptr;
    BitIndex bitIndex = BitIndex();

    // partially computed requirements for areas at a
    // given timeform and for events
    std::unordered_map<int, DNF> eventExprs = {};
    std::unordered_map<int, std::unordered_map<tphdr::logic::area::Area*, DNF>> areaExprs = {};

    // nodes we haven't looked at we don't even need to bother with
    std::set<tphdr::logic::entrance::Entrance*> exitsToTry = {};
    std::set<tphdr::logic::area::EventAccess*> eventsToTry = {};
    std::set<tphdr::logic::area::Area*> areasToTry = {};

    // we only re-check an exit or an event if its dependencies changed.
    // dependencies can be the implicit parent area (for events and exits),
    // formtime expansion in the area, and "remote" requirements arising
    // from the expression itself mentioning an event or an area via can_access
    std::set<tphdr::logic::area::Area*> recentlyUpdatedAreas = {};
    std::set<int> recentlyUpdatedEvents = {};

    std::set<tphdr::logic::area::Area*> newlyUpdatedAreas = {};
    std::set<int> newlyUpdatedEvents = {};

    std::unordered_map<void*, std::set<int>> remoteEventRequirements = {};
    std::unordered_map<void*, std::set<std::string>> remoteAreaRequirements = {};
    bool newThingsFound = false;

    void doSearch();
    bool wasUpdated(tphdr::logic::area::Area* area, void* thing);
    void tryExits();
    void tryEvents();
    void tryTimeFormExpansion();
    void andAreaFormTimes(tphdr::logic::area::Area* area);

    DNF tryEventAtFormTime(tphdr::logic::area::EventAccess* event, const int& formTime);
    DNF tryLocationAtFormTime(tphdr::logic::area::LocationAccess* location, const int& formTime);
    DNF tryExitAtFormTime(tphdr::logic::entrance::Entrance* exit, const int& formTime);
};

template<typename T>
std::function<void(const tphdr::logic::requirement::Requirement& req)> visitor(T* thing, FlattenSearch* search)
{
    auto thingPtr = (void*)thing;
    std::function<void(const tphdr::logic::requirement::Requirement&)> handler =
        [=](const tphdr::logic::requirement::Requirement& req)
    {
        if (req._type == tphdr::logic::requirement::Type::EVENT)
        {
            if (!search->remoteEventRequirements.contains(thingPtr))
            {
                search->remoteEventRequirements[thingPtr] = {};
            }
            search->remoteEventRequirements[thingPtr].insert(std::get<int>(req._args[0]));
        }
    };

    return handler;
}

void visitReq(const tphdr::logic::requirement::Requirement& req,
              std::function<void(const tphdr::logic::requirement::Requirement& req)> f,
              tphdr::logic::world::World* world);

DNF evaluatePartialRequirement(BitIndex& bitIndex,
                               const tphdr::logic::requirement::Requirement& req,
                               FlattenSearch* search,
                               const int& formTime);
