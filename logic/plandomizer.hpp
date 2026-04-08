#pragma once

#include <filesystem>
#include <memory>
#include <vector>

using fspath = std::filesystem::path;

// Forward Declarations
namespace tphdr::logic::world
{
    class World;
    using WorldPool = std::vector<std::unique_ptr<World>>;
} // namespace tphdr::logic::world

namespace tphdr::logic::plandomizer
{
    void LoadPlandomizerData(tphdr::logic::world::WorldPool& worlds, const fspath& filepath, const bool& ignoreErrors = false);
}