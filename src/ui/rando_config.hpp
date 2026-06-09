#pragma once
#include "window.hpp"

// Forward declaration
namespace randomizer::seedgen::config {
    class Config;
}

namespace dusk::ui {
class Pane;

    std::filesystem::path GetRandomizerPath();
    std::filesystem::path GetRandomizerSettingsPath();
    std::filesystem::path GetRandomizerPreferencesPath();
    std::filesystem::path GetRandomizerSeedsPath();
    randomizer::seedgen::config::Config& GetRandomizerConfig();


    class RandomizerWindow  : public Window {
    public:

        RandomizerWindow();
        void rando_excluded_locations_update_left_pane(Pane& innerLeftPane, Pane& rightPane, bool forceUpdate = false);
        auto& get_locations_for_left_pane();
    private:
        std::string m_excludedLocationsFilter{};
    };
}
