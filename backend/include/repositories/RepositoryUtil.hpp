#pragma once
// Type-safe id <-> SQL mapping shared by the repositories.
// Storage form: (id_type, id_value) with id_type 'number' or 'string'.
#include <string>
#include "database/SQLiteStatement.hpp"
#include "models/Validation.hpp"

namespace chronoplan::repositories {

inline const char* idTypeText(const models::Id& id) { return id.numeric ? "number" : "string"; }

// Binds two consecutive parameters: type at `index`, value at `index + 1`.
inline void bindId(database::SQLiteStatement& s, int index, const models::Id& id) {
    s.bindText(index, idTypeText(id));
    s.bindText(index + 1, id.value);
}

// Reads two consecutive columns written by bindId.
inline models::Id readId(const database::SQLiteStatement& s, int col) {
    models::Id id;
    id.numeric = (s.columnText(col) == "number");
    id.value = s.columnText(col + 1);
    return id;
}

inline std::string describeId(const models::Id& id) {
    return id.numeric ? id.value : "\"" + id.value + "\"";
}

// Rejects ids that could never have been produced by models::parseId.
inline void requireValidId(const models::Id& id, const char* what) {
    bool ok = !id.empty();
    if (ok && id.numeric) {
        ok = id.value.size() <= 16 && id.value.find_first_not_of("0123456789") == std::string::npos;
    } else if (ok) {
        ok = models::isValidIdText(id.value);
    }
    if (!ok) throw models::ValidationError(std::string(what) + " is not a valid id");
}

}  // namespace chronoplan::repositories
