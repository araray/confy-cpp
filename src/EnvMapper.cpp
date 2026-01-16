/**
 * @file EnvMapper.cpp
 * @brief Environment variable mapping implementation (Phase 2)
 *
 * Implements the complex environment variable mapping logic:
 * 1. Prefix filtering (RULE E1-E3)
 * 2. Underscore transformation (RULE E4)
 * 3. Remapping against base structure (RULE E5-E7)
 * 4. System variable exclusion
 *
 * This is the most complex component in the entire project.
 * The implementation MUST exactly match Python's behavior for parity.
 *
 * @see CONFY_DESIGN_SPECIFICATION.md Section 3.2
 * @see Python confy/loader.py _collect_env_vars, _remap_and_flatten_env_data
 */

#include "confy/EnvMapper.hpp"
#include "confy/Parse.hpp"
#include "confy/DotPath.hpp"
#include "confy/Errors.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

// Platform-specific environment variable access
#ifdef _WIN32
    // Windows: use _environ or GetEnvironmentStrings
    #include <windows.h>
#else
    // POSIX: use environ
    extern char** environ;
#endif

namespace confy {

// ============================================================================
// System Variable Exclusion List (RULE E2)
// ============================================================================

/**
 * @brief Complete list of system environment variable prefixes to exclude.
 *
 * A variable is skipped if its name STARTS WITH any of these prefixes
 * (case-insensitive) OR if the name EQUALS any single-char prefix exactly.
 *
 * From CONFY_DESIGN_SPECIFICATION.md Appendix A.
 */
const std::vector<std::string> SYSTEM_VAR_PREFIXES = {
    // Shell and system basics
    "PATH", "HOME", "USER", "SHELL", "TERM", "LANG", "LC_",
    "PWD", "OLDPWD", "HOSTNAME", "LOGNAME", "MAIL", "EDITOR",
    "VISUAL", "TMPDIR", "TMP", "TEMP", "XDG_", "DISPLAY",

    // SSH and security
    "SSH_", "GPG_", "DBUS_",

    // Desktop environments
    "DESKTOP_", "GNOME_", "KDE_", "GTK_", "QT_",

    // Programming languages and tools
    "JAVA_", "PYTHON", "NODE_", "NPM_", "NVM_",
    "VIRTUAL_ENV", "CONDA_", "PIP_", "CARGO_", "RUST", "GO",
    "RBENV", "GEM_", "BUNDLE_", "RAILS_", "RACK_",

    // Shell prompts and history
    "_", "PS1", "PS2", "PS4", "PROMPT_",
    "HISTFILE", "HISTSIZE", "SAVEHIST",

    // Pagers and documentation
    "LESS", "MORE", "PAGER", "MANPATH", "INFOPATH",

    // Library paths
    "LD_", "DYLD_", "LIBPATH", "CPATH", "LIBRARY_PATH", "PKG_CONFIG",

    // Build tools
    "CMAKE_", "CC", "CXX", "CFLAGS", "CXXFLAGS", "LDFLAGS",
    "MAKEFLAGS", "MAKELEVEL", "SHLVL",

    // Terminal colors
    "COLORTERM", "COLORFGBG", "WINDOWID", "TERM_PROGRAM",

    // IDE and editor
    "ITERM_", "VSCODE_", "WSL_", "WSLENV", "WT_",
    "CONEMU", "ANSICON", "CLICOLOR", "FORCE_", "NO_COLOR",

    // Debug and CI
    "DEBUG", "VERBOSE", "CI",
    "GITHUB_", "GITLAB_", "TRAVIS_", "CIRCLECI",
    "JENKINS_", "BUILDKITE_", "AZURE_",

    // Cloud and containers
    "AWS_", "GOOGLE_", "DOCKER_", "KUBERNETES_", "K8S_", "COMPOSE_",

    // Additional common system vars
    "ZSH_", "LS_", "PYTHONUTF8", "PYTHONPATH",
    "WINDOWPATH", "QTWEBENGINE_", "MOZ_", "GDK_",
    "BROWSER", "USERNAME", "SYSTEMROOT", "DOMAINNAME",
    "HOSTTYPE", "OSTYPE", "MACHTYPE"
};

// ============================================================================
// String utilities
// ============================================================================

namespace {

/**
 * @brief Convert string to uppercase.
 */
std::string to_upper(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

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
 * @brief Check if string starts with prefix (case-insensitive).
 */
bool starts_with_icase(const std::string& str, const std::string& prefix) {
    if (prefix.size() > str.size()) return false;
    return std::equal(prefix.begin(), prefix.end(), str.begin(),
                      [](char a, char b) {
                          return std::toupper(static_cast<unsigned char>(a)) ==
                                 std::toupper(static_cast<unsigned char>(b));
                      });
}

/**
 * @brief Replace all occurrences of a substring.
 */
std::string replace_all(const std::string& str,
                        const std::string& from,
                        const std::string& to) {
    std::string result = str;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }
    return result;
}

/**
 * @brief Get all environment variables as (name, value) pairs.
 */
std::vector<std::pair<std::string, std::string>> get_all_env_vars() {
    std::vector<std::pair<std::string, std::string>> result;

#ifdef _WIN32
    // Windows implementation using GetEnvironmentStrings
    LPCH env_block = GetEnvironmentStrings();
    if (env_block == nullptr) return result;

    LPCH current = env_block;
    while (*current != '\0') {
        std::string entry(current);
        size_t eq_pos = entry.find('=');
        if (eq_pos != std::string::npos && eq_pos > 0) {
            std::string name = entry.substr(0, eq_pos);
            std::string value = entry.substr(eq_pos + 1);
            result.emplace_back(std::move(name), std::move(value));
        }
        current += entry.length() + 1;
    }
    FreeEnvironmentStrings(env_block);
#else
    // POSIX implementation using environ
    if (environ == nullptr) return result;

    for (char** env = environ; *env != nullptr; ++env) {
        std::string entry(*env);
        size_t eq_pos = entry.find('=');
        if (eq_pos != std::string::npos) {
            std::string name = entry.substr(0, eq_pos);
            std::string value = entry.substr(eq_pos + 1);
            result.emplace_back(std::move(name), std::move(value));
        }
    }
#endif

    return result;
}

/**
 * @brief Flatten nested Value to (dot_path, value) pairs.
 */
std::vector<std::pair<std::string, Value>> flatten_to_pairs(
    const Value& data,
    const std::string& prefix = ""
) {
    std::vector<std::pair<std::string, Value>> result;

    if (!data.is_object()) {
        if (!prefix.empty()) {
            result.emplace_back(prefix, data);
        }
        return result;
    }

    for (auto it = data.begin(); it != data.end(); ++it) {
        std::string new_key = prefix.empty() ? it.key() : prefix + "." + it.key();

        if (it.value().is_object()) {
            // Add the key itself (for section matching)
            result.emplace_back(new_key, it.value());
            // Recurse into nested objects
            auto nested = flatten_to_pairs(it.value(), new_key);
            result.insert(result.end(), nested.begin(), nested.end());
        } else {
            result.emplace_back(new_key, it.value());
        }
    }

    return result;
}

} // anonymous namespace

// ============================================================================
// Public API Implementation
// ============================================================================

bool is_system_variable(const std::string& var_name) {
    std::string var_upper = to_upper(var_name);

    for (const auto& prefix : SYSTEM_VAR_PREFIXES) {
        std::string prefix_upper = to_upper(prefix);

        // For single-character prefixes like "_", check exact match
        if (prefix.length() == 1) {
            if (var_upper == prefix_upper) {
                return true;
            }
        }

        // For other prefixes, check if var starts with prefix
        if (starts_with_icase(var_name, prefix)) {
            return true;
        }
    }

    return false;
}

std::string transform_env_name(const std::string& name) {
    // RULE E4: Underscore transformation
    // 1. Convert to lowercase
    std::string result = to_lower(name);

    // 2. Replace __ with temporary marker
    const std::string TEMP_MARKER = "\x1F_USCORE_\x1F";  // Using null bytes as marker
    result = replace_all(result, "__", TEMP_MARKER);

    // 3. Replace _ with .
    result = replace_all(result, "_", ".");

    // 4. Replace marker back to _
    result = replace_all(result, TEMP_MARKER, "_");

    return result;
}

std::string strip_prefix(const std::string& var_name, const std::string& prefix) {
    if (prefix.empty()) {
        return var_name;
    }

    // Expected format: PREFIX_REST
    std::string prefix_with_underscore = prefix + "_";

    if (starts_with_icase(var_name, prefix_with_underscore)) {
        return var_name.substr(prefix_with_underscore.length());
    }

    return "";  // No match
}

std::vector<std::pair<std::string, std::string>>
collect_env_vars(const std::optional<std::string>& prefix) {
    std::vector<std::pair<std::string, std::string>> result;

    // RULE E3: nullopt disables env loading entirely
    if (!prefix.has_value()) {
        return result;
    }

    const std::string& prefix_str = prefix.value();
    std::string prefix_match;

    if (!prefix_str.empty()) {
        // RULE E1: Non-empty prefix - filter by PREFIX_*
        // Normalize: remove trailing underscore if present
        std::string normalized = prefix_str;
        while (!normalized.empty() && normalized.back() == '_') {
            normalized.pop_back();
        }
        prefix_match = normalized + "_";
    }

    auto all_vars = get_all_env_vars();

    for (const auto& [name, value] : all_vars) {
        bool should_include = false;

        if (!prefix_match.empty()) {
            // RULE E1: Filter by prefix (case-insensitive)
            should_include = starts_with_icase(name, prefix_match);
        } else {
            // RULE E2: Empty prefix - include non-system vars
            should_include = !is_system_variable(name);
        }

        if (should_include) {
            result.emplace_back(name, value);
        }
    }

    return result;
}

std::set<std::string> flatten_keys(const Value& data, const std::string& prefix) {
    std::set<std::string> keys;

    if (!data.is_object()) {
        if (!prefix.empty()) {
            keys.insert(prefix);
        }
        return keys;
    }

    for (auto it = data.begin(); it != data.end(); ++it) {
        std::string new_key = prefix.empty() ? it.key() : prefix + "." + it.key();
        keys.insert(new_key);

        if (it.value().is_object()) {
            auto nested = flatten_keys(it.value(), new_key);
            keys.insert(nested.begin(), nested.end());
        }
    }

    return keys;
}

std::string remap_env_key(
    const std::string& dot_path,
    const std::set<std::string>& base_keys,
    const std::optional<std::string>& prefix,
    bool load_dotenv
) {
    // 1. Exact match? Use as-is
    if (base_keys.count(dot_path) > 0) {
        return dot_path;
    }

    // 2. Try heuristic remapping for keys with underscores
    // Reconstruct flat key by replacing dots with underscores
    std::string reconstructed_flat = replace_all(dot_path, ".", "_");

    // Heuristic 0: Handle base keys that contain underscores
    // e.g., dot_path = "feature.flags.beta.feature" from MYAPP_FEATURE_FLAGS_BETA_FEATURE
    //       base_keys contains "feature_flags.beta_feature"
    // We need to map "feature.flags.beta.feature" -> "feature_flags.beta_feature"
    if (reconstructed_flat.find('_') != std::string::npos) {
        // Try splitting on first underscore to find potential base key prefix
        size_t underscore_pos = reconstructed_flat.find('_');
        while (underscore_pos != std::string::npos) {
            std::string root = reconstructed_flat.substr(0, underscore_pos);
            std::string rest = reconstructed_flat.substr(underscore_pos + 1);

            // Replace remaining underscores with dots in 'rest'
            std::string candidate = root + "." + replace_all(rest, "_", ".");

            if (base_keys.count(candidate) > 0) {
                return candidate;
            }

            // Also try keeping underscores in rest (for keys like feature_flags.beta_feature)
            candidate = root + "." + rest;
            if (base_keys.count(candidate) > 0) {
                return candidate;
            }

            // Try next underscore position
            underscore_pos = reconstructed_flat.find('_', underscore_pos + 1);
        }
    }

    // Attempt: Check if reconstructed flat key exists directly
    if (base_keys.count(reconstructed_flat) > 0) {
        return reconstructed_flat;
    }

    // Try longest prefix matching
    // Split dot_path into segments and try progressively longer underscore-joined prefixes
    auto segments = split_dot_path(dot_path);

    for (size_t prefix_len = segments.size(); prefix_len > 0; --prefix_len) {
        // Build underscore-joined prefix
        std::string candidate_prefix;
        for (size_t i = 0; i < prefix_len; ++i) {
            if (!candidate_prefix.empty()) candidate_prefix += "_";
            candidate_prefix += segments[i];
        }

        // Check if any base key starts with this prefix
        for (const auto& base_key : base_keys) {
            if (base_key == candidate_prefix ||
                (base_key.length() > candidate_prefix.length() &&
                 base_key.substr(0, candidate_prefix.length()) == candidate_prefix &&
                 base_key[candidate_prefix.length()] == '.')) {

                // Found potential match - reconstruct full key
                if (prefix_len < segments.size()) {
                    std::string remainder;
                    for (size_t i = prefix_len; i < segments.size(); ++i) {
                        if (!remainder.empty()) remainder += ".";
                        remainder += segments[i];
                    }
                    std::string full_key = candidate_prefix + "." + remainder;

                    // Verify this key or a prefix of it exists
                    if (base_keys.count(full_key) > 0 ||
                        base_keys.count(candidate_prefix) > 0) {
                        return full_key;
                    }
                } else {
                    return candidate_prefix;
                }
            }
        }
    }

    // 3. Fallback behavior based on context
    bool is_empty_prefix = prefix.has_value() && prefix.value().empty();

    if (is_empty_prefix && load_dotenv) {
        // Conservative mode: discard unmatched keys
        // (prefix="" && load_dotenv means we're being careful about random env vars)
        return "";
    }

    if (!is_empty_prefix && load_dotenv) {
        // .env-loaded variables with non-empty prefix: use original dot_path
        return dot_path;
    }

    if (!is_empty_prefix && !load_dotenv) {
        // Real env vars (no-dotenv) with non-empty prefix: use flat key
        return reconstructed_flat;
    }

    // Default: keep as-is
    return dot_path;
}

Value env_vars_to_nested(
    const std::vector<std::pair<std::string, std::string>>& env_vars,
    const std::optional<std::string>& prefix
) {
    Value result = Value::object();

    const std::string prefix_str = prefix.value_or("");

    for (const auto& [name, raw_value] : env_vars) {
        // Strip prefix from name
        std::string key_part;
        if (!prefix_str.empty()) {
            key_part = strip_prefix(name, prefix_str);
            if (key_part.empty()) continue;  // Shouldn't happen if collect was correct
        } else {
            key_part = name;
        }

        // Transform using underscore rules
        std::string dot_key = transform_env_name(key_part);

        // Parse value
        Value parsed_value = parse_value(raw_value);

        // Set in result structure
        try {
            set_by_dot(result, dot_key, parsed_value, true);
        } catch (const std::exception& e) {
            // Skip variables that cause errors (e.g., invalid paths)
            continue;
        }
    }

    return result;
}

std::vector<std::pair<std::string, Value>> remap_and_flatten_env_data(
    const Value& nested_env_data,
    const Value& defaults_data,
    const Value& file_data,
    const std::optional<std::string>& prefix,
    bool load_dotenv
) {
    std::vector<std::pair<std::string, Value>> result;

    // Combine defaults and file for base structure
    Value base_config = defaults_data;
    if (file_data.is_object()) {
        for (auto it = file_data.begin(); it != file_data.end(); ++it) {
            base_config[it.key()] = it.value();
        }
    }

    // Get valid base keys
    std::set<std::string> base_keys = flatten_keys(base_config);

    // Flatten env data to (dot_path, value) pairs
    auto flat_items = flatten_to_pairs(nested_env_data);

    // Sort by depth (deepest first) to handle overlapping keys correctly
    std::sort(flat_items.begin(), flat_items.end(),
              [](const auto& a, const auto& b) {
                  size_t depth_a = std::count(a.first.begin(), a.first.end(), '.');
                  size_t depth_b = std::count(b.first.begin(), b.first.end(), '.');
                  return depth_a > depth_b;
              });

    // Track already-set keys to avoid duplicates
    std::set<std::string> assigned_keys;

    for (const auto& [dot_key, value] : flat_items) {
        // Skip object values (we only want leaf values)
        if (value.is_object()) {
            continue;
        }

        // Attempt remapping
        std::string final_key = remap_env_key(dot_key, base_keys, prefix, load_dotenv);

        // Skip if remapping returned empty (discard)
        if (final_key.empty()) {
            continue;
        }

        // Skip if already assigned
        if (assigned_keys.count(final_key) > 0) {
            continue;
        }

        result.emplace_back(final_key, value);
        assigned_keys.insert(final_key);
    }

    return result;
}

Value load_env_vars(
    const std::optional<std::string>& prefix,
    const Value& base_structure,
    const Value& defaults_data,
    const Value& file_data,
    bool load_dotenv
) {
    // Suppress unused parameter warning - base_structure could be used for optimization
    // but we delegate to remap_and_flatten_env_data which recomputes the merge
    (void)base_structure;

    // Step 1: Collect environment variables
    auto env_vars = collect_env_vars(prefix);

    if (env_vars.empty()) {
        return Value::object();
    }

    // Step 2: Convert to nested structure
    Value nested_env = env_vars_to_nested(env_vars, prefix);

    // Step 3: Remap and flatten
    auto remapped = remap_and_flatten_env_data(
        nested_env, defaults_data, file_data, prefix, load_dotenv
    );

    // Step 4: Structure into final Value
    Value result = Value::object();

    for (const auto& [key, value] : remapped) {
        try {
            set_by_dot(result, key, value, true);
        } catch (const std::exception& e) {
            // Skip problematic keys
            continue;
        }
    }

    return result;
}

} // namespace confy
