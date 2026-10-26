#pragma once

#include <map>
#include <string>

namespace randomizer::archi {

struct WorldData {
    // Map of APLocationId -> APItemId of item at that location
    std::map<int, int> itemPlacements{};
    // Map of APLocationID -> Off-World Item names for displaying in shops
    std::map<int, std::string> offworldItemNames{};
};

}
