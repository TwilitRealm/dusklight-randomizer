#pragma once

#include <string>

#include "settings.hpp"
#include "../utility/path.hpp"

// forward declaration
namespace YAML
{
    class Node;
}

namespace randomizer::seedgen::config
{
    enum struct [[nodiscard]] ConfigError
    {
        NONE = 0,
        COULD_NOT_OPEN,
        MISSING_KEY,
        DIFFERENT_FILE_VERSION,
        DIFFERENT_RANDO_VERSION,
        BAD_PERMALINK,
        INVALID_VALUE,
        MODEL_ERROR,
        UNKNOWN,
        COUNT
    };

    enum struct [[nodiscard]] PermalinkError
    {
        NONE = 0,
        EMPTY,
        BAD_ENCODING,
        MISSING_PARTS,
        INVALID_VERSION,
        INCORRECT_LENGTH,
        COULD_NOT_READ,
        UNHANDLED_OPTION,
        COULD_NOT_LOAD_LOCATIONS,
        UNKNOWN,
        COUNT
    };

    class Config
    {
       public:
        Config() = default;

        fspath GetGameBasePath() const { return this->_gameBasePath; }
        fspath GetOutputPath() const { return this->_outputPath; }
        fspath GetPlandomizerPath() const { return this->_plandomizerPath; }
        void SetSeed(const std::string& newSeed) { this->_seed = newSeed; }
        std::string GetSeed() const { return this->_seed; }
        auto& GetSettingsList() { return this->_settingsList; }
        bool IsUsingPlandomizer() const { return this->_isUsingPlandomizer; }
        bool IsGeneratingSpoilerLog() const { return this->_isGeneratingSpoilerLog; }

        void resetDefaultSettings();
        void resetDefaultPreferences(const bool& paths = false);
        void LoadFromFile(const fspath& filePath,
                          const fspath& preferencesPath,
                          const bool& createNewIfNotFound = true,
                          const bool& allowRewrite = true);
        YAML::Node SettingsToYaml();
        YAML::Node PreferencesToYaml();
        void WriteSettingsToFile(const fspath& filePath);
        void WritePreferencesToFile(const fspath& preferencesPath);
        void WriteToFile(const fspath& filePath, const fspath& preferencesPath) const;

        PermalinkError loadPermalink(std::string b64permalink);
        std::string getPermalink(const bool& internal = false) const;

        /**
         *  @brief Returns the hash for the config. If the hash is an empty string, then a new one will be generated.
         *
         *  @return The hash as a string
         */
        std::string GetHash();

       private:
        fspath _gameBasePath;
        fspath _outputPath;
        fspath _plandomizerPath;

        std::string _seed;
        std::string _hash;
        std::list<settings::Settings> _settingsList;
        bool _isUsingPlandomizer = false;
        bool _isGeneratingSpoilerLog = true;

        // bool _converted = false;
        // bool _updated = false;
        // bool _configSet = false;
    };

    int WriteDefaultSettings(const fspath& filePath);
    int WriteDefaultPreferences(const fspath& filePath);

    std::string ConfigErrorGetName(ConfigError err);
    std::string PermalinkErrorGetName(ConfigError err);

    int SeedRNG(Config& config, const bool& resolvePreferenceRandom = false, const bool& ignoreInvalidPlandomizer = true);
} // namespace randomizer::seedgen::config
