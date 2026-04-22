#include "imgui.h"

#include "d/d_stage.h"

#include "ImGuiConsole.hpp"
#include "ImGuiMenuRandomizer.hpp"

#include "dusk/app_info.hpp"
#include "dusk/logging.h"
#include "dusk/randomizer/game/randomizer_context.hpp"
#include "dusk/randomizer/game/tools.h"
#include "dusk/randomizer/generator/randomizer.hpp"

#include "SDL3/SDL_filesystem.h"

#include <mutex>
#include <thread>
#include <filesystem>

#include "dusk/randomizer/generator/utility/endian.hpp"
#include "dusk/randomizer/generator/utility/string.hpp"

namespace dusk {

    static bool generatingSeed = false;
    static std::string generationStatusMsg{};
    static std::mutex generationStatusMsgMutex{};

    void GenerateSeed() {
        std::lock_guard lock(generationStatusMsgMutex);
        const auto result = SDL_GetPrefPath(dusk::OrgName, dusk::AppName);
        if (!result) {
            DuskLog.fatal("Unable to get PrefPath: {}", SDL_GetError());
        }
        randomizer::Randomizer r;
        r.SetBaseOutputPath(result);
        auto generationResult = r.Generate();
        if (generationResult.has_value()) {
            generationStatusMsg = fmt::format("Generation failed with the following error:\n{}", generationResult.value());
            return;
        }

        const auto& world = r.GetWorlds()[0];

        RandomizerContext randoData{};

        for (const auto& [setting, info] : *randomizer::seedgen::settings::GetAllSettingsInfo()) {
            if (info->NeedInGame()) {
                randoData.mSettings[setting] = world->Setting(setting).GetCurrentOption();
            }
        }

        // Chest overrides
        for (const auto& location : world->GetAllLocations()) {
            const auto& metaData = location->GetMetadata();
            if (location->HasCategories("Chest")) {
                const auto& stage = metaData[0]["Stage"].as<std::string>();
                const auto& tboxId = metaData[0]["Tbox ID"].as<u8>();
                const auto& itemId = location->GetCurrentItem()->GetID();
                randoData.mTreasureChestOverrides[stage][tboxId] = itemId;
            }
        }

        // Set starting inventory
        for (const auto& item: world->GetStartingItemPool()) {
            randoData.mStartingInventory.push_back(item->GetID());
        }

        // Set starting flags
        auto startFlags = LoadYAML(RANDO_DATA_PATH "startflags.yaml");
        // Event Flags
        for (const auto& flagNode : startFlags["EventFlags"]) {
            if (flagNode.IsScalar()) {
                const auto& flag = flagNode.as<u16>();
                randoData.mStartEventFlags.push_back(flag);
            } else if (flagNode.IsMap()) {
                const auto& condition = flagNode.begin()->first.as<std::string>();
                if (world->EvaluateSettingCondition(condition)) {
                    DuskLog.debug("Setting flags for {}", condition);
                    for (const auto& conditionalFlag : flagNode.begin()->second) {
                        const auto& flag = conditionalFlag.as<u16>();
                        randoData.mStartEventFlags.push_back(flag);
                    }
                }
            }
        }

        // Region Flags
        for (const auto& regionNode : startFlags["RegionFlags"]) {
            const auto& region = regionNode.first.as<std::string>();
            const auto& index = regionNode.second["Index"].as<int>();
            const auto& flags = regionNode.second["Flags"];
            DuskLog.debug("Setting region flags for {}", region);
            // This seems kinda scuffed so maybe we change it later
            for (const auto& flagNode : flags) {
                if (flagNode.IsScalar()) {
                    const auto& flag = flagNode.as<int>();
                    randoData.mStartRegionFlags[index].push_back(flag);
                } else if (flagNode.IsMap()) {
                    const auto& condition = flagNode.begin()->first.as<std::string>();
                    if (world->EvaluateSettingCondition(condition)) {
                        for (const auto& conditionalFlag : flagNode.begin()->second) {
                            const auto& flag = conditionalFlag.as<int>();
                            randoData.mStartRegionFlags[index].push_back(flag);
                        }
                    }
                }
            }
        }

        if (world->Setting("Unlock Map Regions") == "On")
        {
            auto& bits = randoData.mMapBits;
            bits = 0x20;
            if (world->Setting("Snowpeak Does Not Require Reekfish Scent") == "On") {bits |= 0x40;}
            if (world->Setting("Lanayru Twilight Cleared") == "On") {bits |= 0x10;}
            if (world->Setting("Eldin Twilight Cleared") == "On") {bits |= 0x08;}
            if (world->Setting("Faron Twilight Cleared") == "On") {bits |= 0x04;}
            if (world->Setting("Skip Prologue") == "On") {bits |= 0x02;}
        }

        // Set starting time of day
        const auto startTimeSetting = world->Setting("Starting Time of Day");
        if (startTimeSetting == "Morning")
            randoData.mStartHour = 6;
        else if (startTimeSetting == "Noon")
            randoData.mStartHour = 12;
        else if (startTimeSetting == "Evening")
            randoData.mStartHour = 18;
        else if (startTimeSetting == "Night")
            randoData.mStartHour = 24;

        // Actor Patches
        auto actorPatches = LoadYAML(RANDO_DATA_PATH "actor_patches.yaml");
        for (const auto& stageNode : actorPatches) {
            const auto& stageName = stageNode.first.as<std::string>();
            for (const auto& roomNode : stageNode.second) {
                const auto& roomStr = roomNode.first.as<std::string>();
                u8 roomNo = roomStr.back() - '0';
                for (const auto& layerNode : roomNode.second) {
                    const auto& layerStr = layerNode.first.as<std::string>();
                    u8 layerNo = layerStr.back() - '0';
                    // Create key based off of stage index, room, and layer
                    u32 stageRoomLayerKey{};
                    stageRoomLayerKey |= getStageID(stageName.c_str()) << 16;
                    stageRoomLayerKey |= roomNo << 8;
                    stageRoomLayerKey |= layerNo;
                    for (const auto& actorNode : layerNode.second) {
                        using namespace Utility::Endian;
                        // Get all the data for the actor (with endian shenanigans)
                        stage_actor_data_class actor{};
                        const auto& actorName = actorNode["name"].as<std::string>();
                        strncpy(actor.name, actorName.c_str(), 8);
                        actor.base.parameters = toPlatform(target, actorNode["parameters"].as<u32>());
                        actor.base.position.x = toPlatform(target, actorNode["position"]["x"].as<f32>());
                        actor.base.position.y = toPlatform(target, actorNode["position"]["y"].as<f32>());
                        actor.base.position.z = toPlatform(target, actorNode["position"]["z"].as<f32>());
                        // Have to retrieve as u16 and then cast as s16 because otherwise yaml-cpp
                        // complains about values over 32767 not fitting in s16
                        actor.base.angle.x = toPlatform(target, static_cast<s16>(actorNode["angle"]["x"].as<u16>()));
                        actor.base.angle.y = toPlatform(target, static_cast<s16>(actorNode["angle"]["y"].as<u16>()));
                        actor.base.angle.z = toPlatform(target, static_cast<s16>(actorNode["angle"]["z"].as<u16>()));


                        // Create unique hash based off of actor data
                        u32 actorCRC32 = getActorCRC32(&actor);

                        // Then override the actor with whatever parts are being patched
                        const auto& patchNode = actorNode["patch"];
                        if (patchNode["parameters"]) {
                            actor.base.parameters = toPlatform(target, patchNode["parameters"].as<u32>());
                        }
                        if (auto patchPosition = patchNode["position"]) {
                            if (patchPosition["x"]) {
                                actor.base.position.x = toPlatform(target, patchPosition["x"].as<f32>());
                            }
                            if (patchPosition["y"]) {
                                actor.base.position.y = toPlatform(target, patchPosition["y"].as<f32>());
                            }
                            if (patchPosition["z"]) {
                                actor.base.position.z = toPlatform(target, patchPosition["z"].as<f32>());
                            }
                        }
                        if (auto patchAngle = patchNode["angle"]) {
                            // Have to retrieve as u16 and then cast as s16 because otherwise yaml-cpp
                            // complains about values over 32767 not fitting in s16
                            if (patchAngle["x"]) {
                                actor.base.angle.x = toPlatform(target, static_cast<s16>(patchAngle["x"].as<u16>()));
                            }
                            if (patchAngle["y"]) {
                                actor.base.angle.y = toPlatform(target, static_cast<s16>(patchAngle["y"].as<u16>()));
                            }
                            if (patchAngle["z"]) {
                                actor.base.angle.z = toPlatform(target, static_cast<s16>(patchAngle["z"].as<u16>()));
                            }
                        }

                        // Insert the actor patch into the context with our crc32 as the key and the
                        // raw actor patch data as the value
                        std::array<u8, RandomizerContext::ACTOR_CRC_SIZE> patchedActorData{};
                        std::memcpy(patchedActorData.data(), &actor, RandomizerContext::ACTOR_CRC_SIZE);
                        randoData.mActorPatches[stageRoomLayerKey][actorCRC32] = patchedActorData;
                    }
                }
            }
        }

        randoData.mHash = r.GetConfig().GetHash();
        auto writeToFileResult = randoData.WriteToFile();
        if (writeToFileResult.has_value()) {
            generationStatusMsg =
                fmt::format("Failed to write seed data. Reason: {}", writeToFileResult.value());
            return;
        }

        generationStatusMsg = fmt::format("Seed generated! Hash: {}", randoData.mHash);
    }

    static void StartSeedGeneration() {
        if (generatingSeed) {
            return;
        }
        generatingSeed = true;
        GenerateSeed();
        generatingSeed = false;
        DuskLog.debug("{}", generationStatusMsg);
    }

    ImGuiMenuRandomizer::ImGuiMenuRandomizer() {}

    void ImGuiMenuRandomizer::draw() {
        if (ImGui::BeginMenu("Randomizer")) {
            if (ImGui::BeginMenu("Generate")) {
                if (ImGui::MenuItem("Generate Seed", nullptr, false, !generatingSeed)) {

                    std::thread randoGenerationThread(StartSeedGeneration);
                    randoGenerationThread.detach();
                    m_showRandoGeneration = true;
                }

                ImGui::EndMenu();
            }

            std::string loadSeedText = "Load Seed";
            if (!playerIsOnTitleScreen()) {
                loadSeedText += " (Must be on title screen)";
            }
            if (ImGui::BeginMenu(loadSeedText.c_str(), playerIsOnTitleScreen())) {

                std::string seedDirectory =
                    std::string(SDL_GetPrefPath(dusk::OrgName, dusk::AppName)) + "randomizer/seeds";

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

                std::string seedDirectory =
                    std::string(SDL_GetPrefPath(dusk::OrgName, dusk::AppName)) + "randomizer/seeds";

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
                        const std::string hashDirectory = seedDirectory + "/" + hash;
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

            std::string name = "Deactive Randomizer";
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

        ImGui::Text(generationText.c_str());

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
            ImGui::Text("Status: ");
            ImGui::SameLine();
            if (randomizer_IsActive()) {
                ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "ACTIVE");
            } else {
                ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "NOT ACTIVE");
            }
        }

        ImGui::End();
    }

} // namespace dusk