#pragma once

#include <map>

namespace randomizer::archi {

struct WorldData {
    // Map of APLocationId -> APItemId of item at that location
    std::map<int, int> itemPlacements{};
};

}
