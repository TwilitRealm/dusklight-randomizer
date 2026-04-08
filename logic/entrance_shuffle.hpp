#pragma once

#include "entrance.hpp"
#include "world.hpp"

namespace tphdr::logic::entrance_shuffle
{
    void ShuffleWorldEntrances(tphdr::logic::world::World* world, tphdr::logic::world::WorldPool& worlds);
    void SetAllEntrancesData(tphdr::logic::world::World* world);
    tphdr::logic::entrance::EntrancePools CreateEntrancePools(tphdr::logic::world::World* world);
    tphdr::logic::entrance::EntrancePools CreateTargetPools(tphdr::logic::entrance::EntrancePools& entrancePools);
    tphdr::logic::entrance::EntrancePool AssumeEntrancePool(tphdr::logic::entrance::EntrancePool& entrancePool);
    void SetPlandomizedEntrances(tphdr::logic::world::World* world,
                                 tphdr::logic::world::WorldPool& worlds,
                                 tphdr::logic::entrance::EntrancePools& entrancePools,
                                 tphdr::logic::entrance::EntrancePools& targetEntrancePools);
    void ShuffleNonAssumedEntrancesPools(tphdr::logic::world::World* world,
                                         tphdr::logic::world::WorldPool& worlds,
                                         tphdr::logic::entrance::EntrancePools& entrancePools,
                                         tphdr::logic::entrance::EntrancePools& targetEntrancePools);
    void ShuffleEntrancePool(tphdr::logic::world::World* world,
                             tphdr::logic::world::WorldPool& worlds,
                             tphdr::logic::entrance::EntrancePool& entrancePool,
                             tphdr::logic::entrance::EntrancePool& targetEntrancePool,
                             int retries = 20);
    void ShuffleEntrances(tphdr::logic::world::WorldPool& worlds,
                          tphdr::logic::entrance::EntrancePool& entrancePool,
                          tphdr::logic::entrance::EntrancePool& targetEntrancePool,
                          std::unordered_map<tphdr::logic::entrance::Entrance*, tphdr::logic::entrance::Entrance*>& rollbacks);
    bool ReplaceEntrance(tphdr::logic::world::WorldPool& worlds,
                         tphdr::logic::entrance::Entrance* entrance,
                         tphdr::logic::entrance::Entrance* target,
                         std::unordered_map<tphdr::logic::entrance::Entrance*, tphdr::logic::entrance::Entrance*>& rollbacks,
                         const tphdr::logic::item_pool::ItemPool& completeItemPool);

    void CheckEntrancesCompatibility(tphdr::logic::entrance::Entrance* entrance, tphdr::logic::entrance::Entrance* target);
    void ChangeConnections(tphdr::logic::entrance::Entrance* entrance, tphdr::logic::entrance::Entrance* target);
    void RestoreConnections(tphdr::logic::entrance::Entrance* entrance, tphdr::logic::entrance::Entrance* target);
    void ConfirmReplacement(tphdr::logic::entrance::Entrance* entrance, tphdr::logic::entrance::Entrance* target);
    void DeleteTargetEntrance(tphdr::logic::entrance::Entrance* target);
    void ValidateWorld(tphdr::logic::world::World* world,
                       tphdr::logic::world::WorldPool& worlds,
                       tphdr::logic::entrance::Entrance* entrance,
                       const tphdr::logic::item_pool::ItemPool& completeItemPool);

    void SetShuffledEntrances(tphdr::logic::entrance::EntrancePools& entrancePools);
    tphdr::logic::entrance::EntrancePool GetReverseEntrances(const tphdr::logic::entrance::EntrancePool& entrances);

    class EntranceShuffleError: public std::runtime_error
    {
       public:
        explicit EntranceShuffleError(const std::string& message): std::runtime_error(message) {}
    };
} // namespace tphdr::logic::entrance_shuffle
