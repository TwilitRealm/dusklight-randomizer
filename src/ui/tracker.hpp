#pragma once

#include "mods/svc/ui.h"
#include "../../generator/utility/yaml.hpp"

#include <string>
#include <vector>
#include <unordered_set>

namespace randomizer::ui {
class Tracker {
public:
    struct LocationInfo {
        std::string name{};
        std::string lowercaseName{};
        std::unordered_set<std::string> categories{};
        YAML::Node metadata{};
    };

    Tracker() = default;
    ~Tracker() = default;

    void generateLocationInfo();
    const std::vector<UiListItem>& getLocationListItems(const std::vector<std::string>&);
    bool isLocationObtained(const LocationInfo& location);
    bool isLocationObtained(uint64_t key);

    std::vector<LocationInfo> m_LocationInfo;
    std::vector<UiListItem> m_listItems;
};

extern Tracker g_tracker;
}
