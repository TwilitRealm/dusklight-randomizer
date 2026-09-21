#include "ws_tcp.hpp"

#include <mods/svc/log.hpp>
#include <mods/svc/net.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <cstring>
#include <random>

namespace ap {
namespace {

std::string base64(const uint8_t* data, size_t size) {
    static constexpr char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((size + 2) / 3) * 4);
    for (size_t i = 0; i < size; i += 3) {
        const uint32_t a = data[i];
        const uint32_t b = i + 1 < size ? data[i + 1] : 0;
        const uint32_t c = i + 2 < size ? data[i + 2] : 0;
        const uint32_t triple = (a << 16) | (b << 8) | c;
        out += kTable[(triple >> 18) & 0x3F];
        out += kTable[(triple >> 12) & 0x3F];
        out += i + 1 < size ? kTable[(triple >> 6) & 0x3F] : '=';
        out += i + 2 < size ? kTable[triple & 0x3F] : '=';
    }
    return out;
}

// RFC 6455 wants masking keys the peer cannot predict, because a predictable mask lets a
// malicious intermediary steer what an HTTP cache in between sees. The TLS module's CSPRNG
// is right there; std::mt19937 is only a fallback for a platform that gives us no entropy.
void random_bytes(void* out, size_t size) {
    if (secure_random(out, size)) {
        return;
    }
    static std::mt19937 rng{std::random_device{}()};
    auto* bytes = static_cast<uint8_t*>(out);
    for (size_t i = 0; i < size; ++i) {
        bytes[i] = static_cast<uint8_t>(rng());
    }
}

// A message is not allowed to grow past this, whether it arrives in one frame or a thousand
// continuation frames; an HTTP upgrade response has a much smaller budget.
constexpr size_t kMaxMessageBytes = 64ull * 1024 * 1024;
constexpr size_t kMaxHandshakeBytes = 64 * 1024;

std::string frame(uint8_t opcode, const std::string& payload) {
    std::string out;
    out += static_cast<char>(0x80 | opcode);  // FIN + opcode
    const size_t len = payload.size();
    if (len < 126) {
        out += static_cast<char>(0x80 | len);  // MASK + length
    } else if (len <= 0xFFFF) {
        out += static_cast<char>(0x80 | 126);
        out += static_cast<char>((len >> 8) & 0xFF);
        out += static_cast<char>(len & 0xFF);
    } else {
        out += static_cast<char>(0x80 | 127);
        for (int shift = 56; shift >= 0; shift -= 8) {
            out += static_cast<char>((len >> shift) & 0xFF);
        }
    }
    uint8_t mask[4];
    random_bytes(mask, sizeof(mask));
    out.append(reinterpret_cast<const char*>(mask), 4);
    const size_t start = out.size();
    out.append(payload);
    for (size_t i = 0; i < len; ++i) {
        out[start + i] = static_cast<char>(out[start + i] ^ mask[i % 4]);
    }
    return out;
}

}  // namespace

bool TcpWebSocket::connect(
    const std::string& host, uint16_t port, const std::string& path, bool secure) {
    close();
    mHost = host;
    mPath = path.empty() ? "/" : path;
    mSecure = secure;
    mRx.clear();
    mFragment.clear();
    mEvents.clear();

    auto socket = mods::net::connect(fmt::format("tcp://{}:{}", host, port));
    if (!socket) {
        mEvents.push_back({EventType::Closed, {}, "could not open a TCP socket"});
        return false;
    }
    mHandle = socket.handle();
    socket.detach();  // lifetime managed by this class
    mState = State::Connecting;
    return true;
}

void TcpWebSocket::close() {
    if (mHandle != 0) {
        if (mState == State::Open) {
            const std::string payload{"\x03\xe8", 2};  // 1000 normal closure
            out_send(frame(0x8, payload));
        }
        svc_net->close(mod_ctx, mHandle);
        mHandle = 0;
    }
    mTls.reset();
    mState = State::Idle;
    mRx.clear();
    mFragment.clear();
}

void TcpWebSocket::fail(std::string reason) {
    if (mHandle != 0) {
        svc_net->close(mod_ctx, mHandle);
        mHandle = 0;
    }
    mTls.reset();
    mState = State::Idle;
    mEvents.push_back({EventType::Closed, {}, std::move(reason)});
}

bool TcpWebSocket::raw_send(const char* data, size_t size) {
    return mHandle != 0 && svc_net->send(mod_ctx, mHandle, data, size) == MOD_OK;
}

// Everything the protocol writes goes out through here, so TLS is the only difference
// between a ws:// and a wss:// connection.
bool TcpWebSocket::out_send(const std::string& bytes) {
    if (mSecure) {
        return mTls.write(bytes.data(), bytes.size());
    }
    return raw_send(bytes.data(), bytes.size());
}

bool TcpWebSocket::send_upgrade() {
    uint8_t keyBytes[16];
    random_bytes(keyBytes, sizeof(keyBytes));
    const std::string key = base64(keyBytes, sizeof(keyBytes));
    const std::string request = fmt::format(
        "GET {} HTTP/1.1\r\nHost: {}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: {}\r\nSec-WebSocket-Version: 13\r\n"
        "User-Agent: Dusklight-Archipelago\r\n\r\n",
        mPath, mHost, key);
    if (!out_send(request)) {
        fail("could not send the WebSocket handshake");
        return false;
    }
    mState = State::Handshake;
    return true;
}

bool TcpWebSocket::send_text(const std::string& text) {
    if (mState != State::Open || mHandle == 0) {
        return false;
    }
    return out_send(frame(0x1, text));
}

void TcpWebSocket::consume_handshake() {
    const auto end = mRx.find("\r\n\r\n");
    if (end == std::string::npos) {
        if (mRx.size() > kMaxHandshakeBytes) {
            fail("the server sent an oversized handshake response");
        }
        return;
    }
    const std::string head = mRx.substr(0, end);
    mRx.erase(0, end + 4);
    if (head.compare(0, 9, "HTTP/1.1 ") != 0 || head.compare(9, 3, "101") != 0) {
        const auto lineEnd = head.find("\r\n");
        fail("server refused the WebSocket upgrade: " + head.substr(0, std::min(lineEnd, size_t{80})));
        return;
    }
    mState = State::Open;
    mEvents.push_back({EventType::Open, {}, {}});
}

bool TcpWebSocket::consume_frames() {
    while (mRx.size() >= 2) {
        const auto* bytes = reinterpret_cast<const uint8_t*>(mRx.data());
        const bool fin = (bytes[0] & 0x80) != 0;
        const int opcode = bytes[0] & 0x0F;
        const bool masked = (bytes[1] & 0x80) != 0;
        uint64_t len = bytes[1] & 0x7F;
        size_t offset = 2;
        if (len == 126) {
            if (mRx.size() < offset + 2) {
                return true;
            }
            len = (static_cast<uint64_t>(bytes[2]) << 8) | bytes[3];
            offset += 2;
        } else if (len == 127) {
            if (mRx.size() < offset + 8) {
                return true;
            }
            len = 0;
            for (int i = 0; i < 8; ++i) {
                len = (len << 8) | bytes[offset + i];
            }
            offset += 8;
        }
        if (masked) {
            offset += 4;  // servers must not mask, but tolerate it
        }
        if (len > kMaxMessageBytes) {
            fail("the server sent an oversized frame");
            return false;
        }
        if (mRx.size() < offset + len) {
            return true;  // wait for the rest
        }
        std::string payload = mRx.substr(offset, static_cast<size_t>(len));
        if (masked) {
            const uint8_t* mask = bytes + offset - 4;
            for (size_t i = 0; i < payload.size(); ++i) {
                payload[i] = static_cast<char>(payload[i] ^ mask[i % 4]);
            }
        }
        mRx.erase(0, offset + static_cast<size_t>(len));

        switch (opcode) {
        case 0x0:  // continuation
        case 0x1:  // text
        case 0x2:  // binary
            if (mFragment.size() + payload.size() > kMaxMessageBytes) {
                fail("the server sent an oversized message");
                return false;
            }
            if (opcode != 0x0) {
                mFragmentOpcode = opcode;
                mFragment = std::move(payload);
            } else {
                mFragment += payload;
            }
            if (fin) {
                if (mFragmentOpcode == 0x1) {
                    mEvents.push_back({EventType::Message, std::move(mFragment), {}});
                }
                mFragment.clear();
            }
            break;
        case 0x8:  // close
            fail("server closed the connection");
            return false;
        case 0x9:  // ping -> pong
            out_send(frame(0xA, payload));
            break;
        default:
            break;  // pong and reserved opcodes
        }
    }
    return true;
}

bool TcpWebSocket::poll(Event& out) {
    mods::net::Event ev;
    while (mods::net::poll(ev)) {
        if (ev.handle != mHandle || mHandle == 0) {
            continue;
        }
        switch (ev.type) {
        case NET_EVENT_CONNECTED:
            if (mSecure) {
                // TLS first; the HTTP upgrade goes out once that handshake finishes.
                if (!mTls.start(mHost,
                        [this](const char* data, size_t size) { return raw_send(data, size); })) {
                    fail(mTls.error());
                    break;
                }
                mState = State::Tls;
                if (mTls.handshake_done()) {
                    send_upgrade();
                }
                break;
            }
            send_upgrade();
            break;
        case NET_EVENT_STREAM_DATA:
            if (mSecure) {
                mTls.feed(reinterpret_cast<const char*>(ev.data.data()), ev.data.size());
                if (!mTls.pump()) {
                    fail(mTls.error());
                    break;
                }
                if (mState == State::Tls && mTls.handshake_done() && !send_upgrade()) {
                    break;
                }
                mRx += mTls.take_plaintext();
            } else {
                mRx.append(reinterpret_cast<const char*>(ev.data.data()), ev.data.size());
            }
            if (mState == State::Handshake) {
                consume_handshake();
            }
            if (mState == State::Open && !consume_frames()) {
                break;
            }
            if (mSecure && mTls.closed_by_peer() && mRx.empty()) {
                fail("the server closed the connection");
            }
            break;
        case NET_EVENT_CLOSED:
            fail(ev.message.empty() ? "connection closed" : std::string(ev.message));
            break;
        default:
            break;
        }
    }

    if (mEvents.empty()) {
        return false;
    }
    out = std::move(mEvents.front());
    mEvents.erase(mEvents.begin());
    return true;
}

}  // namespace ap
