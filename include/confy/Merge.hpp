/**
 * @file Merge.hpp
 * @brief Deep merge utilities for configuration layers
 *
 * Implements precedence rules P2 and P3 from CONFY_DESIGN_SPECIFICATION.md:
 * - P2: Deep merge applies only to object types
 * - P3: Scalars replace objects entirely
 */

#ifndef CONFY_MERGE_HPP
#define CONFY_MERGE_HPP

#include "confy/Value.hpp"

namespace confy {

/**
 * @brief Deep merge two JSON objects
 *
 * Merging rules:
 * - Both objects: Recursive merge (keys from both are combined)
 * - Non-object overrides object: Override value replaces base entirely
 * - Object overrides non-object: Override object replaces base entirely
 * - Non-objects: Override replaces base
 *
 * Follows RULE P2 and P3 from spec.
 *
 * @param base Base object (lower precedence)
 * @param override_val Override object (higher precedence)
 * @return Merged result
 *
 * Examples:
 * ```cpp
 * // RULE P2: Nested objects are merged
 * Value base = {{"db", {{"host", "a"}, {"port", 1}}}};
 * Value over = {{"db", {{"port", 2}}}};
 * auto result = deep_merge(base, over);
 * // Result: {"db": {"host": "a", "port": 2}}
 *
 * // RULE P3: Scalar replaces object
 * Value base2 = {{"db", {{"host", "a"}}}};
 * Value over2 = {{"db", "string"}};
 * auto result2 = deep_merge(base2, over2);
 * // Result: {"db": "string"}
 *
 * // RULE P3: Object replaces scalar
 * Value base3 = {{"db", "string"}};
 * Value over3 = {{"db", {{"host", "a"}}}};
 * auto result3 = deep_merge(base3, over3);
 * // Result: {"db": {"host": "a"}}
 * ```
 */
Value deep_merge(const Value& base, const Value& override_val);

/**
 * @brief Deep merge multiple configuration sources in order
 *
 * Applies each source in sequence from lowest to highest precedence.
 *
 * @param sources Vector of Value objects to merge (in precedence order)
 * @return Merged result
 *
 * Example:
 * ```cpp
 * Value defaults = {{"a": 1}, {"b": 2}};
 * Value file_config = {{"b": 3}, {"c": 4}};
 * Value env_overrides = {{"c": 5}};
 *
 * auto result = deep_merge_all({defaults, file_config, env_overrides});
 * // Result: {"a": 1, "b": 3, "c": 5}
 * ```
 */
Value deep_merge_all(const std::vector<Value>& sources);

} // namespace confy

#endif // CONFY_MERGE_HPP
