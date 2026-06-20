#pragma once
#include "window.hpp"

// Forward declaration
namespace randomizer::seedgen::config {
    class Config;
}
class dFile_select_c;

namespace dusk::ui {
class Pane;

    void SaveNewRandomizerPreset(const std::string& presetName, bool overwriteExisting = false);
    void ApplyExistingRandomizerPreset(const std::filesystem::path& presetFilePath);
    void CopyPermalinkToClipboard();
    void PastePermalinkFromClipboard();
    std::filesystem::path GetRandomizerPath();
    std::filesystem::path GetRandomizerSettingsPath();
    std::filesystem::path GetRandomizerPreferencesPath();
    std::filesystem::path GetRandomizerSeedsPath();
    std::filesystem::path GetRandomizerPresetsPath();
    randomizer::seedgen::config::Config& GetRandomizerConfig();


    class RandomizerWindow  : public Window {
    public:

        explicit RandomizerWindow(dFile_select_c* fileSelectMenu = nullptr);
        void rando_excluded_locations_update_left_pane(Pane& innerLeftPane, Pane& rightPane, bool forceUpdate = false);
        auto& get_locations_for_left_pane();

    protected:
        dFile_select_c* mFileSelectMenu{nullptr};
    private:
        std::string m_excludedLocationsFilter{};
    };

    class FileSelectRandomizerWindow : public RandomizerWindow {
    public:
        FileSelectRandomizerWindow(dFile_select_c* fileSelectMenu = nullptr);

    protected:
        bool consume_close_request() override;
    };
}
