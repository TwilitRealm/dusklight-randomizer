#include "archipelago.hpp"

#include <queue>

#include "paths.hpp"
#include "session.hpp"
#include "../generator/utility/random.hpp"
#include "../generator/utility/yaml.hpp"
#include "archi/ap_settings_convert.hpp"
#include "JSystem/JStudio/JStudio/ctb.h"
#include "mods/svc/log.hpp"
#include "mods/svc/websocket.hpp"

#include "nlohmann/json.hpp"
#include "ui/rando_seed_generation.hpp"

namespace randomizer::archi {
static constexpr int ARCHI_ITEM_OFFSET = 2320000;

// TODO: move all of this into a context struct instead of statics galore

static mods::ws::Connection connection;
static std::string curConnectionSession;
static RoomInfo roomInfo;
static int slotId = -1;
static int receivedIndex = -1;
static int receivedItemQueueStartIndex = -1;
static std::queue<nlohmann::json> jsonQueue;
static std::queue<ReceivedItems> serverItemQueue;
static std::map<int, int> apItemIdToGameId;
// temp assignments while debugging
static std::string serverIp = "localhost:38281";
static std::string slotName = "Crafty";
static std::string slotPass;
static std::string seedId;
static auto apWorldData = WorldData{};
static bool hasCreatedContext = false;
static bool isSaveActive = false;

static void clearSessionInfo() {
    apWorldData.itemPlacements.clear();

    // drain queues without parsing
    while (!jsonQueue.empty())
        jsonQueue.pop();

    while (!serverItemQueue.empty())
        serverItemQueue.pop();

    hasCreatedContext = false;
}

// NetworkVersion, NetworkItem, and NetworkPlayer all use "class" in their json-ified fields,
// which means we cannot use nlohmann's macros for serialization.

inline void to_json(nlohmann::json& nlohmann_json_j, const NetworkVersion& version) {
    nlohmann_json_j = {
        {"major", version.major},
        {"minor", version.minor},
        {"build", version.build},
        {"class", "Version"}
    };
}
inline void from_json(const nlohmann::json& nlohmann_json_j, NetworkVersion& version) {
    nlohmann_json_j.at("major").get_to(version.major);
    nlohmann_json_j.at("minor").get_to(version.minor);
    nlohmann_json_j.at("build").get_to(version.build);
}
inline void to_json(nlohmann::json& nlohmann_json_j, const NetworkItem& item) {
    nlohmann_json_j = {
        {"item", item.item},
        {"location", item.location},
        {"player", item.player},
        {"flags", item.flags},
        {"class", "NetworkItem"}
    };
}
inline void from_json(const nlohmann::json& nlohmann_json_j, NetworkItem& item) {
    nlohmann_json_j.at("item").get_to(item.item);
    nlohmann_json_j.at("location").get_to(item.location);
    nlohmann_json_j.at("player").get_to(item.player);
    nlohmann_json_j.at("flags").get_to(item.flags);
}
inline void to_json(nlohmann::json& nlohmann_json_j, const NetworkPlayer& player) {
    nlohmann_json_j = {
        {"team", player.team},
        {"slot", player.slot},
        {"alias", player.alias},
        {"name", player.name},
        {"class", "NetworkPlayer"}
    };
}
inline void from_json(const nlohmann::json& nlohmann_json_j, NetworkPlayer& player) {
    nlohmann_json_j.at("team").get_to(player.team);
    nlohmann_json_j.at("slot").get_to(player.slot);
    nlohmann_json_j.at("alias").get_to(player.alias);
    nlohmann_json_j.at("name").get_to(player.name);
}

void queueJsonPacket(const nlohmann::json& data) {
    jsonQueue.push(data);
}

template <typename T>
void queuePacket(const T& packet, const std::string_view& cmdName) {
    nlohmann::json data;
    data["cmd"] = cmdName;
    to_json(data, packet);
    queueJsonPacket(data);
}

void queueLocationScouts(const std::vector<int>& apLocationIds) {
    const auto& tempRef = apLocationIds;

    queueJsonPacket({
        {"cmd", "LocationScouts"},
        {"locations", tempRef},
        {"create_as_hint", false}
    });
}

static ServerPacket getServerCommandEnumByName(const std::string& name) {
    static std::unordered_map<std::string, ServerPacket> entries = {
        {"RoomInfo", ServerPacket::RoomInfo},
        {"ConnectionRefused", ServerPacket::ConnectionRefused},
        {"Connected", ServerPacket::Connected},
        {"ReceivedItems", ServerPacket::ReceivedItems},
        {"LocationInfo", ServerPacket::LocationInfo},
        {"RoomUpdate", ServerPacket::RoomUpdate},
        {"PrintJSON", ServerPacket::PrintJSON},
        {"DataPackage", ServerPacket::DataPackage},
        {"Bounced", ServerPacket::Bounced},
        {"InvalidPacket", ServerPacket::InvalidPacket},
        {"Retrieved", ServerPacket::Retrieved},
        {"SetReply", ServerPacket::SetReply}
    };

    auto it = entries.find(name);
    if (it != entries.end()) {
        return it->second;
    }

    throw std::runtime_error("unknown command type");
}

void parseRoomInfo(RoomInfo* info) {
    queueJsonPacket({
        {"cmd", "GetDataPackage"},
        {"games", roomInfo.games}
    });

    Connect connectPacket {
        .password = slotPass,
        .game = "Twilight Princess",
        .name = slotName,
        .uuid = fmt::format("{}_{}", slotName, static_cast<int>(0xFFFF * utility::random::RandomDouble())),
        .version = {0,6,7},
        .items_handling = ItemHandlingFlags::Self | ItemHandlingFlags::OtherWorlds | ItemHandlingFlags::StartingInv,
        .slot_data = true
    };

    queuePacket(connectPacket, "Connect");
}

void parseConnected(nlohmann::json& data) {
    auto slotData = data["slot_data"];
    slotId = data["slot"];

    // create seed config data with connection info
    auto seedHash = slotData["SeedID"].get<std::string>();
    auto seedDataPath = paths::GetRandomizerSeedsPath("archi") / seedHash / "seed.dat";
    setSeedId(seedHash);
    if (!std::filesystem::exists(seedDataPath)) {
        seedgen::config::Config testConfig;
        testConfig.SetSeed(slotData["SeedID"].get<std::string>());
        createConfigFromArchiSettings(testConfig, slotData["Settings"]);

        testConfig.WriteToFile(paths::GetRandomizerPath("archi" ) /  "settings.yaml",
                                          paths::GetRandomizerPath("archi") / "preferences.yaml");
    }else {
        hasCreatedContext = true;
    }
}

void parseDataPackage(nlohmann::json& data) {
    auto& locationToId = data["data"]["games"]["Twilight Princess"]["location_name_to_id"];
    std::vector<int> apLocationIds;

    for (const auto& [loc_name, id] : locationToId.items())
        apLocationIds.push_back(id.get<int>());

    queueLocationScouts(apLocationIds);
}

void parseLocationInfo(nlohmann::json& data) {
    for (const auto& item : data["locations"].get<std::vector<NetworkItem>>()) {
        int convertedID = item.item - ARCHI_ITEM_OFFSET;

        // TEMP until we have our own apworld that doesn't have these items (or they are added to the generator)
        if (convertedID == 16) // convert Water Bombs (3) to Water Bombs 5
            convertedID = 0x16;
        else if (convertedID == 20)
            convertedID = 0x1A; // convert Bomblings (3) to Bomblings 5

        apWorldData.itemPlacements[item.location - ARCHI_ITEM_OFFSET] = convertedID;
    }
}

void parseReceivedItems(const ReceivedItems& data) {
    // if save isn't active, we need to store received items in our own queue until mod api can handle item gives
    if (!isSaveActive) {
        serverItemQueue.emplace(data.index, data.items);
        return;
    }

    if (receivedIndex >= data.index) {
        // no need to parse items already previously parsed
        return;
    }

    bool isSkipNotify = false; // TODO: make use of this somehow

    for (const auto& item : data.items) {
        if (item.player != slotId)
            continue;

        int convertedID = apItemIdToGameId[item.item - ARCHI_ITEM_OFFSET];
        auto flags = isSkipNotify ? ITEM_GIVE_SILENT : 0;

        // TODO: we need to be able to get check_name from locationId, so that the check observer can set obtain flags if needed
        auto result = session::svc_mng.item->give_item(session::svc_mng.mod_ctx, nullptr, convertedID, flags);
        if (result != MOD_OK) {
            mods::log::warn("Failed to give item: {}", convertedID);
        }
    }

    setReceivedIndex(data.index);
}

void parseServerJson(nlohmann::json* data) {
    if (data->is_array()) {
        for (auto archiPacket : *data) {
            auto cmdStr = archiPacket["cmd"].get<std::string>();
            mods::log::info("Got command: {}", cmdStr);

            switch (auto cmd = getServerCommandEnumByName(cmdStr)) {
            case ServerPacket::RoomInfo: {
                roomInfo = archiPacket.get<RoomInfo>();
                parseRoomInfo(&roomInfo);
                break;
            }
            case ServerPacket::PrintJSON: {
                std::string totalMsg;
                for (auto textEntry : archiPacket["data"]) {
                    if (!textEntry["type"].is_null()) {
                        auto type = textEntry["type"].get<std::string>();
                    }

                    totalMsg += textEntry["text"].get<std::string>();
                }
                UiToastDesc desc {
                    .title_rml = "Archipelago",
                    .body_rml = totalMsg.c_str(),
                    .duration_ms = 3000
                };
                mods::log::info("{}", totalMsg);
                session::svc_mng.ui->push_toast(session::svc_mng.mod_ctx, &desc);
                break;
            }
            case ServerPacket::ReceivedItems: {
                int newIdx = archiPacket["index"].get<int>();
                const auto& itemList = archiPacket["items"].get<std::vector<NetworkItem>>();
                parseReceivedItems({.index = newIdx, .items = itemList});
                break;
            }
            case ServerPacket::DataPackage:
                parseDataPackage(archiPacket);
                break;
            case ServerPacket::Connected:
                parseConnected(archiPacket);
                break;
            case ServerPacket::ConnectionRefused:
                // break;
            case ServerPacket::LocationInfo:
                parseLocationInfo(archiPacket);
                break;
            case ServerPacket::RoomUpdate:
                // break;
            case ServerPacket::Bounced:
                // break;
            case ServerPacket::InvalidPacket:
                // break;
            case ServerPacket::Retrieved:
                // break;
            case ServerPacket::SetReply:
                // break;
            default:
                mods::log::debug("Command JSON: {}", archiPacket.dump());
                break;
            }
        }
    }
}

ModResult init() {
    std::string url;

    // this isn't the best solution but it'll work for now
    if (serverIp.starts_with("localhost") || serverIp.starts_with("127.0.0.1")) {
        url = fmt::format("ws://{}", serverIp);
    }else {
        url = fmt::format("wss://{}", serverIp);
    }

    auto sessionStr = fmt::format("{}_{}", url, slotName); // include slot name with url, so rooms with multiple TP games will work fine.
    if (!curConnectionSession.empty()) {
        if (sessionStr == curConnectionSession) {
            mods::log::debug("Session already active, no need to reconnect.");
            return MOD_OK;
        }else {
            // end previous connection before re-connecting to new socket
            shutdown();
        }
    }
    curConnectionSession = sessionStr;

    connection = mods::ws::connect({.url = url});

    if (connection) {
        mods::log::info("Successfully connected to URL: {}", serverIp);
        return MOD_OK;
    }

    mods::log::error("failed to connect to URL: {}", serverIp);
    return connection.result();
}

void update() {
    if (!jsonQueue.empty()) {
        nlohmann::json rootSendJson = nlohmann::json::array();
        while (!jsonQueue.empty()) {
            auto json = jsonQueue.front();
            rootSendJson.push_back(json);
            jsonQueue.pop();
        }

        std::string outText = rootSendJson.dump();
        // mods::log::debug("Sending JSON: {}", outText);
        auto result = connection.send_text(outText);
        if (result != MOD_OK) {
            mods::log::warn("Failed to send json data to server.");
        }
    }

    mods::ws::Event event;
    while (mods::ws::poll(event)) {
        if (event.type == WEBSOCKET_EVENT_MESSAGE) {
            std::string jsonStr(reinterpret_cast<const char*>(event.data.data()), event.data.size());
            nlohmann::json json = nlohmann::json::parse(jsonStr);
            parseServerJson(&json);
        } else if (event.type == WEBSOCKET_EVENT_CLOSED) {
            mods::log::info("Websocket closed!");
            hasCreatedContext = false;
            // TODO: attempt to handle a reconnection
            session::reportError("Archipelago connection terminated. Returning to main menu.");
        }
    }

    if (!hasCreatedContext && !apWorldData.itemPlacements.empty()) {
        ui::GenerateRandomizerSeed(Type::AP, apWorldData);
        hasCreatedContext = true;
    }

    if (!serverItemQueue.empty() && isSaveActive) {
        while (!serverItemQueue.empty()) {
            auto& receivedItems = serverItemQueue.front();
            parseReceivedItems(receivedItems);
            serverItemQueue.pop();
        }
    }
}

ModResult shutdown() {
    mods::log::info("Ending archipelago connection.");
    clearSessionInfo();
    return connection.close();
}

void loadItemIdsFromDb() {
    auto itemDataTree = LOAD_EMBED_YAML(RANDO_DATA_PATH "items.yaml");
    for (const auto& itemNode : itemDataTree) {
        auto idNode = itemNode["APItemId"];
        if (!idNode)
            continue;

        auto itemId = itemNode["Id"].as<int>();
        auto apItemId = idNode.as<int>();
        apItemIdToGameId[apItemId] = itemId;
    }
}

void sendLocationChecked(int locationId) {
    queueJsonPacket({
        {"cmd", "LocationChecks"},
        {"locations", {
            ARCHI_ITEM_OFFSET + locationId
        }}
    });
}

void setServerIp(const std::string& ip) { serverIp = ip; }
std::string getServerIp() { return serverIp; }

void setSlotName(const std::string& name) { slotName = name; }
std::string getSlotName() { return slotName; }

void setSlotPass(const std::string& name) { slotPass = name; }
std::string getSlotPass() { return slotPass; }

void setSeedId(const std::string& id) { seedId = id; }
std::string getSeedId() { return seedId; }

void setReceivedIndex(int index) {
    receivedIndex = index;
}
int getReceivedIndex() { return receivedIndex; }

void setSeedActive(bool state) { isSaveActive = state; }

} // randomizer::archi