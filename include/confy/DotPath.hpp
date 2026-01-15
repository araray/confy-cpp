/**
 * @file DotPath.hpp
 * @brief Dot-notation path utilities for nested configuration access
 *
 * Provides functions for accessing nested configuration values using
 * dot-separated paths like "database.host" or "logging.handlers.0.type".
 *
 * Follows behavioral rules from CONFY_DESIGN_SPECIFICATION.md:
 * - RULE D1: get() without default raises error if path doesn't exist
 * - RULE D2: get() with default returns default for missing keys, but
 *            still raises error for type mismatches
 * - RULE D3: set() with create_missing=false raises error for missing segments
 * - RULE D4: set() with create_missing=true creates intermediate objects
 * - RULE D5: contains() returns false for missing segments
 * - RULE D6: contains() raises error for type mismatches before final segment
 */

#ifndef CONFY_DOTPATH_HPP
#define CONFY_DOTPATH_HPP

#include "Value.hpp"
#include "Errors.hpp"
#include <string>
#include <vector>
#include <optional>

namespace confy {

/**
 * @brief Split a dot-path into segments
 *
 * @param path Dot-separated path like "a.b.c"
 * @return Vector of segments ["a", "b", "c"]
 *
 * Examples:
 * - "database.host" → ["database", "host"]
 * - "logging.handlers.0.type" → ["logging", "handlers", "0", "type"]
 * - "" → []
 * - "single" → ["single"]
 */
std::vector<std::string> split_dot_path(const std::string& path);

/**
 * @brief Get value from nested structure using dot-path (strict)
 *
 * @param data Source JSON object
 * @param path Dot-separated path
 * @return Pointer to value at path
 * @throws KeyError if any segment not found
 * @throws TypeError if traversal hits non-object before final segment
 *
 * RULE D1: Raises error if path doesn't resolve.
 *
 * Examples:
 * ```cpp
 * Value cfg = {{"db", {{"host", "localhost"}}}};
 * auto* val = get_by_dot(cfg, "db.host");  // OK: points to "localhost"
 * auto* bad = get_by_dot(cfg, "db.port");  // Throws KeyError
 * auto* bad2 = get_by_dot(cfg, "db.host.x"); // Throws TypeError (host is string)
 * ```
 */
const Value* get_by_dot(const Value& data, const std::string& path);

/**
 * @brief Get value from nested structure using dot-path (with default)
 *
 * @param data Source JSON object
 * @param path Dot-separated path
 * @param default_val Value to return if path not found
 * @return Pointer to value at path, or pointer to default_val if not found
 * @throws TypeError if traversal hits non-object before final segment
 *
 * RULE D2: Returns default for missing keys, but still raises TypeError
 * for attempts to traverse into non-containers.
 *
 * Examples:
 * ```cpp
 * Value cfg = {{"db", {{"host", "localhost"}}}};
 * Value def = "default";
 * auto* val = get_by_dot(cfg, "db.port", def);  // Returns &def
 * auto* bad = get_by_dot(cfg, "db.host.x", def); // Throws TypeError
 * ```
 */
const Value* get_by_dot(const Value& data, const std::string& path,
                       const Value& default_val);

/**
 * @brief Set value in nested structure using dot-path
 *
 * @param data Target JSON object (modified in place)
 * @param path Dot-separated path
 * @param value Value to set
 * @param create_missing If true, create intermediate objects as needed;
 *                       if false, raise error for missing intermediates
 * @throws KeyError if create_missing=false and intermediate segment not found
 * @throws TypeError if intermediate exists but is not an object and
 *                  create_missing=false
 *
 * RULE D3: With create_missing=false, errors on missing segments.
 * RULE D4: With create_missing=true, creates missing objects and overwrites
 *          non-objects.
 *
 * Examples:
 * ```cpp
 * Value cfg = Value::object();
 * set_by_dot(cfg, "db.host", "localhost", true);
 * // Result: {"db": {"host": "localhost"}}
 *
 * set_by_dot(cfg, "db.port", 5432, false);
 * // OK: "db" already exists
 *
 * set_by_dot(cfg, "new.key", "val", false);
 * // Throws KeyError: "new" doesn't exist
 * ```
 */
void set_by_dot(Value& data, const std::string& path,
                const Value& value, bool create_missing = true);

/**
 * @brief Check if dot-path exists in nested structure
 *
 * @param data Source JSON object
 * @param path Dot-separated path
 * @return true if path fully resolves, false if any segment missing
 * @throws TypeError if traversal hits non-object before final segment
 *
 * RULE D5: Returns false for missing keys (no error).
 * RULE D6: Raises TypeError for invalid traversal.
 *
 * Examples:
 * ```cpp
 * Value cfg = {{"db", {{"host", "localhost"}}}};
 * contains_dot(cfg, "db.host");     // true
 * contains_dot(cfg, "db.port");     // false (no error)
 * contains_dot(cfg, "db.host.x");   // Throws TypeError (can't traverse into string)
 * ```
 */
bool contains_dot(const Value& data, const std::string& path);

/**
 * @brief Join path segments with dots
 *
 * Utility function for constructing dot-paths.
 *
 * @param segments Vector of path segments
 * @return Dot-joined path string
 *
 * Examples:
 * - ["a", "b", "c"] → "a.b.c"
 * - [] → ""
 * - ["single"] → "single"
 */
std::string join_dot_path(const std::vector<std::string>& segments);

} // namespace confy

#endif // CONFY_DOTPATH_HPP
