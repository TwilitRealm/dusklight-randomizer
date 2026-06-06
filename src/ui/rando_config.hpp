#pragma once
#include "window.hpp"

// Forward declaration
namespace randomizer::seedgen::config {
    class Config;
}

namespace dusk::ui {
class Modal;
class Pane;

    std::filesystem::path GetRandomizerPath();
    std::filesystem::path GetRandomizerSettingsPath();
    std::filesystem::path GetRandomizerPreferencesPath();
    std::filesystem::path GetRandomizerSeedsPath();
    randomizer::seedgen::config::Config& GetRandomizerConfig();

    enum class SeedGenerateStatus {
        Ready,
        Generating,
        Success,
        Error,
    };

    class RandomizerWindow  : public Window {
    public:


        RandomizerWindow();
        void update() override;
        Modal* show_seed_gen_modal(std::string_view message);
        void rando_excluded_locations_update_left_pane(Pane& innerLeftPane, Pane& rightPane, bool forceUpdate = false);
        auto& get_locations_for_left_pane();
    private:
        Modal* m_genSeedModal = nullptr;
        std::string m_excludedLocationsFilter{};
    };
}
