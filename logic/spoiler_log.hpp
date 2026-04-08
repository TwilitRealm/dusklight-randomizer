#pragma once

#include "world.hpp"
#include "../seedgen/config.hpp"

namespace tphdr::logic::spoiler_log
{
    void GenerateSpoilerLog(tphdr::logic::world::WorldPool& worlds, tphdr::seedgen::config::Config& config);
    void GenerateAntiSpoilerLog(tphdr::logic::world::WorldPool& worlds, tphdr::seedgen::config::Config& config);
} // namespace tphdr::logic::spoiler_log
