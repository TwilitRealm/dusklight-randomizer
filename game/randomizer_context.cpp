#include "randomizer_context.hpp"

#include "../generator/utility/yaml.hpp"

#include "dusk/app_info.hpp"
#include "dusk/logging.h"
#include "dusk/main.h"
#include "dusk/randomizer/game/tools.h"

#include "SDL3/SDL_filesystem.h"

#include <fstream>

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

    seedData << YAML::Dump(out);
    seedData.close();

    return std::nullopt;
}

std::optional<std::string> RandomizerContext::LoadFromHash(const std::string& hash) {
    this->mHash = hash;

    auto in = LoadYAML(this->GetSeedDataPath());

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

    DuskLog.debug("Loaded Randomizer Seed {}", this->mHash);

    return std::nullopt;
}

std::string RandomizerContext::GetSeedDataPath() const {
    return std::string(SDL_GetPrefPath(dusk::OrgName, dusk::AppName)) + "randomizer/seeds/" + this->mHash + "/seed.dat";
}

