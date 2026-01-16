/**
 * @file Config.cpp
 * @brief Configuration class implementation (Phase 3)
 *
 * Main configuration class that orchestrates:
 * 1. Loading from multiple sources
 * 2. Precedence ordering (defaults → file → .env → env → overrides)
 * 3. Mandatory key validation
 * 4. Dot-notation access
 *
 * Implements:
 * - RULE P1: Precedence ordering
 * - RULE D1-D6: Dot-path semantics (via DotPath utilities)
 * - RULE M1-M3: Mandatory key validation
 *
 * @copyright (c) 2026. MIT License.
 */

#include "confy/Config.hpp"
#include "confy/DotPath.hpp"
#include "confy/Merge.hpp"
#include "confy/Loader.hpp"
#include "confy/EnvMapper.hpp"
#include "confy/Parse.hpp"

#include <sstream>

// For TOML serialization
#include <toml++/toml.hpp>

namespace confy {

// =============================================================================
// Construction
// =============================================================================

Config::Config(const Value& data) : data_(data) {
    if (!data_.is_object()) {
        throw TypeError("", "object", type_name(data_));
    }
}

// =============================================================================
// Static Factory: load()
// =============================================================================

Config Config::load(const LoadOptions& opts) {
    Config cfg;

    // -------------------------------------------------------------------------
    // Step 1: Start with defaults (lowest precedence)
    // -------------------------------------------------------------------------
    Value merged = opts.defaults;
    if (!merged.is_object()) {
        merged = Value::object();
    }

    // -------------------------------------------------------------------------
    // Step 2: Load and merge config file
    // -------------------------------------------------------------------------
    Value file_data = Value::object();
    if (!opts.file_path.empty()) {
        // load_config_file handles RULE F6-F8:
        // - Empty path returns empty object
        // - Missing file throws FileNotFoundError
        // - Auto-detects JSON/TOML by extension
        // - TOML key promotion based on defaults
        file_data = load_config_file(opts.file_path, merged);
        merged = deep_merge(merged, file_data);
    }

    // -------------------------------------------------------------------------
    // Step 3: Load .env file (populates environment, does NOT override existing)
    // -------------------------------------------------------------------------
    if (opts.load_dotenv_file) {
        // RULE P4: .env does not override existing environment variables
        std::string env_path = opts.dotenv_path;
        if (env_path.empty()) {
            // Search for .env in current directory
            env_path = ".env";
        }
        // load_dotenv_file handles the "don't override" semantics
        load_dotenv_file(env_path, false /* override_existing */);
    }

    // -------------------------------------------------------------------------
    // Step 4: Load and merge environment variables
    // -------------------------------------------------------------------------
    if (opts.prefix.has_value()) {
        // load_env_vars implements RULE E1-E7:
        // - Prefix filtering (E1-E3)
        // - Underscore transformation (E4)
        // - Remapping against base structure (E5-E7)
        // - Value parsing
        //
        // The base structure for remapping includes both defaults and file_data
        Value env_data = load_env_vars(
            opts.prefix.value(),    // Prefix for filtering
            merged,                  // Base structure for remapping
            merged,                  // Defaults (for key lookup)
            file_data,               // File data (for key lookup)
            false                    // Not from dotenv (conservative mode)
        );
        merged = deep_merge(merged, env_data);
    }

    // -------------------------------------------------------------------------
    // Step 5: Apply overrides (highest precedence)
    // -------------------------------------------------------------------------
    if (!opts.overrides.empty()) {
        Value overrides_obj = overrides_to_value(opts.overrides);
        merged = deep_merge(merged, overrides_obj);
    }

    // -------------------------------------------------------------------------
    // Step 6: Validate mandatory keys
    // -------------------------------------------------------------------------
    cfg.data_ = merged;
    cfg.validate_mandatory(opts.mandatory);

    return cfg;
}

// =============================================================================
// Value Access
// =============================================================================

Value Config::get(const std::string& path) const {
    // RULE D1: Strict get throws KeyError if not found
    const Value* result = get_by_dot(data_, path);
    if (result == nullptr) {
        throw KeyError(path, "Key not found in configuration");
    }
    return *result;
}

std::optional<Value> Config::get_optional(const std::string& path) const {
    // Non-throwing version for optional access
    try {
        const Value* result = get_by_dot(data_, path);
        if (result == nullptr) {
            return std::nullopt;
        }
        return *result;
    } catch (const TypeError&) {
        // RULE D2: TypeError still propagates for traversal into non-object
        throw;
    } catch (...) {
        return std::nullopt;
    }
}

void Config::set(const std::string& path, const Value& value,
                 bool create_missing) {
    // RULE D3-D4: set semantics with create_missing option
    set_by_dot(data_, path, value, create_missing);
}

bool Config::contains(const std::string& path) const {
    // RULE D5-D6: contains semantics
    return contains_dot(data_, path);
}

// =============================================================================
// Serialization
// =============================================================================

std::string Config::to_json(int indent) const {
    if (indent < 0) {
        return data_.dump();
    }
    return data_.dump(indent);
}

namespace {

/**
 * @brief Recursively convert nlohmann::json to toml::table
 */
toml::table json_to_toml(const Value& json);

toml::array json_array_to_toml(const Value& json) {
    toml::array arr;
    for (const auto& elem : json) {
        if (elem.is_null()) {
            // TOML doesn't support null - convert to empty string
            arr.push_back("");
        } else if (elem.is_boolean()) {
            arr.push_back(elem.get<bool>());
        } else if (elem.is_number_integer()) {
            arr.push_back(elem.get<int64_t>());
        } else if (elem.is_number_float()) {
            arr.push_back(elem.get<double>());
        } else if (elem.is_string()) {
            arr.push_back(elem.get<std::string>());
        } else if (elem.is_array()) {
            arr.push_back(json_array_to_toml(elem));
        } else if (elem.is_object()) {
            arr.push_back(json_to_toml(elem));
        }
    }
    return arr;
}

toml::table json_to_toml(const Value& json) {
    toml::table tbl;

    if (!json.is_object()) {
        return tbl;
    }

    for (auto it = json.begin(); it != json.end(); ++it) {
        const std::string& key = it.key();
        const Value& val = it.value();

        if (val.is_null()) {
            // TOML doesn't support null - convert to empty string
            tbl.insert_or_assign(key, "");
        } else if (val.is_boolean()) {
            tbl.insert_or_assign(key, val.get<bool>());
        } else if (val.is_number_integer()) {
            tbl.insert_or_assign(key, val.get<int64_t>());
        } else if (val.is_number_float()) {
            tbl.insert_or_assign(key, val.get<double>());
        } else if (val.is_string()) {
            tbl.insert_or_assign(key, val.get<std::string>());
        } else if (val.is_array()) {
            tbl.insert_or_assign(key, json_array_to_toml(val));
        } else if (val.is_object()) {
            tbl.insert_or_assign(key, json_to_toml(val));
        }
    }

    return tbl;
}

} // anonymous namespace

std::string Config::to_toml() const {
    toml::table tbl = json_to_toml(data_);
    std::ostringstream oss;
    oss << tbl;
    return oss.str();
}

// =============================================================================
// Merge Operations
// =============================================================================

void Config::merge(const Config& other) {
    data_ = deep_merge(data_, other.data_);
}

void Config::merge(const Value& other) {
    if (!other.is_object()) {
        throw TypeError("", "object", type_name(other));
    }
    data_ = deep_merge(data_, other);
}

// =============================================================================
// Mandatory Validation
// =============================================================================

void Config::validate_mandatory(const std::vector<std::string>& mandatory) const {
    // RULE M1-M2: Check all mandatory keys, collect ALL missing
    std::vector<std::string> missing;

    for (const auto& key : mandatory) {
        try {
            if (!contains_dot(data_, key)) {
                missing.push_back(key);
            }
        } catch (const TypeError&) {
            // RULE M3: Path into non-container counts as missing
            missing.push_back(key);
        }
    }

    if (!missing.empty()) {
        throw MissingMandatoryConfig(missing);
    }
}

// =============================================================================
// Override Conversion
// =============================================================================

Value Config::overrides_to_value(
    const std::unordered_map<std::string, Value>& overrides) {

    Value result = Value::object();

    for (const auto& [path, value] : overrides) {
        // Parse string values using the same rules as env vars
        Value parsed_value = value;
        if (value.is_string()) {
            parsed_value = parse_value(value.get<std::string>());
        }

        // Use set_by_dot to build nested structure
        set_by_dot(result, path, parsed_value, true /* create_missing */);
    }

    return result;
}

} // namespace confy
