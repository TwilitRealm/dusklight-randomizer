#include "entrance.hpp"

#include "area.hpp"
#include "world.hpp"
#include "../utility/log.hpp"
#include "../utility/string.hpp"

namespace randomizer::logic::entrance
{

    Type TypeFromStr(const std::string& str)
    {
        std::unordered_map<std::string, Type> types = {
            {"None", NONE},
            {"Spawn", SPAWN},
            {"Warp Portal", WARP_PORTAL},
            {"Dungeon", DUNGEON},
            {"Boss", BOSS},
            {"Grotto", GROTTO},
            {"Mixed Pool 1", MIXED_POOL_1},
            {"Mixed Pool 2", MIXED_POOL_2},
            {"Mixed Pool 3", MIXED_POOL_3},
            {"Mixed Pool 4", MIXED_POOL_4},
            {"Mixed Pool 5", MIXED_POOL_5},
            {"Cave", CAVE},
            {"Interior", INTERIOR},
            {"Overworld", OVERWORLD}
        };

        if (!types.contains(str))
        {
            return INVALID;
        }

        return types.at(str);
    }

    std::string TypeToStr(const Type& type)
    {
        std::unordered_map<Type, std::string> types = {
            {NONE, "None"},
            {SPAWN, "Spawn"},
            {WARP_PORTAL, "Warp Portal"},
            {DUNGEON, "Dungeon"},
            {DUNGEON_REVERSE, "Dungeon Reverse"},
            {BOSS, "Boss"},
            {BOSS_REVERSE, "Boss Reverse"},
            {GROTTO, "Grotto"},
            {GROTTO_REVERSE, "Grotto Reverse"},
            {MIXED_POOL_1, "Mixed Pool 1"},
            {MIXED_POOL_2, "Mixed Pool 2"},
            {MIXED_POOL_3, "Mixed Pool 3"},
            {MIXED_POOL_4, "Mixed Pool 4"},
            {MIXED_POOL_5, "Mixed Pool 5"},
            {CAVE, "Cave"},
            {CAVE_REVERSE, "Cave Reverse"},
            {INTERIOR, "Interior"},
            {INTERIOR_REVERSE, "Interior Reverse"},
            {OVERWORLD, "Overworld"}};

        if (!types.contains(type))
        {
            return "INVALID";
        }

        return types.at(type);
    }

    Type TypeToReverse(const Type& type)
    {
        std::unordered_map<Type, Type> reverse = {
            {DUNGEON, DUNGEON_REVERSE},
            {DUNGEON_REVERSE, DUNGEON},
            {BOSS, BOSS_REVERSE},
            {BOSS_REVERSE, BOSS},
            {GROTTO, GROTTO_REVERSE},
            {GROTTO_REVERSE, GROTTO},
            {CAVE, CAVE_REVERSE},
            {CAVE_REVERSE, CAVE},
            {INTERIOR, INTERIOR_REVERSE},
            {INTERIOR_REVERSE, INTERIOR},
            // Yes, this is intentional for the overworld type
            {OVERWORLD, OVERWORLD}};

        if (!reverse.contains(type))
        {
            return INVALID;
        }

        return reverse.at(type);
    }

    Entrance::Entrance(area::Area* parentArea,
                       area::Area* connectedArea,
                       const requirement::Requirement& req,
                       world::World* world):
        _parentArea(parentArea),
        _connectedArea(connectedArea),
        _originalConnectedArea(connectedArea),
        _req(std::move(req)),
        _world(world)
    {
        this->_originalName = this->GetCurrentName();
        this->_computedRequirement._type = requirement::Type::IMPOSSIBLE;
    }

    void Entrance::SetID(const int& id)
    {
        this->_id = id;
    }

    int Entrance::GetID() const
    {
        return this->_id;
    }

    std::string Entrance::GetCurrentName() const
    {
        std::string parentName = this->_parentArea ? this->_parentArea->GetName() : "None";
        std::string connectedName = this->_connectedArea ? this->_connectedArea->GetName() : "None";
        return parentName + " -> " + connectedName;
    }

    std::string Entrance::GetOriginalName() const
    {
        return this->_originalName;
    }

    void Entrance::SetAlias(const std::string& alias)
    {
        this->_alias = alias;
        // If there's no alias, just use the original name
        if (this->_alias.empty())
        {
            this->_alias = this->_originalName;
        }
    }

    std::string Entrance::GetAlias() const
    {
        return this->_alias;
    }

    std::string Entrance::GetAliasFrom()
    {
        std::string parentAreaAlias = this->_alias.substr(0, this->_alias.find(" -> "));
        std::string connectedAreaAlias = this->_alias.substr(this->_alias.find(" -> ") + 4);
        return connectedAreaAlias + " from " + parentAreaAlias;
    }

    void Entrance::GeneralizeName()
    {
        utility::str::Erase(this->_originalName, " North", " South", " East", " West", " Right", " Left");
        utility::str::Erase(this->_alias, " North", " South", " East", " West", " Right", " Left");
    }

    area::Area* Entrance::GetParentArea() const
    {
        return this->_parentArea;
    }

    area::Area* Entrance::GetConnectedArea() const
    {
        return this->_connectedArea;
    }

    area::Area* Entrance::GetOriginalConnectedArea() const
    {
        return this->_originalConnectedArea;
    }

    void Entrance::SetType(const Type& type)
    {
        this->_type = type;
        if (this->_originalType == INVALID)
        {
            this->_originalType = type;
        }
    }

    Type Entrance::GetType() const
    {
        return this->_type;
    }

    Type Entrance::GetOriginalType() const
    {
        return this->_originalType;
    }

    void Entrance::SetRequirement(const requirement::Requirement& req)
    {
        this->_req = req;
    }

    const requirement::Requirement& Entrance::GetRequirement()
    {
        return this->_req;
    }

    void Entrance::SetComputedRequirement(const requirement::Requirement& computedRequirement)
    {
        this->_computedRequirement = computedRequirement;
    }

    requirement::Requirement Entrance::GetComputedRequirement()
    {
        return this->_computedRequirement;
    }

    world::World* Entrance::GetWorld() const
    {
        return this->_world;
    }

    bool Entrance::CanStartAt() const
    {
        return this->_canStartAt;
    }

    void Entrance::SetShuffled(const bool& shuffled)
    {
        this->_shuffled = shuffled;
    }

    bool Entrance::IsShuffled() const
    {
        return this->_shuffled;
    }

    void Entrance::SetDecoupled(const bool& decoupled)
    {
        this->_decoupled = decoupled;
    }

    bool Entrance::IsDecoupled() const
    {
        return this->_decoupled;
    }

    void Entrance::SetDisbled(const bool& disabled)
    {
        this->_disabled = disabled;
        LOG_TO_DEBUG(this->GetOriginalName() + " disabled status set to " + (disabled ? "True" : "False"));
    }

    bool Entrance::IsDisabled() const
    {
        return this->_disabled;
    }

    void Entrance::SetPrimary(const bool& primary)
    {
        this->_primary = primary;
        LOG_TO_DEBUG(this->GetOriginalName() + " primary status set to " + (primary ? "True" : "False"));
    }

    bool Entrance::IsPrimary() const
    {
        return this->_primary;
    }

    void Entrance::SetTarget(const bool& target)
    {
        this->_target = target;
    }

    bool Entrance::IsTarget() const
    {
        return this->_target;
    }

    void Entrance::SetReplaces(Entrance* replaces)
    {
        this->_replaces = replaces;
    }

    Entrance* Entrance::GetReplaces() const
    {
        return this->_replaces;
    }

    void Entrance::SetReverse(Entrance* reverse)
    {
        this->_reverse = reverse;
    }

    Entrance* Entrance::GetReverse() const
    {
        return this->_reverse;
    }

    Entrance* Entrance::GetAssumed() const
    {
        return this->_assumed;
    }

    void Entrance::Connect(area::Area* newConnectedArea)
    {
        this->_connectedArea = newConnectedArea;
        newConnectedArea->AddEntrance(this);
    }

    area::Area* Entrance::Disconnect()
    {
        this->_connectedArea->RemoveEntrance(this);
        auto previouslyConnected = this->_connectedArea;
        this->_connectedArea = nullptr;
        return previouslyConnected;
    }

    void Entrance::BindTwoWay(Entrance* returnEntrance)
    {
        this->SetReverse(returnEntrance);
        returnEntrance->SetReverse(this);
    }

    Entrance* Entrance::GetNewTarget()
    {
        auto root = this->_world->GetRootArea();
        auto targetEntrance =
            std::make_unique<Entrance>(root, nullptr, requirement::NO_REQUIREMENT, this->_world);
        auto target = targetEntrance.get();
        root->AddExit(targetEntrance); // This moves the variable, so we have to use the pointer for the rest of the function
        target->Connect(this->_connectedArea);
        target->SetReplaces(this);
        target->SetTarget(true);
        return target;
    }

    Entrance* Entrance::AssumeReachable()
    {
        if (this->_assumed == nullptr)
        {
            this->_assumed = this->GetNewTarget();
            this->Disconnect();
        }
        return this->_assumed;
    }

    std::tuple<std::string, std::string> GetParentAndConnectedAreaNames(const std::string& originalName)
    {
        std::string parentAreaName;
        std::string connectedAreaName;
        if (utility::str::Contains(originalName, " -> "))
        {
            auto separatorIndex = originalName.find(" -> ");
            parentAreaName = originalName.substr(0, separatorIndex);
            connectedAreaName = originalName.substr(separatorIndex + 4);
        }
        else if (utility::str::Contains(originalName, " from "))
        {
            auto separatorIndex = originalName.find(" from ");
            connectedAreaName = originalName.substr(0, separatorIndex);
            parentAreaName = originalName.substr(separatorIndex + 6);
        }
        else
        {
            throw std::runtime_error("Could not parse area names from entrance string \"" + originalName +
                                     "\". Please make sure your syntax is correct");
        }

        return {parentAreaName, connectedAreaName};
    }

    void Entrance::SetGameInfo(const YAML::Node& node) {
        SetStageId(node["Stage"].as<uint8_t>());
        SetRoomNo(node["Room"].as<int8_t>());
        SetPointNo(node["Spawn"].as<int16_t>());
        SetLayerNo(node["State"].as<int8_t>());
    }
} // namespace randomizer::logic::entrance
