#include "ap_client.hpp"

#include <mods/svc/log.hpp>
#include <mods/svc/websocket.hpp>

#include <memory>
#include <random>
#include <unordered_map>

namespace ap {
namespace {

std::unique_ptr<mods::ws::Connection> s_conn;

// Names resolved from GetDataPackage, per game.
std::unordered_map<std::string, std::unordered_map<int64_t, std::string>> s_itemNames;
std::unordered_map<std::string, std::unordered_map<int64_t, std::string>> s_locationNames;
std::unordered_map<int, std::string> s_slotGames;

std::string make_uuid() {
    static std::string uuid;
    if (uuid.empty()) {
        std::random_device rd;
        std::mt19937_64 rng(rd());
        constexpr char hex[] = "0123456789abcdef";
        for (int i = 0; i < 32; ++i) {
            uuid += hex[rng() & 0xF];
        }
    }
    return uuid;
}

bool is_local_host(const std::string& host) {
    return host.starts_with("localhost") || host.starts_with("127.") || host.starts_with("[::1]");
}

const std::string kEmpty;

}  // namespace

const std::string& Client::playerName(int slot) const {
    if (slot >= 0 && static_cast<size_t>(slot) < mPlayerNames.size() && !mPlayerNames[slot].empty()) {
        return mPlayerNames[slot];
    }
    return kEmpty;
}

void Client::connect(const ConnectInfo& info) {
    disconnect();
    mInfo = info;
    mLastError.clear();
    mUrls.clear();
    mUrlIndex = 0;

    std::string server = info.server;
    while (!server.empty() && (server.back() == ' ' || server.back() == '/')) {
        server.pop_back();
    }
    while (!server.empty() && server.front() == ' ') {
        server.erase(server.begin());
    }
    if (server.starts_with("ws://") || server.starts_with("wss://")) {
        mUrls.push_back(server);
    } else {
        if (server.find(':') == std::string::npos) {
            server += ":38281";
        }
        // Plain ws:// is only allowed to loopback by the host; everything else must be TLS
        // (archipelago.gg serves wss).
        if (is_local_host(server)) {
            mUrls.push_back("ws://" + server);
            mUrls.push_back("wss://" + server);
        } else {
            mUrls.push_back("wss://" + server);
        }
    }
    open(mUrls[0]);
}

void Client::open(const std::string& url) {
    mods::log::info("archipelago: connecting to {}", url);
    mods::ws::Options opts;
    opts.url = url;
    opts.connectTimeoutMs = 8000;
    opts.keepaliveIntervalMs = 20000;
    opts.maxMessageBytes = 16u * 1024u * 1024u;  // data packages can be large
    s_conn = std::make_unique<mods::ws::Connection>(mods::ws::connect(opts));
    if (!*s_conn) {
        mState = State::Disconnected;
        mLastError = "could not open connection to " + url;
        s_conn.reset();
        return;
    }
    mHandle = s_conn->handle();
    mState = State::Connecting;
}

void Client::disconnect() {
    if (s_conn) {
        s_conn->close(1000, "bye");
        s_conn.reset();
    }
    mHandle = 0;
    if (mState != State::Refused) {
        mState = State::Disconnected;
    }
}

void Client::send(const json& packets) {
    if (!s_conn || !*s_conn) {
        return;
    }
    s_conn->send_text(packets.dump(-1, ' ', false, json::error_handler_t::replace));
}

void Client::poll() {
    mods::ws::Event ev;
    while (mods::ws::poll(ev)) {
        if (ev.handle != mHandle) {
            continue;
        }
        switch (ev.type) {
        case WEBSOCKET_EVENT_OPEN:
            mods::log::info("archipelago: socket open");
            mState = State::Handshaking;
            break;
        case WEBSOCKET_EVENT_MESSAGE: {
            const std::string_view text{reinterpret_cast<const char*>(ev.data.data()), ev.data.size()};
            json packets = json::parse(text, nullptr, false);
            if (packets.is_discarded() || !packets.is_array()) {
                mods::log::warn("archipelago: malformed packet");
                break;
            }
            for (const auto& p : packets) {
                handle(p);
            }
            break;
        }
        case WEBSOCKET_EVENT_CLOSED: {
            s_conn.reset();
            mHandle = 0;
            const bool neverOpened = mState == State::Connecting;
            if (neverOpened && mUrlIndex + 1 < mUrls.size()) {
                open(mUrls[++mUrlIndex]);
                break;
            }
            mods::log::info("archipelago: socket closed (state {}, code {}, error {}, '{}' / '{}')",
                static_cast<int>(mState), ev.closeCode, static_cast<int>(ev.error), ev.message,
                ev.closeReason);
            std::string reason = std::string(ev.message.empty() ? ev.closeReason : ev.message);
            if (reason.empty()) {
                reason = "connection closed";
            }
            if (mState != State::Refused) {
                mState = State::Disconnected;
                mLastError = reason;
            }
            if (onDisconnected) {
                onDisconnected(mLastError);
            }
            break;
        }
        default:
            break;
        }
    }
}

void Client::handle(const json& p) {
    const std::string cmd = p.value("cmd", "");
    mods::log::debug("archipelago: <- {}", cmd);
    if (cmd == "RoomInfo") {
        mSeedName = p.value("seed_name", "");
        json games = p.value("games", json::array());
        send(json::array({{{"cmd", "GetDataPackage"}, {"games", games}}}));
        send(json::array({{
            {"cmd", "Connect"},
            {"password", mInfo.password},
            {"game", kGame},
            {"name", mInfo.slot},
            {"uuid", make_uuid()},
            {"version", {{"major", 0}, {"minor", 6}, {"build", 2}, {"class", "Version"}}},
            // Other worlds' items + starting inventory. Items at our own locations are given
            // in-game by the rebuilt seed itself.
            {"items_handling", 0b101},
            {"tags", json::array()},
            {"slot_data", true},
        }}));
    } else if (cmd == "Connected") {
        mSlot = p.value("slot", -1);
        mPlayerNames.clear();
        for (const auto& pl : p.value("players", json::array())) {
            const int s = pl.value("slot", 0);
            if (s >= static_cast<int>(mPlayerNames.size())) {
                mPlayerNames.resize(s + 1);
            }
            mPlayerNames[s] = pl.value("alias", pl.value("name", ""));
        }
        s_slotGames.clear();
        if (p.contains("slot_info")) {
            for (const auto& [k, v] : p["slot_info"].items()) {
                s_slotGames[std::stoi(k)] = v.value("game", "");
            }
        }
        mState = State::Connected;
        if (onConnected) {
            onConnected(p);
        }
    } else if (cmd == "ConnectionRefused") {
        std::string errs;
        for (const auto& e : p.value("errors", json::array())) {
            errs += (errs.empty() ? "" : ", ") + e.get<std::string>();
        }
        mLastError = "refused: " + (errs.empty() ? std::string("unknown") : errs);
        mState = State::Refused;
        if (s_conn) {
            s_conn->close(1000, "refused");
            s_conn.reset();
        }
        mHandle = 0;
        if (onDisconnected) {
            onDisconnected(mLastError);
        }
    } else if (cmd == "ReceivedItems") {
        std::vector<NetworkItem> items;
        for (const auto& it : p.value("items", json::array())) {
            items.push_back({it.value("item", int64_t{0}), it.value("location", int64_t{0}),
                it.value("player", 0), it.value("flags", 0)});
        }
        if (onItems) {
            onItems(p.value("index", 0), items);
        }
    } else if (cmd == "DataPackage") {
        for (const auto& [game, gd] : p["data"]["games"].items()) {
            auto& in = s_itemNames[game];
            for (const auto& [name, id] : gd.value("item_name_to_id", json::object()).items()) {
                in[id.get<int64_t>()] = name;
            }
            auto& ln = s_locationNames[game];
            for (const auto& [name, id] : gd.value("location_name_to_id", json::object()).items()) {
                ln[id.get<int64_t>()] = name;
            }
        }
    } else if (cmd == "PrintJSON") {
        if (onPrint) {
            onPrint(flatten_print(p.value("data", json::array()), *this), p);
        }
    } else if (cmd == "Bounced") {
        if (onBounced) {
            onBounced(p);
        }
    }
}

void Client::sendLocations(const std::vector<int64_t>& locations) {
    if (mState != State::Connected || locations.empty()) {
        return;
    }
    send(json::array({{{"cmd", "LocationChecks"}, {"locations", locations}}}));
}

void Client::sendGoal() {
    if (mState == State::Connected) {
        send(json::array({{{"cmd", "StatusUpdate"}, {"status", 30}}}));
    }
}

void Client::sendSync() {
    if (mState == State::Connected) {
        send(json::array({{{"cmd", "Sync"}}}));
    }
}

void Client::sendBounce(const json& bounce) {
    if (mState == State::Connected) {
        json b = bounce;
        b["cmd"] = "Bounce";
        send(json::array({b}));
    }
}

void Client::say(const std::string& text) {
    if (mState == State::Connected) {
        send(json::array({{{"cmd", "Say"}, {"text", text}}}));
    }
}

std::string flatten_print(const json& data, const Client& client) {
    std::string out;
    for (const auto& part : data) {
        const std::string type = part.value("type", "text");
        const std::string text = part.value("text", "");
        if (type == "player_id") {
            const int slot = std::atoi(text.c_str());
            const auto& name = client.playerName(slot);
            out += name.empty() ? text : name;
        } else if (type == "item_id" || type == "location_id") {
            const int owner = part.value("player", 0);
            const auto gameIt = s_slotGames.find(owner);
            const auto& table = type == "item_id" ? s_itemNames : s_locationNames;
            const int64_t id = std::strtoll(text.c_str(), nullptr, 10);
            std::string name = text;
            if (gameIt != s_slotGames.end()) {
                if (const auto g = table.find(gameIt->second); g != table.end()) {
                    if (const auto n = g->second.find(id); n != g->second.end()) {
                        name = n->second;
                    }
                }
            }
            out += name;
        } else {
            out += text;
        }
    }
    return out;
}

}  // namespace ap
