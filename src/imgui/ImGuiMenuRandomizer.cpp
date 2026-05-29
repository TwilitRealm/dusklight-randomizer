#include "imgui.h"

#include "ImGuiConsole.hpp"
#include "ImGuiMenuRandomizer.hpp"
#include "imgui_internal.h"

#include "dusk/logging.h"
#include "dusk/data.hpp"
#include "dusk/map_loader_definitions.h"
#include "dusk/ui/rando_config.hpp"
#include "dusk/randomizer/generator/logic/search.hpp"
#include "dusk/randomizer/generator/utility/string.hpp"
#include "dusk/randomizer/game/randomizer_context.hpp"
#include "dusk/randomizer/game/tools.h"
#include "dusk/randomizer/game/verify_item_functions.h"

#include <mutex>
#include <thread>
#include <filesystem>
#include <numeric>
#include <ranges>


namespace dusk {

    static bool generatingSeed = false;
    static std::string generationStatusMsg{};
    static std::mutex generationStatusMsgMutex{};

    static constexpr ImVec4 TRACKER_COLOR_INACCESSIBLE = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    static constexpr ImVec4 TRACKER_COLOR_ACCESSIBLE = ImVec4(0.f, 1.f, 0.f, 1.f);
    static constexpr ImVec4 TRACKER_COLOR_COLLECTED = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    static constexpr ImVec4 TRACKER_COLOR_COLLECTED_OUT_LOGIC = ImVec4(0.4f, 0.4f, 0.0f, 1.0f);
    static constexpr ImVec4 TRACKER_COLOR_ACTIVE = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);

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

                std::filesystem::path seedDirectory = ui::GetRandomizerSeedsPath();

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

                std::filesystem::path seedDirectory = ui::GetRandomizerSeedsPath();
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
        ImGui::SetNextWindowBgAlpha(0.5f);

        if (!ImGui::Begin("Rando Tracker", nullptr, windowFlags) || !randomizer_IsActive()) {
            ImGui::End();
            return;
        }

        auto trackerRando = getTrackerRando();

        if (ImGui::Button("Update Tracker") || g_randomizerState.mUpdateTracker) {
            g_randomizerState.mUpdateTracker = false;

            // Generate tracker world if it doesn't exist
            auto contextHash = randomizer_GetContext().mHash;
            auto trackerHash = trackerRando->GetConfig().GetHash(false);
            // If no hash, or seeds switched, try to create tracker world from currently active seed
            if (trackerHash.empty() || (trackerHash != contextHash && !contextHash.empty())) {
                *trackerRando = randomizer::Randomizer(ui::GetRandomizerPath());
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
            ImGui::End();
            return;
        }

        ImGui::Text("Tracker world loaded from seed %s", trackerRando->GetConfig().GetHash().c_str());
        const char* curStage = dComIfGp_getStartStageName();
        const char* curStageName = lookup_map_name(curStage);
        ImGui::Text("Current Stage: %s (%s)", curStageName, curStage);

        auto stageInfo = dComIfGp_getStageStagInfo();
        if (stageInfo && dStage_stagInfo_GetSaveTbl(stageInfo) != getStageSaveId(curStage)) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f,0.0f,1.0f), "Error: Current Stage Save Tbl mismatch!");
        }

        // TODO: move this out of tracker
        if (ImGui::CollapsingHeader("Current Inventory")) {
            if (ImGui::BeginChild("ScrollRegionCurInventory", ImVec2(500, 200), true)) {
                ImGui::SeparatorText("Story Items Count");

                ImGui::Text("Fused Shadows: %d/3", numFusedShadows());
                ImGui::Text("Mirror Shards: %d/4", numMirrorShards());

                static std::unordered_map<int, std::pair<int, std::string>> keyRegionItemNameMap = {
                    {0x10, {4, "Forest Temple"}},
                    {0x11, {3, "Goron Mines"}},
                    {0x12, {3, "Lakebed Temple"}},
                    {0x13, {5, "Arbiters Grounds"}},
                    {0x14, {4, "Snowpeak Ruins"}},
                    {0x15, {3, "Temple of Time"}},
                    {0x16, {1, "City in the Sky"}},
                    {0x17, {7, "Palace of Twilight"}},
                    {0x18, {3, "Hyrule Castle"}},
                    {0xA,  {1, "Gerudo Desert Bulblin Camp"}},
                };
                for (auto& [stage, keyInfo] : keyRegionItemNameMap) {
                    ImGui::SeparatorText(keyInfo.second.c_str());

                    int smallKeys = getTempleKeysFound(stage);

                    ImGui::Text("Keys: %d/%d", smallKeys, keyInfo.first);

                    if (stage == 0xA)
                        continue;

                    ImGui::Text("Has:");

                    if (dComIfGs_isDungeonItemBossKey(stage)) {
                        ImGui::SameLine();
                        ImGui::Text("Big Key");
                    }
                    if (dComIfGs_isDungeonItemMap(stage)) {
                        ImGui::SameLine();
                        ImGui::Text("Map");
                    }
                    if (dComIfGs_isDungeonItemCompass(stage)) {
                        ImGui::SameLine();
                        ImGui::Text("Compass");
                    }

                    if (stage == 0x14) {
                        // Ordon Pumpkin
                        if (haveItem(dItemNo_Randomizer_TOMATO_PUREE_e)) {
                            ImGui::SameLine();
                            ImGui::Text("Pumpkin");
                        }

                        // Ordon Cheese
                        if (haveItem(dItemNo_Randomizer_TASTE_e)) {
                            ImGui::SameLine();
                            ImGui::Text("Cheese");
                        }
                    }
                }

                ImGui::SeparatorText("Overworld Keys");

                ImGui::Text("Faron Gate: %s", haveItem(dItemNo_SMALL_KEY2_e) ? "Yes" : "No");
                ImGui::Text("Coro Gate: %s", haveItem(dItemNo_KEY_OF_FILONE_e) ? "Yes" : "No");
                ImGui::Text("Gate Keys: %s", haveItem(dItemNo_Randomizer_BOSSRIDER_KEY_e) ? "Yes" : "No");
            }
            ImGui::EndChild();
        }

        ImGui::Checkbox("Show Only Accessible Locations", &m_onlyAccessible);
        ImGui::Checkbox("Hide Area Header", &m_hideAreaHeader);
        ImGui::Checkbox("Show Location Requirements", &m_showRequirements);
        ImGui::InputText("Location Filter", m_locationFilter, 100);

        // Show total number of available locations
        auto locations = trackerRando->GetWorld()->GetAllLocations();
        // TODO: maybe add remaining?
        ImGui::Text("Locations Available: %d / %d (%d Checked)", m_numAvailableLocations, m_numProgressionLocations, m_numCollectedLocations);

        ImGui::SetNextWindowBgAlpha(0.5f);
        if (ImGui::BeginChild("ScrollRegion", ImVec2(500, 500), true))
        {
            std::string filter = m_locationFilter;
            auto drawLocationsFunc = [this, &filter](std::vector<LocationTrackerInfo>& locations) {
                for (auto& info : locations) {
                    // Don't show any locations which aren't accessible if only accessible is checked
                    // Don't show any locations which don't meet the filter
                    if ((m_onlyAccessible && !info.accessible) ||
                        !randomizer::utility::str::Contains(info.locationName, filter)) {
                        continue;
                    }

                    if (info.collected) {
                        if (info.accessible) {
                            ImGui::TextColored(TRACKER_COLOR_COLLECTED, "%s [%s]",
                            info.locationName.c_str(),
                            info.locationItem.c_str());
                        }else {
                            ImGui::TextColored(TRACKER_COLOR_COLLECTED_OUT_LOGIC, "%s [%s]",
                            info.locationName.c_str(),
                            info.locationItem.c_str());
                        }
                    } else {
                        if (ImGui::SmallButton(fmt::format("{}###HideCheckBtn_{}",
                            info.hidden ? "+" : "-", info.locationName.c_str()).c_str())) {
                            if (info.hidden) {
                                std::erase(m_HiddenChecks, info.locationName);
                            } else {
                                m_HiddenChecks.push_back(info.locationName);
                            }
                            info.hidden = !info.hidden;
                        }
                        ImGui::SameLine();

                        if (info.hidden) {
                            ImGui::TextColored(TRACKER_COLOR_COLLECTED, "%s [Skipped]",
                                info.locationName.c_str());
                        } else {
                            ImVec4 color;
                            if (info.accessible) {
                                // If the search found this location, change color to green
                                color = TRACKER_COLOR_ACCESSIBLE;
                            } else {
                                color = TRACKER_COLOR_INACCESSIBLE;
                            }

                            ImGui::TextColored(color, "%s", info.locationName.c_str());
                        }
                    }

                    // Show requirements for the location below it (formatting isn't pretty right now)
                    if (m_showRequirements) {
                        ImGui::SetItemTooltip("%s", info.logicStr.c_str());
                    }
                }
            };

            if (m_hideAreaHeader) {
                for (auto& region : m_LocationInfo | std::views::values) {
                    drawLocationsFunc(region.unobtainedLocations);
                }
                for (auto& region : m_LocationInfo | std::views::values) {
                    drawLocationsFunc(region.obtainedLocations);
                }
            }else {
                // Show all locations. Green for accessible. Red for Unaccessible
                for (auto& [areaName, region] : m_LocationInfo) {
                    if (m_onlyAccessible && !region.showArea)
                        continue;

                    int areaCount = region.totalCount;

                    bool isCurrentArea = areaName == curStageName;

                    if (isCurrentArea)
                        ImGui::PushStyleColor(ImGuiCol_Text, TRACKER_COLOR_ACTIVE);
                    else if (!region.showArea)
                        ImGui::PushStyleColor(ImGuiCol_Text, TRACKER_COLOR_COLLECTED);

                    bool isShowNode = ImGui::CollapsingHeader(fmt::format("{} ({}/{})###{}_{}",
                        areaName, areaCount - region.collectedCount, areaCount, areaName, areaCount).c_str());

                    if (!region.showArea || isCurrentArea)
                        ImGui::PopStyleColor();

                    if (isShowNode) {
                        drawLocationsFunc(region.unobtainedLocations);
                        drawLocationsFunc(region.obtainedLocations);
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::End();
    }

    randomizer::Randomizer* ImGuiMenuRandomizer::getTrackerRando() {
        static randomizer::Randomizer trackerRando{ui::GetRandomizerPath()};
        return &trackerRando;
    }

    void ImGuiMenuRandomizer::TrackerAreaGroup::addToGroup(LocationTrackerInfo& info) {
        if (!showArea && info.accessible && !info.collected) {
            showArea = true;
        }

        totalCount++;

        if (info.collected)
            collectedCount++;
        if (info.accessible)
            accessibleCount++;

        if (info.collected) {
            obtainedLocations.push_back(info);
        }else {
            unobtainedLocations.push_back(info);
        }
    }

    void ImGuiMenuRandomizer::generateLocationInfo() {
        auto trackerRando = getTrackerRando();
        auto world = trackerRando->GetWorld();
        auto locations = world->GetAllLocations();

        m_LocationInfo.clear();

        m_numProgressionLocations = std::ranges::count_if(locations, [](auto* location) {return location->IsProgression();});
        m_numAvailableLocations = std::ranges::count_if(m_currentSearch._visitedLocations, [](auto* location) {return location->IsProgression();});

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

            int itemId = getLocationItem(location);

            LocationTrackerInfo info {
                .locationName = location->GetName(),
                .logicStr = location->GetComputedRequirement().to_string(),
                .locationItem = itemId >= 0 ? world->GetItem(itemId)->GetName() : "Unknown",
                .accessible = m_currentSearch._visitedLocations.contains(location)
            };

            info.collected = isLocationObtained(location);

            info.hidden = std::ranges::find(m_HiddenChecks, info.locationName) != m_HiddenChecks.end();

            if (info.accessible && info.collected)
                m_numAvailableLocations--;

            // Gather the hint regions this location is in (set avoids duplicates)
            std::unordered_set<std::string> regions{};
            for (auto access : location->GetAccessList()) {
                for (const auto& region : access->GetArea()->GetHintRegions()) {
                    regions.insert(region);
                }
            }

            for (const auto& region : regions) {
                auto& infoGroup = m_LocationInfo[region];
                infoGroup.addToGroup(info);
            }
        }

        auto counts = m_LocationInfo | std::views::transform([](const std::pair<std::string, TrackerAreaGroup>& location) { return location.second.collectedCount; });
        m_numCollectedLocations = std::accumulate(counts.begin(), counts.end(), 0);
    }
} // namespace dusk