#pragma once
#include "window.hpp"

// Forward declaration
namespace randomizer::seedgen::config {
    class Config;
}

namespace dusk::ui {

    std::filesystem::path GetRandomizerSettingsPath();
    std::filesystem::path GetRandomizerPreferencesPath();
    randomizer::seedgen::config::Config& GetRandomizerConfig();

    class RandomizerWindow  : public Window {
    public:
        RandomizerWindow();

    private:
        bool m_showRandoGeneration = false;
    };
}
