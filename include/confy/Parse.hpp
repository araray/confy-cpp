/**
 * @file Parse.hpp
 * @brief String-to-Value type parsing utilities
 *
 * Implements the type parsing rules (T1-T7) from CONFY_DESIGN_SPECIFICATION.md
 * for converting string values from environment variables and CLI overrides
 * into properly typed Values.
 *
 * Parsing order (first match wins):
 * - T1: Boolean ("true", "false" - case insensitive)
 * - T2: Null ("null" - case insensitive)
 * - T3: Integer (matches ^-?[0-9]+$)
 * - T4: Float (matches ^-?[0-9]+\.[0-9]+([eE][+-]?[0-9]+)?$)
 * - T5: JSON Compound ({...} or [...])
 * - T6: Quoted String ("...")
 * - T7: Raw String (fallback)
 */

#ifndef CONFY_PARSE_HPP
#define CONFY_PARSE_HPP

#include "confy/Value.hpp"
#include <string>

namespace confy {

/**
 * @brief Parse string value to appropriate type
 *
 * Follows Python confy's _parse_value rules:
 * - "true"/"false" (case-insensitive) → boolean
 * - "null" (case-insensitive) → null
 * - Integer pattern → int64
 * - Float pattern → double
 * - JSON objects/arrays → parsed JSON
 * - Quoted strings → unquoted string
 * - Everything else → raw string
 *
 * @param str Input string to parse
 * @return Parsed Value with appropriate type
 *
 * Examples:
 * ```cpp
 * parse_value("true")       // → true (boolean)
 * parse_value("FALSE")      // → false (boolean)
 * parse_value("null")       // → null
 * parse_value("42")         // → 42 (integer)
 * parse_value("-17")        // → -17 (integer)
 * parse_value("3.14")       // → 3.14 (float)
 * parse_value("-2.5e10")    // → -2.5e10 (float)
 * parse_value("{\"a\":1}")  // → {"a": 1} (object)
 * parse_value("[1,2,3]")    // → [1, 2, 3] (array)
 * parse_value("\"hello\"")  // → "hello" (string, unquoted)
 * parse_value("hello")      // → "hello" (string)
 * parse_value("")           // → "" (empty string)
 * ```
 */
Value parse_value(const std::string& str);

} // namespace confy

#endif // CONFY_PARSE_HPP
