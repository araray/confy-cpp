/**
 * @file Util.hpp
 * @brief General utility functions
 *
 * Miscellaneous helper functions that don't fit in other modules.
 */

#ifndef CONFY_UTIL_HPP
#define CONFY_UTIL_HPP

#include "confy/Value.hpp"
#include <string>
#include <vector>

namespace confy {

/**
 * @brief Flatten a nested Value object into dot-path keys
 *
 * Converts {"a": {"b": 1}} into {"a.b": 1}
 * Useful for debugging and comparison.
 *
 * @param data Nested Value object
 * @param prefix Current path prefix (for recursion)
 * @return Vector of (dot-path, value) pairs
 */
std::vector<std::pair<std::string, Value>>
flatten_to_dotpaths(const Value& data, const std::string& prefix = "");

/**
 * @brief Convert overrides dictionary to nested Value object
 *
 * Converts {"a.b": 1, "c.d": 2} into {"a": {"b": 1}, "c": {"d": 2}}
 *
 * @param overrides Map of dot-path to value
 * @return Nested Value object
 */
Value overrides_dict_to_value(const std::unordered_map<std::string, Value>& overrides);

} // namespace confy

#endif // CONFY_UTIL_HPP
