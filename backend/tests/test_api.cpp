#include <gtest/gtest.h>
#include "api/Routes.hpp"

using namespace chronoplan::api;
using nlohmann::json;

namespace {
const json kDocumentedHealth = {{"status", "online"},
                                {"engine_version", "1.0.0-cxx20"},
                                {"solver_core", "chronoplan-greedy"}};
}

TEST(Health, PayloadMatchesDocumentedStructure) {
    EXPECT_EQ(healthPayload(), kDocumentedHealth);
}

// Runs the real Crow router in-process: same route, middleware and handler as
// the server, without opening a socket.
TEST(Health, EndpointReturnsDocumentedStructure) {
    ChronoApp app;
    configureCors(app);
    registerHealthRoutes(app);
    app.validate();

    crow::request req;
    req.method = crow::HTTPMethod::Get;
    req.url = "/api/v1/schedule/optimize/health";
    crow::response res;
    app.handle_full(req, res);

    ASSERT_EQ(res.code, 200);
    EXPECT_EQ(json::parse(res.body), kDocumentedHealth);
}
