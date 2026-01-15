/**
 * @file Value.hpp
 * @brief Value type for configuration data
 *
 * Uses nlohmann::json as the underlying value model to support:
 * - Null
 * - Bool (true | false)
 * - Integer (int64_t)
 * - Float (double)
 * - String (std::string, UTF-8)
 * - Array ([Value, ...])
 * - Object ({String: Value, ...})
 */

#ifndef CONFY_VALUE_HPP
#define CONFY_VALUE_HPP

#include <nlohmann/json.hpp>
#include <string>

namespace confy {

/**
 * @brief JSON-like value type for configuration
 *
 * This is an alias for nlohmann::json, providing the full JSON
 * value model as specified in the design document.
 *
 * Supports:
 * - Type queries: is_null(), is_boolean(), is_number_integer(),
 *   is_number_float(), is_string(), is_array(), is_object()
 * - Type conversion: get<T>()
 * - Container operations: size(), empty(), operator[], at()
 * - Iteration: begin(), end()
 * - Comparison: ==, !=, <, <=, >, >=
 *
 * See nlohmann::json documentation for complete API.
 */
using Value = nlohmann::json;

/**
 * @brief Get human-readable type name for a Value
 * @param val The value to inspect
 * @return Type name string (e.g., "null", "boolean", "integer", "float",
 *         "string", "array", "object")
 */
inline std::string type_name(const Value& val) {
    if (val.is_null()) return "null";
    if (val.is_boolean()) return "boolean";
    if (val.is_number_integer()) return "integer";
    if (val.is_number_float()) return "float";
    if (val.is_string()) return "string";
    if (val.is_array()) return "array";
    if (val.is_object()) return "object";
    return "unknown";
}

/**
 * @brief Check if value is a container (array or object)
 * @param val The value to check
 * @return true if val is array or object, false otherwise
 */
inline bool is_container(const Value& val) {
    return val.is_array() || val.is_object();
}

} // namespace confy

#endif // CONFY_VALUE_HPP
