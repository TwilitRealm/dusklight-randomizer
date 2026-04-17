#pragma once

#include "logic/world.hpp"

#include <optional>

namespace randomizer
{
    class Randomizer
    {
    public:
        explicit Randomizer() = default;

        /**
         *  @brief Generates a complete randomizer seed
         *
         *  @return a std::optional<std::string> containing a message if there was an error.
         */
        std::optional<std::string> Generate();
        void GenerateWorlds();

        auto& GetConfig() { return this->_config; }
        auto& GetWorlds() { return this->_worlds; }

        int GetNewEventID() { return ++(this->_eventIdCounter); }
        int GetNewAreaID() { return ++(this->_areaIdCounter); }
        int GetNewLocAccID() { return ++(this->_locAccIdCounter); }

        auto& GetPlaythroughSpheres() { return this->_playthroughSpheres; }
        auto& GetEntranceSpheres() { return this->_entranceSpheres; }

        std::string GetSeedOutputPath();
        const std::string& GetBaseOutputPath() const { return this->_baseOutputPath; };
        void SetBaseOutputPath(const std::string& path) { this->_baseOutputPath = path + "randomizer/"; };
    private:
        seedgen::config::Config _config{};
        logic::world::WorldPool _worlds{};

        int _eventIdCounter{};
        int _areaIdCounter{};
        int _locAccIdCounter{};

        // Playthrough data
        std::list<std::list<logic::location::Location*>> _playthroughSpheres{};
        std::list<std::list<logic::entrance::Entrance*>> _entranceSpheres{};

        std::string _baseOutputPath{RANDO_SAVE_PATH};
    };
} // namespace randomizer
