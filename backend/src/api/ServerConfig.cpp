#include "api/ServerConfig.hpp"
#include <cstdlib>
#include <stdexcept>

namespace chronoplan::api {

namespace {
std::uint16_t parsePort(const char* name, const char* text) {
    const std::string s = text;
    if (s.empty() || s.size() > 5 || s.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument(std::string(name) + " must be an integer between 1 and 65535");
    const long v = std::stol(s);
    if (v < 1 || v > 65535)
        throw std::invalid_argument(std::string(name) + " must be an integer between 1 and 65535");
    return static_cast<std::uint16_t>(v);
}
}  // namespace

ServerConfig ServerConfig::fromLookup(const Lookup& get) {
    ServerConfig cfg;
    if (const char* b = get("CHRONOPLAN_BIND_ADDR"); b && *b) cfg.bindAddress = b;
    if (const char* p = get("CHRONOPLAN_PORT"); p && *p) cfg.port = parsePort("CHRONOPLAN_PORT", p);
    else if (const char* q = get("PORT"); q && *q) cfg.port = parsePort("PORT", q);   // PaaS convention
    if (const char* o = get("CHRONOPLAN_CORS_ORIGIN"); o && *o) cfg.corsOrigin = o;
    return cfg;
}

ServerConfig ServerConfig::fromEnvironment() {
    return fromLookup([](const char* name) { return std::getenv(name); });
}

}  // namespace chronoplan::api
