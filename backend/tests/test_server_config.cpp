#include <map>
#include <gtest/gtest.h>
#include "api/ServerConfig.hpp"

using chronoplan::api::ServerConfig;

namespace {
ServerConfig load(std::map<std::string, std::string> env) {
    return ServerConfig::fromLookup([&](const char* k) -> const char* {
        auto it = env.find(k);
        return it == env.end() ? nullptr : it->second.c_str();
    });
}
}  // namespace

TEST(ServerConfig, DefaultsAreLocalDevelopment) {
    const auto c = load({});
    EXPECT_EQ(c.bindAddress, "127.0.0.1");
    EXPECT_EQ(c.port, 8080);
    EXPECT_EQ(c.corsOrigin, "*");
    EXPECT_TRUE(c.isLoopback());
    EXPECT_TRUE(c.isWildcardCors());
}

TEST(ServerConfig, SupportsAllInterfacesAndStandardPortVariable) {
    const auto c = load({{"CHRONOPLAN_BIND_ADDR", "0.0.0.0"}, {"PORT", "10000"}});
    EXPECT_EQ(c.bindAddress, "0.0.0.0");
    EXPECT_EQ(c.port, 10000);
    EXPECT_FALSE(c.isLoopback());
}

TEST(ServerConfig, ChronoplanPortOverridesPort) {
    EXPECT_EQ(load({{"PORT", "10000"}, {"CHRONOPLAN_PORT", "9090"}}).port, 9090);
}

TEST(ServerConfig, EmptyValuesFallBackToDefaults) {
    const auto c = load({{"PORT", ""}, {"CHRONOPLAN_BIND_ADDR", ""}, {"CHRONOPLAN_CORS_ORIGIN", ""}});
    EXPECT_EQ(c.port, 8080);
    EXPECT_EQ(c.bindAddress, "127.0.0.1");
    EXPECT_EQ(c.corsOrigin, "*");
}

TEST(ServerConfig, InvalidPortsAreRejected) {
    for (const char* bad : {"0", "65536", "-1", "80a", "abc", "99999999", " 80"})
        EXPECT_THROW(load({{"PORT", bad}}), std::invalid_argument) << bad;
    EXPECT_THROW(load({{"CHRONOPLAN_PORT", "x"}}), std::invalid_argument);
    EXPECT_EQ(load({{"PORT", "65535"}}).port, 65535);
    EXPECT_EQ(load({{"PORT", "1"}}).port, 1);
}

TEST(ServerConfig, CorsOriginIsConfigurable) {
    const auto c = load({{"CHRONOPLAN_CORS_ORIGIN", "https://app.example.com"}});
    EXPECT_EQ(c.corsOrigin, "https://app.example.com");
    EXPECT_FALSE(c.isWildcardCors());
}
