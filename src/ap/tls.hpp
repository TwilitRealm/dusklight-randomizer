#pragma once

// TLS for the mod's own WebSocket client.
//
// This is a transport-agnostic TLS 1.2/1.3 client: it never touches a socket itself. Ciphertext
// arrives through feed() and leaves through the write callback given to start(), so it works
// just as well over NetService's raw TCP sockets in the game as it does over plain BSD sockets
// in tools/tls_test.cpp. Certificates are verified against a CA bundle embedded in the mod
// binary (src/ap/ca_bundle.pem), so nothing has to be installed alongside the mod.
//
// Deliberately free of any dusklight headers so it can be built and tested on the host.

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace ap {

// Cryptographically strong random bytes, from the same entropy source TLS uses. Game thread
// only. Returns false if the platform gave us no entropy, in which case the caller decides
// whether it can live with a weaker source.
bool secure_random(void* out, size_t size);

class TlsStream {
public:
    // Sends ciphertext towards the peer. Returning false fails the connection.
    using WriteFn = std::function<bool(const char* data, size_t size)>;

    TlsStream();
    ~TlsStream();

    TlsStream(const TlsStream&) = delete;
    TlsStream& operator=(const TlsStream&) = delete;

    // Begins a handshake with `host` (also used for SNI and certificate name checking).
    bool start(const std::string& host, WriteFn write);
    void reset();

    // Hands ciphertext received from the peer to the stream.
    void feed(const char* data, size_t size);

    // Advances the handshake and decrypts whatever has been fed so far. False means the
    // connection is dead and error() says why.
    bool pump();

    bool handshake_done() const;
    bool closed_by_peer() const;

    // Encrypts and writes plaintext. Only valid once handshake_done().
    bool write(const char* data, size_t size);

    // Application data decrypted by pump(), moved out of the stream.
    std::string take_plaintext();

    const std::string& error() const { return mError; }

private:
    struct Impl;

    int bio_send(const unsigned char* buf, size_t len);
    int bio_recv(unsigned char* buf, size_t len);
    void fail(const std::string& what, int code);

    std::unique_ptr<Impl> mImpl;
    WriteFn mWrite;
    std::string mInbox;      // ciphertext waiting to be decrypted
    std::string mPlaintext;  // decrypted application data
    std::string mError;
    bool mStarted = false;
    bool mHandshakeDone = false;
    bool mPeerClosed = false;
    bool mFailed = false;
};

}  // namespace ap
