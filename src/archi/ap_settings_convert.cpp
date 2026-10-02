#include "ap_settings_convert.hpp"

#include "mods/svc/log.hpp"

namespace randomizer::archi {

struct SettingsNameConvert {
    static constexpr std::string kDefaultYes = "On";
    static constexpr std::string kDefaultNo = "Off";

    std::string apName;
    std::string dusklightName;
    std::vector<std::pair<std::string, std::string>> optionsConvert;

    const std::string& tryGetOptionConvert(const std::string& option) const {
        if (optionsConvert.empty()) {
            if (option == "Yes")
                return kDefaultYes;
            if (option == "No")
                return kDefaultNo;
            return option;
        }

        for (const auto& value : optionsConvert) {
            if (value.first == option) {
                return value.second;
            }
        }
        return option;
    }
};

static auto sArchiSettingToDusklight = std::to_array<SettingsNameConvert>({
    {"", ""},
    {"Golden Bugs Shuffled", "Golden Bugs"},
    {"Sky Chracters Shuffled", "Sky Characters"},
    {"NPC Items Shuffled", "Gifts From NPCs"},
    {"Shop Items Shuffled", "Shop Items"},
    {"Hidden Skills Shuffled", "Hidden Skills"},
    {"Skip Prologue", "Skip Prologue"},
    {"Faron Twilight Cleared", "Faron Twilight Cleared"},
    {"Eldin Twilight Cleared", "Eldin Twilight Cleared"},
    {"Lanayru Twilight Cleared", "Lanayru Twilight Cleared"},
    {"Skip MDH", "Skip Midna's Desparate Hour"},
    {"Open Map", "Unlock Map Regions"},
    {"Increase Wallet", "Logic Increase Wallet Capacity"},
    {"Transform Anywhere", "Logic Transform Anywhere"},
    {"Bonks do Damage", "Bonks Do Damage"},
    {"Lakebed Entrance Requirements", "Lakebed Does Not Require Water Bombs"},
    {"Arbiters Grounds Entrance Requirements", "Arbiters Does Not Require Bulblin Camp"},
    {"Snowpeak Entrance Requirements", "Snowpeak Does Not Require Reekfish Scent"},
    {"City in the Sky Entrance Requirements", "City Does Not Require Filled Skybook"},
    {"Goron Mines Entrance Requirements", "Goron Mines Entrance"},
    {"Palace of Twilight Requirements", "Palace of Twilight Requirements"},
    {"Faron Woods Logic", "Faron Woods Logic"},
{"Starting ToD", "Starting Time of Day"},
   {"Skip Major Cutscenes", "Skip Major Cutscenes"},
{"Skip Minor Cutscenes", "Skip Minor Cutscenes"},
   {"Open Door of Time", "Open Door of Time"},

    {"Dungeon Rewards Progression", "Dungeon Rewards Can Be Anywhere", {
         // these two are functionally identical in terms of tracker logic, so treat it as such
         {"Anything", "On"},
         {"Any Progressive", "On"},
         {"Vanilla", "Off"},
     }},
    {"Small Key Settings", "Small Keys", {
         {"Startwith", "Keysy"},
     }},
    {"Big Key Settings", "Big Keys", {
         {"Startwith", "Keysy"},
     }},
    {"Map and Compass Settings", "Maps and Compasses", {
         {"Startwith", "Start With"},
     }},
    {"Trap Frequency", "Trap Item Frequency", {
         {"No Traps", "None"},
     }},
    {"Damage Magnification", "Logic Damage Multiplier", {
         {"Ohko", "OHKO"},
     }},
    {"Logic Settings", "Logic Rules", {
         {"Glitchless", "All Locations Reachable"},
         {"Glitched", "Beatable Only"},
    }},
    {"Poes Shuffled", "Poe Souls", {
        {"Yes", "All"},
        {"No", "Vanilla"}
    }}
});

const SettingsNameConvert& GetAPSettingNameConvert(const std::string& apSettingName) {
    for (const auto& entry : sArchiSettingToDusklight) {
        if (entry.apName == apSettingName)
            return entry;
    }
    return sArchiSettingToDusklight[0];
}

void createConfigFromArchiSettings(seedgen::config::Config& config,
    const nlohmann::json& apSettingsJson) {
    auto& settings = config.GetSettings();

    // update settings using ap config
    for (auto& [apSettingName, apSettingValueJson] : apSettingsJson.items()) {
        auto apSettingValue = apSettingValueJson.get<std::string>();

        const auto& settingConvert = GetAPSettingNameConvert(apSettingName);

        if (!settingConvert.apName.empty()) {
            auto& setting = settings.GetMap().at(settingConvert.dusklightName);
            setting.SetCurrentOption(settingConvert.tryGetOptionConvert(apSettingValue));
        } else if (apSettingName == "Castle Requirements") {
            auto& setting = settings.GetMap().at("Hyrule Barrier Requirements");

            // ap assumes max mirror shards/fused shadows/dungeons, so update those settings as well

            if(apSettingValue == "Open")
                setting.SetCurrentOption("Open");
            else if(apSettingValue == "Vanilla")
                setting.SetCurrentOption("Vanilla");
            else if(apSettingValue == "Fused Shadows") {
                setting.SetCurrentOption("Fused Shadows");
                settings.GetMap().at("Hyrule Barrier Fused Shadows").SetCurrentOption("3");
            }else if(apSettingValue == "Mirror Shards") {
                setting.SetCurrentOption("Mirror Shards");
                settings.GetMap().at("Hyrule Barrier Mirror Shards").SetCurrentOption("4");
            }else if(apSettingValue == "All Dungeons") {
                setting.SetCurrentOption("Dungeons");
                settings.GetMap().at("Hyrule Barrier Dungeons").SetCurrentOption("8");
            }
        }else if (apSettingName == "Temple of Time Entrance Requirements") {
            auto& setting = settings.GetMap().at("Sacred Grove Does Not Require Skull Kid");
            auto& setting2 = settings.GetMap().at("Temple of Time Sword Requirement");

            if(apSettingValue == "Closed") {
                setting.SetCurrentOption("Off");
                setting2.SetCurrentOption("Master Sword");
            }else if (apSettingValue == "Open Grove") {
                setting.SetCurrentOption("On");
                setting2.SetCurrentOption("Master Sword");
            }else if (apSettingValue == "Open") {
                setting.SetCurrentOption("On");
                setting2.SetCurrentOption("None");
            }
        }else {
            mods::log::debug("Missing Setting: {} Value: {}", apSettingName, apSettingValue);
        }
    }
}

} // randomizer::archi