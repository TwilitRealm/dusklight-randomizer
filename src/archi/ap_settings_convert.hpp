#pragma once
#include "../../generator/seedgen/seed.hpp"
#include "nlohmann/json.hpp"

namespace randomizer::archi {

    void createConfigFromArchiSettings(seedgen::config::Config& out_config, const nlohmann::json& settings);

} // randomizer::archi