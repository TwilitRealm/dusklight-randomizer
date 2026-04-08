#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

// Forward declarations
namespace tphdr::logic::item
{
    class Item;
}

namespace tphdr::logic::entrance
{
    class Entrance;
}

namespace tphdr::logic::area
{
    class EventAccess;
    class LocationAccess;
} // namespace tphdr::logic::area

namespace tphdr::logic::world
{
    class World;
}

namespace tphdr::logic::search
{
    class Search;
}

namespace tphdr::logic::requirement
{
    enum class Type
    {
        INVALID,
        NOTHING,
        IMPOSSIBLE,
        OR,
        AND,
        ITEM,
        COUNT,
        EVENT,
        MACRO,
        DAY,
        NIGHT,
        HUMAN_LINK,
        WOLF_LINK,
        TWILIGHT,
        GOLDEN_BUGS,
    };

    enum class EvalSuccess
    {
        NONE,
        PARTIAL,
        COMPLETE,
        UNNECESSARY,
    };

    // FormTime is a set of flags that cover all the possible cases of human-wolf/day-night combinations that are needed
    // for logic to work
    namespace FormTime
    {
        enum
        {
            NONE = 0b0000,
            HUMAN_DAY = 0b0001,
            HUMAN_NIGHT = 0b0010,
            WOLF_DAY = 0b0100,
            WOLF_NIGHT = 0b1000,
            HUMAN = HUMAN_DAY | HUMAN_NIGHT,
            WOLF = WOLF_DAY | WOLF_NIGHT,
            DAY = HUMAN_DAY | WOLF_DAY,
            NIGHT = HUMAN_NIGHT | WOLF_NIGHT,
            ALL = 0b1111,
            TWILIGHT = 0b10000,
        };

        extern const std::vector<int> ALL_FORM_TIMES;
        extern const std::vector<int> ALL_FORM_TIMES_AND_TWILIGHT;
        extern const std::vector<int> ALL_FORM_AND_DAY_TIMES;

        std::string to_string(const int& formTime);
    }; // namespace FormTime

    struct Requirement;
    struct Requirement
    {
        using Argument = std::variant<int, Requirement, tphdr::logic::item::Item*>;
        Type _type = Type::INVALID;
        std::vector<Argument> _args;

        std::string to_string() const;
    };

    Requirement ParseRequirementString(const std::string& reqStr,
                                       tphdr::logic::world::World* world,
                                       const bool& forceLogic = false);

    bool EvaluateRequirementAtFormTime(const tphdr::logic::requirement::Requirement& req,
                                       tphdr::logic::search::Search* search,
                                       const int& formTime,
                                       tphdr::logic::world::World*);
    EvalSuccess EvaluateEventRequirement(tphdr::logic::search::Search* search, tphdr::logic::area::EventAccess* event);
    EvalSuccess EvaluateExitRequirement(tphdr::logic::search::Search* search, tphdr::logic::entrance::Entrance* exit);
    EvalSuccess EvaluateLocationRequirement(tphdr::logic::search::Search* search,
                                            tphdr::logic::area::LocationAccess* locAccess);

    const extern Requirement NO_REQUIREMENT;
    const extern Requirement IMPOSSIBLE_REQUIREMENT;
} // namespace tphdr::logic::requirement