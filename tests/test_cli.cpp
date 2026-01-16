/**
 * @file test_cli.cpp
 * @brief Unit tests for CLI functionality (GoogleTest)
 *
 * Phase 4 tests covering CLI commands:
 * - get: Retrieve value at dot-path
 * - set: Update config file
 * - exists: Check key existence
 * - search: Pattern matching
 * - dump: Print entire config
 * - convert: Format conversion
 *
 * Note: These tests verify the underlying functions used by the CLI,
 * not the full CLI binary. Integration tests for the full CLI should
 * be done via shell scripts or a test harness.
 *
 * @copyright (c) 2026. MIT License.
 */

#include <gtest/gtest.h>

#include "confy/Config.hpp"
#include "confy/Loader.hpp"
#include "confy/DotPath.hpp"
#include "confy/Parse.hpp"
#include "confy/Errors.hpp"

#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;
using namespace confy;

// ============================================================================
// Test Utilities
// ============================================================================

/**
 * @brief RAII wrapper for temporary files
 */
class TempFile {
public:
    explicit TempFile(const std::string& filename, const std::string& content)
        : path_(fs::temp_directory_path() / filename) {
        std::ofstream f(path_);
        f << content;
        f.close();
    }

    ~TempFile() {
        try {
            if (fs::exists(path_)) {
                fs::remove(path_);
            }
        } catch (...) {}
    }

    std::string path() const { return path_.string(); }

private:
    fs::path path_;
};

/**
 * @brief RAII wrapper for environment variable
 */
class EnvGuard {
public:
    EnvGuard(const std::string& name, const std::string& value)
        : name_(name), had_value_(false) {
#ifdef _WIN32
        const char* old = std::getenv(name.c_str());
        if (old) {
            old_value_ = old;
            had_value_ = true;
        }
        _putenv_s(name.c_str(), value.c_str());
#else
        const char* old = std::getenv(name.c_str());
        if (old) {
            old_value_ = old;
            had_value_ = true;
        }
        setenv(name.c_str(), value.c_str(), 1);
#endif
    }

    ~EnvGuard() {
#ifdef _WIN32
        if (had_value_) {
            _putenv_s(name_.c_str(), old_value_.c_str());
        } else {
            _putenv_s(name_.c_str(), "");
        }
#else
        if (had_value_) {
            setenv(name_.c_str(), old_value_.c_str(), 1);
        } else {
            unsetenv(name_.c_str());
        }
#endif
    }

private:
    std::string name_;
    std::string old_value_;
    bool had_value_;
};

// ============================================================================
// CLI Get Command Tests
// ============================================================================

TEST(CliGet, SimpleKey) {
    Config cfg(Value{{"key", "value"}});

    Value result = cfg.get("key");
    EXPECT_EQ(result, "value");
}

TEST(CliGet, NestedKey) {
    Config cfg(Value{
        {"database", {
            {"host", "localhost"},
            {"port", 5432}
        }}
    });

    EXPECT_EQ(cfg.get("database.host"), "localhost");
    EXPECT_EQ(cfg.get("database.port"), 5432);
}

TEST(CliGet, MissingKeyThrows) {
    Config cfg(Value{{"existing", "value"}});

    EXPECT_THROW(cfg.get("missing"), KeyError);
}

TEST(CliGet, AllTypes) {
    Config cfg(Value{
        {"string", "hello"},
        {"integer", 42},
        {"float", 3.14},
        {"bool_true", true},
        {"bool_false", false},
        {"null_val", nullptr},
        {"array", {1, 2, 3}},
        {"object", {{"nested", "value"}}}
    });

    EXPECT_EQ(cfg.get("string"), "hello");
    EXPECT_EQ(cfg.get("integer"), 42);
    EXPECT_DOUBLE_EQ(cfg.get("float").get<double>(), 3.14);
    EXPECT_EQ(cfg.get("bool_true"), true);
    EXPECT_EQ(cfg.get("bool_false"), false);
    EXPECT_TRUE(cfg.get("null_val").is_null());
    EXPECT_TRUE(cfg.get("array").is_array());
    EXPECT_TRUE(cfg.get("object").is_object());
}

// ============================================================================
// CLI Set Command Tests (file modification)
// ============================================================================

TEST(CliSet, SetInJsonFile) {
    TempFile file("test_cli_set.json", R"({"key": "original"})");

    // Load, modify, save
    Value data = load_json_file(file.path());
    EXPECT_EQ(data["key"], "original");

    set_by_dot(data, "key", Value("modified"), true);
    EXPECT_EQ(data["key"], "modified");
}

TEST(CliSet, SetNestedInJsonFile) {
    TempFile file("test_cli_set_nested.json", R"({"db": {"host": "old"}})");

    Value data = load_json_file(file.path());
    set_by_dot(data, "db.host", Value("new"), true);
    set_by_dot(data, "db.port", Value(5432), true);

    EXPECT_EQ(data["db"]["host"], "new");
    EXPECT_EQ(data["db"]["port"], 5432);
}

TEST(CliSet, CreateMissingPath) {
    Value data = Value::object();

    set_by_dot(data, "new.nested.path", Value("value"), true);

    EXPECT_TRUE(data.contains("new"));
    EXPECT_TRUE(data["new"].contains("nested"));
    EXPECT_EQ(data["new"]["nested"]["path"], "value");
}

TEST(CliSet, SetInTomlFile) {
    TempFile file("test_cli_set.toml", R"(
[database]
host = "localhost"
port = 5432
)");

    Value data = load_toml_file(file.path());
    EXPECT_EQ(data["database"]["host"], "localhost");

    set_by_dot(data, "database.host", Value("newhost"), true);
    EXPECT_EQ(data["database"]["host"], "newhost");
}

// ============================================================================
// CLI Exists Command Tests
// ============================================================================

TEST(CliExists, ExistingKey) {
    Config cfg(Value{{"key", "value"}});
    EXPECT_TRUE(cfg.contains("key"));
}

TEST(CliExists, MissingKey) {
    Config cfg(Value{{"existing", "value"}});
    EXPECT_FALSE(cfg.contains("missing"));
}

TEST(CliExists, NestedExisting) {
    Config cfg(Value{
        {"database", {{"host", "localhost"}}}
    });

    EXPECT_TRUE(cfg.contains("database"));
    EXPECT_TRUE(cfg.contains("database.host"));
    EXPECT_FALSE(cfg.contains("database.port"));
}

TEST(CliExists, NullValueExists) {
    Config cfg(Value{{"key", nullptr}});
    EXPECT_TRUE(cfg.contains("key"));
}

// ============================================================================
// CLI Search Tests (pattern matching utilities)
// ============================================================================

namespace {
// Helper: flatten config to vector of pairs
std::vector<std::pair<std::string, Value>> flatten_config(
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

std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// Simple substring match for testing
bool simple_match(const std::string& pattern, const std::string& text, bool ignore_case) {
    std::string p = ignore_case ? to_lower(pattern) : pattern;
    std::string t = ignore_case ? to_lower(text) : text;
    return t.find(p) != std::string::npos;
}
} // anonymous namespace

TEST(CliSearch, FlattenConfig) {
    Value data = {
        {"database", {
            {"host", "localhost"},
            {"port", 5432}
        }},
        {"debug", true}
    };

    auto flat = flatten_config(data);

    EXPECT_EQ(flat.size(), 3);

    // Check keys exist
    bool found_host = false, found_port = false, found_debug = false;
    for (const auto& [k, v] : flat) {
        if (k == "database.host") found_host = true;
        if (k == "database.port") found_port = true;
        if (k == "debug") found_debug = true;
    }
    EXPECT_TRUE(found_host);
    EXPECT_TRUE(found_port);
    EXPECT_TRUE(found_debug);
}

TEST(CliSearch, SimpleKeyMatch) {
    Value data = {
        {"database_host", "localhost"},
        {"database_port", 5432},
        {"other_key", "value"}
    };

    auto flat = flatten_config(data);

    int matches = 0;
    for (const auto& [k, v] : flat) {
        if (simple_match("database", k, false)) {
            matches++;
        }
    }
    EXPECT_EQ(matches, 2);
}

TEST(CliSearch, CaseInsensitiveMatch) {
    Value data = {
        {"Database_Host", "localhost"},
        {"OTHER_KEY", "value"}
    };

    auto flat = flatten_config(data);

    int matches = 0;
    for (const auto& [k, v] : flat) {
        if (simple_match("database", k, true)) {
            matches++;
        }
    }
    EXPECT_EQ(matches, 1);
}

TEST(CliSearch, ValueMatch) {
    Value data = {
        {"host", "localhost"},
        {"remote", "server.example.com"},
        {"port", 5432}
    };

    auto flat = flatten_config(data);

    int matches = 0;
    for (const auto& [k, v] : flat) {
        std::string val_str = v.is_string() ? v.get<std::string>() : v.dump();
        if (simple_match("local", val_str, false)) {
            matches++;
        }
    }
    EXPECT_EQ(matches, 1);
}

// ============================================================================
// CLI Dump Tests
// ============================================================================

TEST(CliDump, JsonOutput) {
    Config cfg(Value{
        {"key", "value"},
        {"number", 42}
    });

    std::string json = cfg.to_json(2);

    EXPECT_NE(json.find("\"key\""), std::string::npos);
    EXPECT_NE(json.find("\"value\""), std::string::npos);
    EXPECT_NE(json.find("42"), std::string::npos);
}

TEST(CliDump, CompactJsonOutput) {
    Config cfg(Value{{"key", "value"}});

    std::string json = cfg.to_json(-1);

    // Compact should have no newlines
    EXPECT_EQ(json.find('\n'), std::string::npos);
}

TEST(CliDump, NestedJsonOutput) {
    Config cfg(Value{
        {"database", {
            {"host", "localhost"},
            {"port", 5432}
        }}
    });

    std::string json = cfg.to_json(2);

    EXPECT_NE(json.find("\"database\""), std::string::npos);
    EXPECT_NE(json.find("\"host\""), std::string::npos);
    EXPECT_NE(json.find("\"localhost\""), std::string::npos);
}

// ============================================================================
// CLI Convert Tests
// ============================================================================

TEST(CliConvert, ToJson) {
    Config cfg(Value{
        {"key", "value"},
        {"number", 42}
    });

    std::string json = cfg.to_json(2);

    // Verify it's valid JSON by parsing
    auto parsed = nlohmann::json::parse(json);
    EXPECT_EQ(parsed["key"], "value");
    EXPECT_EQ(parsed["number"], 42);
}

TEST(CliConvert, ToToml) {
    Config cfg(Value{
        {"key", "value"},
        {"number", 42}
    });

    std::string toml = cfg.to_toml();

    // Basic TOML checks
    EXPECT_NE(toml.find("key"), std::string::npos);
    EXPECT_NE(toml.find("value"), std::string::npos);
    EXPECT_NE(toml.find("42"), std::string::npos);
}

TEST(CliConvert, ToTomlNested) {
    Config cfg(Value{
        {"database", {
            {"host", "localhost"},
            {"port", 5432}
        }}
    });

    std::string toml = cfg.to_toml();

    // TOML should have [database] section
    EXPECT_NE(toml.find("[database]"), std::string::npos);
    EXPECT_NE(toml.find("host"), std::string::npos);
    EXPECT_NE(toml.find("localhost"), std::string::npos);
}

// ============================================================================
// Overrides Parsing Tests
// ============================================================================

TEST(CliOverrides, ParseSimple) {
    // Test parse_value which is used for overrides
    EXPECT_EQ(parse_value("42"), 42);
    EXPECT_EQ(parse_value("true"), true);
    EXPECT_EQ(parse_value("false"), false);
    EXPECT_EQ(parse_value("null"), nullptr);
    EXPECT_EQ(parse_value("3.14").get<double>(), 3.14);
}

TEST(CliOverrides, ParseString) {
    EXPECT_EQ(parse_value("\"hello\""), "hello");
    EXPECT_EQ(parse_value("'hello'"), "hello");
    EXPECT_EQ(parse_value("hello"), "hello");  // Unquoted string
}

TEST(CliOverrides, ParseArray) {
    Value arr = parse_value("[1, 2, 3]");
    EXPECT_TRUE(arr.is_array());
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0], 1);
}

TEST(CliOverrides, ParseObject) {
    Value obj = parse_value(R"({"key": "value"})");
    EXPECT_TRUE(obj.is_object());
    EXPECT_EQ(obj["key"], "value");
}

// ============================================================================
// Full Config Loading Tests (simulating CLI workflow)
// ============================================================================

TEST(CliFullWorkflow, LoadWithDefaults) {
    LoadOptions opts;
    opts.defaults = {
        {"database", {{"host", "default_host"}, {"port", 5432}}}
    };

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("database.host"), "default_host");
    EXPECT_EQ(cfg.get("database.port"), 5432);
}

TEST(CliFullWorkflow, LoadWithFile) {
    TempFile file("test_cli_workflow.json", R"({
        "database": {"host": "file_host"}
    })");

    LoadOptions opts;
    opts.file_path = file.path();
    opts.defaults = {
        {"database", {{"host", "default_host"}, {"port", 5432}}}
    };

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("database.host"), "file_host");  // File overrides
    EXPECT_EQ(cfg.get("database.port"), 5432);         // From defaults
}

TEST(CliFullWorkflow, LoadWithOverrides) {
    TempFile file("test_cli_workflow2.json", R"({
        "database": {"host": "file_host"}
    })");

    LoadOptions opts;
    opts.file_path = file.path();
    opts.defaults = {{"database", {{"host", "default"}}}};
    opts.overrides = {{"database.host", "override_host"}};

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("database.host"), "override_host");  // Overrides win
}

TEST(CliFullWorkflow, LoadWithEnvVars) {
    TempFile file("test_cli_workflow3.json", R"({
        "database": {"host": "file_host"}
    })");

    EnvGuard env("TESTCLI_DATABASE_HOST", "env_host");

    LoadOptions opts;
    opts.file_path = file.path();
    opts.prefix = "TESTCLI";
    opts.load_dotenv_file = false;

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("database.host"), "env_host");  // Env overrides file
}

TEST(CliFullWorkflow, MandatoryKeyValidation) {
    LoadOptions opts;
    opts.defaults = {{"existing", "value"}};
    opts.mandatory = {"missing_key"};

    EXPECT_THROW(Config::load(opts), MissingMandatoryConfig);
}

TEST(CliFullWorkflow, MandatoryKeyPresent) {
    LoadOptions opts;
    opts.defaults = {{"required_key", "value"}};
    opts.mandatory = {"required_key"};

    EXPECT_NO_THROW(Config::load(opts));
}
