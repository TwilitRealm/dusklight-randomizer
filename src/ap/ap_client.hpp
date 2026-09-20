#pragma once

// Minimal Archipelago network protocol client over the mod's own WebSocket client
// (see ws_tcp.hpp). Runs entirely on the game thread: call poll() once per frame.

#include "ws_tcp.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ap {

using json = nlohmann::json;

struct NetworkItem {
    int64_t item = 0;
    int64_t location = 0;
    int player = 0;
    int flags = 0;
};

struct ConnectInfo {
    std::string server;  // "host:port", optionally with ws:// or wss://
    std::string slot;
    std::string password;
};

enum class State {
    Disconnected,
    Connecting,    // socket opening
    Handshaking,   // waiting for RoomInfo / Connected
    Connected,
    Refused,
};

class Client {
public:
    // Callbacks (game thread)
    std::function<void(const json& connected)> onConnected;
    std::function<void(int index, const std::vector<NetworkItem>& items)> onItems;
    std::function<void(const std::string& text, const json& msg)> onPrint;
    std::function<void(const json& bounced)> onBounced;
    std::function<void(const std::string& reason)> onDisconnected;

    void connect(const ConnectInfo& info);
    void disconnect();
    void poll();

    void sendLocations(const std::vector<int64_t>& locations);
    void sendGoal();
    void sendSync();
    void sendBounce(const json& bounce);
    void say(const std::string& text);

    State state() const { return mState; }
    const std::string& lastError() const { return mLastError; }
    const ConnectInfo& info() const { return mInfo; }
    int slot() const { return mSlot; }
    const std::string& playerName(int slot) const;
    const std::string& seedName() const { return mSeedName; }

    static constexpr const char* kGame = "Twilight Princess (Dusklight)";

private:
    void open(const std::string& url);
    void on_open();
    void on_message(std::string_view text);
    void on_closed(std::string reason);
    void handle(const json& packet);
    void send(const json& packets);
    std::string nextUrl();

    ConnectInfo mInfo{};
    State mState = State::Disconnected;
    std::string mLastError;
    std::vector<std::string> mUrls;
    size_t mUrlIndex = 0;
    TcpWebSocket mSocket;
    int mSlot = -1;
    std::string mSeedName;
    std::vector<std::string> mPlayerNames;
};

// Parses AP's PrintJSON "data" parts into plain text, resolving player/item/location ids with
// the names the server gave us (item/location names come in the text for our game via slot data).
std::string flatten_print(const json& data, const Client& client);

}  // namespace ap
