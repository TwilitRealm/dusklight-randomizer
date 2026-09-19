// Offline check of the in-game seed rebuild: slot_data.json -> official generator (AP mode).
// usage: ap_gen_test <slot_data.json> <work dir>
#include "../generator/randomizer.hpp"
#include "../generator/logic/world.hpp"

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: ap_gen_test <slot_data.json> <work dir>\n";
        return 2;
    }
    namespace fs = std::filesystem;
    auto sd = nlohmann::json::parse(std::ifstream(argv[1]));
    const fs::path base = argv[2];
    fs::create_directories(base);

    YAML::Node settings;
    settings["Seed"] = "AP-" + sd.value("seed", std::string{}) + "-test";
    settings["Plandomizer"] = true;
    settings["Generate Spoiler Log"] = false;
    settings["Starting Inventory"] = YAML::Node(YAML::NodeType::Map);
    settings["Excluded Locations"] = YAML::Node(YAML::NodeType::Sequence);
    settings["Mixed Entrance Pools"] = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& [k, v] : sd["settings"].items()) settings[k] = v.get<std::string>();
    std::ofstream(base / "settings.yaml") << YAML::Dump(settings);

    YAML::Node plando, locs(YAML::NodeType::Map);
    for (const auto& [loc, v] : sd["placements"].items())
        locs[loc] = v.is_string() ? v.get<std::string>() : v.value("item", std::string{"Archipelago Item"});
    plando["World 1"]["Locations"] = locs;
    std::ofstream(base / "plando.yaml") << YAML::Dump(plando);
    YAML::Node prefs;
    prefs["Plandomizer Path"] = (base / "plando.yaml").generic_string();
    std::ofstream(base / "preferences.yaml") << YAML::Dump(prefs);

    randomizer::g_archipelagoMode = true;
    randomizer::Randomizer r{base};
    auto err = r.Generate();
    if (err) {
        std::cout << "GENERATION FAILED: " << *err << "\n";
        return 1;
    }
    auto* world = r.GetWorld();
    int ok = 0, bad = 0;
    for (const auto& [loc, v] : sd["placements"].items()) {
        const std::string want = v.is_string() ? v.get<std::string>() : "Archipelago Item";
        const std::string got = world->GetLocation(loc)->GetCurrentItem()->GetName();
        bool bottle = want == "Empty Bottle" && got.starts_with("Bottle");
        if (got == want || bottle) ++ok; else { ++bad; if (bad < 10) std::cout << "MISMATCH " << loc << ": " << got << " != " << want << "\n"; }
    }
    int empty = 0;
    for (auto* l : world->GetAllLocations()) if (l->IsEmpty()) ++empty;
    std::cout << "hash " << r.GetConfig().GetHash() << " placements ok " << ok << " bad " << bad
              << " empty locations " << empty << "\n";
    return bad == 0 && empty == 0 ? 0 : 1;
}
