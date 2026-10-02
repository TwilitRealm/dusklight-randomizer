#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <mods/api.h>

#include "mods/svc/game_mode.h"
#include "mods/svc/ui.h"

// Forward declaration
namespace randomizer::seedgen::config {
class Config;
}
class dFile_select_c;

namespace randomizer::ui {

enum dialogSelectModeState : uint8_t {
    SelectReady,
    SelectWait,
};
extern GameModeNewSaveState *g_dialogSelectModeState;

struct FileSelectGateWindowCtx {
    UiWindowHandle window_handle{};
};
extern FileSelectGateWindowCtx g_file_select_window_ctx;

std::vector<std::string> get_compatible_seed_hashes();
void load_excluded_locations();

void SaveNewRandomizerPreset(const std::string& presetName, bool overwriteExisting = false);
void ApplyExistingRandomizerPreset(const std::filesystem::path& presetFilePath);
void CopyPermalinkToClipboard();
void PastePermalinkFromClipboard();

ModResult buildMenuTab();
ModResult removeMenuTab();
ModResult buildFileSelectGateMenu();
ModResult buildArchipelagoGateMenu();
}
