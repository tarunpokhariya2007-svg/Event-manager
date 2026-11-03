#pragma once
#include <functional>
#include <stdexcept>
#include <string>
#include <crow.h>
#include <nlohmann/json.hpp>
#include "models/Validation.hpp"

namespace chronoplan::api {

class InvalidJsonError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Parses the request body. Throws InvalidJsonError when it is empty or not JSON.
nlohmann::json parseJsonBody(const crow::request& req);

// URL path segment -> typed id (see API_CONTRACT.md "IDs in URLs"):
//   7        -> numeric id 7        (canonical non-negative integer)
//   %227%22  -> string id "7"       (JSON string literal, URL-encoded quotes)
//   abc, 007 -> string id "abc" / "007"
models::Id parseIdSegment(const std::string& segment, const char* what);

// Runs a handler and maps every exception to the documented JSON error shape.
// Never leaks SQL/internal text.
crow::response guarded(const std::function<crow::response()>& handler);

crow::response noContent();   // 204, empty body

}  // namespace chronoplan::api
