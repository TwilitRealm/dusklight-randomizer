#pragma once
#include <map>
#include <string>
#include <vector>

#include "mods/api.h"
#include "nlohmann/json.hpp"

namespace randomizer::archi {

enum ItemHandlingFlags {
    None = 0, // No ReceivedItems is sent to you, ever.
    OtherWorlds = 1 << 0, // Indicates you get items sent from other worlds.
    Self = 1 << 1, // Indicates you get items sent from your own world. Requires 0b001 to be set.
    StartingInv = 1 << 2, // Indicates you get your starting inventory sent. Requires 0b001 to be set.
};

enum class HintStatus {
    HINT_UNSPECIFIED = 0,  // The receiving player has not specified any status
    HINT_NO_PRIORITY = 10, // The receiving player has specified that the item is unneeded
    HINT_AVOID = 20,       // The receiving player has specified that the item is detrimental
    HINT_PRIORITY = 30,    // The receiving player has specified that the item is needed
    HINT_FOUND = 40,       // The location has been collected. Status cannot be changed once found.
};

enum class ClientStatus {
    CLIENT_UNKNOWN = 0,
    CLIENT_CONNECTED = 5,
    CLIENT_READY = 10,
    CLIENT_PLAYING = 20,
    CLIENT_GOAL = 30
};

enum class ServerPacket {
    RoomInfo,
    ConnectionRefused,
    Connected,
    ReceivedItems,
    LocationInfo,
    RoomUpdate,
    PrintJSON,
    DataPackage,
    Bounced,
    InvalidPacket,
    Retrieved,
    SetReply
};

enum class ClientPacket {
    Connect,
    ConnectUpdate,
    Sync,
    LocationChecks,
    LocationScouts,
    UpdateHint,
    StatusUpdate,
    Say,
    GetDataPackage,
    Bounce,
    Get,
    Set,
    SetNotify
};

struct NetworkVersion {
    int major;
    int minor;
    int build;
};

struct NetworkItem {
    int item;
    int location;
    int player;
    int flags;
};

struct NetworkPlayer {
    int team;
    int slot;
    std::string alias;
    std::string name;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NetworkPlayer, team, slot, alias, name)

struct RoomInfo {
    NetworkVersion version;
    NetworkVersion generator_version;
    std::vector<std::string> tags;
    bool password;
    std::map<std::string, int> permissions;
    int hint_cost;
    int location_check_points;
    std::vector<std::string> games;
    std::map<std::string, std::string> datapackage_checksums;
    std::string seed_name;
    int time;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RoomInfo, version, generator_version, tags, password, permissions, hint_cost, location_check_points, games, datapackage_checksums, seed_name, time)

struct Connect {
    std::string password;
    std::string game;
    std::string name;
    std::string uuid;
    NetworkVersion version;
    int items_handling;
    std::vector<std::string> tags;
    bool slot_data;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Connect, password, game, name, uuid, version, items_handling, tags, slot_data)

struct GameData {
    std::map<std::string, int> item_name_to_id;
    std::map<std::string, int> location_name_to_id;
    int data_checksum;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GameData, item_name_to_id, location_name_to_id, data_checksum)

ModResult init();

void update();

void setServerIp(const std::string& ip);
std::string getServerIp();

void setSlotName(const std::string& name);
std::string getSlotName();

void setSlotPass(const std::string& name);
std::string getSlotPass();

} // randomizer::archi