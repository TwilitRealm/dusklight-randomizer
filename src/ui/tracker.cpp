#include "tracker.hpp"

#include "config_store.hpp"

#include "../tools.h"
#include "../paths.hpp"
#include "../../generator/randomizer.hpp"

#include <algorithm>
#include <cctype>

namespace randomizer::ui {
Tracker g_tracker;

void Tracker::generateLocationInfo() {
    if (m_LocationInfo.empty()) {
        auto locationDataTree = LOAD_EMBED_YAML(RANDO_DATA_PATH "locations.yaml");
        for (const auto& locationNode : locationDataTree) {
            LocationInfo location{};
            auto& name = location.name;
            auto& lowercaseName = location.lowercaseName;
            name = locationNode["Name"].as<std::string>();
            lowercaseName = name;
            std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(),
                [](unsigned char c) { return std::tolower(c); });

            for (const auto& category : locationNode["Categories"]) {
                location.categories.insert(category.as<std::string>());
            }

            if (locationNode["Metadata"].IsMap()) {
                location.metadata = locationNode["Metadata"];

                for (const auto& data : locationNode["Metadata"]) {
                    location.categories.insert(data.first.as<std::string>());
                }
            }

            // Don't include warp portals
            if (location.categories.contains("Warp Portal")) {
                continue;
            }

            m_LocationInfo.push_back(std::move(location));
        }

        // TODO: sorting breaks the location-metadata relationship atm
        // std::ranges::sort(m_LocationInfo, [](const auto& a, const auto& b) { return a.name < b.name; });
    }
}

const std::vector<UiListItem>& Tracker::getLocationListItems(const std::vector<std::string>& categoryFilter) {
    // Get settings values
    auto& randoSettings = GetRandomizerConfig().GetSettings().GetMap();
    bool goldenBugs = randoSettings.at("Golden Bugs") == "On";
    bool skyCharacters = randoSettings.at("Sky Characters") == "On";
    bool npcs = randoSettings.at("Gifts From NPCs") == "On";
    bool shops = randoSettings.at("Shop Items") == "On";
    bool goldenWolves = randoSettings.at("Hidden Skills") == "On";
    bool hiddenRupees = randoSettings.at("Hidden Rupees") == "On";
    bool freestandingRupees = randoSettings.at("Freestanding Rupees") == "On";
    bool overworldPoes = randoSettings.at("Poe Souls").IsAnyOf("Overworld", "All");
    bool dungeonPoes = randoSettings.at("Poe Souls").IsAnyOf("Dungeon", "All");

    m_listItems.clear();
    m_listItems.reserve(m_LocationInfo.size());

    for (size_t i = 0; i < m_LocationInfo.size(); ++i) {
        const auto& location = m_LocationInfo[i];
        const auto& categories = location.categories;

        // Skip categories that aren't shuffled
        if ((!goldenBugs && categories.contains("Golden Bug"))
            || (!skyCharacters && categories.contains("Sky Character"))
            || (!npcs && categories.contains("Npc"))
            || (!shops && categories.contains("Shop"))
            || (!goldenWolves && categories.contains("Golden Wolf"))
            || (!hiddenRupees && categories.contains("Rupee - Hidden"))
            || (!freestandingRupees && categories.contains("Rupee - Freestanding"))
            || (!overworldPoes && categories.contains("Poe") && categories.contains("Overworld"))
            || (!dungeonPoes && categories.contains("Poe") && categories.contains("Dungeon"))
            || categories.contains("Twilit Insect"))
        {
            continue;
        }

        for (auto& category : categoryFilter) {
            if (categories.contains(category)) {
                UiListItem item = UI_LIST_ITEM_INIT;
                item.key = i;
                item.label = location.name.c_str();
                m_listItems.push_back(item);
                break;
            }
        }
    }

    return m_listItems;
}

bool Tracker::isLocationObtained(const LocationInfo& location) {
    return isLocationMetadataObtained(location.metadata, location.name);
}

bool Tracker::isLocationObtained(uint64_t key) {
    if (key >= m_LocationInfo.size()) {
        return false;
    }

    return isLocationObtained(m_LocationInfo[key]);
}



}
