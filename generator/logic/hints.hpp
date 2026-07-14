#pragma once

#include <memory>

namespace randomizer::logic::world {
class World;
using WorldPool = std::vector<std::unique_ptr<World>>;
}

namespace randomizer::logic::hints {

    void GenerateAllHints(world::WorldPool& worldPool);

}