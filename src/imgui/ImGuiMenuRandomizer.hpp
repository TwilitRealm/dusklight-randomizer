#ifndef DUSK_IMGUI_MENU_RANDOMIZER_HPP
#define DUSK_IMGUI_MENU_RANDOMIZER_HPP
#include "dusk/randomizer/generator/logic/search.hpp"

namespace randomizer {
class Randomizer;

namespace logic::search {
class Search;
}
}

namespace dusk {
class ImGuiMenuRandomizer {
public:
    ImGuiMenuRandomizer();
    void draw();

    void windowRandoStats();
    void windowRandoGeneration();
    void windowRandoTracker();

    randomizer::Randomizer* getTrackerRando();

private:
    struct LocationTrackerInfo {
        std::string locationName;
        std::string logicStr;
        bool accessible = false;
        bool collected = false;
    };

    struct TrackerAreaGroup {
        std::vector<LocationTrackerInfo> locations;
        bool showArea;
        int collectedCount;
        int accessibleCount;
    };

    size_t m_numAvailableLocations;
    size_t m_numProgressionLocations;
    size_t m_numCollectedLocations;

    bool m_showRandoStats{false};
    bool m_showRandoGeneration{false};

    bool m_onlyAccessible{false};
    bool m_showRequirements{false};
    char m_locationFilter[100];

    randomizer::logic::search::Search m_currentSearch = randomizer::logic::search::Search();

    std::map<std::string, TrackerAreaGroup> m_LocationInfo;

    void generateLocationInfo();
};
}

#endif //DUSK_IMGUI_MENU_RANDOMIZER_HPP
