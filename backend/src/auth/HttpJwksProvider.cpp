#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "auth/AuthErrors.hpp"
#include "auth/Crypto.hpp"
#include "auth/GoogleTokenVerifier.hpp"

namespace chronoplan::auth {

namespace {

constexpr std::size_t kMaxBodyBytes = 256 * 1024;
constexpr long long kMinRefetchSeconds = 60;      // unknown kid: at most one refetch per minute
constexpr long long kDefaultTtlSeconds = 3600;
constexpr long long kMinTtlSeconds = 60;
constexpr long long kMaxTtlSeconds = 24 * 3600;

struct Download {
    std::string body;
    long long maxAge = -1;
    bool tooLarge = false;
};

std::size_t onBody(char* data, std::size_t size, std::size_t nmemb, void* user) {
    auto* d = static_cast<Download*>(user);
    const std::size_t n = size * nmemb;
    if (d->body.size() + n > kMaxBodyBytes) { d->tooLarge = true; return 0; }   // abort the transfer
    d->body.append(data, n);
    return n;
}

std::size_t onHeader(char* data, std::size_t size, std::size_t nmemb, void* user) {
    auto* d = static_cast<Download*>(user);
    const std::size_t n = size * nmemb;
    std::string line(data, n);
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.rfind("cache-control:", 0) == 0) {
        const auto pos = lower.find("max-age=");
        if (pos != std::string::npos) {
            const long long v = std::strtoll(lower.c_str() + pos + 8, nullptr, 10);
            if (v > 0) d->maxAge = v;
        }
    }
    return n;
}

void curlInitOnce() {
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

}  // namespace

HttpJwksProvider::HttpJwksProvider(Clock clock, std::string url) : clock_(std::move(clock)), url_(std::move(url)) {}

std::optional<RsaPublicKey> HttpJwksProvider::lookup(const std::string& kid) const {
    for (const Entry& e : keys_)
        if (e.kid == kid) return e.key;
    return std::nullopt;
}

std::optional<RsaPublicKey> HttpJwksProvider::findKey(const std::string& kid) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = clock_();
    const bool expired = fetchedAt_ == 0 || now >= validUntil_;
    if (!expired) {
        if (auto k = lookup(kid)) return k;
        if (now - fetchedAt_ < kMinRefetchSeconds) return std::nullopt;   // recently fetched: kid really is unknown
    }
    try {
        refresh();
    } catch (const GoogleUnavailableError&) {
        // Keep using older keys for a while rather than failing every sign-in during a short outage.
        if (fetchedAt_ == 0 || now - fetchedAt_ > kMaxTtlSeconds) throw;
    }
    return lookup(kid);
}

void HttpJwksProvider::refresh() {
    curlInitOnce();
    CURL* curl = curl_easy_init();
    if (!curl) throw GoogleUnavailableError("could not initialise the HTTP client");
    Download dl;
    curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, onBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &dl);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, onHeader);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &dl);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, static_cast<long>(CURLPROTO_HTTPS));
#endif
    const CURLcode rc = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK || status != 200 || dl.tooLarge) {
        std::cerr << "[google] could not fetch signing keys: curl=" << rc << " http=" << status << '\n';
        throw GoogleUnavailableError("could not fetch Google's signing keys");
    }

    const nlohmann::json doc = nlohmann::json::parse(dl.body, nullptr, false);
    if (doc.is_discarded() || !doc.is_object() || !doc.contains("keys") || !doc["keys"].is_array())
        throw GoogleUnavailableError("Google returned an unexpected key document");
    std::vector<Entry> fresh;
    for (const auto& k : doc["keys"]) {
        if (!k.is_object() || k.value("kty", "") != "RSA" || k.value("kid", "").empty()) continue;
        const std::string alg = k.value("alg", "RS256");
        if (alg != "RS256" || k.value("use", "sig") != "sig") continue;
        const auto n = base64UrlDecode(k.value("n", ""));
        const auto e = base64UrlDecode(k.value("e", ""));
        if (!n || !e) continue;
        fresh.push_back({k.value("kid", ""), RsaPublicKey{*n, *e}});
    }
    if (fresh.empty()) throw GoogleUnavailableError("Google returned no usable signing keys");

    const long long now = clock_();
    const long long ttl = std::clamp(dl.maxAge > 0 ? dl.maxAge : kDefaultTtlSeconds, kMinTtlSeconds, kMaxTtlSeconds);
    keys_ = std::move(fresh);
    fetchedAt_ = now;
    validUntil_ = now + ttl;
}

}  // namespace chronoplan::auth
