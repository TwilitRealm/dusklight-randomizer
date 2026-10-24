#include "entrance_shuffle.hpp"

#include "item_pool.hpp"
#include "search.hpp"
#include "../randomizer.hpp"
#include "../utility/random.hpp"
#include "../utility/yaml.hpp"

#include <ranges>

using namespace randomizer::logic::entrance;

namespace randomizer::logic::entrance_shuffle
{
    void ShuffleWorldEntrances(world::World* world)
    {
        SetAllEntrancesData(world);

        auto entrancePools = CreateEntrancePools(world);
        auto targetEntrancePools = CreateTargetPools(entrancePools);

        // Set plando entrances first
        try {
            SetPlandomizedEntrances(world, entrancePools, targetEntrancePools);
        } catch (std::runtime_error& e) {
            throw std::runtime_error("Plandomizer Error: " + std::string(e.what()));
        }

        // Then shuffle non-assumed types (currently this is just spawn)
        ShuffleNonAssumedEntrancesPools(world, entrancePools, targetEntrancePools);

        // Shuffle the rest of the entrance pools
        for (auto& [entranceType, entrancePool] : entrancePools)
        {
            // Skip shuffling reverse boss entrances if we're adjusting them later based on dungeon
            // entrances
            if (world->AdjustBossReturns() && entranceType == BOSS_REVERSE) {
                continue;
            }
            ShuffleEntrancePool(entrancePool, targetEntrancePools[entranceType]);
        }

        // Validate the world one last time to ensure everything worked
        auto completeItemPool = item_pool::GetCompleteItemPool(world->GetRandomizer()->GetWorlds());
        ValidateWorld(world, nullptr, completeItemPool);
    }

    void SetAllEntrancesData(world::World* world)
    {
        // Keep track of which entrances are together
        std::unordered_map<std::string, std::list<Entrance*>> coupledEntrances = {};

        auto entranceDataTree = LOAD_EMBED_YAML(RANDO_DATA_PATH "entrance_shuffle_data.yaml");
        for (const auto& entranceDataNode : entranceDataTree)
        {
            // Check to make sure all required fields are present
            YAMLVerifyFields(entranceDataNode, "Type", "Forward");

            // Check if we only want to load this data under certain conditions
            if (entranceDataNode["Only If"]) {
                if (!world->EvaluateSettingCondition(entranceDataNode["Only If"].as<std::string>())) {
                    continue;
                }
            }

            auto typeStr = entranceDataNode["Type"].as<std::string>();
            auto type = TypeFromStr(typeStr);
            if (type == INVALID)
            {
                throw std::runtime_error("Unknown entrance type \"" + typeStr + "\" in entrance shuffle node:\n" +
                                         YAML::Dump(entranceDataNode));
            }

            auto& forwardEntry = entranceDataNode["Forward"];

            // If the entrance can't actually be mapped in-game, skip it 
            auto stageString = forwardEntry["Stage"].as<std::string>();
            auto roomString = forwardEntry["Stage"].as<std::string>();
            if (stageString == "null" || stageString == "-1" || roomString == "null" || roomString == "-1") {
                continue;
            }
            if (entranceDataNode["Return"]) {
                stageString = entranceDataNode["Return"]["Stage"].as<std::string>();
                roomString = entranceDataNode["Return"]["Stage"].as<std::string>();
                if (stageString == "null" || stageString == "-1" || roomString == "null" || roomString == "-1") {
                    continue;
                }
            }

            // Check to make sure all required fields are present for the forward entry
            YAMLVerifyFields(forwardEntry, "Connection", "Stage", "Room", "Spawn", "Spawn Type", "Parameters", "State");

            auto forwardEntrance = world->GetEntrance(forwardEntry["Connection"].as<std::string>());
            forwardEntrance->SetType(type);
            forwardEntrance->SetGameInfo(forwardEntry);
            forwardEntrance->SetID(world->GetNewEntranceID());
            forwardEntrance->SetPrimary(true);
            forwardEntrance->SetAlias(
                forwardEntry["Alias"] ? forwardEntry["Alias"].as<std::string>() : "");
            if (forwardEntry["Follower Entrances"]) {
                forwardEntrance->SetFollowerEntrances(forwardEntry["Follower Entrances"]);
            }
            if (forwardEntry["Coupled Points"]) {
                for (const auto& pointNode : forwardEntry["Coupled Points"]) {
                    forwardEntrance->AddCoupledPoint(pointNode.as<int16_t>());
                }
            }
            if (forwardEntry["Dungeon Stage Returns"]) {
                forwardEntrance->SetDungeonStageReturns(forwardEntry["Dungeon Stage Returns"]);
            }
            if (forwardEntry["Boss Entrance"]) {
                const auto& bossEntranceName = forwardEntry["Boss Entrance"].as<std::string>();
                auto bossEntrance = world->GetEntrance(bossEntranceName);
                forwardEntrance->SetBossEntrance(bossEntrance);
            }

            Entrance* returnEntrance = nullptr;
            if (entranceDataNode["Return"])
            {
                auto& returnEntry = entranceDataNode["Return"];
                YAMLVerifyFields(returnEntry, "Connection", "Stage", "Room", "Spawn", "Spawn Type", "Parameters", "State");

                returnEntrance = world->GetEntrance(returnEntry["Connection"].as<std::string>());
                returnEntrance->SetType(type);
                returnEntrance->SetGameInfo(returnEntry);
                returnEntrance->SetID(world->GetNewEntranceID());
                returnEntrance->SetAlias(
                    returnEntry["Alias"] ? returnEntry["Alias"].as<std::string>() : "");
                forwardEntrance->BindTwoWay(returnEntrance);

                if (returnEntry["Follower Entrances"]) {
                    returnEntrance->SetFollowerEntrances(returnEntry["Follower Entrances"]);
                }
                if (returnEntry["Coupled Points"]) {
                    for (const auto& pointNode : returnEntry["Coupled Points"]) {
                        returnEntrance->AddCoupledPoint(pointNode.as<int16_t>());
                    }
                }
                // Add coupled entrances to their respective tag group
                if (entranceDataNode["Entrance Couple Tag"])
                {
                    auto tag = entranceDataNode["Entrance Couple Tag"].as<std::string>();
                    if (!coupledEntrances.contains(tag))
                    {
                        coupledEntrances[tag] = {};
                    }
                    coupledEntrances.at(tag).push_back(forwardEntrance);
                    coupledEntrances.at(tag).push_back(returnEntrance);
                }
            }

            // Set any extra override data this entrance has
            if (entranceDataNode["Extra Override Data"]) {
                for (const auto& entry : entranceDataNode["Extra Override Data"]) {
                    const auto& name = entry.first.as<std::string>();
                    const auto& data = entry.second;
                    forwardEntrance->SetExtraOverrideInfo(name, data);
                    if (returnEntrance) {
                        returnEntrance->SetExtraOverrideInfo(name, data);
                    }
                }
            }
        }

        // If entrances are coupled, add the coupled entrance's spawn point to the main door, remove the coupled door entrance, and
        // rename the main door to be more general
        if (world->Setting("Decouple Double Door Entrances") == "Off")
        {
            for (auto& [tag, entrances] : coupledEntrances)
            {
                while (!entrances.empty())
                {
                    auto mainEntrance = entrances.back();
                    entrances.pop_back();

                    for (auto it = entrances.begin(); it != entrances.end();) {
                        auto coupledEntrance = *it;
                        if (coupledEntrance->IsPrimary() == mainEntrance->IsPrimary()) {
                            mainEntrance->AddCoupledPoint(coupledEntrance->GetPointNo());

                            // Completely remove the coupled door from the world graph
                            coupledEntrance->GetConnectedArea()->RemoveEntrance(coupledEntrance);
                            coupledEntrance->GetParentArea()->RemoveExit(coupledEntrance);
                            it = entrances.erase(it);
                        }else{
                            ++it;
                        }
                    }

                    // Change the main door's name to be more general
                    mainEntrance->GeneralizeName();
                }
            }
        }
    }

    EntrancePools CreateEntrancePools(world::World* world)
    {
        EntrancePools entrancePools = {};

        // Spawn
        if (world->Setting("Randomize Starting Spawn") == "On")
        {
            entrancePools[SPAWN] = world->GetShuffleableEntrances(SPAWN);
        }

        // Dungeon Entrances
        if (world->Setting("Randomize Dungeon Entrances") >= "On")
        {
            entrancePools[DUNGEON] = world->GetShuffleableEntrances(DUNGEON, /*onlyPrimary = */ true);

            // Remove Hyrule Castle if it's not being shuffled
            if (world->Setting("Randomize Dungeon Entrances") != "On + Hyrule Castle")
            {
                std::erase_if(entrancePools[DUNGEON], [](const auto& entrance) {
                    return entrance->GetOriginalName() == "Castle Town North Inside Barrier -> Hyrule Castle Entrance";
                });
            }

            if (world->Setting("Decouple Entrances") == "On")
            {
                entrancePools[DUNGEON_REVERSE] = GetReverseEntrances(entrancePools[DUNGEON]);
            }
        }

        // Boss Entrances
        if (world->Setting("Randomize Boss Entrances") == "On")
        {
            entrancePools[BOSS] = world->GetShuffleableEntrances(BOSS, /*onlyPrimary = */ true);

            if (world->Setting("Decouple Entrances") == "On")
            {
                entrancePools[BOSS_REVERSE] = GetReverseEntrances(entrancePools[BOSS]);
            }
        }

        // Grotto Entrances
        if (world->Setting("Randomize Grotto Entrances") == "On")
        {
            entrancePools[GROTTO] = world->GetShuffleableEntrances(GROTTO, /*onlyPrimary = */ true);

            if (world->Setting("Decouple Entrances") == "On")
            {
                entrancePools[GROTTO_REVERSE] = GetReverseEntrances(entrancePools[GROTTO]);
            }
        }

        // Cave Entrances
        if (world->Setting("Randomize Cave Entrances") == "On")
        {
            entrancePools[CAVE] = world->GetShuffleableEntrances(CAVE, /*onlyPrimary = */ true);

            if (world->Setting("Decouple Entrances") == "On")
            {
                entrancePools[CAVE_REVERSE] = GetReverseEntrances(entrancePools[CAVE]);
            }
        }

        // Interior Entrances
        if (world->Setting("Randomize Interior Entrances") == "On")
        {
            entrancePools[INTERIOR] = world->GetShuffleableEntrances(INTERIOR, /*onlyPrimary = */ true);

            if (world->Setting("Decouple Entrances") == "On")
            {
                entrancePools[INTERIOR_REVERSE] = GetReverseEntrances(entrancePools[INTERIOR]);
            }
        }

        // Overworld Entrances
        if (world->Setting("Randomize Overworld Entrances") == "On")
        {
            // Normally we allow any overworld entrances to link together.
            // However, if overworld entrances are mixed with other entrance types
            // that expect to only match with exclusively primary or non-primary
            // entrances, we have to separate overworld entrances by their primary/
            // non-primary distinction to fit with the other entrances
            const auto& mixedPools = world->GetSettings().GetMixedEntrancePools();
            bool excludeOverworldReverse =
                 world->Setting("Decouple Entrances") == "Off" &&
                 std::ranges::any_of(mixedPools, [](const auto& pool) {
                     return utility::container::ElementInContainer(pool, "Overworld");
                 }); /*Overworld in a mixed pool*/
            entrancePools[OVERWORLD] =
                world->GetShuffleableEntrances(OVERWORLD, /*onlyPrimary = */ excludeOverworldReverse);
        }

        // Match pool types
        for (auto& [entranceType, entrancePool] : entrancePools)
        {
            for (auto& entrance : entrancePool)
            {
                entrance->SetType(entranceType);
            }
        }

        SetShuffledEntrances(entrancePools);

        // Set appropriate types as decoupled
        auto potentiallyDecoupledTypes = {
            DUNGEON,
            DUNGEON_REVERSE,
            BOSS,
            BOSS_REVERSE,
            GROTTO,
            GROTTO_REVERSE,
            CAVE,
            CAVE_REVERSE,
            INTERIOR,
            INTERIOR_REVERSE,
            OVERWORLD,
        };
        if (world->Setting("Decouple Entrances") == "On")
        {
            for (const auto& type : potentiallyDecoupledTypes)
            {
                if (entrancePools.contains(type))
                {
                    for (auto& entrance : entrancePools.at(type))
                    {
                        entrance->SetDecoupled(true);
                    }
                }
            }
        }

        // If we're adjusting boss returns to account for randomized dungeon entrances,
        // decouple the forward boss entrances which have out-of-dungeon return entrances.
        // Then decouple all reverse boss entrances. We'll set the boss returns while setting
        // forward dungeon entrances in this scenario.
        if (world->AdjustBossReturns())
        {
            for (auto& entrance : entrancePools.at(DUNGEON)) {
                if (entrance->GetBossEntrance()) {
                    entrance->GetBossEntrance()->SetDecoupled(true);
                    entrance->GetBossEntrance()->GetReverse()->SetDecoupled(true);
                    entrance->GetBossEntrance()->GetReverse()->SetShuffled(true);
                    entrancePools[BOSS_REVERSE].push_back(entrance->GetBossEntrance()->GetReverse());
                }
            }

            if (entrancePools.contains(BOSS)) {
                for (auto& entrance : entrancePools.at(BOSS)) {
                    entrance->GetReverse()->SetDecoupled(true);
                }
            }
        }

        // Combine the Mixed pools into their respective pools
        const auto& mixedPoolList = world->GetSettings().GetMixedEntrancePools();
        int counter = 1;
        for (const auto& mixedPool : mixedPoolList)
        {
            auto mixedType = TypeFromStr("Mixed Pool " + std::to_string(counter));
            for (const auto& typeStr : mixedPool)
            {
                auto type = TypeFromStr(typeStr);
                if (type == INVALID)
                {
                    throw std::runtime_error("Unknown entrance type \"" + typeStr + "\" in mixed pools");
                }
                // Only bother with entrance types that are being shuffled
                for (const auto& entranceType : {type, TypeToReverse(type)})
                {
                    if (entrancePools.contains(entranceType))
                    {
                        // Create the Mixed Pool entry if it doesn't exist
                        if (!entrancePools.contains(mixedType))
                        {
                            entrancePools[mixedType] = {};
                        }
                        for (const auto& entrance : entrancePools.at(entranceType))
                        {
                            entrancePools.at(mixedType).push_back(entrance);
                        }
                        // Delete the original pool once it's been added
                        entrancePools.erase(entranceType);
                    }
                }
            }
            counter += 1;
        }

        return entrancePools;
    }

    EntrancePools CreateTargetPools(EntrancePools& entrancePools)
    {
        EntrancePools targetEntrancePools = {};
        for (auto& [type, entrancePool] : entrancePools)
        {
            if (type == SPAWN)
            {
                EntrancePool spawnPool = {};
                auto world = entrancePool[0]->GetWorld();
                // Get all the entrances of these types to use as spawn targets
                for (const auto& typeForSpawn : {SPAWN, INTERIOR, CAVE, OVERWORLD, GROTTO})
                {
                    for (const auto& entrance : world->GetShuffleableEntrances(typeForSpawn))
                    {
                        auto newTarget = entrance->GetNewTarget();
                        spawnPool.push_back(newTarget);

                        // Don't assume we have access to random spawn targets. We're only connecting to one of them
                        // so assuming we have access to all of them would be erroneous.
                        newTarget->SetRequirement(requirement::IMPOSSIBLE_REQUIREMENT);
                    }
                }
                targetEntrancePools[type] = spawnPool;
                for (auto& entrance : entrancePool)
                {
                    entrance->Disconnect();
                }
            }
            else
            {
                targetEntrancePools[type] = AssumeEntrancePool(entrancePool);
            }
        }
        return targetEntrancePools;
    }

    EntrancePool AssumeEntrancePool(EntrancePool& entrancePool)
    {
        EntrancePool assumedPool = {};
        for (auto& entrance : entrancePool)
        {
            auto assumedForward = entrance->AssumeReachable();
            if (entrance->GetReverse() && !entrance->IsDecoupled())
            {
                auto assumedReturn = entrance->GetReverse()->AssumeReachable();
                assumedForward->BindTwoWay(assumedReturn);
            }
            assumedPool.push_back(assumedForward);
        }
        return assumedPool;
    }

    void SetPlandomizedEntrances(world::World* world,
                                 EntrancePools& entrancePools,
                                 EntrancePools& targetEntrancePools)
    {
        LOG_TO_DEBUG("Now placing plandomizer entrances");
        auto& worlds = world->GetRandomizer()->GetWorlds();
        auto itemPool = item_pool::GetCompleteItemPool(worlds);

        for (auto& [plandoEntrance, plandoTarget] : world->GetPlandomizerEntrances())
        {
            auto entranceToConnect = plandoEntrance;
            auto targetToConnect = plandoTarget;
            auto entranceType = plandoEntrance->GetType();

            // Throw error if entrance/target types are not shuffleable
            if (entranceType == INVALID)
            {
                throw std::runtime_error(entranceToConnect->GetOriginalName() +
                                         " is not an entrance that can be shuffled");
            }
            if (plandoTarget->GetType() == INVALID)
            {
                throw std::runtime_error(plandoTarget->GetOriginalName() +
                                         " is not an entrance that can be shuffled");
            }

            // Throw error if entrance type is shuffleable, but the type itself is not randomized currently
            if (!entrancePools.contains(entranceType))
            {
                throw std::runtime_error("Entrance type " + TypeToStr(entranceType) + " for " +
                                          entranceToConnect->GetOriginalName() + " is not being shuffled and thus can't be plandomized.");
            }

            // Get the appropriate pools
            auto& entrancePool = entrancePools.at(entranceType);
            auto& targetPool = targetEntrancePools.at(entranceType);

            // If entrances are coupled, but the user tries to plandomize a non-primary connection, get the primary connection
            // instead
            if (world->Setting("Decouple Entrances") == "Off" &&
                utility::container::ElementInContainer(entrancePool, entranceToConnect->GetReverse()))
            {
                entranceToConnect = entranceToConnect->GetReverse();
                targetToConnect = targetToConnect->GetReverse();
            }

            if (utility::container::ElementInContainer(entrancePool, entranceToConnect))
            {
                Entrance* connectedTarget = nullptr;
                for (auto& target : targetPool)
                {
                    // If we've found the proper target
                    if (targetToConnect == target->GetReplaces())
                    {
                        try
                        {
                            CheckEntrancesCompatibility(entranceToConnect, target);
                            ChangeConnections(entranceToConnect, target);
                            // If the spawn entrance isn't placed, then we can't validate the world
                            if (world->GetEntrance("Links Spawn -> Outside Links House")->GetConnectedArea() != nullptr)
                            {
                                ValidateWorld(world, entranceToConnect, itemPool);
                            }
                            connectedTarget = target;
                            ConfirmReplacement(entranceToConnect, target);
                        }
                        catch(const EntranceShuffleError& e)
                        {
                            throw std::runtime_error("Could not connect entrance " +
                                                     entranceToConnect->GetOriginalName() + " to " + target->GetOriginalName() +
                                                     " Reason:\n" + e.what());
                        }
                        if (connectedTarget)
                        {
                            break;
                        }
                    }
                }

                // If we found our target, delete the entrance and its now connected target from their respective pools
                if (connectedTarget)
                {
                    utility::container::Erase(entrancePool, entranceToConnect);
                    utility::container::Erase(targetPool, connectedTarget);
                }
                // Otherwise, the target is invalid
                else
                {
                    throw std::runtime_error("Entrance " + targetToConnect->GetOriginalName() + " is not a valid target for " +
                                             entranceToConnect->GetOriginalName());
                }
            }
            else
            {
                throw std::runtime_error("Plandomizer Error: " + entranceToConnect->GetOriginalName() +
                                         " could not be found.");
            }
        }

        LOG_TO_DEBUG("All plandomized entrances have been placed.");
    }

    void ShuffleNonAssumedEntrancesPools(world::World* world,
                                         EntrancePools& entrancePools,
                                         EntrancePools& targetEntrancePools)
    {
        // If we aren't shuffling any non-assumed types, return early
        if (std::ranges::none_of(entrancePools | std::ranges::views::keys, [](const auto& type) {
            return NON_ASSUMED_TYPES.contains(type);
        })) {
            return;
        }

        auto& worlds = world->GetRandomizer()->GetWorlds();
        auto completeItemPool = item_pool::GetCompleteItemPool(worlds);

        // The idea here is we want to try shuffling all the non-assumed entrances
        // at the same time since we can't validate the world after each one individually.
        // (That would require assuming access to entrances which we can't guarantee access to.)
        // Realistically, this should never take more than 1 or 2 tries unless there's some wacky
        // plandomizer stuff going on. Currently, the only non-assumed entrance we're shuffling
        // is the randomized spawn, but if we ever shuffle warp portals, they'll go here too.

        int retries = 10000;
        while (retries > 0)
        {
            std::unordered_map<Entrance*, Entrance*> rollbacks = {};
            // Connect each non-assumed entrance to a random target in its pool
            for (auto& [entranceType, entrancePool] : entrancePools)
            {
                if (NON_ASSUMED_TYPES.contains(entranceType))
                {
                    auto& targetEntrancePool = targetEntrancePools.at(entranceType);
                    for (auto& entrance : entrancePool)
                    {
                        randomizer::utility::random::ShufflePool(targetEntrancePool);

                        // Loop through and find a valid target entrance to connect to
                        for (auto& target : targetEntrancePool)
                        {
                            // If this target has already been used, skip over it
                            if (target->GetConnectedArea() == nullptr)
                            {
                                continue;
                            }

                            LOG_TO_DEBUG("Attempting to connect " + entrance->GetOriginalName() + " to " +
                                         target->GetConnectedArea()->GetName() + " [W" +
                                         std::to_string(entrance->GetWorld()->GetID()) + "]");
                            ChangeConnections(entrance, target);
                            rollbacks[entrance] = target;
                            break;
                        }
                    }
                }
            }

            // After each entrance is connected, then try to validate the world
            bool successfulConnection = false;
            try
            {
                ValidateWorld(world, nullptr, completeItemPool);
                for (auto& [entrance, target] : rollbacks)
                {
                    ConfirmReplacement(entrance, target);
                    utility::container::Erase(targetEntrancePools[entrance->GetType()], target);
                }
                // Once we've made a valid world, delete all other targets that didn't get used
                for (auto& [entranceType, targetPool] : targetEntrancePools)
                {
                    if (NON_ASSUMED_TYPES.contains(entranceType))
                    {
                        for (auto& target : targetPool)
                        {
                            DeleteTargetEntrance(target);
                        }
                        // Also delete the non-assumed entrance type from the pool
                        entrancePools.erase(entranceType);
                    }
                }
                successfulConnection = true;
            }
            catch(const EntranceShuffleError& e)
            {
                // If we're unsuccessful, revert all connections and try again
                LOG_TO_DEBUG(std::string("Failed to connect non-assumed entrances. Reason: ") + e.what());
                retries -= 1;
                for (auto& [entrance, target] : rollbacks)
                {
                    RestoreConnections(entrance, target);
                }
            }
            if(successfulConnection)
            {
                break;
            }
        }

        if (retries <= 0)
        {
            throw std::runtime_error("Ran out of retries when attempting to place non-assumed entrances");
        }
    }

    void ShuffleEntrancePool(EntrancePool& entrancePool,
                             EntrancePool& targetEntrancePool,
                             int retries /* = 20*/)
    {
        while (retries > 0)
        {
            retries -= 1;
            std::unordered_map<Entrance*, Entrance*> rollbacks = {};
            try
            {
                ShuffleEntrances(entrancePool, targetEntrancePool, rollbacks);
                for (auto& [entrance, target] : rollbacks)
                {
                    ConfirmReplacement(entrance, target);
                }
                return;
            }
            catch(const EntranceShuffleError& e)
            {
                for (auto& [entrance, target] : rollbacks)
                {
                    RestoreConnections(entrance, target);
                }
                LOG_TO_DEBUG("Failed to place all entrances in a pool for World " + std::to_string(entrancePool[0]->GetWorld()->GetID()) +
                             ". Will retry " + std::to_string(retries) + " more times");
                LOG_TO_DEBUG(e.what());
            }
        }

        throw std::runtime_error(
            "Ran out of retries when shuffling entrances. If you see this error, try using a few different seeds to see if any "
            "generate successfully.");
    }

    void ShuffleEntrances(EntrancePool& entrancePool,
                          EntrancePool& targetEntrancePool,
                          std::unordered_map<Entrance*, Entrance*>& rollbacks)
    {
        // This shouldn't be empty, but just incase
        if (entrancePool.empty()) {
            return;
        }

        auto& worlds = entrancePool.front()->GetWorld()->GetRandomizer()->GetWorlds();
        auto completeItemPool = item_pool::GetCompleteItemPool(worlds);
        utility::random::ShufflePool(entrancePool);

        for (auto& entrance : entrancePool)
        {
            // If this entrance is already connected, don't connect it to another area
            if (entrance->GetConnectedArea() != nullptr)
            {
                continue;
            }
            utility::random::ShufflePool(targetEntrancePool);

            // Loop through and find a valid target entrance to connect to
            for (auto& target : targetEntrancePool)
            {
                // If this target has already been used, skip over it
                if (target->GetConnectedArea() == nullptr)
                {
                    continue;
                }

                LOG_TO_DEBUG("Attempting to connect " + entrance->GetOriginalName() + " to " +
                             target->GetConnectedArea()->GetName() + " from " +
                             target->GetReplaces()->GetParentArea()->GetName() + " [W" +
                             std::to_string(entrance->GetWorld()->GetID()) +
                             "]");
                if (ReplaceEntrance(entrance, target, rollbacks, completeItemPool))
                {
                    break;
                }
            }

            // If this entrance was unable to connect to target, throw an error
            if (entrance->GetConnectedArea() == nullptr)
            {
                throw EntranceShuffleError("No more valid entrances to replace " + entrance->GetOriginalName() + " in world " +
                                           std::to_string(entrance->GetWorld()->GetID()));
            }
        }

        // Check to make sure there are no dangling targets. If there are, something is very wrong
        for (auto& target : targetEntrancePool)
        {
            if (target->GetConnectedArea() != nullptr)
            {
                throw std::runtime_error("Dangling Target Entrance " + target->GetReplaces()->GetOriginalName());
            }
        }
    }

    bool ReplaceEntrance(Entrance* entrance,
                         Entrance* target,
                         std::unordered_map<Entrance*, Entrance*>& rollbacks,
                         const item_pool::ItemPool& completeItemPool)
    {
        try
        {
            CheckEntrancesCompatibility(entrance, target);
            ChangeConnections(entrance, target);
            ValidateWorld(entrance->GetWorld(), entrance, completeItemPool);
            rollbacks[entrance] = target;
            return true;
        }
        catch(const EntranceShuffleError& e)
        {
            LOG_TO_DEBUG("Failed to connect " + entrance->GetOriginalName() + " to " +
                         target->GetReplaces()->GetOriginalName() + " (Reason: " + e.what() + ") World " +
                         std::to_string(entrance->GetWorld()->GetID()));
            if (entrance->GetConnectedArea() != nullptr)
            {
                RestoreConnections(entrance, target);
            }
        }
        return false;
    }

    void CheckEntrancesCompatibility(const Entrance* entrance, Entrance* target)
    {
        if (entrance->GetReverse() && entrance->GetReverse() == target->GetReplaces())
        {
            throw EntranceShuffleError("Attempted self-connection");
        }
    }

    void ChangeConnections(Entrance* entrance, Entrance* target)
    {
        entrance->Connect(target->Disconnect());
        entrance->SetReplaces(target->GetReplaces());
        // If entrances are coupled, set the opposite connection as well
        if (entrance->GetReverse() != nullptr && !entrance->IsDecoupled())
        {
            target->GetReplaces()->GetReverse()->Connect(entrance->GetReverse()->GetAssumed()->Disconnect());
            target->GetReplaces()->GetReverse()->SetReplaces(entrance->GetReverse());
        }
        CheckAndChangeBossReturn(entrance, target);
    }

    void RestoreConnections(Entrance* entrance, Entrance* target)
    {
        target->Connect(entrance->Disconnect());
        entrance->SetReplaces(nullptr);
        if (entrance->GetReverse() && !entrance->IsDecoupled())
        {
            entrance->GetReverse()->GetAssumed()->Connect(target->GetReplaces()->GetReverse()->Disconnect());
            target->GetReplaces()->GetReverse()->SetReplaces(nullptr);
        }
        CheckAndRestoreBossReturn(entrance, target);
    }

    void ConfirmReplacement(Entrance* entrance, Entrance* target)
    {
        CheckAndConfirmBossReturn(entrance, target);
        DeleteTargetEntrance(target);
        LOG_TO_DEBUG("Finalized Connection " + entrance->GetOriginalName() + " to " + entrance->GetConnectedArea()->GetName() +
                     " [W" + std::to_string(entrance->GetWorld()->GetID()) + "]");
        if (entrance->GetReverse() != nullptr && !entrance->IsDecoupled())
        {
            auto replacedReverse = entrance->GetReplaces()->GetReverse();
            LOG_TO_DEBUG("Finalized Connection " + replacedReverse->GetOriginalName() + " to " +
                         replacedReverse->GetConnectedArea()->GetName() + " [W" +
                         std::to_string(entrance->GetWorld()->GetID()) + "]");
            DeleteTargetEntrance(entrance->GetReverse()->GetAssumed());
        }
    }

    void CheckAndChangeBossReturn(Entrance* entrance, Entrance* target) {
        if (!entrance->GetWorld()->AdjustBossReturns()) {
            return;
        }

        auto bossEntrance = target->GetReplaces()->GetBossEntrance();
        if (bossEntrance)
        {
            // TODO: Properly account for this without needing to also plandomize the boss entrance
            if (!bossEntrance->GetReplaces(true)) {
                throw std::runtime_error("Randomized boss entrance is not set for plandomized dungeon connection " +
                    entrance->GetCurrentName() + ". Please plandomize the associated boss connection as well.");
            }

            // If this is a dungeon entrance with a natural out-of-dungeon boss return, set the
            // proper boss return for the boss found inside this dungeon. If the dungeon this entrance
            // is randomized to expects an out-of-dungeon boss return, but this dungeon doesn't have one,
            // just use the reverse of entering the dungeon (i.e. falling into Lake Hylia when exiting CitS)
            if (entrance->GetBossEntrance()) {
                auto bossReturn = entrance->GetBossEntrance()->GetReverse();
                ChangeConnections(bossEntrance->GetReplaces(true)->GetReverse(), bossReturn->GetAssumed());
            } else {
                auto bossReturn = entrance->GetReverse();
                bossEntrance->GetReplaces(true)->GetReverse()->Connect(bossReturn->GetOriginalConnectedArea());
                bossEntrance->GetReplaces(true)->GetReverse()->SetReplaces(bossReturn);
            }

            // Special case where we need to also set the entrance returning from the mirror chamber
            // to Arbiters Grounds to lead to whatever boss is inside the dungeon that the Arbiters
            // Grounds entrance leads to.
            if (entrance->GetAlias() == "Outside Arbiters Grounds -> Arbiters Grounds") {
                auto mirrorToAG = entrance->GetWorld()->GetEntrance("Mirror Chamber Lower -> Arbiters Grounds Boss Room");
                if (mirrorToAG->GetConnectedArea()) {
                    mirrorToAG->Disconnect();
                }
                mirrorToAG->Connect(entrance->GetReplaces()->GetBossEntrance()->GetConnectedArea());
                mirrorToAG->SetReplaces(entrance->GetReplaces()->GetBossEntrance()->GetReplaces(true));
                mirrorToAG->SetShuffled(true);
            }
        } else if (entrance->GetBossEntrance()) {
            entrance->GetBossEntrance()->GetReverse()->GetAssumed()->Disconnect();
        }
    }

    void CheckAndRestoreBossReturn(Entrance* entrance, Entrance* target) {
        if (!entrance->GetWorld()->AdjustBossReturns()) {
            return;
        }

        auto bossEntrance = target->GetReplaces()->GetBossEntrance();
        if (bossEntrance)
        {
            if (entrance->GetBossEntrance()) {
                auto bossReturn = entrance->GetBossEntrance()->GetReverse();
                RestoreConnections(bossEntrance->GetReplaces(true)->GetReverse(), bossReturn->GetAssumed());
            } else {
                bossEntrance->GetReplaces(true)->GetReverse()->Disconnect();
                bossEntrance->GetReplaces(true)->GetReverse()->SetReplaces(nullptr);
            }
            if (entrance->GetAlias() == "Outside Arbiters Grounds -> Arbiters Grounds") {
                auto mirrorToAG = entrance->GetWorld()->GetEntrance("Mirror Chamber Lower -> Arbiters Grounds Boss Room");
                mirrorToAG->Disconnect();
                mirrorToAG->SetReplaces(nullptr);
            }
        } else if (entrance->GetBossEntrance()) {
            entrance->GetBossEntrance()->GetReverse()->GetAssumed()->Connect(entrance->GetBossEntrance()->GetReverse()->GetOriginalConnectedArea());
        }
    }

    void CheckAndConfirmBossReturn(Entrance* entrance, Entrance* target) {
        if (!entrance->GetWorld()->AdjustBossReturns()) {
            return;
        }

        auto bossEntrance = target->GetReplaces()->GetBossEntrance();
        if (bossEntrance)
        {
            if (entrance->GetBossEntrance()) {
                auto bossReturn = entrance->GetBossEntrance()->GetReverse();
                ConfirmReplacement(bossEntrance->GetReplaces(true)->GetReverse(), bossReturn->GetAssumed());
            }
        } else if (entrance->GetBossEntrance()) {
            DeleteTargetEntrance(entrance->GetBossEntrance()->GetReverse()->GetAssumed());
        }
    }

    void DeleteTargetEntrance(Entrance* target)
    {
        if (target->GetConnectedArea() != nullptr)
        {
            target->Disconnect();
        }
        if (target->GetParentArea() != nullptr)
        {
            target->GetParentArea()->RemoveExit(target);
        }
    }

    void ValidateWorld(world::World* world,
                       Entrance* entrance,
                       const item_pool::ItemPool& completeItemPool)
    {
        // Validate that all logic is still satisfied
        auto& worlds = world->GetRandomizer()->GetWorlds();
        auto verifyLogicError = search::VerifyLogic(&worlds, completeItemPool);
        if (verifyLogicError.has_value())
        {
            throw EntranceShuffleError("Not all logic is satisfied! Reason:\n" + verifyLogicError.value());
        }

        // Check to make sure there's at least 1 sphere zero location available
        auto sphereZeroSearch = search::Search::SphereZero(&worlds);
        sphereZeroSearch.SearchWorlds();
        const auto& foundLocations = sphereZeroSearch._visitedLocations;
        const auto numSphereZeroLocations = std::ranges::count_if(foundLocations, [](const auto& location) {
            return location->IsProgression();
        });

        // If there are no sphere zero locations available and we didn't find an accessible disconnected exit, then this world will not
        // be valid. Often times when many entrances are randomized we won't find any locations, but will find accessible disconnected
        // exits that haven't been shuffled yet. In this case we can usually wait until these exits are connected and more often
        // than not this will lead us to sphere zero locations.
        if (numSphereZeroLocations == 0 && !sphereZeroSearch.HasAccessibleDisconnectedExit())
        {
            throw EntranceShuffleError("No sphere 0 locations reachable at the start!");
        }
    }

    void SetShuffledEntrances(EntrancePools& entrancePools)
    {
        for (auto& [entranceType, entrancePool] : entrancePools)
        {
            for (auto& entrance : entrancePool)
            {
                entrance->SetShuffled(true);
                if (entrance->GetReverse() != nullptr)
                {
                    entrance->GetReverse()->SetShuffled(true);
                }
            }
        }
    }

    EntrancePool GetReverseEntrances(const EntrancePool& entrances)
    {
        EntrancePool reverseEntrances = {};
        for (const auto& entrance : entrances)
        {
            reverseEntrances.push_back(entrance->GetReverse());
        }
        return reverseEntrances;
    }

    const std::set<std::string>& GetPossibleMixedPoolTypes() {
        static std::set<std::string> possibleMixedPoolTypes = {
            TypeToStr(DUNGEON),
            TypeToStr(BOSS),
            TypeToStr(GROTTO),
            TypeToStr(INTERIOR),
            TypeToStr(CAVE),
            TypeToStr(OVERWORLD),
        };

        return possibleMixedPoolTypes;
    }
} // namespace randomizer::logic::entrance_shuffle
