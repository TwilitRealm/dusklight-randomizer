#pragma once

#include "world.hpp"
#include "../seedgen/config.hpp"

namespace randomizer::logic::generate
{
    /**
     *  @brief Generates a complete randomizer seed
     *
     *  @param worlds The list of worlds for the generated randomizer seed
     *  @return the worldpool which was generated
     */
    randomizer::logic::world::WorldPool GenerateWorlds();

    /**
     *  @brief Generates a complete randomizer seed with the provided config
     *
     *  @param config The configuration to use for this seed
     *  @param worlds The list of worlds for the generated randomizer seed
     *  @return 0 if no errors. 1 if there were errors
     */
    void GenerateRandomizer(randomizer::seedgen::config::Config& config, randomizer::logic::world::WorldPool& worlds);
} // namespace randomizer::logic::generate
