#include "randomizer_context.hpp"

#include "dusk/app_info.hpp"
#include "dusk/logging.h"
#include "dusk/main.h"
#include "dusk/randomizer/game/tools.h"
#include "dusk/randomizer/generator/utility/endian.hpp"
#include "dusk/randomizer/generator/utility/yaml.hpp"
#include "dusk/randomizer/generator/randomizer.hpp"

#include "SDL3/SDL_filesystem.h"
#include <zlib-ng.h>

#include <fstream>

#include "d/actor/d_a_alink.h"

std::optional<std::string> RandomizerContext::WriteToFile() {

    std::ofstream seedData(this->GetSeedDataPath());
    if (!seedData.is_open()) {
        return "Could not open seed data file";
    }

    YAML::Node out{};

    for (const auto& [settingName, option] : this->mSettings) {
        out["mSettings"][settingName] = option;
    }

    // NOTE: When dumping u8s, they must be converted to u16s (or higher), otherwise they get dumped
    // as single characters and not numbers

    out["mStartEventFlags"] = this->mStartEventFlags;
    for (const auto& [region, flags] : this->mStartRegionFlags) {
        const std::list<u16> u16Flags(flags.begin(), flags.end());
        out["mStartRegionFlags"][static_cast<u16>(region)] = u16Flags;
    }

    const std::list<u16> u16Inventory(this->mStartingInventory.begin(), this->mStartingInventory.end());
    out["mStartingInventory"] = u16Inventory;

    for (const auto& [stageName, chestOverride] : this->mTreasureChestOverrides) {
        for (const auto& [tboxId, itemId] : chestOverride) {
            out["mTreasureChestOverrides"][stageName][static_cast<u16>(tboxId)] = static_cast<u16>(itemId);
        }
    }

    for (const auto& [stageIdx, itemOverride] : this->mFreestandingItemOverrides) {
        for (const auto& [flag, itemId] : itemOverride) {
            out["mFreestandingItemOverrides"][static_cast<u16>(stageIdx)][static_cast<u16>(flag)] = static_cast<u16>(itemId);
        }
    }

    const std::unordered_map<u16, u16> u16BugRewardOverrides(this->mBugRewardOverrides.begin(), this->mBugRewardOverrides.end());
    out["mBugRewardOverrides"] = u16BugRewardOverrides;

    const std::unordered_map<u16, u16> u16SkyCharacterOverrides(this->mSkyCharacterOverrides.begin(), this->mSkyCharacterOverrides.end());
    out["mSkyCharacterOverrides"] = u16SkyCharacterOverrides;

    out["mItemLocations"] = this->mItemLocations;

    out["mStartHour"] = static_cast<u16>(this->mStartHour);
    out["mMapBits"] = static_cast<u16>(this->mMapBits);

    for (const auto& [stageRoomLayer, actorPatches] : this->mActorPatches) {
        for (const auto& [actorCRC, actorPatch] : actorPatches) {
            out["mActorPatches"][stageRoomLayer][actorCRC] = ContainerToHexString(actorPatch);
        }
    }

    seedData << YAML::Dump(out);
    seedData.close();

    return std::nullopt;
}

std::optional<std::string> RandomizerContext::LoadFromHash(const std::string& hash) {
    this->mHash = hash;

    auto in = LoadYAML(this->GetSeedDataPath());

    // Necessary settings
    for (const auto& settingNode : in["mSettings"] ) {
        const auto& settingName = settingNode.first.as<std::string>();
        const auto& option = settingNode.second.as<std::string>();
        this->mSettings[settingName] = option;
    }

    // Event flags
    for (const auto& flag : in["mStartEventFlags"]) {
        this->mStartEventFlags.push_back(flag.as<u16>());
    }
    // Region Flags
    for (const auto& regionNode : in["mStartRegionFlags"]) {
        const auto& regionId = regionNode.first.as<u8>();
        for (const auto& flag : regionNode.second) {
            this->mStartRegionFlags[regionId].push_back(flag.as<u8>());
        }
    }

    // Starting inventory
    for (const auto& itemId : in["mStartingInventory"]) {
        this->mStartingInventory.push_back(itemId.as<u8>());
    }

    // Chest overrides
    for (const auto& stageNode : in["mTreasureChestOverrides"]) {
        const auto& stageName = stageNode.first.as<std::string>();
        // Single nodes with a zero in their key will get dumped as sequences
        if (stageNode.second.IsSequence()) {
            this->mTreasureChestOverrides[stageName][0] = stageNode.second[0].as<u8>();
        } else {
            for (const auto& chestItemPair : stageNode.second) {
                auto tboxId = chestItemPair.first.as<u8>();
                auto itemId = chestItemPair.second.as<u8>();
                this->mTreasureChestOverrides[stageName][tboxId] = itemId;
            }
        }
    }

    // Freestanding overrides
    for (const auto& stageNode : in["mFreestandingItemOverrides"]) {
        const auto& stageIdx = stageNode.first.as<u8>();
        // Single nodes with a zero in their key will get dumped as sequences
        if (stageNode.second.IsSequence()) {
            this->mFreestandingItemOverrides[stageIdx][0] = stageNode.second[0].as<u8>();
        } else {
            for (const auto& flagItemPair : stageNode.second) {
                auto flag = flagItemPair.first.as<u8>();
                auto itemId = flagItemPair.second.as<u8>();
                this->mFreestandingItemOverrides[stageIdx][flag] = itemId;
            }
        }
    }

    // Bug Rewards
    for (const auto& bugNode : in["mBugRewardOverrides"]) {
        u8 bugItemId = bugNode.first.as<u8>();
        u8 itemId = bugNode.second.as<u8>();
        this->mBugRewardOverrides[bugItemId] = itemId;
    }

    // Sky Characters
    for (const auto& skyCharacterNode : in["mSkyCharacterOverrides"]) {
        u16 key = skyCharacterNode.first.as<u16>();
        u8 itemId = skyCharacterNode.second.as<u8>();
        this->mSkyCharacterOverrides[key] = itemId;
    }

    // Items we call by location name
    for (const auto& locationNode : in["mItemLocations"]) {
        const auto& locationName = locationNode.first.as<std::string>();
        int itemId = locationNode.second.as<int>();
        this->mItemLocations[locationName] = itemId;
    }

    // Starting hour
    this->mStartHour = in["mStartHour"].as<u8>();
    // Starting map bits
    this->mMapBits = in["mMapBits"].as<u8>();

    // Actor Patches
    for (const auto& stageRoomLayerNode: in["mActorPatches"]) {
        u32 stageRoomLayer = stageRoomLayerNode.first.as<u32>();
        for (const auto& actorPatchNode : stageRoomLayerNode.second) {
            u32 actorCRC = actorPatchNode.first.as<u32>();
            auto actorBytes = HexToBytes(actorPatchNode.second.as<std::string>());
            auto& patchedActor = this->mActorPatches[stageRoomLayer][actorCRC];
            std::copy_n(actorBytes.begin(), actorBytes.size(), patchedActor.begin());
        }
    }

    DuskLog.debug("Loaded Randomizer Seed {}", this->mHash);

    return std::nullopt;
}

std::string RandomizerContext::GetSeedDataPath() const {
    return std::string(SDL_GetPrefPath(dusk::OrgName, dusk::AppName)) + "randomizer/seeds/" + this->mHash + "/seed.dat";
}

RandomizerContext& randomizer_GetContext() {
    static RandomizerContext instance;
    return instance;
}

bool randomizer_IsActive() {
    return dusk::IsGameLaunched && (!playerIsOnTitleScreen() || randomizer_GetContext().mCreatingSave) && !randomizer_GetContext().mHash.empty();
}

std::vector<u8> HexToBytes(std::string hex) {
    std::vector<u8> bytes;
    // Strip "0x" if present
    if (hex.substr(0, 2) == "0x") hex = hex.substr(2);

    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        u8 byte = static_cast<u8>(strtol(byteString.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

int randomizer_getItemAtLocation(const std::string& locationName) {
    return randomizer_GetContext().mItemLocations[locationName];
}

u32 getActorPatchesCurrentStageKey() {
    u32 actorPatchesStageKey{};
    actorPatchesStageKey |= getStageID(dComIfGp_getStartStageName()) << 16;
    actorPatchesStageKey |= dComIfGp_getStartStageRoomNo() << 8;
    actorPatchesStageKey |= dComIfGp_getLayerNo();
    return actorPatchesStageKey;
}

u32 getActorCRC32(stage_actor_data_class* actor) {
    return zng_crc32(0, reinterpret_cast<u8*>(actor), RandomizerContext::ACTOR_CRC_SIZE);
}

void GenerateAndWriteSeed(std::string& generationStatusMsg) {
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

    // Settings we need to check ingame
    for (const auto& [setting, info] : *randomizer::seedgen::settings::GetAllSettingsInfo()) {
        if (info->NeedInGame()) {
            randoData.mSettings[setting] = world->Setting(setting).GetCurrentOption();
        }
    }

    // Set data for all locations
    for (const auto& location : world->GetAllLocations()) {
        const auto& metaData = location->GetMetadata();

        // Chest Overrides
        if (location->HasCategories("Chest")) {
            const auto& stage = metaData[0]["Stage"].as<std::string>();
            const auto& tboxId = metaData[0]["Tbox ID"].as<u8>();
            const auto& itemId = location->GetCurrentItem()->GetID();
            randoData.mTreasureChestOverrides[stage][tboxId] = itemId;
        }

        // Freestanding Overrides
        // Keyed by the stage index and collectible flag of the item
        if (location->HasCategories("Freestanding Item")) {
            u8 stage = metaData[0]["Stage"].as<u8>();
            u8 flag = metaData[0]["Flag"].as<u8>();
            u8 itemId = location->GetCurrentItem()->GetID();
            randoData.mFreestandingItemOverrides[stage][flag] = itemId;
        }

        // Bug Rewards
        // Keyed by the item id of the original bug
        if (location->HasCategories("Bug Reward")) {
            u8 bugItemId = metaData[0]["Bug Item Id"].as<u8>();
            u8 itemId = location->GetCurrentItem()->GetID();
            randoData.mBugRewardOverrides[bugItemId] = itemId;
        }

        // Sky Characters
        // Keyed by u16 of 0xFF00 (stage index) and 0x00FF (roomNo)
        if (location->HasCategories("Sky Book")) {
            u8 stageIdx = metaData[0]["Stage"].as<u8>();
            u8 roomNo = metaData[0]["Room"].as<u8>();
            u8 itemId = location->GetCurrentItem()->GetID();
            u16 key = (stageIdx << 8) | roomNo;
            randoData.mSkyCharacterOverrides[key] = itemId;
        }

        // Items that we lookup just by calling their location name
        if (location->HasCategories("Location Name Lookup")) {
            const auto& locationName = metaData.as<std::string>();
            const int itemId = location->GetCurrentItem()->GetID();
            randoData.mItemLocations[locationName] = itemId;
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
            u8 roomNo = roomNode.first.as<u8>();
            for (const auto& actorNode : roomNode.second) {
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
                if (patchNode["name"]) {
                    const auto& newName = patchNode["name"].as<std::string>();
                    strncpy(actor.name, newName.c_str(), 8);
                }
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
                for (const auto& layerNode : actorNode["layers"]) {
                    u8 layerNo = layerNode.as<u8>();
                    // Create key based off of stage index, room, and layer
                    u32 stageRoomLayerKey{};
                    stageRoomLayerKey |= getStageID(stageName.c_str()) << 16;
                    stageRoomLayerKey |= roomNo << 8;
                    stageRoomLayerKey |= layerNo;
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