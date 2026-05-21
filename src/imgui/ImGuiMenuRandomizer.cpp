#include "imgui.h"

#include "ImGuiConsole.hpp"
#include "ImGuiMenuRandomizer.hpp"

#include "dusk/logging.h"
#include "dusk/data.hpp"
#include "dusk/randomizer/generator/logic/search.hpp"
#include "dusk/randomizer/game/randomizer_context.hpp"
#include "dusk/randomizer/game/tools.h"

#include <mutex>
#include <thread>
#include <filesystem>

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

            ImGui::Checkbox("Show Rando Tracker", &m_showRandoTracker);

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
        if (!m_showRandoTracker) {
            return;
        }

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

        if (ImGui::Begin("Rando Tracker", nullptr, windowFlags)) {
            auto trackerRando = getTrackerRando();

            // Uncomment button for manual updating
            if (/*ImGui::Button("Update Tracker") || */g_randomizerState.mUpdateTracker) {
                g_randomizerState.mUpdateTracker = false;
                auto trackerRando = getTrackerRando();

                // Generate tracker world if it doesn't exist
                auto contextHash = randomizer_GetContext().mHash;
                auto trackerHash = trackerRando->GetConfig().GetHash(false);
                // If no hash, or seeds switched, try to create tracker world from currently active seed
                if (trackerHash.empty() || (trackerHash != contextHash && !contextHash.empty())) {
                    *trackerRando = randomizer::Randomizer(data::configured_data_path());
                    trackerRando->GenerateTrackerWorld();
                } else {
                    if (randomizer_IsActive()) {
                        auto currentItems = getSaveItemPool(trackerRando->GetWorlds()[0].get());
                        m_currentSearch = randomizer::logic::search::Search::AccessibleNoStartingInventory(&trackerRando->GetWorlds(), currentItems);
                    }
                    m_currentSearch.SearchWorlds();
                }
            }

            if (trackerRando->GetConfig().GetHash(false).empty()) {
                ImGui::Text("Load a seed and start a file to activate the tracker");
            } else {
                ImGui::Text("Tracker world loaded from seed %s", trackerRando->GetConfig().GetHash().c_str());

                ImGui::Checkbox("Show Only Accessible Locations", &m_onlyAccessible);
                ImGui::Checkbox("Show Location Requirements", &m_showRequirements);
                ImGui::InputText("Location Filter", m_locationFilter, 100);

                // Show total number of available locations
                auto locations = trackerRando->GetWorlds()[0]->GetAllLocations();
                auto numProgressionLocations = std::ranges::count_if(locations, [](auto* location) {return location->IsProgression();});
                auto numAvailableLocations = m_currentSearch._visitedLocations.size();
                ImGui::Text("Locations Available: %zu / %zu", numAvailableLocations, numProgressionLocations);

                if (ImGui::BeginChild("ScrollRegion", ImVec2(500, 500), true))
                {
                    std::string filter = m_locationFilter;
                    // Show all locations. Green for accessible. Red for Unaccessible
                    for (auto location : locations) {
                        // Don't show locations which aren't progression
                        // Don't show any locations which aren't accessible if only accessible is checked
                        // Don't show any locations which don't meet the filter
                        if (!location->IsProgression() ||
                            (m_onlyAccessible && !m_currentSearch._visitedLocations.contains(location)) ||
                            !randomizer::utility::str::Contains(location->GetName(), filter)) {
                            continue;
                        }

                        // Don't show warp portals for now either
                        if (location->HasCategories("Warp Portal")) {
                            continue;
                        }

                        // Color red
                        auto color = ImVec4(1.f, 0.f, 0.f, 1.f);

                        // If the search found this location, change color to green
                        if (m_currentSearch._visitedLocations.contains(location)) {
                            color = ImVec4(0.f, 1.f, 0.f, 1.f);
                        }

                        ImGui::TextColored(color, "%s", location->GetName().c_str());

                        // Show requirements for the location below it (formatting isn't pretty right now)
                        if (m_showRequirements) {
                            ImGui::Text("    %s", location->GetComputedRequirement().to_string().c_str());
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

} // namespace dusk