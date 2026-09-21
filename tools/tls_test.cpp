// Host-side test for the mod's defenses against a hostile Archipelago server: the TLS
// client (src/ap/tls.cpp) and the text sanitizer (src/ap/text_safe.hpp).
//
// TlsStream never touches a socket itself, so it can be driven over ordinary blocking
// sockets here, against real servers, without launching the game. Run with no arguments
// for the default suite, which checks both that good certificates are accepted and that
// bad ones are rejected:
//
//     tls_test
//     tls_test archipelago.gg:443=ok expired.badssl.com:443=fail
//
// Exits non-zero if any case did not behave as expected.

#include "ap/text_safe.hpp"
#include "ap/tls.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
static constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#define close_socket closesocket
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
using socket_t = int;
static constexpr socket_t kInvalidSocket = -1;
#define close_socket ::close
#endif

namespace {

struct Case {
    std::string host;
    std::string port;
    bool expectSuccess = true;
};

socket_t dial(const std::string& host, const std::string& port, std::string& error) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* results = nullptr;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &results) != 0 || results == nullptr) {
        error = "could not resolve " + host;
        return kInvalidSocket;
    }
    socket_t fd = kInvalidSocket;
    for (addrinfo* it = results; it != nullptr; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd == kInvalidSocket) {
            continue;
        }
        if (connect(fd, it->ai_addr, static_cast<int>(it->ai_addrlen)) == 0) {
            break;
        }
        close_socket(fd);
        fd = kInvalidSocket;
    }
    freeaddrinfo(results);
    if (fd == kInvalidSocket) {
        error = "could not connect to " + host + ":" + port;
    }
    return fd;
}

// Returns true when the case behaved as expected.
bool run(const Case& item) {
    std::printf("%-32s ", (item.host + ":" + item.port).c_str());
    std::fflush(stdout);

    std::string error;
    const socket_t fd = dial(item.host, item.port, error);
    if (fd == kInvalidSocket) {
        std::printf("SKIP  (%s)\n", error.c_str());
        return true;  // a network problem is not a TLS result
    }

#ifdef _WIN32
    DWORD timeout = 10000;
#else
    timeval timeout{10, 0};
#endif
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    ap::TlsStream tls;
    bool writeFailed = false;
    const bool started = tls.start(item.host, [&](const char* data, size_t size) {
        size_t sent = 0;
        while (sent < size) {
            const int n = static_cast<int>(send(fd, data + sent, static_cast<int>(size - sent), 0));
            if (n <= 0) {
                writeFailed = true;
                return false;
            }
            sent += static_cast<size_t>(n);
        }
        return true;
    });

    bool ok = started;
    char buffer[8192];
    while (ok && !tls.handshake_done()) {
        const int n = static_cast<int>(recv(fd, buffer, sizeof(buffer), 0));
        if (n <= 0) {
            ok = false;
            if (tls.error().empty()) {
                error = writeFailed ? "the connection dropped while sending" :
                                      "the connection dropped during the handshake";
            }
            break;
        }
        tls.feed(buffer, static_cast<size_t>(n));
        ok = tls.pump();
    }

    std::string firstLine;
    if (ok && tls.handshake_done()) {
        // Prove application data survives the round trip in both directions.
        const std::string request = "HEAD / HTTP/1.1\r\nHost: " + item.host +
                                    "\r\nConnection: close\r\nUser-Agent: tls_test\r\n\r\n";
        ok = tls.write(request.data(), request.size());
        while (ok && firstLine.empty()) {
            const int n = static_cast<int>(recv(fd, buffer, sizeof(buffer), 0));
            if (n <= 0) {
                break;
            }
            tls.feed(buffer, static_cast<size_t>(n));
            if (!tls.pump()) {
                ok = false;
                break;
            }
            const std::string text = tls.take_plaintext();
            const auto end = text.find("\r\n");
            if (end != std::string::npos) {
                firstLine = text.substr(0, end);
            }
        }
    }

    close_socket(fd);

    const bool succeeded = ok && tls.handshake_done();
    const std::string reason = !tls.error().empty() ? tls.error() : error;
    if (succeeded == item.expectSuccess) {
        if (succeeded) {
            std::printf("PASS  connected, server said: %s\n",
                firstLine.empty() ? "(no response line)" : firstLine.c_str());
        } else {
            std::printf("PASS  rejected: %s\n", reason.c_str());
        }
        return true;
    }
    if (succeeded) {
        std::printf("FAIL  handshake succeeded but should have been rejected\n");
    } else {
        std::printf("FAIL  %s\n", reason.empty() ? "handshake failed" : reason.c_str());
    }
    return false;
}

// The other half of what protects us from a hostile room: everything the server says goes
// through message_safe() before it reaches the game's message renderer or a toast.
bool check_text_safety() {
    struct Check {
        const char* what;
        std::string input;
        size_t cap;
        std::string expected;
    };
    const std::vector<Check> checks{
        {"plain text survives", "Link's Sword", 64, "Link's Sword"},
        {"newlines survive", "a\nb", 64, "a\nb"},
        {"tag escapes are dropped", "a\x1A\x05qqqqb", 64, "aqqqqb"},
        {"nulls are dropped", std::string("a\0b", 3), 64, "ab"},
        // Split literals: a hex escape swallows every hex digit that follows it.
        {"control bytes are dropped", "a\x01\x02\x7F" "b", 64, "ab"},
        {"length is capped", std::string(500, 'x'), 16, std::string(16, 'x')},
        {"utf-8 is not cut in half", "aaa\xC3\xA9", 4, "aaa"},
    };

    bool ok = true;
    for (const Check& check : checks) {
        const std::string got = ap::message_safe(check.input, check.cap);
        std::printf("%-32s ", check.what);
        if (got == check.expected) {
            std::printf("PASS\n");
        } else {
            std::printf("FAIL  got %zu bytes, expected %zu\n", got.size(), check.expected.size());
            ok = false;
        }
    }
    return ok;
}

Case parse(const std::string& text) {
    Case item;
    std::string address = text;
    const auto eq = address.rfind('=');
    if (eq != std::string::npos) {
        item.expectSuccess = address.substr(eq + 1) != "fail";
        address = address.substr(0, eq);
    }
    const auto colon = address.rfind(':');
    if (colon != std::string::npos) {
        item.host = address.substr(0, colon);
        item.port = address.substr(colon + 1);
    } else {
        item.host = address;
        item.port = "443";
    }
    return item;
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    std::vector<Case> cases;
    for (int i = 1; i < argc; ++i) {
        cases.push_back(parse(argv[i]));
    }
    if (cases.empty()) {
        cases = {
            {"archipelago.gg", "443", true},
            {"github.com", "443", true},
            {"expired.badssl.com", "443", false},
            {"wrong.host.badssl.com", "443", false},
            {"self-signed.badssl.com", "443", false},
            {"untrusted-root.badssl.com", "443", false},
        };
    }

    int failures = check_text_safety() ? 0 : 1;
    std::printf("\n");
    for (const Case& item : cases) {
        if (!run(item)) {
            ++failures;
        }
    }
    std::printf("\n%s\n",
        failures == 0 ? "everything behaved as expected" : "SOMETHING FAILED");

#ifdef _WIN32
    WSACleanup();
#endif
    return failures == 0 ? 0 : 1;
}
