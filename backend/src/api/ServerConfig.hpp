#pragma once
// HTTP server settings read from the environment (12-factor style).
#include <cstdint>
#include <functional>
#include <string>

namespace chronoplan::api {

struct ServerConfig {
    std::string bindAddress = "127.0.0.1";   // local development default (not reachable from other hosts)
    std::uint16_t port = 8080;
    std::string corsOrigin = "*";            // development only; set an explicit origin when deployed

    bool isLoopback() const { return bindAddress == "127.0.0.1" || bindAddress == "::1" || bindAddress == "localhost"; }
    bool isWildcardCors() const { return corsOrigin == "*"; }

    using Lookup = std::function<const char*(const char*)>;
    // Variables: CHRONOPLAN_BIND_ADDR, CHRONOPLAN_PORT (wins) then PORT, CHRONOPLAN_CORS_ORIGIN.
    // Throws std::invalid_argument on an invalid value (port must be an integer 1..65535).
    static ServerConfig fromLookup(const Lookup& getenvLike);
    static ServerConfig fromEnvironment();
};

}  // namespace chronoplan::api
