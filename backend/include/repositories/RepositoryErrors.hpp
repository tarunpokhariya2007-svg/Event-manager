#pragma once
#include <stdexcept>

namespace chronoplan::repositories {

// Model-level problems throw models::ValidationError; storage-level problems throw
// database::DatabaseError. These three describe the outcome of a valid request.
class NotFoundError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
class ConflictError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
class CircularDependencyError : public ConflictError { public: using ConflictError::ConflictError; };

}  // namespace chronoplan::repositories
