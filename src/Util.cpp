/**
 * @file Util.cpp
 * @brief General utility functions implementation
 */

#include "confy/Util.hpp"
#include "confy/DotPath.hpp"

namespace confy {

std::vector<std::pair<std::string, Value>>
flatten_to_dotpaths(const Value& data, const std::string& prefix) {
    std::vector<std::pair<std::string, Value>> result;

    if (!data.is_object()) {
        // Leaf node
        result.emplace_back(prefix, data);
        return result;
    }

    // Recursively flatten object
    for (auto it = data.begin(); it != data.end(); ++it) {
        const auto& key = it.key();
        const auto& value = it.value();

        std::string new_prefix = prefix.empty() ? key : prefix + "." + key;

        if (value.is_object()) {
            // Recurse into nested object
            auto nested = flatten_to_dotpaths(value, new_prefix);
            result.insert(result.end(), nested.begin(), nested.end());
        } else {
            // Leaf value
            result.emplace_back(new_prefix, value);
        }
    }

    return result;
}

Value overrides_dict_to_value(const std::unordered_map<std::string, Value>& overrides) {
    Value result = Value::object();

    for (const auto& [path, value] : overrides) {
        set_by_dot(result, path, value, true);
    }

    return result;
}

} // namespace confy
