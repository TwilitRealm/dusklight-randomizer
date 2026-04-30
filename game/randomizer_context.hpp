#ifndef DUSK_RANDOMIZER_CONTEXT_HPP
#define DUSK_RANDOMIZER_CONTEXT_HPP

#include <dolphin/types.h>

#include <list>
#include <iomanip>
#include <optional>
#include <string>
#include <sstream>
#include <unordered_map>

/*
 * Class holding all the information necessary for playing
 * the current randomizer seed
 */
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
    std::unordered_map<u16, u8> mPoeOverrides{};
    std::unordered_map<u8, std::unordered_map<u8, u8>> mFreestandingItemOverrides{};
    std::unordered_map<u8, u8> mBugRewardOverrides{};
    std::unordered_map<u16, u8> mSkyCharacterOverrides{};
    std::unordered_map<u16, u8> mGoldenWolfOverrides{};
    std::unordered_map<u16, u8> mShopOverrides{};
    std::unordered_map<std::string, int> mItemLocations{};
    u8 mStartHour{0};
    u8 mMapBits{};

    std::unordered_map<u32, std::unordered_map<u32, std::array<u8, 30>>> mActorPatches{};
    std::unordered_map<u32, std::unordered_map<u32, std::list<std::array<u8, 30>>>> mActorAdditions{};
    std::unordered_map<u32, u64> mFlowPatches{};

    std::optional<std::string> WriteToFile();
    std::optional<std::string> LoadFromHash(const std::string& hash);
    std::string GetSeedDataPath() const;
};

/*
 * Class holding seed-agnostic dynamic information about current randomizer play.
 * This gets reset when reseting to the title screen.
 */
class RandomizerState {
public:
    enum TimeChange {
        NO_CHANGE = 0,
        CHANGE_TO_NIGHT,
        CHANGE_TO_DAY,
    };

    enum EventItemStatus {
        QUEUE_EMPTY,
        ITEM_IN_QUEUE,
        CLEAR_QUEUE,
    };

    static constexpr u8 EVENT_ITEM_QUEUE_SIZE = 10;

    RandomizerState() {mInitialized = false;}

    int _create();
    int _delete();
    int execute();
    int draw();
    void addItemToEventQueue(u8 item);
    void initGiveItemToPlayer();
    //void handleBonkDamage();
    void handleTimeOfDayChange();
    void handleTimeSpeed();
    void offLoad();

    void handlePoeItem(u8 bitSw);

    u8 getGiveItemToPlayerStatus() const { return mEventItemStatus;}
    u8 getTimeChange() const { return mTimeChange; }
    bool getRoomReloadingState() const { return mRoomReloadingState; }
    bool getHasPendingToDChange() const { return mHasPendingToDChange; }

    void setGiveItemToPlayerStatus(u8 status) { mEventItemStatus = status;}
    void setHasPendingToDChange(bool hasPending) { mHasPendingToDChange = hasPending; }
    void setTimeChange(u8 newTimeChange) { mTimeChange = newTimeChange; }
    void setRoomReloadingState(bool newState) { mRoomReloadingState = newState; }

    bool mInitialized{false};
    u8 mEventItemStatus{};
    bool mHasPendingToDChange{false};
    u8 mTimeChange{};
    u8 mEventItemQueue[EVENT_ITEM_QUEUE_SIZE];
    bool mRoomReloadingState{false};
};

extern RandomizerState g_randomizerState;

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
u32 getActorPatchesCurrentStageKey(u8 roomNo);

class stage_actor_data_class;
/*
 * Gets the CRC32 hash of an actors name, parameters, position, and angle
 */
u32 getActorCRC32(stage_actor_data_class*);

void GenerateAndWriteSeed(std::string& generationStatusMsg);
#endif //DUSK_RANDOMIZER_CONTEXT_HPP
