#pragma once

// Text from the Archipelago server ends up in two renderers that were not written with a
// hostile source in mind: the game's own message system, which reads in-band control codes
// out of what it is given, and the mod's toasts. Everything the server says passes through
// here first, so those only ever see printable text of a bounded length.
//
// Header-only and dependency-free so tools/tls_test.cpp can test it directly.

#include <cstddef>
#include <string>
#include <string_view>

namespace ap {

inline std::string message_safe(std::string_view in, size_t maxBytes) {
    std::string out;
    bool truncated = false;
    for (const char c : in) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte != '\n' && (byte < 0x20 || byte == 0x7F)) {
            continue;  // control codes, including the tag escape the message format uses
        }
        if (out.size() >= maxBytes) {
            truncated = true;
            break;
        }
        out += c;
    }
    if (truncated) {
        // Do not leave half a UTF-8 sequence at the end.
        while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0x80) {
            out.pop_back();
        }
        if (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0xC0) {
            out.pop_back();
        }
    }
    return out;
}

}  // namespace ap
