#include "api/ApiResponse.hpp"

namespace chronoplan::api {

crow::response jsonResponse(int status, const nlohmann::json& body) {
    crow::response res(status, body.dump());
    res.set_header("Content-Type", "application/json");
    return res;
}

crow::response errorResponse(int status, const std::string& code, const std::string& message) {
    return jsonResponse(status, {{"error", {{"code", code}, {"message", message}}}});
}

}  // namespace chronoplan::api
