/**
 * @file Merge.cpp
 * @brief Implementation of deep merge
 */

#include "confy/Merge.hpp"

namespace confy {

Value deep_merge(const Value& base, const Value& override_val) {
    // If override is null, return base (null doesn't override)
    if (override_val.is_null()) {
        return base;
    }

    // If base is null, return override
    if (base.is_null()) {
        return override_val;
    }

    // RULE P2: Both are objects → recursive merge
    if (base.is_object() && override_val.is_object()) {
        Value result = base; // Start with base

        // Merge in all keys from override
        for (auto it = override_val.begin(); it != override_val.end(); ++it) {
            const auto& key = it.key();
            const auto& override_value = it.value();

            if (result.contains(key)) {
                // Key exists in both: recursively merge
                result[key] = deep_merge(result[key], override_value);
            } else {
                // Key only in override: add it
                result[key] = override_value;
            }
        }

        return result;
    }

    // RULE P3: Non-object replaces everything
    // This handles:
    // - Scalar overrides object
    // - Object overrides scalar
    // - Array overrides anything
    // - Anything overrides array
    return override_val;
}

Value deep_merge_all(const std::vector<Value>& sources) {
    if (sources.empty()) {
        return Value::object();
    }

    Value result = sources[0];
    for (size_t i = 1; i < sources.size(); ++i) {
        result = deep_merge(result, sources[i]);
    }

    return result;
}

} // namespace confy
