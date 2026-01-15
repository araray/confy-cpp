/**
 * @file Loader.cpp
 * @brief File loading implementation (Phase 2)
 *
 * Implements file loading for:
 * - JSON files (using nlohmann::json)
 * - TOML files (using toml++)
 * - .env files (custom parser)
 *
 * RULE F1-F8: File format behavior from design specification.
 * RULE P4: .env does NOT override existing env vars.
 *
 * @see CONFY_DESIGN_SPECIFICATION.md Section 3.6
 */

#include "confy/Loader.hpp"
#include "confy/Errors.hpp"

#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace confy {

// ============================================================================
// Utility functions
// ============================================================================

namespace {

/**
 * @brief Convert string to lowercase.
 */
std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

/**
 * @brief Trim whitespace from both ends of string.
 */
std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/**
 * @brief Remove quotes from a quoted string value.
 */
std::string unquote(const std::string& s) {
    if (s.length() < 2) return s;

    char first = s.front();
    char last = s.back();

    // Handle single quotes
    if (first == '\'' && last == '\'') {
        return s.substr(1, s.length() - 2);
    }

    // Handle double quotes with escape processing
    if (first == '"' && last == '"') {
        std::string content = s.substr(1, s.length() - 2);
        std::string result;
        result.reserve(content.length());

        for (size_t i = 0; i < content.length(); ++i) {
            if (content[i] == '\\' && i + 1 < content.length()) {
                char next = content[i + 1];
                switch (next) {
                    case 'n': result += '\n'; ++i; break;
                    case 'r': result += '\r'; ++i; break;
                    case 't': result += '\t'; ++i; break;
                    case '\\': result += '\\'; ++i; break;
                    case '"': result += '"'; ++i; break;
                    case '\'': result += '\''; ++i; break;
                    default: result += content[i]; break;
                }
            } else {
                result += content[i];
            }
        }
        return result;
    }

    return s;
}

/**
 * @brief Check if file exists.
 */
bool file_exists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_regular_file(path, ec);
}

/**
 * @brief Read entire file into string.
 */
std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw FileNotFoundError(path);
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

/**
 * @brief Convert toml++ value to nlohmann::json.
 */
Value toml_value_to_json(const toml::node& node) {
    switch (node.type()) {
        case toml::node_type::string:
            return Value(node.as_string()->get());

        case toml::node_type::integer:
            return Value(node.as_integer()->get());

        case toml::node_type::floating_point:
            return Value(node.as_floating_point()->get());

        case toml::node_type::boolean:
            return Value(node.as_boolean()->get());

        case toml::node_type::date: {
            auto d = node.as_date()->get();
            std::ostringstream ss;
            ss << d;
            return Value(ss.str());
        }

        case toml::node_type::time: {
            auto t = node.as_time()->get();
            std::ostringstream ss;
            ss << t;
            return Value(ss.str());
        }

        case toml::node_type::date_time: {
            auto dt = node.as_date_time()->get();
            std::ostringstream ss;
            ss << dt;
            return Value(ss.str());
        }

        case toml::node_type::array: {
            Value arr = Value::array();
            for (const auto& elem : *node.as_array()) {
                arr.push_back(toml_value_to_json(elem));
            }
            return arr;
        }

        case toml::node_type::table: {
            Value obj = Value::object();
            for (const auto& [key, val] : *node.as_table()) {
                obj[std::string(key.str())] = toml_value_to_json(val);
            }
            return obj;
        }

        default:
            return Value(nullptr);
    }
}

} // anonymous namespace

// ============================================================================
// JSON File Loading
// ============================================================================

Value load_json_file(const std::string& path) {
    // Check file exists
    if (!file_exists(path)) {
        throw FileNotFoundError(path);
    }

    // Read file content
    std::string content;
    try {
        content = read_file(path);
    } catch (const FileNotFoundError&) {
        throw;
    } catch (const std::exception& e) {
        throw ConfigParseError(path, 0, 0, std::string("Failed to read file: ") + e.what());
    }

    // Parse JSON
    try {
        return nlohmann::json::parse(content);
    } catch (const nlohmann::json::parse_error& e) {
        // Extract line/column from error message if possible
        throw ConfigParseError(path, 0, 0, e.what());
    }
}

// ============================================================================
// TOML File Loading
// ============================================================================

Value load_toml_file(const std::string& path, const Value& defaults) {
    // Check file exists
    if (!file_exists(path)) {
        throw FileNotFoundError(path);
    }

    // Parse TOML
    toml::table table;
    try {
        table = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        throw ConfigParseError(
            path,
            static_cast<int>(e.source().begin.line),
            static_cast<int>(e.source().begin.column),
            std::string(e.description())
        );
    }

    // Convert to JSON
    Value result = toml_value_to_json(table);

    // RULE F5: Key promotion
    // If a TOML key inside a section matches a root-level default key,
    // it MAY be promoted to root level.
    if (defaults.is_object() && !defaults.empty() && result.is_object()) {
        std::set<std::string> root_default_keys;
        for (auto it = defaults.begin(); it != defaults.end(); ++it) {
            root_default_keys.insert(it.key());
        }

        std::set<std::string> promoted_keys;
        std::vector<std::string> empty_sections;

        // Iterate through sections
        for (auto it = result.begin(); it != result.end(); ++it) {
            if (!it.value().is_object()) continue;

            const std::string& section_key = it.key();
            Value& section_data = it.value();

            std::vector<std::string> keys_to_promote;
            for (auto sit = section_data.begin(); sit != section_data.end(); ++sit) {
                if (root_default_keys.count(sit.key()) > 0) {
                    keys_to_promote.push_back(sit.key());
                }
            }

            for (const auto& key_to_promote : keys_to_promote) {
                // Only promote if not already at root or already promoted
                if (!result.contains(key_to_promote) || promoted_keys.count(key_to_promote) > 0) {
                    if (promoted_keys.count(key_to_promote) == 0) {
                        result[key_to_promote] = section_data[key_to_promote];
                        promoted_keys.insert(key_to_promote);
                    }
                    section_data.erase(key_to_promote);
                }
            }

            // Mark empty sections for removal
            if (section_data.empty()) {
                empty_sections.push_back(section_key);
            }
        }

        // Remove empty sections
        for (const auto& section : empty_sections) {
            result.erase(section);
        }
    }

    return result;
}

// ============================================================================
// Auto-detect File Loading
// ============================================================================

std::string get_file_extension(const std::string& path) {
    fs::path p(path);
    std::string ext = p.extension().string();
    return to_lower(ext);
}

Value load_config_file(const std::string& path, const Value& defaults) {
    // RULE F6: Empty path = no file loaded
    if (path.empty()) {
        return Value::object();
    }

    // RULE F7: File must exist
    if (!file_exists(path)) {
        throw FileNotFoundError(path);
    }

    // RULE F8: Detect format by extension
    std::string ext = get_file_extension(path);

    if (ext == ".json") {
        return load_json_file(path);
    } else if (ext == ".toml") {
        return load_toml_file(path, defaults);
    } else {
        throw std::runtime_error(
            "Unsupported config file type: " + ext + " (expected .json or .toml)"
        );
    }
}

// ============================================================================
// .env File Loading
// ============================================================================

DotenvResult parse_dotenv_file(const std::string& path) {
    DotenvResult result;
    result.loaded_path = path;
    result.found = false;

    if (!file_exists(path)) {
        return result;
    }

    std::ifstream file(path);
    if (!file) {
        return result;
    }

    result.found = true;
    std::string line;

    while (std::getline(file, line)) {
        // Trim the line
        std::string trimmed = trim(line);

        // Skip empty lines and comments
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        // Handle 'export' prefix
        const std::string export_prefix = "export ";
        if (trimmed.length() > export_prefix.length() &&
            to_lower(trimmed.substr(0, export_prefix.length())) == export_prefix) {
            trimmed = trim(trimmed.substr(export_prefix.length()));
        }

        // Find the = sign
        size_t eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos || eq_pos == 0) {
            continue;  // Invalid line, skip
        }

        // Extract key and value
        std::string key = trim(trimmed.substr(0, eq_pos));
        std::string value = trimmed.substr(eq_pos + 1);

        // Handle inline comments (but not inside quotes)
        bool in_single_quote = false;
        bool in_double_quote = false;
        size_t comment_pos = std::string::npos;

        for (size_t i = 0; i < value.length(); ++i) {
            char c = value[i];
            if (c == '\'' && !in_double_quote) {
                in_single_quote = !in_single_quote;
            } else if (c == '"' && !in_single_quote) {
                in_double_quote = !in_double_quote;
            } else if (c == '#' && !in_single_quote && !in_double_quote) {
                comment_pos = i;
                break;
            }
        }

        if (comment_pos != std::string::npos) {
            value = value.substr(0, comment_pos);
        }

        // Trim and unquote value
        value = trim(value);
        value = unquote(value);

        // Add to results
        if (!key.empty()) {
            result.entries.emplace_back(std::move(key), std::move(value));
        }
    }

    return result;
}

std::string find_dotenv(const std::string& start_dir) {
    fs::path search_path;

    if (start_dir.empty()) {
        std::error_code ec;
        search_path = fs::current_path(ec);
        if (ec) return "";
    } else {
        search_path = start_dir;
    }

    // Search current directory and parent directories
    while (!search_path.empty()) {
        fs::path dotenv_path = search_path / ".env";

        std::error_code ec;
        if (fs::exists(dotenv_path, ec) && fs::is_regular_file(dotenv_path, ec)) {
            return dotenv_path.string();
        }

        // Move to parent directory
        fs::path parent = search_path.parent_path();
        if (parent == search_path) {
            // Reached root, stop
            break;
        }
        search_path = parent;
    }

    return "";
}

bool set_env_var(const std::string& name, const std::string& value, bool overwrite) {
    // Check if variable already exists
    if (!overwrite && has_env_var(name)) {
        return false;
    }

#ifdef _WIN32
    // Windows: use SetEnvironmentVariableA
    return SetEnvironmentVariableA(name.c_str(), value.c_str()) != 0;
#else
    // POSIX: use setenv
    return setenv(name.c_str(), value.c_str(), overwrite ? 1 : 0) == 0;
#endif
}

std::optional<std::string> get_env_var(const std::string& name) {
#ifdef _WIN32
    // Windows: use GetEnvironmentVariableA
    char buffer[32767];  // Max env var size on Windows
    DWORD result = GetEnvironmentVariableA(name.c_str(), buffer, sizeof(buffer));
    if (result == 0) {
        return std::nullopt;
    }
    return std::string(buffer, result);
#else
    // POSIX: use getenv
    const char* value = std::getenv(name.c_str());
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(value);
#endif
}

bool has_env_var(const std::string& name) {
#ifdef _WIN32
    // Windows: GetEnvironmentVariableA returns 0 if not found
    return GetEnvironmentVariableA(name.c_str(), nullptr, 0) != 0 ||
           GetLastError() != ERROR_ENVVAR_NOT_FOUND;
#else
    // POSIX: getenv returns nullptr if not found
    return std::getenv(name.c_str()) != nullptr;
#endif
}

bool load_dotenv_file(const std::string& path, bool override_existing) {
    std::string dotenv_path = path;

    // Auto-search if no path provided
    if (dotenv_path.empty()) {
        dotenv_path = find_dotenv();
        if (dotenv_path.empty()) {
            return false;  // No .env file found
        }
    }

    // Parse the file
    DotenvResult result = parse_dotenv_file(dotenv_path);

    if (!result.found) {
        return false;
    }

    // Load entries into environment
    // RULE P4: Does NOT override existing env vars (unless override_existing=true)
    for (const auto& [name, value] : result.entries) {
        set_env_var(name, value, override_existing);
    }

    return true;
}

} // namespace confy
