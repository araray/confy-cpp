/**
 * @file EnvMapper.hpp
 * @brief Environment variable collection and mapping (Phase 2)
 *
 * Implements the complex environment variable mapping logic:
 * 1. Prefix filtering (RULE E1-E3)
 * 2. Underscore transformation (RULE E4)
 * 3. Remapping against base structure (RULE E5-E7)
 * 4. System variable exclusion
 *
 * This is the most complex component in the entire confy project.
 * The C++ implementation MUST exactly match Python's behavior.
 *
 * @see CONFY_DESIGN_SPECIFICATION.md Section 3.2
 */

#ifndef CONFY_ENVMAPPER_HPP
#define CONFY_ENVMAPPER_HPP

#include "confy/Value.hpp"
#include <string>
#include <vector>
#include <set>
#include <optional>
#include <utility>

namespace confy {

/**
 * @brief System environment variable prefixes to exclude when prefix is empty.
 *
 * These prefixes (or exact matches for single-char entries) are skipped
 * when collecting environment variables with an empty prefix ("").
 * This prevents picking up unrelated system variables.
 *
 * RULE E2: System variable exclusion list.
 */
extern const std::vector<std::string> SYSTEM_VAR_PREFIXES;

/**
 * @brief Check if an environment variable name should be skipped as a system variable.
 *
 * @param var_name Environment variable name (case-insensitive comparison)
 * @return true if the variable should be excluded, false otherwise
 */
bool is_system_variable(const std::string& var_name);

/**
 * @brief Transform an environment variable name using underscore mapping rules.
 *
 * Implements RULE E4:
 * 1. Convert to lowercase
 * 2. Replace `__` (double underscore) with temporary marker
 * 3. Replace `_` (single underscore) with `.`
 * 4. Replace temporary marker back to `_`
 *
 * Examples:
 *   - DATABASE_HOST -> database.host
 *   - FEATURE_FLAGS__BETA -> feature_flags.beta
 *   - A__B__C_D -> a_b_c.d
 *
 * @param name The environment variable name (after prefix removal)
 * @return The transformed dot-path string
 */
std::string transform_env_name(const std::string& name);

/**
 * @brief Strip prefix from environment variable name.
 *
 * Handles case-insensitive prefix matching.
 * If prefix is "MYAPP", strips "MYAPP_" from "MYAPP_DATABASE_HOST".
 *
 * @param var_name Full environment variable name
 * @param prefix The prefix to strip (without trailing underscore)
 * @return The name without prefix, or empty string if no match
 */
std::string strip_prefix(const std::string& var_name, const std::string& prefix);

/**
 * @brief Collect environment variables matching the prefix filter.
 *
 * Implements RULE E1-E3:
 * - E1: Non-empty prefix filters to {PREFIX}_* (case-insensitive)
 * - E2: Empty prefix ("") includes most vars, excludes system vars
 * - E3: nullopt disables env loading entirely
 *
 * @param prefix Optional prefix filter:
 *               - Non-empty string: filter by prefix
 *               - Empty string "": include non-system vars
 *               - nullopt: return empty vector (disabled)
 * @return Vector of (name, value) pairs that passed the filter.
 *         Names are the original full env var names.
 */
std::vector<std::pair<std::string, std::string>>
collect_env_vars(const std::optional<std::string>& prefix);

/**
 * @brief Flatten a nested Value object into dot-path keys.
 *
 * Implements RULE E5: Extracts all paths from a nested structure.
 *
 * Example:
 *   {"database": {"host": "x", "port": 5432}, "debug": true}
 *   -> {"database.host", "database.port", "debug", "database"}
 *
 * @param data The Value to flatten
 * @param prefix Optional prefix for recursion (internal use)
 * @return Set of all dot-path keys in the structure
 */
std::set<std::string> flatten_keys(const Value& data, const std::string& prefix = "");

/**
 * @brief Remap an env-derived dot-path to match base structure keys.
 *
 * Implements RULE E6-E7: The smart remapping logic.
 *
 * Algorithm:
 * 1. If exact match in base_keys -> use as-is
 * 2. Try heuristic remapping for keys with underscores:
 *    - "feature.flags.beta" -> "feature_flags.beta" if "feature_flags" exists
 * 3. If still no match:
 *    - conservative mode (prefix="" && load_dotenv): discard
 *    - otherwise: keep as-is or fallback to flat key
 *
 * @param dot_path The transformed dot-path from environment variable
 * @param base_keys Set of valid dot-paths from defaults + config file
 * @param prefix The prefix used (for fallback behavior)
 * @param load_dotenv Whether .env loading was enabled (for conservative mode)
 * @return Remapped key, or empty string if should be discarded
 */
std::string remap_env_key(
    const std::string& dot_path,
    const std::set<std::string>& base_keys,
    const std::optional<std::string>& prefix,
    bool load_dotenv
);

/**
 * @brief Convert collected env vars to nested Value structure.
 *
 * Internal helper that:
 * 1. Strips prefix from each variable name
 * 2. Transforms name using underscore rules
 * 3. Parses value using parse_value()
 * 4. Sets value at dot-path in result structure
 *
 * @param env_vars Collected environment variables (name, value pairs)
 * @param prefix The prefix used for filtering (to strip)
 * @return Nested Value object with env var data
 */
Value env_vars_to_nested(
    const std::vector<std::pair<std::string, std::string>>& env_vars,
    const std::optional<std::string>& prefix
);

/**
 * @brief Remap and flatten environment data against base structure.
 *
 * Implements the full remapping pipeline:
 * 1. Flatten nested_env_data to (dot_path, value) pairs
 * 2. For each pair, attempt to remap against base_keys
 * 3. Apply fallback rules based on context
 * 4. Return flat map of final_key -> value
 *
 * @param nested_env_data Nested structure from env_vars_to_nested()
 * @param defaults_data The defaults Value
 * @param file_data The loaded config file Value
 * @param prefix The prefix used for filtering
 * @param load_dotenv Whether .env loading was enabled
 * @return Flat map of dot-path keys to values
 */
std::vector<std::pair<std::string, Value>> remap_and_flatten_env_data(
    const Value& nested_env_data,
    const Value& defaults_data,
    const Value& file_data,
    const std::optional<std::string>& prefix,
    bool load_dotenv
);

/**
 * @brief Full environment variable loading pipeline.
 *
 * High-level function that orchestrates:
 * 1. collect_env_vars() - Filter by prefix
 * 2. env_vars_to_nested() - Transform and parse
 * 3. remap_and_flatten_env_data() - Smart remapping
 * 4. Structure into final Value
 *
 * @param prefix Optional prefix filter (nullopt disables)
 * @param base_structure Combined defaults + file structure for remapping
 * @param defaults_data Original defaults
 * @param file_data Original file data
 * @param load_dotenv Whether .env loading was enabled
 * @return Value object with all environment overrides
 */
Value load_env_vars(
    const std::optional<std::string>& prefix,
    const Value& base_structure,
    const Value& defaults_data,
    const Value& file_data,
    bool load_dotenv
);

} // namespace confy

#endif // CONFY_ENVMAPPER_HPP
