#pragma once

#include "requirement.hpp"

#include <unordered_set>
#include <map>
#include <string>

// Forward Declarations
namespace tphdr::logic::area
{
    class Area;
}

namespace tphdr::logic::world
{
    class World;
}

namespace tphdr::logic::entrance
{
    enum Type
    {
        INVALID = 0,
        // The order of this enum is also the order in which the different types of entrances
        // will be shuffled. So this ordering is important. Generally we want to shuffle entrances
        // near the "outside" of the world graph first (dungeon entrances/grottos) and then follow that
        // up with entrance types that are closer to the "inside" of the world graph which have more
        // consequences to being shuffled. The mixed pools are thrown in the middle since this is the most stable
        // place for them to be to not interfere with the ordering too much.
        SPAWN,
        WARP_PORTAL,
        GROTTO,
        GROTTO_REVERSE,
        BOSS,
        BOSS_REVERSE,
        DUNGEON,
        DUNGEON_REVERSE,
        MIXED_POOL_1,
        MIXED_POOL_2,
        MIXED_POOL_3,
        MIXED_POOL_4,
        MIXED_POOL_5,
        CAVE,
        CAVE_REVERSE,
        INTERIOR,
        INTERIOR_REVERSE,
        OVERWORLD,
        ALL,
    };

    extern std::unordered_set<Type> NON_ASSUMED_TYPES;

    /**
     *  @brief Takes a string representation of a Type and returns the
     *  associated enum value.
     *
     *  @param str The string representation of a Type.
     *  @return The associated enum value for the passed in type.
     */
    Type TypeFromStr(const std::string& str);

    std::string TypeToStr(const Type& type);

    Type TypeToReverse(const Type& type);

    class Entrance
    {
       public:
        Entrance(tphdr::logic::area::Area* parentArea,
                 tphdr::logic::area::Area* connectedArea,
                 const tphdr::logic::requirement::Requirement& req,
                 tphdr::logic::world::World* world);

        void SetID(const int& id);
        int GetID() const;
        std::string GetCurrentName() const;
        std::string GetOriginalName() const;
        /**
         * @brief Removes cardinal/direction specifiers from the entrance's name (North, South, East, West, Left, Right)
         */
        void GeneralizeOriginalName();
        tphdr::logic::area::Area* GetParentArea() const;
        tphdr::logic::area::Area* GetConnectedArea() const;
        tphdr::logic::area::Area* GetOriginalConnectedArea() const;
        void SetType(const Type& type);
        Type GetType() const;
        Type GetOriginalType() const;
        void SetRequirement(const tphdr::logic::requirement::Requirement& req);
        const tphdr::logic::requirement::Requirement& GetRequirement();
        void SetComputedRequirement(const tphdr::logic::requirement::Requirement& computedRequirement);
        tphdr::logic::requirement::Requirement GetComputedRequirement();
        tphdr::logic::world::World* GetWorld() const;
        bool CanStartAt() const;
        void SetShuffled(const bool& shuffled);
        bool IsShuffled() const;
        void SetDecoupled(const bool& decoupled);
        bool IsDecoupled() const;
        void SetDisbled(const bool& disabled);
        bool IsDisabled() const;
        void SetPrimary(const bool& primary);
        bool IsPrimary() const;
        void SetTarget(const bool& target);
        bool IsTarget() const;

        void SetReplaces(Entrance* replaces);
        Entrance* GetReplaces() const;
        void SetReverse(Entrance* reverse);
        Entrance* GetReverse() const;
        Entrance* GetAssumed() const;

        /**
         * @brief Connect this entrance to the passed in area, and add this entrance to the list of entrances for the passed in
         * area
         *
         * @param newConnectedArea The area to connect this entrance to
         */
        void Connect(tphdr::logic::area::Area* newConnectedArea);

        /**
         * @brief Disconnect this entrance from the area it leads to. Will also remove this entrance from it's connected area's
         * entrances.
         *
         * @return The area this entrance was previously connected to
         */
        tphdr::logic::area::Area* Disconnect();

        /**
         * @brief Links two entrances by setting them as each others' reverse entrance
         */
        void BindTwoWay(Entrance* returnEntrance);

        /**
         * @brief Creates a new target entrance that corresponds to where this one leads, and
         * attaches it to the root of the world graph.
         */
        Entrance* GetNewTarget();

        /**
         * @brief Create this entrance's target and disconnect it from the original entrance.
         * This assumes reachable access to the entrance for the entrance shuffling algorithm
         */
        Entrance* AssumeReachable();

       private:
        int _id = -1;
        tphdr::logic::area::Area* _parentArea = nullptr;
        tphdr::logic::area::Area* _connectedArea = nullptr;
        tphdr::logic::area::Area* _originalConnectedArea = nullptr;
        Type _type = Type::INVALID;
        Type _originalType = Type::INVALID;
        std::string _originalName = "";
        tphdr::logic::world::World* _world = nullptr;

        /**
         * @brief The local requirement for this entrance assuming we have access to its parent area.
         */
        tphdr::logic::requirement::Requirement _req;

        /**
         * @brief The flattened requirement which includes everything necessary to reach this entrance from the root of the
         * world graph.
         */
        tphdr::logic::requirement::Requirement _computedRequirement;

        // Variables used for entrance shuffling
        bool _canStartAt = false;
        bool _shuffled = false;
        bool _decoupled = false;
        bool _disabled = false;

        // A target entrance is one created to mimic the effect of going 
        // through a specific real entrance. The target is attatched to
        // the root of the world graph and is connected to it's correpsonding
        // entrance's connected area.
        bool _target = false;

        // Primary entrances are those that we think of as
        // "going into" areas. Entering dungeons, entering grottos,
        // and entering doors are all primary entrances. The opposite
        // idea, "leaving" areas, are not primary entrances.
        bool _primary = false;

        // The reverse is the entrance that sends the player in the
        // natural opposite direction of this entrance. The reverse entrance
        // of entering a grotto would be the entrance that leaves the grotto
        Entrance* _reverse = nullptr;

        // If the entrance is shuffled, _replaces is the target entrance that replaces
        // this one. If this *is* a target entrance, then _replaces holds the
        // entrance that this target *corresponds* to.
        Entrance* _replaces = nullptr;

        // If the entrance is shuffled, _assumed is the target entrance that *corresponds*
        // to this one. So if the entrance is North Faron Woods -> Forest Temple Entrance,
        // then _assumed is the target entrance Root -> Forest Temple Entrance
        Entrance* _assumed = nullptr;
    };

    using EntrancePool = std::vector<Entrance*>;
    using EntrancePools = std::map<Type, EntrancePool>;

    std::tuple<std::string, std::string> GetParentAndConnectedAreaNames(const std::string& originalName);
} // namespace tphdr::logic::entrance
