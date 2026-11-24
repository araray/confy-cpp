/**
 * @file Schema.hpp
 * @brief Declarative configuration schema and validation (C-04)
 *
 * A schema lists keys by dot-path with a type and optional constraints
 * (enumeration, range, pattern, required), an unknown-key policy, and
 * deprecated or renamed keys. validate() returns every violation; it never
 * throws and never stops at the first problem.
 */
#pragma once

#include "confy/Value.hpp"

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace confy {

struct KeySpec {
    enum class Type { any, string, integer, number, boolean, array, object };
    KeySpec() = default;
    KeySpec(std::string p, Type t = Type::any) : path(std::move(p)), type(t) {}
    std::string path;                      ///< dot-path, e.g. "limits.max_depth"
    Type type = Type::any;
    bool required = false;
    std::vector<Value> one_of;             ///< allowed values (empty: any)
    std::optional<double> min, max;        ///< inclusive range for numbers; length for strings and arrays
    std::optional<std::string> pattern;    ///< ECMAScript regex the whole string must match
    std::optional<std::string> deprecated; ///< message: the key still works but should go
    /// The key moved: its value is accepted for the new path (see apply_renames)
    std::optional<std::string> renamed_to;
    bool open = false;                     ///< object whose own sub-keys are not checked
};

struct Schema {
    enum class Unknown { allow, warn, error };
    std::vector<KeySpec> keys;
    Unknown unknown = Unknown::error;

    /// Keys typed as string (for LoadOptions::string_keys, C-05).
    std::set<std::string> string_keys() const;
};

struct Violation {
    enum class Kind { unknown_key, wrong_type, not_in_enum, out_of_range, pattern_mismatch, missing_required, deprecated, renamed };
    Kind kind;
    std::string path;
    std::string message;
    bool error = true;  ///< false for warnings (deprecated, renamed, unknown under Unknown::warn)
    std::string suggestion;  ///< closest known key for unknown_key, new path for renamed
};

/// Every violation of @p data against @p schema, in key order.
std::vector<Violation> validate(const Value& data, const Schema& schema);

/// Moves values of renamed keys to their new paths (an explicit new value wins).
Value apply_renames(const Value& data, const Schema& schema);

}  // namespace confy
