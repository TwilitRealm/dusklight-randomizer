#ifndef DUSK_RANDOMIZER_CONTEXT_HPP
#define DUSK_RANDOMIZER_CONTEXT_HPP

#include <dolphin/types.h>
#include <optional>

#include <string>
#include <unordered_map>

class RandomizerContext {
public:
    RandomizerContext() = default;

    bool mActive{false};
    bool mCreatingSave{false};
    u32 mSeedID{0};
    std::string mHash{""};

    std::list<u16> mStartEventFlags{};
    std::unordered_map<u8, std::list<u8>> mStartRegionFlags{};
    std::list<u8> mStartingInventory{};
    std::unordered_map<std::string, std::unordered_map<u8, u8>> mTreasureChestOverrides{};
    std::unordered_map<std::string, std::unordered_map<u8, u8>> mPoeOverrides{};
    u8 mStartHour{0};
    u8 mMapBits{};

    std::optional<std::string> WriteToFile();
    std::optional<std::string> LoadFromHash(const std::string& hash);
    std::string GetSeedDataPath() const;
};

RandomizerContext& randomizer_GetContext();

bool randomizer_IsActive();

#endif //DUSK_RANDOMIZER_CONTEXT_HPP
