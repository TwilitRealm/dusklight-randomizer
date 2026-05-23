#include "imgui.h"

#include "ImGuiConsole.hpp"
#include "ImGuiMenuRandomizer.hpp"

#include "dusk/logging.h"
#include "dusk/data.hpp"
#include "dusk/randomizer/generator/logic/search.hpp"
#include "dusk/randomizer/generator/utility/string.hpp"
#include "dusk/randomizer/game/randomizer_context.hpp"
#include "dusk/randomizer/game/tools.h"

#include <mutex>
#include <thread>
#include <filesystem>

#include "dusk/map_loader_definitions.h"
#include "dusk/randomizer/generator/utility/string.hpp"

namespace dusk {

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

    static const char* lookup_map_name(const char* mapFile) {
        if (!mapFile || mapFile[0] == '\0')
            return nullptr;
        for (const auto& region : gameRegions) {
            for (const auto& map : region.maps) {
                if (map.mapFile && strcmp(mapFile, map.mapFile) == 0) {
                    return map.mapName;
                }
            }
        }
        return nullptr;
    }

    ImGuiMenuRandomizer::ImGuiMenuRandomizer() {}

    void ImGuiMenuRandomizer::draw() {
        if (ImGui::BeginMenu("Randomizer")) {
            if (ImGui::BeginMenu("Generate")) {
                if (ImGui::MenuItem("Generate Seed", nullptr, false, !generatingSeed)) {

                    // Put seed generation on a separate thread so it doesn't freeze the game
                    std::thread randoGenerationThread(StartSeedGeneration);
                    randoGenerationThread.detach();
                    // StartSeedGeneration();
                    m_showRandoGeneration = true;
                }

                ImGui::EndMenu();
            }

            std::string loadSeedText = "Load Seed";
            if (!playerIsOnTitleScreen()) {
                loadSeedText += " (Must be on title screen)";
            }
            if (ImGui::BeginMenu(loadSeedText.c_str(), playerIsOnTitleScreen())) {

                std::filesystem::path seedDirectory =
                    dusk::data::configured_data_path() / "randomizer" / "seeds";

                std::list<std::string> hashes{};

                if (std::filesystem::exists(seedDirectory)) {
                    for (const auto& entry : std::filesystem::directory_iterator(seedDirectory)) {
                        if (entry.is_directory()) {
                            hashes.push_back(entry.path().filename().string());
                        }
                    }
                }else {
                    std::filesystem::create_directories(seedDirectory);
                }

                if (hashes.empty()) {
                    if (ImGui::MenuItem("No seeds generated")) {}
                }

                for (const auto& hash : hashes) {
                    std::string name = hash;
                    if (randomizer_GetContext().mHash == hash) {
                        name += " (Current Seed)";
                    }
                    if (ImGui::MenuItem(name.c_str())) {
                        if (!randomizer_IsActive()) {
                            randomizer_GetContext() = RandomizerContext();
                            randomizer_GetContext().LoadFromHash(hash);
                        }
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Delete Seed")) {

                std::filesystem::path seedDirectory =
                    dusk::data::configured_data_path() / "randomizer" / "seeds";

                std::list<std::string> hashes{};

                for (const auto& entry : std::filesystem::directory_iterator(seedDirectory)) {
                    if (entry.is_directory()) {
                        hashes.push_back(entry.path().filename().string());
                    }
                }

                if (hashes.empty()) {
                    if (ImGui::MenuItem("No seeds generated")) {}
                }

                for (const auto& hash : hashes) {
                    std::string name = hash;
                    if (randomizer_GetContext().mHash == hash) {
                        name += " (Current Seed)";
                    }
                    if (ImGui::MenuItem(name.c_str(), nullptr, false, playerIsOnTitleScreen() || randomizer_GetContext().mHash != hash)) {
                        std::filesystem::path hashDirectory = seedDirectory / hash;
                        if (randomizer_GetContext().mHash != hash) {
                            std::filesystem::remove_all(hashDirectory);
                        } else if (!randomizer_IsActive()){
                            // If the user selected the currently seed, but it's not active, we'll allow the delete
                            std::filesystem::remove_all(hashDirectory);
                            randomizer_GetContext() = RandomizerContext();
                        }
                    }
                }

                ImGui::EndMenu();
            }

            std::string name = "Deactivate Randomizer";
            if (!playerIsOnTitleScreen()) {
                name += " (Must be on title screen)";
            }

            if (ImGui::MenuItem(name.c_str(), nullptr, false, playerIsOnTitleScreen())) {
                // Reset the main randomizer context only if we're not active
                if (!randomizer_IsActive()) {
                    randomizer_GetContext() = RandomizerContext();
                }
            }
            ImGui::Checkbox("Show Rando Stats", &m_showRandoStats);

            ImGui::Checkbox("Show Rando Tracker", &g_randomizerState.mShowTracker);

            ImGui::EndMenu();
        }
    }

    void ImGuiMenuRandomizer::windowRandoGeneration() {
        if (!m_showRandoGeneration) {
            return;
        }

        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_AlwaysAutoResize;

        ImGui::SetNextWindowBgAlpha(0.65f);

        if (!ImGui::Begin("Randomizer Generation", &m_showRandoGeneration, windowFlags)) {
            ImGui::End();
            return;
        }

        // Print "Generating..." until the seed finishes generating (at which point it will release
        // the lock on the mutex)
        std::string generationText = "Generating Randomizer Seed...";
        if (generationStatusMsgMutex.try_lock()) {
            generationText = generationStatusMsg;
            generationStatusMsgMutex.unlock();
        }

        ImGui::Text("%s", generationText.c_str());

        ImGui::End();
    }

    void ImGuiMenuRandomizer::windowRandoStats() {
        if (!m_showRandoStats) {
            return;
        }

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

        ImGui::SetNextWindowBgAlpha(0.65f);
        if (ImGui::Begin("Rando Stats", nullptr, windowFlags)) {

            const char* seed = randomizer_GetContext().mHash.empty() ? "None" : randomizer_GetContext().mHash.c_str();

            ImGui::Text("Current Seed: %s", seed);
            ImGui::Text("Status:");
            ImGui::SameLine();
            if (randomizer_IsActive()) {
                ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "ACTIVE");
            } else {
                std::string reason{"Unknown"};
                if (randomizer_GetContext().mHash.empty()) {
                    reason = "No Seed Selected";
                } else if (playerIsOnTitleScreen()) {
                    reason = "On Title Screen";
                }
                ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "NOT ACTIVE (Reason: %s)", reason.c_str());
            }
        }

        ImGui::End();
    }


    void ImGuiMenuRandomizer::windowRandoTracker() {
        if (!g_randomizerState.mShowTracker) {
            return;
        }

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

        if (ImGui::Begin("Rando Tracker", nullptr, windowFlags)) {
            auto trackerRando = getTrackerRando();

            // Uncomment button for manual updating
            if (ImGui::Button("Update Tracker") || g_randomizerState.mUpdateTracker) {
                g_randomizerState.mUpdateTracker = false;

                // Generate tracker world if it doesn't exist
                auto contextHash = randomizer_GetContext().mHash;
                auto trackerHash = trackerRando->GetConfig().GetHash(false);
                // If no hash, or seeds switched, try to create tracker world from currently active seed
                if (trackerHash.empty() || (trackerHash != contextHash && !contextHash.empty())) {
                    *trackerRando = randomizer::Randomizer(data::configured_data_path());
                    trackerRando->GenerateTrackerWorld();
                }

                if (randomizer_IsActive()) {
                    auto currentItems = getSaveItemPool(trackerRando->GetWorld());
                    m_currentSearch = randomizer::logic::search::Search::AccessibleNoStartingInventory(&trackerRando->GetWorlds(), currentItems);
                    m_currentSearch.SearchWorlds();
                    generateLocationInfo();
                }
            }

            if (trackerRando->GetConfig().GetHash(false).empty()) {
                ImGui::Text("Load a seed and start a file to activate the tracker");
            } else {
                ImGui::Text("Tracker world loaded from seed %s", trackerRando->GetConfig().GetHash().c_str());
                const char* curStage = dComIfGp_getStartStageName();
                ImGui::Text("Current Stage: %s (%s)", lookup_map_name(curStage), curStage);

                auto stageInfo = dComIfGp_getStageStagInfo();
                if (stageInfo && dStage_stagInfo_GetSaveTbl(stageInfo) != getStageSaveId(curStage)) {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f,0.0f,1.0f), "Error: Current Stage Save Tbl mismatch!");
                }

                ImGui::Checkbox("Show Only Accessible Locations", &m_onlyAccessible);
                ImGui::Checkbox("Show Location Requirements", &m_showRequirements);
                ImGui::InputText("Location Filter", m_locationFilter, 100);

                // Show total number of available locations
                auto locations = trackerRando->GetWorld()->GetAllLocations();
                auto numProgressionLocations = std::ranges::count_if(locations, [](auto* location) {return location->IsProgression();});
                auto numAvailableLocations = std::ranges::count_if(m_currentSearch._visitedLocations, [](auto* location) {return location->IsProgression();});
                ImGui::Text("Locations Available: %zu / %zu", numAvailableLocations, numProgressionLocations);

                if (ImGui::BeginChild("ScrollRegion", ImVec2(500, 500), true))
                {
                    std::string filter = m_locationFilter;
                    // Show all locations. Green for accessible. Red for Unaccessible
                    for (const auto& [areaName, location] : m_LocationInfo) {
                        if (m_onlyAccessible && !location.showArea)
                            continue;

                        if (!location.showArea) {
                            ImGui::BeginDisabled();
                        }

                        int areaCount = m_onlyAccessible ? location.accessibleCount : location.locations.size();
                        if (ImGui::TreeNode(fmt::format("{} ({}/{})", areaName, areaCount - location.collectedCount, areaCount).c_str())) {
                            for (const auto& info : location.locations) {
                                // Don't show any locations which aren't accessible if only accessible is checked
                                // Don't show any locations which don't meet the filter
                                if ((m_onlyAccessible && !info.accessible) || !randomizer::utility::str::Contains(info.locationName, filter)) {
                                    continue;
                                }

                                ImVec4 color;
                                if (info.collected) {
                                    // gray to show collected
                                    color = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
                                }else if (info.accessible) {
                                    // If the search found this location, change color to green
                                    color = ImVec4(0.f, 1.f, 0.f, 1.f);
                                }else {
                                    color = ImVec4(1.f, 0.f, 0.f, 1.f); // red for inaccessible
                                }

                                ImGui::TextColored(color, "%s", info.locationName.c_str());

                                // Show requirements for the location below it (formatting isn't pretty right now)
                                if (m_showRequirements) {
                                    ImGui::SetItemTooltip("    %s", info.logicStr.c_str());
                                }
                            }

                            ImGui::TreePop();
                        }

                        if (!location.showArea) {
                            ImGui::EndDisabled();
                        }
                    }
                }
                ImGui::EndChild();
            }
        }

        ImGui::End();
    }

    randomizer::Randomizer* ImGuiMenuRandomizer::getTrackerRando() {
        static randomizer::Randomizer trackerRando{data::configured_data_path()};
        return &trackerRando;
    }

    void ImGuiMenuRandomizer::generateLocationInfo() {
        auto trackerRando = getTrackerRando();
        auto locations = trackerRando->GetWorld()->GetAllLocations();

        m_LocationInfo.clear();

        for (auto location : locations) {
            // Don't show locations which aren't progression
            // Don't show any locations which aren't accessible if only accessible is checked
            if (!location->IsProgression()) {
                continue;
            }

            // Don't show warp portals for now either
            if (location->HasCategories("Warp Portal")) {
                continue;
            }

            LocationTrackerInfo info {
                .locationName = location->GetName(),
                .logicStr = location->GetComputedRequirement().to_string(),
                .accessible = m_currentSearch._visitedLocations.contains(location)
            };

            auto& locationMeta = location->GetMetadata();

            if (auto& chestNode = locationMeta["Chest"]) {
                if (chestNode.size() != 1) {
                    DuskLog.warn("Assumption that theres one tbox id for this location was false!");
                }
                auto tboxId = chestNode[0]["Tbox Id"].as<u8>();
                auto stageId = getStageSaveId(chestNode[0]["Stage"].as<u8>());
                info.collected = dComIfGs_isStageTbox(stageId, tboxId);
            } else if (auto& poeNode = locationMeta["Poe"]) {
                auto flag = poeNode[0]["Flag"].as<u8>();
                auto stageId = getStageSaveId(poeNode[0]["Stage"].as<u8>());
                info.collected = tracker_isStageSwitch(stageId, flag);
            } else if (auto& freeStandingItemNode = locationMeta["Freestanding Item"]) {
                auto flag = freeStandingItemNode[0]["Flag"].as<u8>();
                auto stageId = getStageSaveId(freeStandingItemNode[0]["Stage"].as<u8>());
                info.collected = tracker_isStageSwitch(stageId, flag);
            } else if (auto& eventFlagNode = locationMeta["Event Flag"]) {
                auto flag = eventFlagNode.as<u16>();
                info.collected = tracker_isEventBit(flag);
            } else if (auto& wolfNode = locationMeta["Golden Wolf"]) {
                auto flag = wolfNode[0]["Flag"].as<u16>();
                info.collected = tracker_isEventBit(flag);
            } else if (auto& switchFlagNode = locationMeta["Switch Flag"]) {
                auto flag = switchFlagNode["Flag"].as<u8>();
                auto stageId = getStageSaveId(switchFlagNode["Stage"].as<u8>());
                info.collected = tracker_isStageSwitch(stageId, flag);
            } else {
                info.collected = false;
            }

            // Gather the hint regions this location is in (set avoids duplicates)
            std::unordered_set<std::string> regions{};
            for (auto access : location->GetAccessList()) {
                for (const auto& region : access->GetArea()->GetHintRegions()) {
                    regions.insert(region);
                }
            }

            for (const auto& region : regions) {
                auto& infoGroup = m_LocationInfo[region];
                if (!infoGroup.showArea && info.accessible && !info.collected) {
                    infoGroup.showArea = true;
                }

                if (info.collected)
                    infoGroup.collectedCount++;
                if (info.accessible)
                    infoGroup.accessibleCount++;

                infoGroup.locations.push_back(info);
            }
        }
    }
} // namespace dusk