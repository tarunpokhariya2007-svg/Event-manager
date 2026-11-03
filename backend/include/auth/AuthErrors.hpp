#pragma once
#include <stdexcept>
#include "repositories/RepositoryErrors.hpp"

namespace chronoplan::auth {

// 401 {code: "unauthorized"}: no/invalid/expired session.
class UnauthorizedError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
// 401 {code: "invalid_credentials"}: deliberately identical for unknown e-mail and wrong password.
class InvalidCredentialsError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
// 403 {code: "forbidden"}: CSRF / origin check failed.
class ForbiddenError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
// 429 {code: "too_many_attempts"}
class TooManyAttemptsError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
// 409 {code: "email_taken"} / {code: "account_exists"}
class EmailTakenError : public repositories::ConflictError { public: using repositories::ConflictError::ConflictError; };
class AccountExistsError : public repositories::ConflictError { public: using repositories::ConflictError::ConflictError; };
// 401 {code: "invalid_google_token"}
class InvalidGoogleTokenError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
// 503 {code: "google_unavailable"}: could not fetch Google's signing keys.
class GoogleUnavailableError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
// 503 {code: "google_not_configured"}
class GoogleNotConfiguredError : public std::runtime_error { public: using std::runtime_error::runtime_error; };

}  // namespace chronoplan::auth
