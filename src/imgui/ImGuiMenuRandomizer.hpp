#ifndef DUSK_IMGUI_MENU_RANDOMIZER_HPP
#define DUSK_IMGUI_MENU_RANDOMIZER_HPP
#include "dusk/randomizer/generator/logic/requirement.hpp"
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
        randomizer::logic::requirement::Requirement logicReq;
        std::string locationItem;
        bool accessible = false;
        bool collected = false;
        bool hidden = false;
    };

    struct TrackerAreaGroup {
        std::vector<LocationTrackerInfo> obtainedLocations;
        std::vector<LocationTrackerInfo> unobtainedLocations;
        bool showArea;
        int totalCount;
        int collectedCount;
        int accessibleCount;

        void addToGroup(LocationTrackerInfo& loc);
    };

    int m_numAvailableLocations;
    int m_numProgressionLocations;
    int m_numCollectedLocations;

    bool m_showRandoStats{false};
    bool m_showRandoGeneration{false};

    bool m_onlyAccessible{false};
    bool m_showRequirements{false};
    bool m_hideAreaHeader{false};
    char m_locationFilter[100];

    randomizer::logic::search::Search m_currentSearch = randomizer::logic::search::Search();

    std::map<std::string, TrackerAreaGroup> m_LocationInfo;
    std::vector<std::string> m_HiddenChecks;

    void generateImGuiRequirementTooltip(const randomizer::logic::requirement::Requirement& req, int nestingLevel = 0);
    void generateLocationInfo();
};
}

#endif //DUSK_IMGUI_MENU_RANDOMIZER_HPP
