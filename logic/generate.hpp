#pragma once

#include "world.hpp"
#include "../seedgen/config.hpp"

namespace tphdr::logic::generate
{
    /**
     *  @brief Generates a complete randomizer seed
     *
     *  @param worlds The list of worlds for the generated randomizer seed
     *  @return the worldpool which was generated
     */
    tphdr::logic::world::WorldPool GenerateWorlds();

    /**
     *  @brief Generates a complete randomizer seed with the provided config
     *
     *  @param config The configuration to use for this seed
     *  @param worlds The list of worlds for the generated randomizer seed
     *  @return 0 if no errors. 1 if there were errors
     */
    void GenerateRandomizer(tphdr::seedgen::config::Config& config, tphdr::logic::world::WorldPool& worlds);
} // namespace tphdr::logic::generate
