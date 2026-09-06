#pragma once

#include "hint_types.hpp"

#include <memory>
#include <vector>
#include <unordered_set>

namespace randomizer::logic::world {
class World;
using WorldPool = std::vector<std::unique_ptr<World>>;
}

namespace randomizer::logic::location {
class Location;
using LocationPool = std::vector<Location*>;
}

namespace randomizer::logic::hints {

class HintGenerator {
public:
    HintGenerator(world::World* world);

    void GeneratePathHints();
    void GenerateBarrenHints();
    void GenerateItemHints();
    void GenerateRemoteLocationHints();
    void GenerateLocationHints();
    void DistributeHints();
    bool CheckAvailableSign(location::Location* location, const std::string& setting);
    void ReserveLimitedAvailabilityHint(location::Location* location, const std::string& setting, Hint& hint);
    void AssignHintSignHints(const location::LocationPool& hintSigns, std::vector<Hint> hints);
    void FinalizeHintSignText();

private:
    world::World* _world{};
    std::vector<Hint> _pathHints{};
    std::vector<Hint> _barrenHints{};
    std::vector<Hint> _itemHints{};
    std::vector<Hint> _locationHints{};

    std::unordered_set<location::Location*> _reservedSigns{};
};

void GenerateAllHints(world::WorldPool& worldPool);

}