/**
 * @file Config.hpp
 * @brief Main configuration class (Phase 3)
 *
 * This header defines the Config class - the primary user-facing API for
 * the confy configuration management library.
 *
 * The Config class orchestrates loading from multiple sources with
 * defined precedence:
 *   1. Defaults (lowest)
 *   2. Config file (JSON/TOML)
 *   3. .env file variables
 *   4. Environment variables
 *   5. Overrides (highest)
 *
 * Implements design spec rules:
 * - RULE D1-D6: Dot-path access semantics
 * - RULE M1-M3: Mandatory key validation
 * - RULE P1: Precedence ordering
 *
 * @copyright (c) 2026. MIT License.
 */

#ifndef CONFY_CONFIG_HPP
#define CONFY_CONFIG_HPP

#include "confy/Value.hpp"
#include "confy/Errors.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <functional>

namespace confy {

/**
 * @brief Configuration loading options
 *
 * Specifies all parameters for the Config::load() operation.
 * Each field corresponds to a source in the precedence chain.
 */
struct LoadOptions {
    /**
     * @brief Path to JSON or TOML config file
     *
     * If empty, no file is loaded. File format is auto-detected by extension.
     * - ".json" → JSON parser
     * - ".toml" → TOML parser with key promotion
     *
     * @throws FileNotFoundError if path is non-empty and file doesn't exist
     * @throws ConfigParseError if file has syntax errors
     */
    std::string file_path;

    /**
     * @brief Environment variable prefix for filtering
     *
     * Controls which environment variables are loaded:
     * - Non-empty string (e.g., "MYAPP"): Filter to {PREFIX}_* variables
     * - Empty string "": Include most env vars, exclude 120+ system prefixes
     * - std::nullopt: Disable environment variable loading entirely
     *
     * Prefix comparison is case-insensitive.
     */
    std::optional<std::string> prefix = std::nullopt;

    /**
     * @brief Whether to load .env file
     *
     * If true, searches for and loads a .env file before processing
     * environment variables. The .env file does NOT override existing
     * environment variables (RULE P4).
     */
    bool load_dotenv_file = true;

    /**
     * @brief Explicit path to .env file
     *
     * If non-empty, uses this specific path instead of auto-searching.
     * If empty and load_dotenv_file is true, searches current directory
     * and parent directories for ".env".
     */
    std::string dotenv_path;

    /**
     * @brief Default values (lowest precedence)
     *
     * Base configuration that all other sources override.
     * Should be a JSON object (map).
     */
    Value defaults = Value::object();

    /**
     * @brief Final overrides (highest precedence)
     *
     * Map of dot-paths to values. Applied last, overriding all other sources.
     * Keys are dot-notation paths like "database.host".
     * Values can be any JSON-compatible type.
     *
     * Example:
     * @code
     * opts.overrides = {
     *     {"database.port", 5433},
     *     {"debug.enabled", true}
     * };
     * @endcode
     */
    std::unordered_map<std::string, Value> overrides;

    /**
     * @brief Mandatory dot-paths that must exist after merge
     *
     * After all sources are merged, each path in this list is checked.
     * If ANY mandatory key is missing, MissingMandatoryConfig is thrown
     * containing ALL missing keys (not just the first).
     *
     * Example:
     * @code
     * opts.mandatory = {"database.host", "api.key"};
     * @endcode
     */
    std::vector<std::string> mandatory;
};

/**
 * @brief Configuration container with dot-notation access
 *
 * The Config class is the primary interface for loading and accessing
 * configuration data. It provides:
 * - Static factory method for loading from multiple sources
 * - Dot-notation get/set/contains operations
 * - Serialization to JSON and TOML formats
 * - Mandatory key validation
 *
 * Thread-safety: NOT thread-safe. External synchronization required
 * for concurrent access to the same Config instance.
 *
 * Example usage:
 * @code
 * LoadOptions opts;
 * opts.file_path = "config.toml";
 * opts.prefix = "MYAPP";
 * opts.defaults = {{"database", {{"port", 5432}}}};
 * opts.mandatory = {"database.host"};
 *
 * Config cfg = Config::load(opts);
 *
 * std::string host = cfg.get<std::string>("database.host");
 * int port = cfg.get<int>("database.port", 5432);
 * @endcode
 */
class Config {
public:
    /**
     * @brief Default constructor (empty config)
     *
     * Creates an empty configuration object. Use Config::load() for
     * normal initialization with source loading.
     */
    Config() = default;

    /**
     * @brief Construct from existing JSON value
     *
     * @param data Initial configuration data (should be object type)
     */
    explicit Config(const Value& data);

    /**
     * @brief Copy constructor
     */
    Config(const Config& other) = default;

    /**
     * @brief Move constructor
     */
    Config(Config&& other) noexcept = default;

    /**
     * @brief Copy assignment
     */
    Config& operator=(const Config& other) = default;

    /**
     * @brief Move assignment
     */
    Config& operator=(Config&& other) noexcept = default;

    /**
     * @brief Default destructor
     */
    ~Config() = default;

    // =========================================================================
    // Static Factory
    // =========================================================================

    /**
     * @brief Load configuration from multiple sources
     *
     * Orchestrates loading from all sources in precedence order:
     * 1. Start with defaults
     * 2. Merge config file (if specified)
     * 3. Load .env file (if enabled)
     * 4. Merge environment variables (if prefix set)
     * 5. Apply overrides
     * 6. Validate mandatory keys
     *
     * @param opts Loading options specifying all sources
     * @return Fully configured Config instance
     *
     * @throws FileNotFoundError if file_path specified but not found
     * @throws ConfigParseError if config file has syntax errors
     * @throws MissingMandatoryConfig if mandatory keys are missing
     */
    static Config load(const LoadOptions& opts);

    // =========================================================================
    // Value Access (Dot-Path)
    // =========================================================================

    /**
     * @brief Get value at dot-path with type conversion
     *
     * Retrieves the value at the specified dot-path and converts it
     * to the requested type. Returns the default value if the path
     * does not exist.
     *
     * @tparam T Expected type (std::string, int, bool, double, etc.)
     * @param path Dot-separated path like "database.host"
     * @param default_val Value to return if path not found
     * @return Value at path converted to T, or default_val
     *
     * @throws TypeError if path exists but value cannot convert to T
     * @throws TypeError if traversal encounters non-object (RULE D2)
     *
     * Example:
     * @code
     * int port = cfg.get<int>("database.port", 5432);
     * std::string host = cfg.get<std::string>("database.host", "localhost");
     * @endcode
     */
    template<typename T>
    T get(const std::string& path, const T& default_val) const;

    /**
     * @brief Get value at dot-path (strict, no default)
     *
     * Retrieves the raw Value at the specified dot-path.
     * Throws if the path does not exist.
     *
     * @param path Dot-separated path
     * @return Value at path
     *
     * @throws KeyError if path not found (RULE D1)
     * @throws TypeError if traversal encounters non-object (RULE D1)
     *
     * Example:
     * @code
     * Value db = cfg.get("database");
     * std::string host = db["host"].get<std::string>();
     * @endcode
     */
    Value get(const std::string& path) const;

    /**
     * @brief Get value at dot-path with optional return
     *
     * Retrieves the value at path, returning std::nullopt if not found.
     * Does not throw for missing keys.
     *
     * @param path Dot-separated path
     * @return std::optional containing Value, or std::nullopt if missing
     *
     * @throws TypeError if traversal encounters non-object
     */
    std::optional<Value> get_optional(const std::string& path) const;

    /**
     * @brief Set value at dot-path
     *
     * Sets the value at the specified dot-path. Creates intermediate
     * objects if they don't exist (when create_missing is true).
     *
     * @param path Dot-separated path
     * @param value Value to set
     * @param create_missing Create intermediate objects if true (default)
     *
     * @throws KeyError if create_missing=false and path doesn't exist (RULE D3)
     * @throws TypeError if create_missing=false and intermediate is not object
     *
     * Example:
     * @code
     * cfg.set("database.host", "prod.db.example.com");
     * cfg.set("new.nested.key", 42, true);  // Creates new.nested if missing
     * @endcode
     */
    void set(const std::string& path, const Value& value,
             bool create_missing = true);

    /**
     * @brief Check if dot-path exists
     *
     * Returns true if the full path resolves to a value.
     * Does not throw for missing keys.
     *
     * @param path Dot-separated path
     * @return true if path exists, false if any segment missing
     *
     * @throws TypeError if traversal encounters non-object (RULE D6)
     *
     * Example:
     * @code
     * if (cfg.contains("database.ssl.enabled")) {
     *     // Use SSL settings
     * }
     * @endcode
     */
    bool contains(const std::string& path) const;

    // =========================================================================
    // Raw Data Access
    // =========================================================================

    /**
     * @brief Get underlying JSON object (const)
     *
     * @return Const reference to internal data
     */
    const Value& data() const { return data_; }

    /**
     * @brief Get underlying JSON object (mutable)
     *
     * @return Mutable reference to internal data
     */
    Value& data() { return data_; }

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Serialize to JSON string
     *
     * @param indent Indentation level (default 2 for pretty print, -1 for compact)
     * @return JSON string representation
     */
    std::string to_json(int indent = 2) const;

    /**
     * @brief Serialize to TOML string
     *
     * @return TOML string representation
     */
    std::string to_toml() const;

    /**
     * @brief Convert to plain dictionary (recursively)
     *
     * Returns the underlying Value, which is already a JSON-like structure.
     * This is mainly for API compatibility with the Python version.
     *
     * @return Copy of internal data
     */
    Value to_dict() const { return data_; }

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Check if configuration is empty
     *
     * @return true if no keys are set
     */
    bool empty() const { return data_.empty(); }

    /**
     * @brief Get number of top-level keys
     *
     * @return Count of top-level keys in configuration
     */
    size_t size() const { return data_.size(); }

    /**
     * @brief Merge another Config into this one
     *
     * Performs deep merge, with the other config's values taking precedence.
     *
     * @param other Config to merge from
     */
    void merge(const Config& other);

    /**
     * @brief Merge a Value into this config
     *
     * @param other Value to merge (must be object type)
     */
    void merge(const Value& other);

private:
    Value data_ = Value::object();

    /**
     * @brief Validate that all mandatory keys exist
     *
     * @param mandatory List of required dot-paths
     * @throws MissingMandatoryConfig if any keys are missing
     */
    void validate_mandatory(const std::vector<std::string>& mandatory) const;

    /**
     * @brief Convert overrides map to nested Value
     *
     * Transforms flat dot-path map to nested structure.
     *
     * @param overrides Map of dot-paths to values
     * @return Nested Value object
     */
    static Value overrides_to_value(
        const std::unordered_map<std::string, Value>& overrides);
};

// =============================================================================
// Template Implementation
// =============================================================================

template<typename T>
T Config::get(const std::string& path, const T& default_val) const {
    auto opt = get_optional(path);
    if (!opt.has_value()) {
        return default_val;
    }

    try {
        return opt->get<T>();
    } catch (const nlohmann::json::type_error& e) {
        throw TypeError(
            "Cannot convert value at '" + path + "' to requested type: " +
            std::string(e.what())
        );
    }
}

} // namespace confy

#endif // CONFY_CONFIG_HPP
