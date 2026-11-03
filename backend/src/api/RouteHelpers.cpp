#include "api/RouteHelpers.hpp"
#include <iostream>
#include "api/ApiResponse.hpp"
#include "auth/AuthErrors.hpp"
#include "database/Database.hpp"
#include "repositories/RepositoryErrors.hpp"

namespace chronoplan::api {

nlohmann::json parseJsonBody(const crow::request& req) {
    nlohmann::json j = nlohmann::json::parse(req.body, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) throw InvalidJsonError("request body is not valid JSON");
    return j;
}

namespace {

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string percentDecode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] != '%') { out += in[i]; continue; }
        const int hi = i + 2 < in.size() ? hexValue(in[i + 1]) : -1;
        const int lo = i + 2 < in.size() ? hexValue(in[i + 2]) : -1;
        if (hi < 0 || lo < 0) throw models::ValidationError("id in URL has invalid percent-encoding");
        out += static_cast<char>(hi * 16 + lo);
        i += 2;
    }
    return out;
}

bool isCanonicalInteger(const std::string& s) {
    if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos) return false;
    return s == "0" || s[0] != '0';
}

}  // namespace

models::Id parseIdSegment(const std::string& segment, const char* what) {
    const std::string s = percentDecode(segment);
    if (s.empty()) throw models::ValidationError(std::string(what) + " must not be empty");
    if (s.front() == '"') {
        const nlohmann::json j = nlohmann::json::parse(s, nullptr, false);
        if (!j.is_string()) throw models::ValidationError(std::string(what) + " is not a valid quoted string id");
        return models::parseId(j, what);
    }
    if (isCanonicalInteger(s)) {
        if (s.size() > 16) throw models::ValidationError(std::string(what) + " is too large");
        return models::parseId(nlohmann::json(std::stoull(s)), what);
    }
    return models::parseId(nlohmann::json(s), what);
}

crow::response guarded(const std::function<crow::response()>& handler) {
    try {
        return handler();
    } catch (const auth::UnauthorizedError& e) {
        return errorResponse(401, "unauthorized", e.what());
    } catch (const auth::ForbiddenError& e) {
        return errorResponse(403, "forbidden", e.what());
    } catch (const InvalidJsonError& e) {
        return errorResponse(400, "invalid_json", e.what());
    } catch (const models::ValidationError& e) {
        return errorResponse(400, "validation_error", e.what());
    } catch (const repositories::NotFoundError& e) {
        return errorResponse(404, "not_found", e.what());
    } catch (const repositories::CircularDependencyError& e) {
        return errorResponse(409, "circular_dependency", e.what());
    } catch (const repositories::ConflictError& e) {
        return errorResponse(409, "conflict", e.what());
    } catch (const database::DatabaseError& e) {
        if (e.isUnavailable())
            return errorResponse(503, "database_unavailable", "the database is temporarily unavailable");
        return errorResponse(500, "internal_error", "internal server error");
    } catch (const std::exception& e) {
        std::cerr << "[api] unexpected error: " << e.what() << '\n';
        return errorResponse(500, "internal_error", "internal server error");
    } catch (...) {
        return errorResponse(500, "internal_error", "internal server error");
    }
}

crow::response noContent() { return crow::response(204); }

}  // namespace chronoplan::api
