/**
 * @file Loader.hpp
 * @brief File loading utilities (Phase 2)
 *
 * Implements loading configuration from:
 * - JSON files (using nlohmann::json)
 * - TOML files (using toml++)
 * - .env files (custom parser)
 *
 * RULE F1-F8: File format behavior from design specification.
 *
 * @see CONFY_DESIGN_SPECIFICATION.md Section 3.6
 */

#ifndef CONFY_LOADER_HPP
#define CONFY_LOADER_HPP

#include "confy/Value.hpp"
#include <string>
#include <optional>
#include <vector>
#include <utility>

namespace confy {

// ============================================================================
// JSON File Loading (RULE F1-F2)
// ============================================================================

/**
 * @brief Load configuration from a JSON file.
 *
 * RULE F1: Standard JSON parsing with UTF-8 encoding.
 * RULE F2: Parse errors produce descriptive exceptions.
 *
 * @param path Path to the JSON file
 * @return Parsed Value object
 * @throws FileNotFoundError if file doesn't exist
 * @throws ConfigParseError if JSON syntax is invalid
 */
Value load_json_file(const std::string& path);

// ============================================================================
// TOML File Loading (RULE F3-F5)
// ============================================================================

/**
 * @brief Load configuration from a TOML file.
 *
 * RULE F3: TOML 1.0 compliant parsing.
 * RULE F4: TOML sections map to nested objects.
 * RULE F5: Key promotion from sections to root (if root key exists in defaults).
 *
 * @param path Path to the TOML file
 * @param defaults Optional defaults for key promotion logic
 * @return Parsed Value object
 * @throws FileNotFoundError if file doesn't exist
 * @throws ConfigParseError if TOML syntax is invalid
 */
Value load_toml_file(const std::string& path, const Value& defaults = Value::object());

/**
 * @brief Convert TOML table to nlohmann::json Value.
 *
 * Internal helper for TOML loading.
 * Recursively converts toml++ types to JSON-compatible types.
 *
 * @param table The TOML table to convert
 * @return Equivalent JSON Value
 */
Value toml_to_json(const void* toml_table);

// ============================================================================
// Auto-detect File Loading (RULE F6-F8)
// ============================================================================

/**
 * @brief Load configuration from file, auto-detecting format by extension.
 *
 * RULE F6: If path is empty -> return empty object (no file loaded).
 * RULE F7: If file doesn't exist -> throw FileNotFoundError.
 * RULE F8: Detect format by extension (.json -> JSON, .toml -> TOML).
 *
 * @param path Path to config file (empty string = no file)
 * @param defaults Optional defaults for TOML key promotion
 * @return Parsed Value object, or empty object if path is empty
 * @throws FileNotFoundError if path is non-empty and file doesn't exist
 * @throws ConfigParseError if file has syntax errors
 * @throws std::runtime_error if extension is not .json or .toml
 */
Value load_config_file(const std::string& path, const Value& defaults = Value::object());

/**
 * @brief Get file extension (lowercase).
 *
 * @param path File path
 * @return Extension including the dot (e.g., ".json"), or empty if none
 */
std::string get_file_extension(const std::string& path);

// ============================================================================
// .env File Loading (RULE P4)
// ============================================================================

/**
 * @brief Result of parsing a .env file.
 */
struct DotenvResult {
    /// Key-value pairs from the .env file
    std::vector<std::pair<std::string, std::string>> entries;

    /// Path that was loaded (for diagnostics)
    std::string loaded_path;

    /// Whether any file was found and parsed
    bool found = false;
};

/**
 * @brief Parse a .env file.
 *
 * Minimal implementation following Python's python-dotenv behavior:
 * - Parse KEY=value format
 * - Handle comments (# at start of line)
 * - Support quoted values (single and double quotes)
 * - Handle inline comments (# after value)
 * - Support export prefix (export KEY=value)
 *
 * DOES NOT modify the process environment.
 *
 * @param path Path to .env file
 * @return DotenvResult with parsed entries
 */
DotenvResult parse_dotenv_file(const std::string& path);

/**
 * @brief Search for .env file starting from current directory.
 *
 * Searches current directory and parent directories for a file named ".env".
 * Similar to python-dotenv's find_dotenv(usecwd=True).
 *
 * @param start_dir Starting directory (empty = current working directory)
 * @return Path to found .env file, or empty string if not found
 */
std::string find_dotenv(const std::string& start_dir = "");

/**
 * @brief Load .env file into process environment.
 *
 * RULE P4: Does NOT override environment variables that already exist
 * (unless override_existing is true).
 *
 * @param path Path to .env file (empty = auto-search using find_dotenv)
 * @param override_existing If true, overwrite existing env vars
 * @return true if a file was found and loaded, false otherwise
 */
bool load_dotenv_file(const std::string& path = "", bool override_existing = false);

/**
 * @brief Set environment variable.
 *
 * Cross-platform wrapper for setting environment variables.
 *
 * @param name Variable name
 * @param value Variable value
 * @param overwrite If true, overwrite existing; if false, only set if not exists
 * @return true if variable was set, false if existed and overwrite=false
 */
bool set_env_var(const std::string& name, const std::string& value, bool overwrite = true);

/**
 * @brief Get environment variable value.
 *
 * @param name Variable name
 * @return Value if exists, nullopt otherwise
 */
std::optional<std::string> get_env_var(const std::string& name);

/**
 * @brief Check if environment variable exists.
 *
 * @param name Variable name
 * @return true if variable is set (even if empty), false otherwise
 */
bool has_env_var(const std::string& name);

} // namespace confy

#endif // CONFY_LOADER_HPP
