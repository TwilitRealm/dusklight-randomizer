#include "randomizer_context.hpp"

#include "../generator/utility/yaml.hpp"

#include "dusk/app_info.hpp"
#include "dusk/logging.h"
#include "dusk/main.h"
#include "dusk/randomizer/game/tools.h"
#include "dusk/randomizer/generator/utility/endian.hpp"

#include "SDL3/SDL_filesystem.h"
#include <zlib-ng.h>

#include <fstream>

#include "d/actor/d_a_alink.h"

RandomizerContext& randomizer_GetContext() {
    static RandomizerContext instance;
    return instance;
}

bool randomizer_IsActive() {
    return dusk::IsGameLaunched && (!playerIsOnTitleScreen() || randomizer_GetContext().mCreatingSave) && randomizer_GetContext().mActive;
}

std::optional<std::string> RandomizerContext::WriteToFile() {

    std::ofstream seedData(this->GetSeedDataPath());
    if (!seedData.is_open()) {
        return "Could not open seed data file";
    }

    YAML::Node out{};

    for (const auto& [settingName, option] : this->mSettings) {
        out["Settings"][settingName] = option;
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