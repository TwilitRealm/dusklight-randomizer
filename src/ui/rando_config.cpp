#include "rando_config.hpp"

#include <mutex>

#include "bool_button.hpp"
#include "pane.hpp"
#include "dusk/config.hpp"
#include "dusk/logging.h"

namespace dusk::ui {

struct ConfigBoolProps {
    Rml::String key;
    Rml::String icon;
    Rml::String helpText;
    std::function<void(bool)> onChange;
    std::function<bool()> isDisabled;
};

static bool generatingSeed = false;
static std::string generationStatusMsg{};
static std::mutex generationStatusMsgMutex{};

static void StartSeedGeneration() {
    if (generatingSeed) {
        return;
    }

    generatingSeed = true;
    std::lock_guard lock(generationStatusMsgMutex);
    GenerateAndWriteSeed(generationStatusMsg);
    generatingSeed = false;
    DuskLog.debug("{}", generationStatusMsg);
}

// ripped straight from settings window
SelectButton& config_bool_select(
    Pane& leftPane, Pane& rightPane, ConfigVar<bool>& var, ConfigBoolProps props) {
    auto& button = leftPane.add_child<BoolButton>(BoolButton::Props{
        .key = std::move(props.key),
        .icon = std::move(props.icon),
        .getValue = [&var] { return var.getValue(); },
        .setValue =
            [&var, callback = std::move(props.onChange)](bool value) {
                if (value == var.getValue()) {
                    return;
                }
                var.setValue(value);
                config::Save();
                if (callback) {
                    callback(value);
                }
            },
        .isDisabled = std::move(props.isDisabled),
        .isModified = [&var] { return var.getValue() != var.getDefaultValue(); },
    });
    leftPane.register_control(
        button, rightPane, [helpText = std::move(props.helpText)](Pane& pane) {
            pane.clear();
            pane.add_rml(helpText);
        });
    return button;
}

auto& FindSetting(const std::string& key) {
    // TODO: handle multi-world selection
    auto& settings = g_RandomizerGenerator.GetConfig().GetSettingsList().front();
    return settings.GetMap().at(key);
}

auto* FindSettingPtr(const std::string& key) {
    // TODO: handle multi-world selection
    auto& settings = g_RandomizerGenerator.GetConfig().GetSettingsList().front();
    return &settings.GetMap().at(key);
}

void SaveConfig() {
    g_RandomizerGenerator.GetConfig().WriteSettingsToFile(g_RandomizerGenerator.GetConfigPath());
}

void rando_config_group(Pane& leftPane, Pane& rightPane, std::string settingKey) {
    auto randoSettings = randomizer::seedgen::settings::GetAllSettingsInfo();
    auto& settingData = randoSettings->at(settingKey);

    if (settingData == nullptr) {
        return;
    }
    auto& curSetting = FindSetting(settingKey);

    leftPane.register_control(
    leftPane.add_select_button({
        .key = settingKey,
        .getValue =
        [settingKey = std::move(settingKey)] { return Rml::String{FindSetting(settingKey).GetCurrentOption()}; },
    }),
    rightPane, [&curSetting, settingKey](Pane& pane) {
        auto curSelIdx = curSetting.GetCurrentOptionIndex();
        auto settingInfo = curSetting.GetInfo();

        Rml::Element* text_elem = pane.add_rml(settingInfo->GetDescriptions().at(curSelIdx));
        for (int i = 0; i < settingInfo->GetOptions().size(); ++i) {
            pane.add_button(
                {
                    .text = settingInfo->GetOptions()[i],
                    .isSelected = [settingKey = std::move(settingKey), i] { return FindSetting(settingKey).GetCurrentOptionIndex() == i; },
                })
                .on_pressed([i, text_elem, settingKey = std::move(settingKey)] {
                    auto& curSetting = FindSetting(settingKey);
                    auto settingInfo = curSetting.GetInfo();

                    mDoAud_seStartMenu(kSoundItemChange);
                    curSetting.SetCurrentOption(i);
                    text_elem->SetInnerRML(settingInfo->GetDescriptions().at(i));

                    SaveConfig();
                });
        }
    });
}

SelectButton& rando_config_toggle(
    Pane& leftPane, Pane& rightPane, std::string settingKey) {
    auto& button = leftPane.add_child<BoolButton>(BoolButton::Props{
        .key = settingKey,
        .getValue = [settingKey] { return FindSetting(settingKey).GetCurrentOptionIndex() != 0; },
        .setValue =
            [settingKey](bool value) {
                auto& setting = FindSetting(settingKey);
                auto idx = setting.GetCurrentOptionIndex();
                if (idx == value) {
                    return;
                }

                setting.SetCurrentOption(value);

                SaveConfig();
            },
    });
    auto& comp = leftPane.register_control(
        button, rightPane, [settingKey](Pane& pane) {
            pane.clear();

            auto& setting = FindSetting(settingKey);
            auto info = setting.GetInfo();

            pane.add_rml(info->GetDescriptions()[setting.GetCurrentOptionIndex()]);
        });

    comp.listen(comp.root(), Rml::EventId::Click, [&rightPane, settingKey](Rml::Event&) {
        rightPane.clear();
        auto& setting = FindSetting(settingKey);
        auto info = setting.GetInfo();

        rightPane.add_rml(info->GetDescriptions()[setting.GetCurrentOptionIndex()]);
    });

    return button;
}

RandomizerWindow::RandomizerWindow() {
    add_tab("Seed Management", [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Controlled);

        config_bool_select(leftPane, rightPane, getSettings().game.randomizerEnabled, {
            .key = "Randomizer Enabled",
            .helpText = "Determines if a new Save will load as a regular save or randomized save. (Doesn't do anything currently)"
        });

        leftPane.add_button("Generate Seed").on_pressed([this] {
            std::thread randoGenerationThread(StartSeedGeneration);
            randoGenerationThread.detach();
            m_showRandoGeneration = true;
        });
    });

    add_tab("Seed Options", [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section("Logic Settings");

        rando_config_group(leftPane, rightPane, "Logic Rules");

        leftPane.add_section("Access Options");

        rando_config_group(leftPane, rightPane, "Hyrule Barrier Requirements");
        rando_config_group(leftPane, rightPane, "Palace of Twilight Requirements");
        rando_config_group(leftPane, rightPane, "Faron Woods Logic");

        leftPane.add_section("World (TODO)");

        leftPane.add_section("Item Pool");

        rando_config_toggle(leftPane, rightPane, "Golden Bugs");
        rando_config_toggle(leftPane, rightPane, "Sky Characters");
        rando_config_toggle(leftPane, rightPane, "Gifts From NPCs");
        rando_config_toggle(leftPane, rightPane, "Shop Items");
        rando_config_toggle(leftPane, rightPane, "Hidden Skills");
        rando_config_toggle(leftPane, rightPane, "Hidden Rupees");
        rando_config_toggle(leftPane, rightPane, "Freestanding Rupees");

        rando_config_group(leftPane, rightPane, "Poe Souls");
        rando_config_group(leftPane, rightPane, "Ilia Memory Quest");
        rando_config_group(leftPane, rightPane, "Item Scarcity");

        leftPane.add_section("Dungeon Items");

        rando_config_group(leftPane, rightPane, "Small Keys");
        rando_config_group(leftPane, rightPane, "Big Keys");
        rando_config_group(leftPane, rightPane, "Maps and Compasses");
        rando_config_group(leftPane, rightPane, "Hyrule Castle Big Key Requirements");
        // TODO: figure out a way to add conditional options depending on selection above

        rando_config_toggle(leftPane, rightPane, "Dungeon Rewards Can Be Anywhere");
        rando_config_toggle(leftPane, rightPane, "No Small Keys on Bosses");
        rando_config_toggle(leftPane, rightPane, "Unrequired Dungeons Are Barren");

        leftPane.add_section("Timesavers");

        rando_config_toggle(leftPane, rightPane, "Skip Prologue");
        rando_config_toggle(leftPane, rightPane, "Faron Twilight Cleared");
        rando_config_toggle(leftPane, rightPane, "Eldin Twilight Cleared");
        rando_config_toggle(leftPane, rightPane, "Lanayru Twilight Cleared");
        rando_config_toggle(leftPane, rightPane, "Skip Midna's Desparate Hour");
        rando_config_toggle(leftPane, rightPane, "Skip Minor Cutscenes");
        rando_config_toggle(leftPane, rightPane, "Skip Major Cutscenes");
        rando_config_toggle(leftPane, rightPane, "Unlock Map Regions");
        rando_config_toggle(leftPane, rightPane, "Open Door of Time");
        rando_config_toggle(leftPane, rightPane, "Active Goron Mines Magnets");
        rando_config_toggle(leftPane, rightPane, "Lower Hyrule Castle Chandelier");
        rando_config_toggle(leftPane, rightPane, "Skip Bridge Donation");

        leftPane.add_section("Additional Settings");

        rando_config_group(leftPane, rightPane, "Starting Form");
        rando_config_toggle(leftPane, rightPane, "Increase Wallet Capacity");
        rando_config_toggle(leftPane, rightPane, "Shops Display The Replaced Item");
        rando_config_toggle(leftPane, rightPane, "Bonks Do Damage");
        rando_config_group(leftPane, rightPane, "Trap Item Frequency");
        rando_config_toggle(leftPane, rightPane, "Starting Time of Day");

        leftPane.add_section("Dungeon Entrance Settings");

        rando_config_toggle(leftPane, rightPane, "Lakebed Does Not Require Water Bombs");
        rando_config_toggle(leftPane, rightPane, "Arbiters Does Not Require Bulblin Camp");
        rando_config_toggle(leftPane, rightPane, "Snowpeak Does Not Require Reekfish Scent");
        rando_config_toggle(leftPane, rightPane, "Sacred Grove Does Not Require Skull Kid");
        rando_config_toggle(leftPane, rightPane, "City Does Not Require Filled Skybook");
        rando_config_group(leftPane, rightPane, "Goron Mines Entrance");
        rando_config_group(leftPane, rightPane, "Temple of Time Sword Requirement");
        rando_config_toggle(leftPane, rightPane, "Randomize Starting Spawn");
        rando_config_group(leftPane, rightPane, "Randomize Dungeon Entrances");
        rando_config_toggle(leftPane, rightPane, "Randomize Boss Entrances");
        rando_config_toggle(leftPane, rightPane, "Randomize Grotto Entrances");
        rando_config_toggle(leftPane, rightPane, "Randomize Cave Entrances");
        rando_config_toggle(leftPane, rightPane, "Randomize Interior Entrances");
        rando_config_toggle(leftPane, rightPane, "Randomize Overworld Entrances");
        rando_config_toggle(leftPane, rightPane, "Decouple Double Door Entrances");
        rando_config_toggle(leftPane, rightPane, "Decouple Entrances");

        leftPane.add_section("Tricks");

        rando_config_toggle(leftPane, rightPane, "Back Slice as Sword");
        rando_config_toggle(leftPane, rightPane, "Ball and Chain Webs");
        rando_config_toggle(leftPane, rightPane, "Logic Transform Anywhere");
    });
}

}