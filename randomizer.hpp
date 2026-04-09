#pragma once

#include "logic/world.hpp"

namespace randomizer
{
    class Randomizer
    {
    public:
        explicit Randomizer() = default;

        /**
         *  @brief Generates a complete randomizer seed
         *
         *  @return 0 if no errors. 1 if there were errors
         */
        int Generate();
        void GenerateWorlds();

        auto& GetConfig() { return this->_config; }
        auto& GetWorlds() { return this->_worlds; }

        int GetNewEventID() { return ++(this->_eventIdCounter); }
        int GetNewAreaID() { return ++(this->_areaIdCounter); }
        int GetNewLocAccID() { return ++(this->_locAccIdCounter); }

        auto& GetPlaythroughSpheres() { return this->_playthroughSpheres; }
        auto& GetEntranceSpheres() { return this->_entranceSpheres; }
    private:
        seedgen::config::Config _config{};
        logic::world::WorldPool _worlds{};

        int _eventIdCounter{};
        int _areaIdCounter{};
        int _locAccIdCounter{};

        // Playthrough data
        std::list<std::list<logic::location::Location*>> _playthroughSpheres{};
        std::list<std::list<logic::entrance::Entrance*>> _entranceSpheres{};
    };
} // namespace randomizer
