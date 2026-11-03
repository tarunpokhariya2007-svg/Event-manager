#pragma once
#include <string>
#include <crow.h>
#include <nlohmann/json.hpp>

namespace chronoplan::api {

// Any-status JSON response with the right Content-Type. O(size of body).
crow::response jsonResponse(int status, const nlohmann::json& body);

// Structured error: {"error": {"code": "...", "message": "..."}}
crow::response errorResponse(int status, const std::string& code, const std::string& message);

}  // namespace chronoplan::api
