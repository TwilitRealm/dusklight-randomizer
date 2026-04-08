#pragma once

#include "entrance.hpp"
#include "world.hpp"

namespace randomizer::logic::entrance_shuffle
{
    void ShuffleWorldEntrances(randomizer::logic::world::World* world, randomizer::logic::world::WorldPool& worlds);
    void SetAllEntrancesData(randomizer::logic::world::World* world);
    randomizer::logic::entrance::EntrancePools CreateEntrancePools(randomizer::logic::world::World* world);
    randomizer::logic::entrance::EntrancePools CreateTargetPools(randomizer::logic::entrance::EntrancePools& entrancePools);
    randomizer::logic::entrance::EntrancePool AssumeEntrancePool(randomizer::logic::entrance::EntrancePool& entrancePool);
    void SetPlandomizedEntrances(randomizer::logic::world::World* world,
                                 randomizer::logic::world::WorldPool& worlds,
                                 randomizer::logic::entrance::EntrancePools& entrancePools,
                                 randomizer::logic::entrance::EntrancePools& targetEntrancePools);
    void ShuffleNonAssumedEntrancesPools(randomizer::logic::world::World* world,
                                         randomizer::logic::world::WorldPool& worlds,
                                         randomizer::logic::entrance::EntrancePools& entrancePools,
                                         randomizer::logic::entrance::EntrancePools& targetEntrancePools);
    void ShuffleEntrancePool(randomizer::logic::world::World* world,
                             randomizer::logic::world::WorldPool& worlds,
                             randomizer::logic::entrance::EntrancePool& entrancePool,
                             randomizer::logic::entrance::EntrancePool& targetEntrancePool,
                             int retries = 20);
    void ShuffleEntrances(randomizer::logic::world::WorldPool& worlds,
                          randomizer::logic::entrance::EntrancePool& entrancePool,
                          randomizer::logic::entrance::EntrancePool& targetEntrancePool,
                          std::unordered_map<randomizer::logic::entrance::Entrance*, randomizer::logic::entrance::Entrance*>& rollbacks);
    bool ReplaceEntrance(randomizer::logic::world::WorldPool& worlds,
                         randomizer::logic::entrance::Entrance* entrance,
                         randomizer::logic::entrance::Entrance* target,
                         std::unordered_map<randomizer::logic::entrance::Entrance*, randomizer::logic::entrance::Entrance*>& rollbacks,
                         const randomizer::logic::item_pool::ItemPool& completeItemPool);

    void CheckEntrancesCompatibility(randomizer::logic::entrance::Entrance* entrance, randomizer::logic::entrance::Entrance* target);
    void ChangeConnections(randomizer::logic::entrance::Entrance* entrance, randomizer::logic::entrance::Entrance* target);
    void RestoreConnections(randomizer::logic::entrance::Entrance* entrance, randomizer::logic::entrance::Entrance* target);
    void ConfirmReplacement(randomizer::logic::entrance::Entrance* entrance, randomizer::logic::entrance::Entrance* target);
    void DeleteTargetEntrance(randomizer::logic::entrance::Entrance* target);
    void ValidateWorld(randomizer::logic::world::World* world,
                       randomizer::logic::world::WorldPool& worlds,
                       randomizer::logic::entrance::Entrance* entrance,
                       const randomizer::logic::item_pool::ItemPool& completeItemPool);

    void SetShuffledEntrances(randomizer::logic::entrance::EntrancePools& entrancePools);
    randomizer::logic::entrance::EntrancePool GetReverseEntrances(const randomizer::logic::entrance::EntrancePool& entrances);

    class EntranceShuffleError: public std::runtime_error
    {
       public:
        explicit EntranceShuffleError(const std::string& message): std::runtime_error(message) {}
    };
} // namespace randomizer::logic::entrance_shuffle
