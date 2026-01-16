/**
 * @file cli_main.cpp
 * @brief CLI tool entry point (Phase 4)
 *
 * Command-line interface for confy-cpp.
 * Provides: get, set, exists, search, dump, convert commands.
 *
 * Usage:
 *   confy-cpp [GLOBAL OPTIONS] COMMAND [ARGS]
 *
 * Global Options:
 *   -c, --config PATH      Path to config file (JSON/TOML)
 *   -p, --prefix TEXT      Environment variable prefix
 *   --overrides TEXT       Comma-separated key:value pairs
 *   --defaults PATH        Path to defaults file (JSON)
 *   --mandatory TEXT       Comma-separated mandatory keys
 *   --dotenv-path PATH     Explicit .env file path
 *   --no-dotenv            Disable .env loading
 *   -h, --help             Show help
 *
 * Commands:
 *   get KEY                Get value at dot-path
 *   set KEY VALUE          Set value in config file
 *   exists KEY             Check if key exists
 *   search [OPTIONS]       Search keys/values
 *   dump                   Print entire config
 *   convert --to FORMAT    Convert to JSON/TOML
 *
 * @see CONFY_DESIGN_SPECIFICATION.md Section 6
 * @copyright (c) 2026. MIT License.
 */

#include <cxxopts.hpp>

#include "confy/Config.hpp"
#include "confy/Loader.hpp"
#include "confy/DotPath.hpp"
#include "confy/Parse.hpp"
#include "confy/Errors.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

// ============================================================================
// ANSI Color Codes (for terminal output)
// ============================================================================

namespace color {
    const char* RED    = "\033[31m";
    const char* GREEN  = "\033[32m";
    const char* YELLOW = "\033[33m";
    const char* RESET  = "\033[0m";

    // Detect if colors should be used
    bool enabled() {
        static bool checked = false;
        static bool use_color = false;
        if (!checked) {
            checked = true;
            // Check NO_COLOR env var and terminal type
            const char* no_color = std::getenv("NO_COLOR");
            const char* term = std::getenv("TERM");
            use_color = (no_color == nullptr) && (term != nullptr) &&
                        (std::string(term) != "dumb");
#ifdef _WIN32
            // Windows: also check if we're in a proper terminal
            use_color = use_color || (std::getenv("WT_SESSION") != nullptr);
#endif
        }
        return use_color;
    }

    std::string red(const std::string& s) {
        return enabled() ? std::string(RED) + s + RESET : s;
    }
    std::string green(const std::string& s) {
        return enabled() ? std::string(GREEN) + s + RESET : s;
    }
    std::string yellow(const std::string& s) {
        return enabled() ? std::string(YELLOW) + s + RESET : s;
    }
}

// ============================================================================
// Utility Functions
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
 * @brief Trim whitespace from both ends.
 */
std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/**
 * @brief Parse overrides string into key-value map.
 *
 * Format: "key1:value1,key2:value2,..."
 * Values are parsed using confy's type parsing rules.
 */
std::unordered_map<std::string, confy::Value> parse_overrides(const std::string& str) {
    std::unordered_map<std::string, confy::Value> result;

    if (str.empty()) return result;

    // Split by comma (but respect JSON arrays/objects)
    std::string current;
    int bracket_depth = 0;
    int brace_depth = 0;
    bool in_string = false;

    for (size_t i = 0; i < str.size(); ++i) {
        char c = str[i];

        // Track string literals
        if (c == '"' && (i == 0 || str[i-1] != '\\')) {
            in_string = !in_string;
        }

        if (!in_string) {
            if (c == '[') bracket_depth++;
            else if (c == ']') bracket_depth--;
            else if (c == '{') brace_depth++;
            else if (c == '}') brace_depth--;
        }

        // Split on comma only at top level
        if (c == ',' && bracket_depth == 0 && brace_depth == 0 && !in_string) {
            if (!current.empty()) {
                // Parse this key:value pair
                size_t colon_pos = current.find(':');
                if (colon_pos != std::string::npos) {
                    std::string key = trim(current.substr(0, colon_pos));
                    std::string val_str = trim(current.substr(colon_pos + 1));
                    result[key] = confy::parse_value(val_str);
                }
            }
            current.clear();
        } else {
            current += c;
        }
    }

    // Don't forget the last pair
    if (!current.empty()) {
        size_t colon_pos = current.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = trim(current.substr(0, colon_pos));
            std::string val_str = trim(current.substr(colon_pos + 1));
            result[key] = confy::parse_value(val_str);
        }
    }

    return result;
}

/**
 * @brief Parse comma-separated list of strings.
 */
std::vector<std::string> parse_list(const std::string& str) {
    std::vector<std::string> result;
    if (str.empty()) return result;

    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string trimmed = trim(item);
        if (!trimmed.empty()) {
            result.push_back(trimmed);
        }
    }
    return result;
}

/**
 * @brief Load defaults from JSON file.
 */
confy::Value load_defaults_file(const std::string& path) {
    if (path.empty()) {
        return confy::Value::object();
    }

    std::ifstream file(path);
    if (!file) {
        throw confy::FileNotFoundError(path);
    }

    std::ostringstream ss;
    ss << file.rdbuf();

    try {
        return nlohmann::json::parse(ss.str());
    } catch (const nlohmann::json::parse_error& e) {
        throw confy::ConfigParseError(path, e.what());
    }
}

/**
 * @brief Flatten nested config to dot-path -> value map.
 */
std::vector<std::pair<std::string, confy::Value>> flatten_config(
    const confy::Value& data,
    const std::string& prefix = ""
) {
    std::vector<std::pair<std::string, confy::Value>> result;

    if (!data.is_object()) {
        if (!prefix.empty()) {
            result.emplace_back(prefix, data);
        }
        return result;
    }

    for (auto it = data.begin(); it != data.end(); ++it) {
        std::string key = prefix.empty() ? it.key() : prefix + "." + it.key();

        if (it.value().is_object()) {
            auto nested = flatten_config(it.value(), key);
            result.insert(result.end(), nested.begin(), nested.end());
        } else {
            result.emplace_back(key, it.value());
        }
    }

    return result;
}

/**
 * @brief Match text against pattern (glob, regex, or exact).
 *
 * Pattern detection:
 * - Contains *, ?, [ or ] -> glob
 * - Contains regex special chars -> regex
 * - Otherwise -> exact substring match
 */
bool match_pattern(const std::string& pattern, const std::string& text, bool ignore_case) {
    std::string pat = ignore_case ? to_lower(pattern) : pattern;
    std::string txt = ignore_case ? to_lower(text) : text;

    // 1. Glob pattern?
    if (pat.find('*') != std::string::npos ||
        pat.find('?') != std::string::npos ||
        pat.find('[') != std::string::npos) {

        // Convert glob to regex
        std::string regex_pat;
        for (char c : pat) {
            switch (c) {
                case '*': regex_pat += ".*"; break;
                case '?': regex_pat += "."; break;
                case '.': regex_pat += "\\."; break;
                case '[': regex_pat += "["; break;
                case ']': regex_pat += "]"; break;
                case '(': regex_pat += "\\("; break;
                case ')': regex_pat += "\\)"; break;
                case '+': regex_pat += "\\+"; break;
                case '^': regex_pat += "\\^"; break;
                case '$': regex_pat += "\\$"; break;
                case '|': regex_pat += "\\|"; break;
                case '\\': regex_pat += "\\\\"; break;
                default: regex_pat += c; break;
            }
        }

        try {
            std::regex re("^" + regex_pat + "$");
            return std::regex_match(txt, re);
        } catch (const std::regex_error&) {
            // Fallback to exact match if regex fails
            return txt.find(pat) != std::string::npos;
        }
    }

    // 2. Regex pattern? (contains regex-specific chars)
    if (pat.find('^') != std::string::npos ||
        pat.find('$') != std::string::npos ||
        pat.find('|') != std::string::npos ||
        pat.find('(') != std::string::npos ||
        pat.find('+') != std::string::npos) {

        try {
            std::regex re(pat);
            return std::regex_search(txt, re);
        } catch (const std::regex_error&) {
            // Fallback to exact match
            return txt.find(pat) != std::string::npos;
        }
    }

    // 3. Exact substring match
    return txt.find(pat) != std::string::npos;
}

/**
 * @brief Get file extension (lowercase).
 */
std::string get_extension(const std::string& path) {
    fs::path p(path);
    std::string ext = p.extension().string();
    return to_lower(ext);
}

/**
 * @brief Write JSON to file.
 */
void write_json_file(const std::string& path, const confy::Value& data) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open file for writing: " + path);
    }
    file << data.dump(2) << std::endl;
}

/**
 * @brief Write TOML to file (using confy's to_toml).
 */
void write_toml_file(const std::string& path, const confy::Value& data) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open file for writing: " + path);
    }

    // Use Config's to_toml method
    confy::Config cfg(data);
    file << cfg.to_toml();
}

} // anonymous namespace

// ============================================================================
// Command Implementations
// ============================================================================

/**
 * @brief CMD: get KEY
 * Print the value at dot-path as JSON.
 */
int cmd_get(confy::Config& cfg, const std::string& key) {
    try {
        confy::Value val = cfg.get(key);
        std::cout << val.dump(2) << std::endl;
        return 0;
    } catch (const confy::KeyError& e) {
        std::cerr << color::yellow("Key not found: " + key) << std::endl;
        return 1;
    } catch (const confy::TypeError& e) {
        std::cerr << color::red("Error: ") << e.what() << std::endl;
        return 1;
    }
}

/**
 * @brief CMD: set KEY VALUE
 * Update a key in the source config file.
 */
int cmd_set(const std::string& file_path, const std::string& key, const std::string& value_str) {
    if (file_path.empty()) {
        std::cerr << color::red("Error: --config/-c is required for 'set' command") << std::endl;
        return 1;
    }

    // Load existing file
    confy::Value data;
    std::string ext = get_extension(file_path);

    try {
        if (ext == ".json") {
            data = confy::load_json_file(file_path);
        } else if (ext == ".toml") {
            data = confy::load_toml_file(file_path);
        } else {
            std::cerr << color::red("Error: Unsupported file format: " + ext) << std::endl;
            return 1;
        }
    } catch (const confy::FileNotFoundError&) {
        // Create new file
        data = confy::Value::object();
    } catch (const std::exception& e) {
        std::cerr << color::red("Error loading file: ") << e.what() << std::endl;
        return 1;
    }

    // Parse the value
    confy::Value parsed_value = confy::parse_value(value_str);

    // Set the value
    try {
        confy::set_by_dot(data, key, parsed_value, true);
    } catch (const std::exception& e) {
        std::cerr << color::red("Error setting value: ") << e.what() << std::endl;
        return 1;
    }

    // Write back
    try {
        if (ext == ".json") {
            write_json_file(file_path, data);
        } else if (ext == ".toml") {
            write_toml_file(file_path, data);
        }
    } catch (const std::exception& e) {
        std::cerr << color::red("Error writing file: ") << e.what() << std::endl;
        return 1;
    }

    std::cout << "Set " << key << " = " << parsed_value.dump()
              << " in " << file_path << std::endl;
    return 0;
}

/**
 * @brief CMD: exists KEY
 * Check if a key exists in the config.
 */
int cmd_exists(confy::Config& cfg, const std::string& key) {
    try {
        bool exists = cfg.contains(key);
        std::cout << (exists ? "true" : "false") << std::endl;
        return exists ? 0 : 1;
    } catch (const confy::TypeError&) {
        // Path traversal error - key doesn't exist in expected form
        std::cout << "false" << std::endl;
        return 1;
    }
}

/**
 * @brief CMD: search [--key PAT] [--val PAT] [-i]
 * Search for keys/values matching patterns.
 */
int cmd_search(confy::Config& cfg,
               const std::string& key_pattern,
               const std::string& val_pattern,
               bool ignore_case) {

    if (key_pattern.empty() && val_pattern.empty()) {
        std::cerr << color::red("Error: Please supply --key and/or --val pattern") << std::endl;
        return 1;
    }

    // Flatten config
    auto flat = flatten_config(cfg.data());

    // Find matches
    confy::Value matches = confy::Value::object();

    for (const auto& [k, v] : flat) {
        bool key_match = key_pattern.empty() || match_pattern(key_pattern, k, ignore_case);

        bool val_match = true;
        if (!val_pattern.empty() && key_match) {
            std::string val_str = v.is_string() ? v.get<std::string>() : v.dump();
            val_match = match_pattern(val_pattern, val_str, ignore_case);
        }

        if (key_match && val_match) {
            // Build nested structure for output
            confy::set_by_dot(matches, k, v, true);
        }
    }

    if (matches.empty()) {
        std::cout << "No matches found." << std::endl;
        return 1;
    }

    std::cout << matches.dump(2) << std::endl;
    return 0;
}

/**
 * @brief CMD: dump
 * Pretty-print entire config as JSON.
 */
int cmd_dump(confy::Config& cfg) {
    std::cout << cfg.to_json(2) << std::endl;
    return 0;
}

/**
 * @brief CMD: convert --to FORMAT [--out FILE]
 * Convert config to different format.
 */
int cmd_convert(confy::Config& cfg,
                const std::string& format,
                const std::string& output_file) {

    std::string fmt = to_lower(format);
    std::string output;

    if (fmt == "json") {
        output = cfg.to_json(2);
    } else if (fmt == "toml") {
        output = cfg.to_toml();
    } else {
        std::cerr << color::red("Error: Unknown format '" + format + "'. Use 'json' or 'toml'.") << std::endl;
        return 1;
    }

    if (output_file.empty()) {
        // Write to stdout
        std::cout << output << std::endl;
    } else {
        // Write to file
        std::ofstream file(output_file);
        if (!file) {
            std::cerr << color::red("Error: Cannot open file for writing: " + output_file) << std::endl;
            return 1;
        }
        file << output;
        std::cout << "Wrote " << to_lower(format) << " output to " << output_file << std::endl;
    }

    return 0;
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    try {
        // =====================================================================
        // Define CLI options
        // =====================================================================
        cxxopts::Options options("confy-cpp",
            "Configuration Management CLI\n\n"
            "Inspect & mutate JSON/TOML configs via dot-notation.\n"
            "Supports defaults, config files, .env files, environment variables, and overrides.");

        options.add_options()
            ("c,config", "Path to JSON/TOML config file",
                cxxopts::value<std::string>()->default_value(""))
            ("p,prefix", "Env-var prefix for overrides (e.g., MYAPP)",
                cxxopts::value<std::string>()->default_value(""))
            ("overrides", "Comma-separated key:value pairs",
                cxxopts::value<std::string>()->default_value(""))
            ("defaults", "Path to JSON file with default values",
                cxxopts::value<std::string>()->default_value(""))
            ("mandatory", "Comma-separated mandatory dot-keys",
                cxxopts::value<std::string>()->default_value(""))
            ("dotenv-path", "Explicit .env file path",
                cxxopts::value<std::string>()->default_value(""))
            ("no-dotenv", "Disable .env file loading",
                cxxopts::value<bool>()->default_value("false"))
            ("h,help", "Show help message")
            ("command", "Command to execute",
                cxxopts::value<std::string>()->default_value(""))
            ("args", "Command arguments",
                cxxopts::value<std::vector<std::string>>());

        // Positional arguments: command and args
        options.parse_positional({"command", "args"});
        options.positional_help("COMMAND [ARGS...]");

        // Allow unrecognized options for subcommand parsing
        options.allow_unrecognised_options();

        // =====================================================================
        // Parse global options
        // =====================================================================
        auto result = options.parse(argc, argv);

        // Show help if requested or no command
        if (result.count("help") || result["command"].as<std::string>().empty()) {
            std::cout << options.help() << std::endl;
            std::cout << "Commands:" << std::endl;
            std::cout << "  get KEY                Get value at dot-path" << std::endl;
            std::cout << "  set KEY VALUE          Set value in config file" << std::endl;
            std::cout << "  exists KEY             Check if key exists (exit 0/1)" << std::endl;
            std::cout << "  search [OPTIONS]       Search keys/values" << std::endl;
            std::cout << "    --key PATTERN        Pattern to match against keys" << std::endl;
            std::cout << "    --val PATTERN        Pattern to match against values" << std::endl;
            std::cout << "    -i, --ignore-case    Case-insensitive matching" << std::endl;
            std::cout << "  dump                   Print entire config as JSON" << std::endl;
            std::cout << "  convert [OPTIONS]      Convert to different format" << std::endl;
            std::cout << "    --to FORMAT          Target format (json or toml)" << std::endl;
            std::cout << "    --out FILE           Output file (default: stdout)" << std::endl;
            std::cout << std::endl;
            std::cout << "Examples:" << std::endl;
            std::cout << "  confy-cpp -c config.toml get database.host" << std::endl;
            std::cout << "  confy-cpp -c config.toml -p MYAPP dump" << std::endl;
            std::cout << "  confy-cpp -c config.json set db.port 5433" << std::endl;
            std::cout << "  confy-cpp -c config.toml search --key 'db.*'" << std::endl;
            std::cout << "  confy-cpp -c config.toml convert --to json --out config.json" << std::endl;
            return result.count("help") ? 0 : 1;
        }

        // Extract global options
        std::string config_path = result["config"].as<std::string>();
        std::string prefix = result["prefix"].as<std::string>();
        std::string overrides_str = result["overrides"].as<std::string>();
        std::string defaults_path = result["defaults"].as<std::string>();
        std::string mandatory_str = result["mandatory"].as<std::string>();
        std::string dotenv_path = result["dotenv-path"].as<std::string>();
        bool no_dotenv = result["no-dotenv"].as<bool>();

        std::string command = result["command"].as<std::string>();
        std::vector<std::string> args;
        if (result.count("args")) {
            args = result["args"].as<std::vector<std::string>>();
        }

        // Also include unrecognized options in args (for subcommand parsing)
        auto unmatched = result.unmatched();
        for (const auto& u : unmatched) {
            args.push_back(u);
        }

        // =====================================================================
        // Build LoadOptions
        // =====================================================================
        confy::LoadOptions opts;
        opts.file_path = config_path;

        if (!prefix.empty()) {
            opts.prefix = prefix;
        }

        opts.load_dotenv_file = !no_dotenv;
        opts.dotenv_path = dotenv_path;

        // Parse defaults file
        if (!defaults_path.empty()) {
            opts.defaults = load_defaults_file(defaults_path);
        }

        // Parse overrides
        opts.overrides = parse_overrides(overrides_str);

        // Parse mandatory keys
        opts.mandatory = parse_list(mandatory_str);

        // =====================================================================
        // Load configuration
        // =====================================================================
        confy::Config cfg;
        try {
            cfg = confy::Config::load(opts);
        } catch (const confy::MissingMandatoryConfig& e) {
            std::cerr << color::red("Error: ") << e.what() << std::endl;
            return 1;
        } catch (const confy::FileNotFoundError& e) {
            std::cerr << color::red("Error: ") << e.what() << std::endl;
            return 1;
        } catch (const confy::ConfigParseError& e) {
            std::cerr << color::red("Error: ") << e.what() << std::endl;
            return 1;
        }

        // =====================================================================
        // Dispatch command
        // =====================================================================
        std::string cmd = to_lower(command);

        if (cmd == "get") {
            if (args.empty()) {
                std::cerr << color::red("Error: 'get' requires KEY argument") << std::endl;
                return 1;
            }
            return cmd_get(cfg, args[0]);
        }
        else if (cmd == "set") {
            if (args.size() < 2) {
                std::cerr << color::red("Error: 'set' requires KEY and VALUE arguments") << std::endl;
                return 1;
            }
            return cmd_set(config_path, args[0], args[1]);
        }
        else if (cmd == "exists") {
            if (args.empty()) {
                std::cerr << color::red("Error: 'exists' requires KEY argument") << std::endl;
                return 1;
            }
            return cmd_exists(cfg, args[0]);
        }
        else if (cmd == "search") {
            // Parse search-specific options
            std::string key_pattern, val_pattern;
            bool ignore_case = false;

            for (size_t i = 0; i < args.size(); ++i) {
                if (args[i] == "--key" && i + 1 < args.size()) {
                    key_pattern = args[++i];
                } else if (args[i] == "--val" && i + 1 < args.size()) {
                    val_pattern = args[++i];
                } else if (args[i] == "-i" || args[i] == "--ignore-case") {
                    ignore_case = true;
                }
            }

            return cmd_search(cfg, key_pattern, val_pattern, ignore_case);
        }
        else if (cmd == "dump") {
            return cmd_dump(cfg);
        }
        else if (cmd == "convert") {
            // Parse convert-specific options
            std::string format, output_file;

            for (size_t i = 0; i < args.size(); ++i) {
                if (args[i] == "--to" && i + 1 < args.size()) {
                    format = args[++i];
                } else if (args[i] == "--out" && i + 1 < args.size()) {
                    output_file = args[++i];
                }
            }

            if (format.empty()) {
                std::cerr << color::red("Error: 'convert' requires --to FORMAT") << std::endl;
                return 1;
            }

            return cmd_convert(cfg, format, output_file);
        }
        else {
            std::cerr << color::red("Unknown command: " + command) << std::endl;
            std::cerr << "Use --help for available commands." << std::endl;
            return 1;
        }

    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << color::red("CLI Error: ") << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << color::red("Error: ") << e.what() << std::endl;
        return 1;
    }

    return 0;
}
