#include "rando_config.hpp"

#include <mutex>
#include <thread>

#include "bool_button.hpp"
#include "modal.hpp"
#include "dusk/config.hpp"
#include "dusk/data.hpp"
#include "dusk/logging.h"
#include "number_button.hpp"
#include "pane.hpp"
#include "string_button.hpp"
#include "dusk/randomizer/game/tools.h"
#include "dusk/randomizer/generator/seedgen/seed.hpp"

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

static void StartSeedGeneration() {
    GenerateAndWriteSeed(generationStatusMsg);
    generatingSeed = false;
    DuskLog.debug("{}", generationStatusMsg);
}

randomizer::seedgen::settings::Setting* FindSetting(const std::string& key) {
    if (key.empty()) {
        DuskLog.fatal("Key is empty! Unable to find setting.");
    }

    // TODO: handle multi-world selection
    auto& settings = GetRandomizerConfig().GetSettings();
    try {
        return &settings.GetMap().at(key);
    } catch (std::exception e) {
        DuskLog.fatal("Failed to get Settings Key: {}", key);
    }
}

void SaveConfig() {
    GetRandomizerConfig().WriteToFile(GetRandomizerSettingsPath(), GetRandomizerPreferencesPath());
}

bool TryCreateRandomSeed() {
    auto& config = GetRandomizerConfig();

    const std::string& configSeed = config.GetSeed();
    if (configSeed.empty()) {
        config.SetSeed(randomizer::seedgen::seed::GenerateSeed());
        SaveConfig();
        return true;
    }
    return false;
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

void rando_config_group(Pane& leftPane, Pane& rightPane, std::string settingKey,
    std::function<Component*(const std::string&, Pane&)> onSelected = nullptr) {
    auto randoSettings = randomizer::seedgen::settings::GetAllSettingsInfo();
    auto& settingData = randoSettings->at(settingKey);

    if (settingData == nullptr) {
        return;
    }
    auto curSetting = FindSetting(settingKey);

    leftPane.register_control(
        leftPane.add_select_button({
            .key = settingKey,
            .getValue =
            [curSetting] { return Rml::String{curSetting->GetCurrentOption()}; },
        }),
        rightPane, [curSetting, onSelected](Pane& pane) {
            auto curSelIdx = curSetting->GetCurrentOptionIndex();
            auto settingInfo = curSetting->GetInfo();

            Rml::Element* text_elem = pane.add_rml(settingInfo->GetDescriptions().at(curSelIdx));

            for (int i = 0; i < settingInfo->GetOptions().size(); ++i) {
                pane.add_button(
                    {
                        .text = settingInfo->GetOptions()[i],
                        .isSelected = [curSetting, i] {
                            return curSetting->GetCurrentOptionIndex() == i;
                        },
                    })
                    .on_pressed([i, text_elem, curSetting] {
                        auto settingInfo = curSetting->GetInfo();

                        mDoAud_seStartMenu(kSoundItemChange);
                        curSetting->SetCurrentOption(i);
                        text_elem->SetInnerRML(settingInfo->GetDescriptions().at(i));

                        SaveConfig();
                    });
            }

            if (onSelected) {
                onSelected(curSetting->GetCurrentOption(), pane);
            }
        });
}

SelectButton& rando_config_toggle(
    Pane& leftPane, Pane& rightPane, std::string settingKey) {
    auto setting = FindSetting(settingKey);

    auto& button = leftPane.add_child<BoolButton>(BoolButton::Props{
        .key = settingKey,
        .getValue = [setting] { return setting->GetCurrentOptionIndex() != 0; },
        .setValue =
        [setting](bool value) {
            auto idx = setting->GetCurrentOptionIndex();
            if (idx == value) {
                return;
            }

            setting->SetCurrentOption(value);

            SaveConfig();
        },
    });
    auto& comp = leftPane.register_control(
        button, rightPane, [setting](Pane& pane) {
            pane.clear();

            auto info = setting->GetInfo();
            pane.add_rml(info->GetDescriptions()[setting->GetCurrentOptionIndex()]);
        });

    comp.listen(comp.root(), Rml::EventId::Click, [&rightPane, setting](Rml::Event&) {
        rightPane.clear();

        auto info = setting->GetInfo();
        rightPane.add_rml(info->GetDescriptions()[setting->GetCurrentOptionIndex()]);
    });

    return button;
}

NumberButton* rando_add_optional_setting(std::string optionValue, std::string optionsKeyPrefix,
    Pane& pane) {
    std::string fullOptionalKey = fmt::format("{} {}", optionsKeyPrefix, optionValue);

    // check if setting exists
    auto randoSettings = randomizer::seedgen::settings::GetAllSettingsInfo();
    if (!randoSettings->contains(fullOptionalKey)) {
        return nullptr;
    }

    auto curSetting = FindSetting(fullOptionalKey);
    const auto& options = curSetting->GetInfo()->GetOptions();

    return &pane.add_child<NumberButton>(NumberButton::Props{
        .key = fmt::format("{} Count", optionValue),
        .getValue = [curSetting] { return curSetting->GetCurrentOptionAsNumber(); },
        .setValue = [curSetting](int value) {
            curSetting->SetCurrentOption(std::to_string(value));
            SaveConfig();
        },
        .min = std::stoi(options.front()),
        .max = std::stoi(options.back()),
    });
}


const std::vector<std::pair<std::string, std::string>>& GetStartingInventoryLayoutOrder() {
    static const std::vector<std::pair<std::string, std::string>> layoutOrder = {
        // { display name , logic item name }
        {"Shadow Crystal", "Shadow Crystal"},
        {"Horse Call", "Horse Call"},
        {"Fishing Rod", "Progressive Fishing Rod"},
        {"Slingshot", "Slingshot"},
        {"Lantern", "Lantern"},
        {"Gale Boomerang", "Gale Boomerang"},
        {"Iron Boots", "Iron Boots"},
        {"Bow", "Progressive Bow"},
        {"Hawkeye", "Hawkeye"},
        {"Bomb Bags", "Bomb Bag"},
        {"Giant Bomb Bags", "Giant Bomb Bag"},
        {"Clawshot", "Progressive Clawshot"},
        {"Spinner", "Spinner"},
        {"Ball and Chain", "Ball and Chain"},
        {"Dominion Rod", "Progressive Dominion Rod"},
        {"Empty Bottle", "Empty Bottle"},
        {"Auru's Memo", "Aurus Memo"},
        {"Ashei's Sketch", "Asheis Sketch"},
        {"Sky Book", "Progressive Sky Book"},
        {"Sword", "Progressive Sword"},
        {"Ordon Shield", "Ordon Shield"},
        {"Hylian Shield", "Hylian Shield"},
        {"Zora Armor", "Zora Armor"},
        {"Magic Armor", "Magic Armor"},
        {"Wallet", "Progressive Wallet"},
        {"Hidden Skills", "Progressive Hidden Skill"},
        {"Poe Souls", "Poe Soul"},
        {"Fused Shadows", "Progressive Fused Shadow"},
        {"Mirror Shards", "Progressive Mirror Shard"},
        {"Gate Keys", "Gate Keys"},
        {"Gerudo Desert Bulblin Camp Key", "Gerudo Desert Bulblin Camp Key"},
        {"Forest Temple Small Keys", "Forest Temple Small Key"},
        {"Goron Mines Small Keys", "Goron Mines Small Key"},
        {"Lakebed Temple Small Keys", "Lakebed Temple Small Key"},
        {"Arbiter's Grounds Small Keys", "Arbiters Grounds Small Key"},
        {"Snowpeak Ruins Small Keys", "Snowpeak Ruins Small Key"},
        {"Ordon Pumpkin", "Ordon Pumpkin"},
        {"Ordon Cheese", "Ordon Cheese"},
        {"Temple of Time Small Keys", "Temple of Time Small Key"},
        {"City in the Sky Small Keys", "City in the Sky Small Key"},
        {"Palace of Twilight Small Keys", "Palace of Twilight Small Key"},
        {"Hyrule Castle Small Keys", "Hyrule Castle Small Key"},
        {"Forest Temple Big Key", "Forest Temple Big Key"},
        {"Goron Mines Key Shards", "Goron Mines Key Shard"},
        {"Lakebed Temple Big Key", "Lakebed Temple Big Key"},
        {"Arbiter's Grounds Big Key", "Arbiters Grounds Big Key"},
        {"Snowpeak Ruins Bedroom Key", "Snowpeak Ruins Bedroom Key"},
        {"Temple of Time Big Key", "Temple of Time Big Key"},
        {"City in the Sky Big Key", "City in the Sky Big Key"},
        {"Palace of Twilight Big Key", "Palace of Twilight Big Key"},
        {"Hyrule Castle Big Key", "Hyrule Castle Big Key"},
        {"Gerudo Desert Portal", "Gerudo Desert Portal"},
        {"Mirror Chamber Portal", "Mirror Chamber Portal"},
        {"Snowpeak Portal", "Snowpeak Portal"},
        {"Sacred Grove Portal", "Sacred Grove Portal"},
        {"Bridge of Eldin Portal", "Bridge of Eldin Portal"},
        {"Upper Zora's River Portal", "Upper Zoras River Portal"}
    };
    return layoutOrder;
}

void rando_starting_inventory_update_right_pane(Pane& rightPane) {
    rightPane.clear();
    rightPane.add_section("Selected Starting Items");

    const auto& inventory = GetRandomizerConfig().GetSettings().GetStartingInventory();
    const auto& layoutOrder = GetStartingInventoryLayoutOrder();

    for (const auto& [itemText, itemName] : layoutOrder) {
        if (!inventory.contains(itemName)) {
            continue;
        }

        int count = inventory.at(itemName);
        if (count <= 0) {
            continue;
        }

        // If we have a prettier name for the item, prioritize that
        std::string prettyItemName = fmt::format("{} x{}", itemName, count);
        if (randomizer::textObjectExists(prettyItemName)) {
            rightPane.add_text(fmt::format("• {}", randomizer::getTextStr(prettyItemName)));
        }
        // Display the count before the itemname for these items
        else if (itemName.find("Small Key") != std::string::npos ||
            itemName.find("Shard") != std::string::npos ||
            itemName.find("Fused Shadow") != std::string::npos ||
            itemName.find("Hidden Skill") != std::string::npos ||
            itemName == "Poe Soul" ||
            itemName == "Bomb Bag")
        {
            rightPane.add_text(fmt::format("• {} {}", count, itemText));
        } else {
            rightPane.add_text(fmt::format("• {}", itemText));
        }
    }
}

void rando_starting_item_toggle(Pane& leftPane, Pane& rightPane, std::string itemText, std::string itemName = "", int max = 1) {
    if (itemName.empty()) {
        itemName = itemText;
    }

    // Helper function for increasing a starting item count by 1
    auto increaseCount = [itemName, max]() {
        auto& inventory = GetRandomizerConfig().GetSettings().GetModifiableStartingInventory();
        int newCount = inventory[itemName] + 1;
        if (newCount > max) {
            inventory.erase(itemName);
        } else {
            inventory.at(itemName) = newCount;
        }
        SaveConfig();
    };

    // Helper function for decreasing a starting item count by 1
    auto decreaseCount = [itemName, max]() {
        auto& inventory = GetRandomizerConfig().GetSettings().GetModifiableStartingInventory();
        int newCount = inventory[itemName] - 1;
        if (newCount < 0) {
            newCount = max;
        }

        if (newCount == 0) {
            inventory.erase(itemName);
        } else {
            inventory.at(itemName) = newCount;
        }
        SaveConfig();
    };

    leftPane.add_select_button({
        .key = itemText,
        .getValue =
        [itemName, itemText] {
            auto& inventory = GetRandomizerConfig().GetSettings().GetModifiableStartingInventory();
            int curCount = inventory.contains(itemName) ? inventory[itemName] : 0;
            std::string prettyItemName = fmt::format("{} x{}", itemName, curCount);
            if (randomizer::textObjectExists(prettyItemName)) {
                return Rml::String{randomizer::getTextStr(prettyItemName)};
            }
            if (curCount > 0) {
                return Rml::String{itemText};
            }

            return Rml::String{"None"};
        },
    })
    // Allow pressing the button to advance through the item choices
    .on_pressed([increaseCount]() {
        increaseCount();
        mDoAud_seStartMenu(kSoundItemChange);
    })
    // Also allow pressing left/right to go back/advance through them
    .on_nav_command([increaseCount, decreaseCount](Rml::Event&, NavCommand cmd) {
        if (cmd == NavCommand::Right) {
            increaseCount();
            mDoAud_seStartMenu(kSoundItemChange);
            return true;
        }
        if (cmd == NavCommand::Left) {
            decreaseCount();
            mDoAud_seStartMenu(kSoundItemChange);
            return true;
        }
        return false;
    })
    // Listen for the change event so that we can update the list on the right pane
    .listen(Rml::EventId::Change, [&rightPane](Rml::Event&) {
        rando_starting_inventory_update_right_pane(rightPane);
    });
}

void rando_starting_item_number_toggle(Pane& leftPane, Pane& rightPane, std::string itemText, std::string itemName = "", int max = 1) {
    if (itemName.empty()) {
        itemName = itemText;
    }
    leftPane.add_child<NumberButton>(NumberButton::Props{
        .key = itemText,
        .getValue =
        [itemName] {
            auto& inventory = GetRandomizerConfig().GetSettings().GetModifiableStartingInventory();
            return inventory.contains(itemName) ? inventory[itemName] : 0;
        },
        .setValue = [itemName](int value) {
            auto& inventory = GetRandomizerConfig().GetSettings().GetModifiableStartingInventory();
            if (value == 0) {
                inventory.erase(itemName);
            } else {
                inventory[itemName] = value;
            }
            SaveConfig();
        },
        .min = 0,
        .max = max,
    })
    .listen(Rml::EventId::Change, [&rightPane](Rml::Event&) {
        rando_starting_inventory_update_right_pane(rightPane);
    });
}

Document* show_seed_gen_modal(std::string_view message) {
    auto* modal = &push_document(std::make_unique<Modal>(Modal::Props{
        .title = "Randomizer",
        .bodyRml = escape(message),
        .onDismiss = [](Modal& modal) {
            mDoAud_seStartMenu(kSoundWindowClose);
            modal.pop();
        },
        .icon = "verifying",
    }));

    if (auto* doc = top_document()) {
        doc->focus();
    }

    return modal;
}

RandomizerWindow::RandomizerWindow() {

    // Create rando directories if they don't exist
    if (!std::filesystem::exists(GetRandomizerSeedsPath())) {
        std::filesystem::create_directories(GetRandomizerSeedsPath());
    }

    add_tab("Seed Management", [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Controlled);

        leftPane.register_control(
            leftPane.add_select_button({
                .key = "Selected Seed",
                .getValue =
                [] {
                    return randomizer_GetContext().mHash.empty() ?
                               "None" :
                               randomizer_GetContext().mHash;
                },
            }),
            rightPane, [](Pane& pane) {
                std::filesystem::path seedDirectory = GetRandomizerSeedsPath();

                if (std::filesystem::is_empty(seedDirectory)) {
                    pane.add_rml(
                    "No seeds generated! Check out the options tab and generate a seed using the button below.");
                    return;
                }

                pane.add_rml(
                    "Apply a seed generated using the randomizer generator. (Note: must be done before selecting a save file).");

                for (const auto& entry : std::filesystem::directory_iterator(seedDirectory)) {
                    if (entry.is_directory()) {
                        std::string hash = entry.path().filename().string();

                        pane.add_button(
                            {
                                .text = hash,
                                .isSelected = [hash] {
                                    return randomizer_GetContext().mHash == hash;
                                },
                            })
                            .on_pressed([hash] {
                                randomizer_GetContext() = RandomizerContext();
                                randomizer_GetContext().LoadFromHash(hash);
                            });
                    }
                }
            });

        leftPane.register_control(
            leftPane.add_button("Delete Seeds"),
            rightPane, [](Pane& pane) {
                std::filesystem::path seedDirectory = GetRandomizerSeedsPath();

                if (std::filesystem::is_empty(seedDirectory)) {
                    pane.add_rml(
                    "No seeds generated.");
                    return;
                }

                pane.add_rml(
                "Delete any seed currently not active in the randomizer.");

                for (const auto& entry : std::filesystem::directory_iterator(seedDirectory)) {
                    if (entry.is_directory()) {
                        std::string hash = entry.path().filename().string();

                        // TODO: our ui lib doesnt have an easy way to either refresh or remove values from the right pane
                        pane.add_button(
                            {
                                .text = hash,
                                .isDisabled = [hash] {
                                    return !playerIsOnTitleScreen() || randomizer_GetContext().mHash == hash;
                                }
                            })
                            .on_pressed([hash, entry] {
                                if (randomizer_GetContext().mHash != hash) {
                                    std::filesystem::remove_all(entry);
                                } else if (!randomizer_IsActive()){
                                    // If the user selected the currently seed, but it's not active, we'll allow the delete
                                    std::filesystem::remove_all(entry);
                                    randomizer_GetContext() = RandomizerContext();
                                }
                            });
                    }
                }
            });

        leftPane.register_control(leftPane.add_button(ControlledButton::Props{
            .text = "Deactivate Seed",
            .isDisabled = [] { return randomizer_GetContext().mHash.empty() || !playerIsOnTitleScreen(); }
        }).on_pressed([] {
            if (!randomizer_IsActive()) {
                randomizer_GetContext() = RandomizerContext();
            }
        }), rightPane, [](Pane& pane) {
            pane.clear();
            pane.add_rml("Disables currently chosen seed.");
        });

        leftPane.register_control(leftPane.add_button("Generate Seed").on_pressed(
        [this, &rightPane] {
            if (TryCreateRandomSeed()) {
                DuskLog.info("Created new Seed for generator.");
            }

            std::thread randoGenerationThread(StartSeedGeneration);
            randoGenerationThread.detach();

            generatingSeed = true;
            m_genSeedModal = show_seed_gen_modal("Generating Seed...");

        }),rightPane, [](Pane& pane) {
            pane.clear();
            pane.add_rml(
                "Generate a Randomizer seed using the current configuration options, and the supplied seed string.");
        });

        leftPane.register_control(leftPane.add_child<StringButton>(StringButton::Props{
                .key = "Seed String",
                .getValue = [] {
                    return GetRandomizerConfig().GetSeed();
                },
                .setValue = [](Rml::String value) {
                    GetRandomizerConfig().SetSeed(value);
                    SaveConfig();
                },
                .maxLength = 32,
            }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_rml(
                    "Current value of the seed used by the randomizer for generation. Leave blank for a random value.");
            });
    });

    add_tab("Seed Options", [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section("Logic Settings");

        rando_config_group(leftPane, rightPane, "Logic Rules");

        leftPane.add_section("Access Options");

        rando_config_group(leftPane, rightPane, "Hyrule Barrier Requirements",
            [](const std::string& value, Pane& pane) {
                return rando_add_optional_setting(value, "Hyrule Barrier", pane);
            });
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
        rando_config_group(leftPane, rightPane, "Hyrule Castle Big Key Requirements",
            [](const std::string& value, Pane& pane) {
                return rando_add_optional_setting(value, "Hyrule Castle Big Key", pane);
            });

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

        // rando_config_group(leftPane, rightPane, "Starting Form");
        // rando_config_toggle(leftPane, rightPane, "Increase Wallet Capacity");
        // rando_config_toggle(leftPane, rightPane, "Shops Display The Replaced Item");
        // rando_config_toggle(leftPane, rightPane, "Bonks Do Damage");
        rando_config_group(leftPane, rightPane, "Trap Item Frequency");
        rando_config_group(leftPane, rightPane, "Starting Time of Day");

        leftPane.add_section("Dungeon Entrance Settings");

        rando_config_toggle(leftPane, rightPane, "Lakebed Does Not Require Water Bombs");
        rando_config_toggle(leftPane, rightPane, "Arbiters Does Not Require Bulblin Camp");
        rando_config_toggle(leftPane, rightPane, "Snowpeak Does Not Require Reekfish Scent");
        rando_config_toggle(leftPane, rightPane, "Sacred Grove Does Not Require Skull Kid");
        rando_config_toggle(leftPane, rightPane, "City Does Not Require Filled Skybook");
        rando_config_group(leftPane, rightPane, "Goron Mines Entrance");
        rando_config_group(leftPane, rightPane, "Temple of Time Sword Requirement");
        // rando_config_toggle(leftPane, rightPane, "Randomize Starting Spawn");
        // rando_config_group(leftPane, rightPane, "Randomize Dungeon Entrances");
        // rando_config_toggle(leftPane, rightPane, "Randomize Boss Entrances");
        // rando_config_toggle(leftPane, rightPane, "Randomize Grotto Entrances");
        // rando_config_toggle(leftPane, rightPane, "Randomize Cave Entrances");
        // rando_config_toggle(leftPane, rightPane, "Randomize Interior Entrances");
        // rando_config_toggle(leftPane, rightPane, "Randomize Overworld Entrances");
        // rando_config_toggle(leftPane, rightPane, "Decouple Double Door Entrances");
        // rando_config_toggle(leftPane, rightPane, "Decouple Entrances");

        leftPane.add_section("Tricks");

        rando_config_toggle(leftPane, rightPane, "Back Slice as Sword");
        rando_config_toggle(leftPane, rightPane, "Ball and Chain Webs");
        rando_config_toggle(leftPane, rightPane, "Logic Transform Anywhere");
    });

    add_tab("Starting Inventory", [this](Rml::Element* content) {

        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Controlled);

        // Hijack the Next Nav Command to let controller users interact with the right pane's scrollbar
        leftPane.listen(Rml::EventId::Keydown, [&rightPane](Rml::Event& event) {
            const auto cmd = map_nav_event(event);
            if (cmd == NavCommand::Next) {
                rightPane.root()->Focus();
                event.StopPropagation();
            }
        });

        // Map up and down to use the scrollbar on the right pane for controllers
        rightPane.listen(Rml::EventId::Keydown, [&rightPane, &leftPane](Rml::Event& event) {
            const auto cmd = map_nav_event(event);
            if (cmd == NavCommand::Up) {
                rightPane.root()->SetScrollTop(rightPane.root()->GetScrollTop() - 40.0f);
                event.StopPropagation();
            } else if (cmd == NavCommand::Down) {
                rightPane.root()->SetScrollTop(rightPane.root()->GetScrollTop() + 40.0f);
            } else if (cmd == NavCommand::Previous) {
                leftPane.root()->Focus();
                event.StopPropagation();
            }
        });

        leftPane.add_button({
            .text = "Clear Selected Starting Items",
            .isSelected = []{ return false;},
        })
        .on_pressed([&rightPane]() {
            auto& inventory = GetRandomizerConfig().GetSettings().GetModifiableStartingInventory();
            inventory.clear();
            SaveConfig();
            rando_starting_inventory_update_right_pane(rightPane);
        });

        leftPane.add_section("Main Items");
        rando_starting_item_toggle(leftPane, rightPane, "Shadow Crystal");
        rando_starting_item_toggle(leftPane, rightPane, "Horse Call");
        rando_starting_item_toggle(leftPane, rightPane, "Fishing Rod", "Progressive Fishing Rod", 2);
        rando_starting_item_toggle(leftPane, rightPane, "Slingshot");
        rando_starting_item_toggle(leftPane, rightPane, "Lantern");
        rando_starting_item_toggle(leftPane, rightPane, "Gale Boomerang");
        rando_starting_item_toggle(leftPane, rightPane, "Iron Boots");
        rando_starting_item_toggle(leftPane, rightPane, "Bow", "Progressive Bow", 3);
        rando_starting_item_toggle(leftPane, rightPane, "Hawkeye");
        rando_starting_item_number_toggle(leftPane, rightPane, "Bomb Bags", "Bomb Bag", 3);
        rando_starting_item_toggle(leftPane, rightPane, "Giant Bomb Bags", "Giant Bomb Bag");
        rando_starting_item_toggle(leftPane, rightPane, "Clawshot", "Progressive Clawshot", 2);
        rando_starting_item_toggle(leftPane, rightPane, "Spinner");
        rando_starting_item_toggle(leftPane, rightPane, "Ball and Chain");
        rando_starting_item_toggle(leftPane, rightPane, "Dominion Rod", "Progressive Dominion Rod", 2);
        rando_starting_item_toggle(leftPane, rightPane, "Empty Bottle");
        rando_starting_item_toggle(leftPane, rightPane, "Auru's Memo", "Aurus Memo");
        rando_starting_item_toggle(leftPane, rightPane, "Ashei's Sketch", "Asheis Sketch");
        rando_starting_item_toggle(leftPane, rightPane, "Sky Book", "Progressive Sky Book", 7);

        leftPane.add_section("Gear Screen");
        rando_starting_item_toggle(leftPane, rightPane, "Sword", "Progressive Sword", 4);
        rando_starting_item_toggle(leftPane, rightPane, "Ordon Shield");
        rando_starting_item_toggle(leftPane, rightPane, "Hylian Shield");
        rando_starting_item_toggle(leftPane, rightPane, "Zora Armor");
        rando_starting_item_toggle(leftPane, rightPane, "Magic Armor");
        rando_starting_item_toggle(leftPane, rightPane, "Wallet", "Progressive Wallet", 2);
        rando_starting_item_number_toggle(leftPane, rightPane, "Hidden Skills", "Progressive Hidden Skill", 7);
        rando_starting_item_number_toggle(leftPane, rightPane, "Poe Souls", "Poe Soul", 60);
        rando_starting_item_number_toggle(leftPane, rightPane, "Fused Shadows", "Progressive Fused Shadow", 3);
        rando_starting_item_number_toggle(leftPane, rightPane, "Mirror Shards", "Progressive Mirror Shard", 4);

        leftPane.add_section("Overworld Keys");
        rando_starting_item_toggle(leftPane, rightPane, "Gate Keys");
        rando_starting_item_toggle(leftPane, rightPane, "Gerudo Desert Bulblin Camp Key");

        leftPane.add_section("Dungeon Items");
        rando_starting_item_number_toggle(leftPane, rightPane, "Forest Temple Small Keys", "Forest Temple Small Key", 4);
        rando_starting_item_number_toggle(leftPane, rightPane, "Goron Mines Small Keys", "Goron Mines Small Key", 3);
        rando_starting_item_number_toggle(leftPane, rightPane, "Lakebed Temple Small Keys", "Lakebed Temple Small Key", 3);
        rando_starting_item_number_toggle(leftPane, rightPane, "Arbiter's Grounds Small Keys", "Arbiters Grounds Small Key", 5);
        rando_starting_item_number_toggle(leftPane, rightPane, "Snowpeak Ruins Small Keys", "Snowpeak Ruins Small Key", 4);
        rando_starting_item_toggle(leftPane, rightPane, "Ordon Pumpkin");
        rando_starting_item_toggle(leftPane, rightPane, "Ordon Cheese");
        rando_starting_item_number_toggle(leftPane, rightPane, "Temple of Time Small Keys", "Temple of Time Small Key", 4);
        rando_starting_item_number_toggle(leftPane, rightPane, "City in the Sky Small Keys", "City in the Sky Small Key", 1);
        rando_starting_item_number_toggle(leftPane, rightPane, "Palace of Twilight Small Keys", "Palace of Twilight Small Key", 7);
        rando_starting_item_number_toggle(leftPane, rightPane, "Hyrule Castle Small Keys", "Hyrule Castle Small Key", 3);

        rando_starting_item_toggle(leftPane, rightPane, "Forest Temple Big Key");
        rando_starting_item_number_toggle(leftPane, rightPane, "Goron Mines Key Shards", "Goron Mines Key Shard", 3);
        rando_starting_item_toggle(leftPane, rightPane, "Lakebed Temple Big Key");
        rando_starting_item_toggle(leftPane, rightPane, "Arbiter's Grounds Big Key", "Arbiters Grounds Big Key");
        rando_starting_item_toggle(leftPane, rightPane, "Snowpeak Ruins Bedroom Key");
        rando_starting_item_toggle(leftPane, rightPane, "Temple of Time Big Key");
        rando_starting_item_toggle(leftPane, rightPane, "City in the Sky Big Key");
        rando_starting_item_toggle(leftPane, rightPane, "Palace of Twilight Big Key");
        rando_starting_item_toggle(leftPane, rightPane, "Hyrule Castle Big Key");

        leftPane.add_section("Warp Portals");
        rando_starting_item_toggle(leftPane, rightPane, "Gerudo Desert Portal");
        rando_starting_item_toggle(leftPane, rightPane, "Mirror Chamber Portal");
        rando_starting_item_toggle(leftPane, rightPane, "Snowpeak Portal");
        rando_starting_item_toggle(leftPane, rightPane, "Sacred Grove Portal");
        rando_starting_item_toggle(leftPane, rightPane, "Bridge of Eldin Portal");
        rando_starting_item_toggle(leftPane, rightPane, "Upper Zora's River Portal", "Upper Zoras River Portal");
    });

    if (randomizer_IsActive()) {
        add_tab("In-Game", [this](Rml::Element* content) {
            auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
            auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

            leftPane.add_section("General");

            leftPane.register_control(leftPane.add_button("Warp to Start").on_pressed([] {
                mDoAud_seStartMenu(kSoundClick);
                auto& locData = randomizer_GetContext().mStartLocation;
                dComIfGp_setNextStage(locData.mapName.c_str(), locData.pointNo, locData.roomNo, locData.mapLayer);
            }), rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_rml("Respawns the player at their appropriate starting location.");
            });

            leftPane.add_button("Toggle Tracker Window").on_pressed([] {
                g_randomizerState.mShowTracker = !g_randomizerState.mShowTracker;
            });
        });
    }
}

void RandomizerWindow::update() {
    Window::update();

    if (m_genSeedModal && !generatingSeed) {
        m_genSeedModal->pop();
        m_genSeedModal = nullptr;
    }
}

std::filesystem::path GetRandomizerPath() {
    return data::configured_data_path() / "randomizer";
}

std::filesystem::path GetRandomizerSettingsPath() {
    return GetRandomizerPath() / "settings.yaml";
}

std::filesystem::path GetRandomizerPreferencesPath() {
    return GetRandomizerPath() / "preferences.yaml";
}

std::filesystem::path GetRandomizerSeedsPath() {
    return GetRandomizerPath() / "seeds";
}

randomizer::seedgen::config::Config& GetRandomizerConfig() {
    static randomizer::seedgen::config::Config s_config{GetRandomizerSettingsPath(),
                                                        GetRandomizerPreferencesPath()};
    return s_config;
}
}
