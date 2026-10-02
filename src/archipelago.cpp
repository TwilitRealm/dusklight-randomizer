#include "archipelago.hpp"

#include <queue>

#include "paths.hpp"
#include "session.hpp"
#include "../generator/utility/random.hpp"
#include "archi/ap_settings_convert.hpp"
#include "JSystem/JStudio/JStudio/ctb.h"
#include "mods/svc/log.hpp"
#include "mods/svc/websocket.hpp"

#include "nlohmann/json.hpp"

namespace randomizer::archi {
static mods::ws::Connection connection;
static RoomInfo roomInfo;
static std::queue<nlohmann::json> jsonQueue;
static std::string serverIp;
static std::string slotName;
static std::string slotPass;

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
    mods::log::info("Room Version: {}.{}.{}", info->version.major, info->version.minor, info->version.build);
    mods::log::info("Generator Version: {}.{}.{}", info->generator_version.major, info->generator_version.minor, info->generator_version.build);
    mods::log::info("Tag Count: {}", info->tags.size());
    mods::log::info("Requires Password: {}", info->password);
    mods::log::info("Permission Count: {}", info->permissions.size());
    mods::log::info("Hint Cost: {}", info->hint_cost);
    mods::log::info("Available Points: {}", info->location_check_points);
    mods::log::info("Games Size: {}", info->games.size());
    mods::log::info("Game Hashes Size: {}", info->datapackage_checksums.size());
    mods::log::info("Seed Name: {}", info->seed_name);
    mods::log::info("Server Time: {}", info->time);

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
    mods::log::debug("Slot Data: {}", slotData.dump());

    seedgen::config::Config testConfig;
    testConfig.SetSeed(slotData["SeedID"].get<std::string>());
    createConfigFromArchiSettings(testConfig, slotData["Settings"]);

    testConfig.WriteToFile(paths::GetRandomizerPath() / "archi" / "settings.yaml",
                                      paths::GetRandomizerPath() / "archi" / "preferences.yaml");
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
            case ServerPacket::ReceivedItems:
                break;
            case ServerPacket::DataPackage:
                break;
            case ServerPacket::Connected:
                parseConnected(archiPacket);
                break;
            case ServerPacket::ConnectionRefused:
                // break;
            case ServerPacket::LocationInfo:
                // break;
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
        mods::log::debug("Sending JSON: {}", outText);
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
        }
    }
}

void setServerIp(const std::string& ip) { serverIp = ip; }
std::string getServerIp() { return serverIp; }

void setSlotName(const std::string& name) { slotName = name; }
std::string getSlotName() { return slotName; }

void setSlotPass(const std::string& name) { slotPass = name; }
std::string getSlotPass() { return slotPass; }

} // randomizer::archi