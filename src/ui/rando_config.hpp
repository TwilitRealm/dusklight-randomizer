#pragma once
#include "window.hpp"

// Forward declaration
namespace randomizer::seedgen::config {
    class Config;
}

namespace dusk::ui {

    std::filesystem::path GetRandomizerPath();
    std::filesystem::path GetRandomizerSettingsPath();
    std::filesystem::path GetRandomizerPreferencesPath();
    std::filesystem::path GetRandomizerSeedsPath();
    randomizer::seedgen::config::Config& GetRandomizerConfig();

    class RandomizerWindow  : public Window {
    public:
        RandomizerWindow();
        void update() override;
    private:
        Document* m_genSeedModal = nullptr;
    };
}
