#pragma once

// Minimal RFC 6455 WebSocket client over NetService's raw TCP sockets.
//
// Dusklight's WebSocketService (borealis/WinHTTP) only delivers the first message a client
// sends on a connection; everything queued afterwards never reaches the server, which makes
// it unusable for a protocol that keeps talking (location checks, status updates). This
// implementation talks the protocol directly, so it is used for every ws:// server. TLS
// (wss://) still goes through the host service.

#include <cstdint>
#include <string>
#include <vector>

namespace ap {

class TcpWebSocket {
public:
    enum class EventType { None, Open, Message, Closed };

    struct Event {
        EventType type = EventType::None;
        std::string text;   // Message
        std::string error;  // Closed
    };

    bool connect(const std::string& host, uint16_t port, const std::string& path);
    void close();
    bool send_text(const std::string& text);
    bool poll(Event& out);
    bool active() const { return mState != State::Idle; }

private:
    enum class State { Idle, Connecting, Handshake, Open };

    void fail(std::string reason);
    void consume_handshake();
    bool consume_frames();

    uint64_t mHandle = 0;
    State mState = State::Idle;
    std::string mRx;
    std::string mFragment;
    int mFragmentOpcode = 0;
    std::vector<Event> mEvents;
    std::string mHost;
    std::string mPath;
};

}  // namespace ap
