#pragma once

// Forward Declarations
namespace randomizer
{
    class Randomizer;
}

namespace randomizer::logic::spoiler_log
{
    void GenerateSpoilerLog(randomizer::Randomizer* randomizer);
    void GenerateAntiSpoilerLog(randomizer::Randomizer* randomizer);
} // namespace randomizer::logic::spoiler_log
