#pragma once
#include <cstdint>

namespace chronoplan::repositories {

// users.id. Every repository is constructed for exactly one user and every SQL statement it
// runs is filtered by that id, so a repository can never read or change another user's rows.
using UserId = std::int64_t;

}  // namespace chronoplan::repositories
