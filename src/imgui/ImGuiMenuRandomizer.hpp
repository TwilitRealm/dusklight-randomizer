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
    bool m_showRandoStats{false};
    bool m_showRandoGeneration{false};
    bool m_showRandoTracker{false};

    randomizer::logic::search::Search m_currentSearch = randomizer::logic::search::Search();
};
}

#endif //DUSK_IMGUI_MENU_RANDOMIZER_HPP
