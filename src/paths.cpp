#include "paths.hpp"

#include "session.hpp"

namespace randomizer::paths {

std::filesystem::path GetBaseRandomizerPath() {
    const char* dataDir = nullptr;
    if (session::svc_mng.host->data_dir(session::svc_mng.mod_ctx, &dataDir) != MOD_OK) {
        session::svc_mng.host->fail(session::svc_mng.mod_ctx, MOD_ERROR, "Failed to get data directory");
    }

    return reinterpret_cast<const char8_t*>(dataDir);
}

std::filesystem::path GetRandomizerPath(const std::string& subfolder) {
    if (subfolder.empty()) {
        return GetBaseRandomizerPath();
    }
    return GetBaseRandomizerPath() / subfolder;
}

std::filesystem::path GetRandomizerSettingsPath(const std::string& subfolder) {
    return GetRandomizerPath(subfolder) / "settings.yaml";
}

std::filesystem::path GetRandomizerPreferencesPath(const std::string& subfolder) {
    return GetRandomizerPath(subfolder) / "preferences.yaml";
}

std::filesystem::path GetRandomizerPresetsPath(const std::string& subfolder) {
    return GetRandomizerPath(subfolder) / "presets";
}

std::filesystem::path GetRandomizerSeedsPath(const std::string& subfolder) {
    return GetRandomizerPath(subfolder) / "seeds";
}

}  // namespace randomizer::paths
