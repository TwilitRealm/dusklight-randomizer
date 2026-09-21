#include "tls.hpp"

#include <battery/embed.hpp>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <algorithm>
#include <cstring>

namespace ap {
namespace {

// The CA bundle is parsed once and shared by every connection: it is read-only once built,
// and parsing 120-odd roots is not something to redo on each connect.
mbedtls_x509_crt* shared_roots(std::string& error) {
    static mbedtls_x509_crt chain;
    static int parseResult = [] {
        mbedtls_x509_crt_init(&chain);
        const auto bundle = b::embed<"src/ap/ca_bundle.pem">();
        // mbedtls requires PEM input to include the terminating NUL in the length.
        std::string text(bundle.data(), bundle.size());
        return mbedtls_x509_crt_parse(&chain,
            reinterpret_cast<const unsigned char*>(text.c_str()), text.size() + 1);
    }();
    if (parseResult < 0) {
        error = "could not read the built-in certificate bundle";
        return nullptr;
    }
    return &chain;
}

std::string describe(int code) {
    char buffer[192] = {};
    mbedtls_strerror(code, buffer, sizeof(buffer) - 1);
    if (buffer[0] == '\0') {
        return "error " + std::to_string(code);
    }
    return buffer;
}

}  // namespace

bool secure_random(void* out, size_t size) {
    static mbedtls_entropy_context entropy;
    static mbedtls_ctr_drbg_context drbg;
    static const bool ready = [] {
        static constexpr char kPersonalization[] = "dusklight-archipelago-rng";
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&drbg);
        return mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
                   reinterpret_cast<const unsigned char*>(kPersonalization),
                   sizeof(kPersonalization) - 1) == 0;
    }();
    return ready &&
           mbedtls_ctr_drbg_random(&drbg, static_cast<unsigned char*>(out), size) == 0;
}

struct TlsStream::Impl {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_entropy_context entropy;
    bool initialized = false;

    void init() {
        mbedtls_ssl_init(&ssl);
        mbedtls_ssl_config_init(&conf);
        mbedtls_ctr_drbg_init(&drbg);
        mbedtls_entropy_init(&entropy);
        initialized = true;
    }

    void free() {
        if (!initialized) {
            return;
        }
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&conf);
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
        initialized = false;
    }

    ~Impl() { free(); }
};

TlsStream::TlsStream() : mImpl{std::make_unique<Impl>()} {}
TlsStream::~TlsStream() = default;

int TlsStream::bio_send(const unsigned char* buf, size_t len) {
    if (!mWrite || !mWrite(reinterpret_cast<const char*>(buf), len)) {
        // Any negative value is fatal to mbedtls; this avoids pulling in net_sockets.h
        // (and winsock with it) just for MBEDTLS_ERR_NET_SEND_FAILED.
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    return static_cast<int>(len);
}

int TlsStream::bio_recv(unsigned char* buf, size_t len) {
    if (mInbox.empty()) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    const size_t take = std::min(len, mInbox.size());
    std::memcpy(buf, mInbox.data(), take);
    mInbox.erase(0, take);
    return static_cast<int>(take);
}

void TlsStream::fail(const std::string& what, int code) {
    mFailed = true;
    mError = what + ": " + describe(code);
    if (code == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED && mImpl->initialized) {
        const uint32_t flags = mbedtls_ssl_get_verify_result(&mImpl->ssl);
        char detail[512] = {};
        if (mbedtls_x509_crt_verify_info(detail, sizeof(detail) - 1, "", flags) > 0) {
            std::string text{detail};
            while (!text.empty() && (text.back() == '\n' || text.back() == ' ')) {
                text.pop_back();
            }
            std::replace(text.begin(), text.end(), '\n', ';');
            mError = "the server's certificate was rejected: " + text;
        }
    }
}

bool TlsStream::start(const std::string& host, WriteFn write) {
    reset();
    mWrite = std::move(write);
    mImpl->init();

    mbedtls_x509_crt* roots = shared_roots(mError);
    if (roots == nullptr) {
        mFailed = true;
        return false;
    }

    static constexpr char kPersonalization[] = "dusklight-archipelago";
    int result = mbedtls_ctr_drbg_seed(&mImpl->drbg, mbedtls_entropy_func, &mImpl->entropy,
        reinterpret_cast<const unsigned char*>(kPersonalization), sizeof(kPersonalization) - 1);
    if (result != 0) {
        fail("could not seed the random generator", result);
        return false;
    }

    result = mbedtls_ssl_config_defaults(
        &mImpl->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (result != 0) {
        fail("could not configure TLS", result);
        return false;
    }

    // Verification is not optional: a rejected certificate must fail the connection.
    mbedtls_ssl_conf_authmode(&mImpl->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&mImpl->conf, roots, nullptr);
    mbedtls_ssl_conf_rng(&mImpl->conf, mbedtls_ctr_drbg_random, &mImpl->drbg);
    mbedtls_ssl_conf_min_tls_version(&mImpl->conf, MBEDTLS_SSL_VERSION_TLS1_2);

    result = mbedtls_ssl_setup(&mImpl->ssl, &mImpl->conf);
    if (result != 0) {
        fail("could not set up TLS", result);
        return false;
    }
    // Sets SNI and, more importantly, the name the certificate has to match.
    result = mbedtls_ssl_set_hostname(&mImpl->ssl, host.c_str());
    if (result != 0) {
        fail("could not set the TLS host name", result);
        return false;
    }
    mbedtls_ssl_set_bio(
        &mImpl->ssl, this,
        [](void* ctx, const unsigned char* buf, size_t len) {
            return static_cast<TlsStream*>(ctx)->bio_send(buf, len);
        },
        [](void* ctx, unsigned char* buf, size_t len) {
            return static_cast<TlsStream*>(ctx)->bio_recv(buf, len);
        },
        nullptr);

    mStarted = true;
    return pump();
}

void TlsStream::reset() {
    mImpl->free();
    mWrite = nullptr;
    mInbox.clear();
    mPlaintext.clear();
    mError.clear();
    mStarted = false;
    mHandshakeDone = false;
    mPeerClosed = false;
    mFailed = false;
}

void TlsStream::feed(const char* data, size_t size) {
    // A TLS record is at most ~16 KB and pump() drains every complete one, so this only trips
    // if the peer is streaming bytes that will never become a record. Refuse rather than grow.
    static constexpr size_t kMaxPending = 1024 * 1024;
    if (mInbox.size() + size > kMaxPending) {
        mFailed = true;
        mError = "the server sent more data than TLS could make sense of";
        mInbox.clear();
        return;
    }
    mInbox.append(data, size);
}

bool TlsStream::pump() {
    if (mFailed) {
        return false;
    }
    if (!mStarted) {
        mError = "TLS was not started";
        return false;
    }

    if (!mHandshakeDone) {
        const int result = mbedtls_ssl_handshake(&mImpl->ssl);
        if (result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE) {
            return true;  // needs more bytes from the peer
        }
        if (result != 0) {
            fail("TLS handshake failed", result);
            return false;
        }
        mHandshakeDone = true;
    }

    char buffer[16 * 1024];
    for (;;) {
        const int result =
            mbedtls_ssl_read(&mImpl->ssl, reinterpret_cast<unsigned char*>(buffer), sizeof(buffer));
        if (result > 0) {
            mPlaintext.append(buffer, static_cast<size_t>(result));
            continue;
        }
        if (result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE) {
            break;
        }
        // TLS 1.3 servers send session tickets after the handshake; they surface here and
        // are not an error.
        if (result == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
            continue;
        }
        if (result == 0 || result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            mPeerClosed = true;
            break;
        }
        fail("TLS read failed", result);
        return false;
    }
    return true;
}

bool TlsStream::handshake_done() const {
    return mHandshakeDone;
}

bool TlsStream::closed_by_peer() const {
    return mPeerClosed;
}

bool TlsStream::write(const char* data, size_t size) {
    if (mFailed || !mHandshakeDone) {
        return false;
    }
    size_t sent = 0;
    while (sent < size) {
        const int result = mbedtls_ssl_write(&mImpl->ssl,
            reinterpret_cast<const unsigned char*>(data + sent), size - sent);
        if (result > 0) {
            sent += static_cast<size_t>(result);
            continue;
        }
        if (result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;  // our BIO never blocks, so this cannot spin
        }
        fail("TLS write failed", result);
        return false;
    }
    return true;
}

std::string TlsStream::take_plaintext() {
    std::string out;
    out.swap(mPlaintext);
    return out;
}

}  // namespace ap
