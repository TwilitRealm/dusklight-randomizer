#pragma once

#include "world.hpp"
#include "../seedgen/config.hpp"

namespace randomizer::logic::spoiler_log
{
    void GenerateSpoilerLog(randomizer::logic::world::WorldPool& worlds, randomizer::seedgen::config::Config& config);
    void GenerateAntiSpoilerLog(randomizer::logic::world::WorldPool& worlds, randomizer::seedgen::config::Config& config);
} // namespace randomizer::logic::spoiler_log
