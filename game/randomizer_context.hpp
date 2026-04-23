#ifndef DUSK_RANDOMIZER_CONTEXT_HPP
#define DUSK_RANDOMIZER_CONTEXT_HPP

#include <dolphin/types.h>

#include <iomanip>
#include <optional>
#include <string>
#include <sstream>
#include <unordered_map>

class RandomizerContext {
public:
    static constexpr size_t ACTOR_CRC_SIZE = 30;

    RandomizerContext() = default;

    bool mCreatingSave{false};
    u32 mSeedID{0};
    std::string mHash{""};

    std::unordered_map<std::string, std::string> mSettings{};

    std::list<u16> mStartEventFlags{};
    std::unordered_map<u8, std::list<u8>> mStartRegionFlags{};
    std::list<u8> mStartingInventory{};
    std::unordered_map<std::string, std::unordered_map<u8, u8>> mTreasureChestOverrides{};
    std::unordered_map<std::string, std::unordered_map<u8, u8>> mPoeOverrides{};
    std::unordered_map<u8, std::unordered_map<u8, u8>> mFreestandingItemOverrides{};
    std::unordered_map<u8, u8> mBugRewardOverrides{};
    std::unordered_map<std::string, int> mItemLocations{};
    u8 mStartHour{0};
    u8 mMapBits{};

    std::unordered_map<u32, std::unordered_map<u32, std::array<u8, 30>>> mActorPatches{};

    std::optional<std::string> WriteToFile();
    std::optional<std::string> LoadFromHash(const std::string& hash);
    std::string GetSeedDataPath() const;
};

RandomizerContext& randomizer_GetContext();

bool randomizer_IsActive();

int randomizer_getItemAtLocation(const std::string& locationName);

/**
 * Helper function to convert raw bytes of a container to a hex string
 */
template <typename T>
std::string ContainerToHexString(const T& container, bool includePrefix = true) {
    std::ostringstream oss;

    if (includePrefix) {
        oss << "0x";
    }

    // Get the raw byte pointer to the start of the data
    const auto* rawBytes = reinterpret_cast<const u8*>(container.data());

    // Calculate total byte size (number of elements * size of each element)
    size_t totalBytes = container.size() * sizeof(typename T::value_type);

    oss << std::hex << std::setfill('0') << std::uppercase;

    for (size_t i = 0; i < totalBytes; ++i) {
        // static_cast to not u8 is necessary so oss treats it as a number, not a char
        oss << std::setw(2) << static_cast<u16>(rawBytes[i]);
    }

    return oss.str();
}

/**
 * Helper function to convert hex string to raw bytes
 */
std::vector<u8> HexToBytes(std::string hex);

/*
 * Gets the current stage id, room no, and layer no in the format for a key in mActorPatches
 */
u32 getActorPatchesCurrentStageKey();

class stage_actor_data_class;
/*
 * Gets the CRC32 hash of an actors name, parameters, position, and angle
 */
u32 getActorCRC32(stage_actor_data_class*);

#endif //DUSK_RANDOMIZER_CONTEXT_HPP
