#pragma once

#include "randomizer_type.hpp"
#include "logic/world.hpp"
#include "../src/archi/ap_data.hpp"

#include <optional>

namespace randomizer
{
    class Randomizer
    {
    public:
        Randomizer() = delete;
        Randomizer(Type type, const std::filesystem::path& baseOutputPath, const archi::WorldData& apData = archi::WorldData{}) :
            _type(type),
            _baseOutputPath(baseOutputPath),
            _apData(apData)
        {}

        /**
         *  @brief Generates a complete randomizer seed
         *
         *  @return a std::optional<std::string> containing a message if there was an error.
         */
        std::optional<std::string> Generate();
        void GenerateWorlds();
#ifndef RANDOMIZER_ONLY
        void GenerateTrackerWorld();
#endif
        void GenerateAPWorld();

        Type GetType() { return this->_type; }
        auto& GetConfig() { return this->_config; }
        auto& GetWorlds() { return this->_worlds; }
        /**
         * @param worldId
         * @return The world with the specified Id. If no Id is specified, the first world will
         * be returned. If a world with the Id does not exist, a nullptr will be returned.
         */
        logic::world::World* GetWorld(int worldId = 1);

        int GetNewEventID() { return ++(this->_eventIdCounter); }
        int GetNewAreaID() { return ++(this->_areaIdCounter); }
        int GetNewLocAccID() { return ++(this->_locAccIdCounter); }

        auto& GetPlaythroughSpheres() { return this->_playthroughSpheres; }
        auto& GetEntranceSpheres() { return this->_entranceSpheres; }

        const auto& GetAPData() { return this->_apData; }

        std::filesystem::path GetSeedOutputPath();
        std::filesystem::path GetBaseOutputPath() const { return this->_baseOutputPath; };
        void SetBaseOutputPath(const std::filesystem::path& path) { this->_baseOutputPath = path; };

        std::filesystem::path GetConfigPath() const { return this->GetBaseOutputPath() / "settings.yaml"; }
        std::filesystem::path GetPrefPath() const { return this->GetBaseOutputPath() / "preferences.yaml"; }
    private:
        Type _type = Type::DEFAULT;
        seedgen::config::Config _config{};
        logic::world::WorldPool _worlds{};

        int _eventIdCounter{};
        int _areaIdCounter{};
        int _locAccIdCounter{};

        // Playthrough data
        std::list<std::list<logic::location::Location*>> _playthroughSpheres{};
        std::list<std::list<logic::entrance::Entrance*>> _entranceSpheres{};

        archi::WorldData _apData{};

        std::filesystem::path _baseOutputPath{};
    };
} // namespace randomizer
