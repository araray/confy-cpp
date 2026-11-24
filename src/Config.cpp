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

#include <cctype>
#include <cstdlib>
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

namespace {
// Leaf dot-paths of an object (C-03).
void leaf_paths(const Value& v, const std::string& prefix, std::vector<std::string>& out) {
    if (!v.is_object()) {
        if (!prefix.empty()) out.push_back(prefix);
        return;
    }
    for (auto it = v.begin(); it != v.end(); ++it)
        leaf_paths(it.value(), prefix.empty() ? it.key() : prefix + "." + it.key(), out);
}
// The canonical environment variable of a dot-path: '.' as '_', a literal '_' as '__' (rule E4 inverted).
std::string env_name_of(const std::string& prefix, const std::string& path) {
    std::string n = prefix.empty() ? "" : prefix + "_";
    for (char ch : path) {
        if (ch == '.') n += '_';
        else if (ch == '_') n += "__";
        else n += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return n;
}
}  // namespace

Config Config::load(const LoadOptions& opts) {
    Config cfg;
    auto mark = [&](const Value& layer, const std::string& label) {
        if (!opts.track_provenance) return;
        std::vector<std::string> paths;
        leaf_paths(layer, "", paths);
        for (auto& p : paths) cfg.provenance_[p] = label;
    };

    // -------------------------------------------------------------------------
    // Step 1: Start with defaults (lowest precedence)
    // -------------------------------------------------------------------------
    Value merged = opts.defaults;
    if (!merged.is_object()) {
        merged = Value::object();
    }
    mark(merged, "defaults");

    // -------------------------------------------------------------------------
    // Step 2: Load and merge config file
    // -------------------------------------------------------------------------
    Value file_data = Value::object();
    // file_path first, then file_paths in order (C-01); each file's TOML key
    // promotion is guided by everything merged before it.
    std::vector<std::string> files;
    if (!opts.file_path.empty()) files.push_back(opts.file_path);
    for (const auto& f : opts.file_paths)
        if (!f.empty()) files.push_back(f);
    for (const auto& f : files) {
        // load_config_file handles RULE F6-F8:
        // - Missing file throws FileNotFoundError
        // - Auto-detects JSON/TOML by extension
        // - TOML key promotion based on defaults
        Value one = load_config_file(f, opts.toml_key_promotion ? merged : Value::object());
        file_data = deep_merge(file_data, one);
        merged = deep_merge(merged, one);
        mark(one, "file:" + f);
    }

    if (opts.reject_empty_prefix && opts.prefix.has_value() && opts.prefix->empty()) {
        throw ConfigError("an empty environment prefix would load most of the environment (reject_empty_prefix is set)");
    }

    // -------------------------------------------------------------------------
    // Step 3: Load .env file (populates environment, does NOT override existing)
    // -------------------------------------------------------------------------
    std::vector<std::pair<std::string, std::string>> dotenv_overlay;
    if (opts.load_dotenv_file) {
        // RULE P4: .env does not override existing environment variables.
        // Without an explicit path, the working directory and its parents are
        // searched, as find_dotenv documents (C-02).
        std::string env_path = opts.dotenv_path.empty() ? find_dotenv() : opts.dotenv_path;
        if (!env_path.empty()) {
            if (opts.dotenv_mode == LoadOptions::DotenvMode::overlay) {
                dotenv_overlay = parse_dotenv_file(env_path).entries;
            } else {
                // load_dotenv_file handles the "don't override" semantics
                load_dotenv_file(env_path, false /* override_existing */);
            }
        }
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
            false,                   // Not from dotenv (conservative mode)
            dotenv_overlay,          // C-02 overlay entries (empty unless overlay mode)
            opts.string_keys.empty() ? nullptr : &opts.string_keys  // C-05
        );
        const Value before_env = merged;
        merged = deep_merge(merged, env_data);
        if (opts.track_provenance) {
            // The variable that set each key, as written in the environment: map every
            // collected name through the same transform and remap as loading did.
            const std::set<std::string> base_keys = flatten_keys(before_env);
            std::unordered_map<std::string, std::string> name_of;
            const auto vars = dotenv_overlay.empty() ? collect_env_vars(opts.prefix) : collect_env_vars(opts.prefix, dotenv_overlay);
            for (const auto& [name, value] : vars) {
                const std::string part = opts.prefix->empty() ? name : strip_prefix(name, opts.prefix.value());
                if (part.empty()) continue;
                const std::string key = remap_env_key(transform_env_name(part), base_keys, opts.prefix, false);
                if (!key.empty() && !name_of.count(key)) name_of[key] = name;
            }
            std::vector<std::string> paths;
            leaf_paths(env_data, "", paths);
            for (auto& p : paths) {
                auto it = name_of.find(p);
                const std::string name = it != name_of.end() ? it->second : env_name_of(opts.prefix.value(), p);
                bool from_dotenv = false;
                if (!std::getenv(name.c_str()))
                    for (auto& [k, v] : dotenv_overlay) from_dotenv |= k == name;
                cfg.provenance_[p] = from_dotenv ? "dotenv:" + (opts.dotenv_path.empty() ? find_dotenv() : opts.dotenv_path) : "env:" + name;
            }
        }
    }

    // -------------------------------------------------------------------------
    // Step 5: Apply overrides (highest precedence)
    // -------------------------------------------------------------------------
    if (!opts.overrides.empty()) {
        Value overrides_obj = Value::object();
        for (const auto& [path, value] : opts.overrides) {
            // C-05: a string key keeps its string; other strings are parsed as before
            const bool keep = value.is_string() && opts.string_keys.count(path);
            set_by_dot(overrides_obj, path, keep ? value : (value.is_string() ? parse_value(value.get<std::string>()) : value), true);
        }
        merged = deep_merge(merged, overrides_obj);
        mark(overrides_obj, "override");
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

namespace {
Value redact(const Value& data, const std::vector<std::string>& paths, const std::string& mask) {
    Value copy = data;
    for (const auto& p : paths)
        if (contains_dot(copy, p)) set_by_dot(copy, p, Value(mask), false);
    return copy;
}
}  // namespace

std::string Config::to_json_redacted(const std::vector<std::string>& paths, int indent, const std::string& mask) const {
    Value r = redact(data_, paths, mask);
    return indent < 0 ? r.dump() : r.dump(indent);
}

std::string Config::to_toml_redacted(const std::vector<std::string>& paths, const std::string& mask) const {
    toml::table tbl = json_to_toml(redact(data_, paths, mask));
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

namespace confy {

LoadResult Config::try_load(const LoadOptions& opts) noexcept {
    LoadResult r;
    auto fail = [&](LoadError::Kind k, std::string file, std::string key, const char* what) {
        LoadError e;
        e.kind = k;
        e.file = std::move(file);
        e.key = std::move(key);
        e.message = what ? what : "";
        r.error = std::move(e);
    };
    try {
        try {
            r.config.emplace(load(opts));
        } catch (const FileNotFoundError& e) {
            fail(LoadError::Kind::file_not_found, e.path(), "", e.what());
        } catch (const ConfigParseError& e) {
            fail(LoadError::Kind::parse, e.file(), "", e.what());
        } catch (const MissingMandatoryConfig& e) {
            fail(LoadError::Kind::missing_mandatory, "", e.missing_keys().empty() ? "" : e.missing_keys().front(), e.what());
        } catch (const KeyError& e) {
            fail(LoadError::Kind::key, "", e.path(), e.what());
        } catch (const TypeError& e) {
            fail(LoadError::Kind::type, "", e.path(), e.what());
        } catch (const std::exception& e) {
            fail(LoadError::Kind::other, "", "", e.what());
        }
    } catch (...) {
        r.config.reset();
        r.error = LoadError{LoadError::Kind::other, "", "", "unknown error"};
    }
    return r;
}

} // namespace confy
