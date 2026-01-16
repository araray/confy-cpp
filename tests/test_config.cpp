/**
 * @file test_config.cpp
 * @brief Unit tests for Config class (GoogleTest)
 *
 * Phase 3 tests covering Config class functionality:
 * - Construction and initialization
 * - Config::load() factory method
 * - Precedence ordering (defaults → file → .env → env → overrides)
 * - Dot-path access (get, set, contains)
 * - Mandatory key validation
 * - Serialization (JSON, TOML)
 * - Integration scenarios
 *
 * Test coverage for design spec rules:
 * - RULE P1: Precedence ordering
 * - RULE D1-D6: Dot-path access semantics
 * - RULE M1-M3: Mandatory key validation
 *
 * @copyright (c) 2026. MIT License.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "confy/Config.hpp"
#include "confy/Errors.hpp"
#include "confy/Value.hpp"
#include "confy/Loader.hpp"

#include <fstream>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;
using namespace confy;
using ::testing::HasSubstr;
using ::testing::UnorderedElementsAre;

// =============================================================================
// Test Utilities
// =============================================================================

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

// =============================================================================
// Construction Tests
// =============================================================================

TEST(ConfigConstruction, DefaultConstructor) {
    Config cfg;
    EXPECT_TRUE(cfg.empty());
    EXPECT_EQ(cfg.size(), 0);
    EXPECT_TRUE(cfg.data().is_object());
}

TEST(ConfigConstruction, ConstructFromValue) {
    Value data = {
        {"database", {
            {"host", "localhost"},
            {"port", 5432}
        }}
    };

    Config cfg(data);

    EXPECT_FALSE(cfg.empty());
    EXPECT_EQ(cfg.size(), 1);
    EXPECT_EQ(cfg.get("database.host"), "localhost");
    EXPECT_EQ(cfg.get("database.port"), 5432);
}

TEST(ConfigConstruction, ConstructFromNonObjectThrows) {
    Value arr = Value::array({1, 2, 3});
    EXPECT_THROW(Config cfg(arr), TypeError);

    Value str = "not an object";
    EXPECT_THROW(Config cfg(str), TypeError);
}

TEST(ConfigConstruction, CopyConstructor) {
    Config original;
    original.set("key", "value");

    Config copy(original);

    EXPECT_EQ(copy.get("key"), "value");

    // Modifying copy doesn't affect original
    copy.set("key", "modified");
    EXPECT_EQ(original.get("key"), "value");
    EXPECT_EQ(copy.get("key"), "modified");
}

TEST(ConfigConstruction, MoveConstructor) {
    Config original;
    original.set("key", "value");

    Config moved(std::move(original));

    EXPECT_EQ(moved.get("key"), "value");
}

// =============================================================================
// Get Tests (RULE D1-D2)
// =============================================================================

TEST(ConfigGet, SimpleKey) {
    Config cfg({{"key", "value"}});
    EXPECT_EQ(cfg.get("key"), "value");
}

TEST(ConfigGet, NestedKey) {
    Config cfg({
        {"database", {
            {"host", "localhost"},
            {"port", 5432}
        }}
    });

    EXPECT_EQ(cfg.get("database.host"), "localhost");
    EXPECT_EQ(cfg.get("database.port"), 5432);
}

TEST(ConfigGet, DeeplyNestedKey) {
    Config cfg({
        {"level1", {
            {"level2", {
                {"level3", {
                    {"value", 42}
                }}
            }}
        }}
    });

    EXPECT_EQ(cfg.get("level1.level2.level3.value"), 42);
}

TEST(ConfigGet, MissingKeyThrows) {
    Config cfg({{"existing", "value"}});

    EXPECT_THROW(cfg.get("nonexistent"), KeyError);
    EXPECT_THROW(cfg.get("existing.child"), TypeError);  // Can't traverse into string
}

TEST(ConfigGet, GetWithDefault) {
    Config cfg({{"existing", 42}});

    EXPECT_EQ(cfg.get<int>("existing", 0), 42);
    EXPECT_EQ(cfg.get<int>("missing", 99), 99);
    EXPECT_EQ(cfg.get<std::string>("missing", "default"), "default");
}

TEST(ConfigGet, GetWithDefaultStillThrowsOnTypeError) {
    Config cfg({{"key", 42}});  // integer, not object

    // RULE D2: TypeError propagates even with default
    EXPECT_THROW(cfg.get<int>("key.child", 0), TypeError);
}

TEST(ConfigGet, GetOptional) {
    Config cfg({{"existing", "value"}});

    auto existing = cfg.get_optional("existing");
    EXPECT_TRUE(existing.has_value());
    EXPECT_EQ(*existing, "value");

    auto missing = cfg.get_optional("nonexistent");
    EXPECT_FALSE(missing.has_value());
}

TEST(ConfigGet, GetOptionalThrowsOnTypeError) {
    Config cfg({{"key", 42}});

    // TypeError still propagates
    EXPECT_THROW(cfg.get_optional("key.child"), TypeError);
}

TEST(ConfigGet, GetTypedConversion) {
    Config cfg({
        {"string", "hello"},
        {"integer", 42},
        {"floating", 3.14},
        {"boolean", true},
        {"null_val", nullptr}
    });

    EXPECT_EQ(cfg.get<std::string>("string", ""), "hello");
    EXPECT_EQ(cfg.get<int>("integer", 0), 42);
    EXPECT_DOUBLE_EQ(cfg.get<double>("floating", 0.0), 3.14);
    EXPECT_EQ(cfg.get<bool>("boolean", false), true);
    EXPECT_TRUE(cfg.get("null_val").is_null());
}

// =============================================================================
// Set Tests (RULE D3-D4)
// =============================================================================

TEST(ConfigSet, SimpleKey) {
    Config cfg;
    cfg.set("key", "value");

    EXPECT_EQ(cfg.get("key"), "value");
}

TEST(ConfigSet, NestedKey) {
    Config cfg;
    cfg.set("database.host", "localhost");

    EXPECT_EQ(cfg.get("database.host"), "localhost");
}

TEST(ConfigSet, CreateMissingIntermediates) {
    Config cfg;
    cfg.set("a.b.c.d", 42, true);

    EXPECT_EQ(cfg.get("a.b.c.d"), 42);
    EXPECT_TRUE(cfg.contains("a"));
    EXPECT_TRUE(cfg.contains("a.b"));
    EXPECT_TRUE(cfg.contains("a.b.c"));
}

TEST(ConfigSet, CreateMissingFalseThrows) {
    Config cfg;

    EXPECT_THROW(cfg.set("nonexistent.key", "value", false), KeyError);
}

TEST(ConfigSet, OverwriteExisting) {
    Config cfg({{"key", "old"}});
    cfg.set("key", "new");

    EXPECT_EQ(cfg.get("key"), "new");
}

TEST(ConfigSet, OverwriteNestedExisting) {
    Config cfg({
        {"database", {{"host", "old"}}}
    });

    cfg.set("database.host", "new");

    EXPECT_EQ(cfg.get("database.host"), "new");
}

TEST(ConfigSet, SetVariousTypes) {
    Config cfg;

    cfg.set("string", "hello");
    cfg.set("int", 42);
    cfg.set("double", 3.14);
    cfg.set("bool", true);
    cfg.set("null", nullptr);
    cfg.set("array", Value::array({1, 2, 3}));
    cfg.set("object", Value({{"nested", "value"}}));

    EXPECT_EQ(cfg.get("string"), "hello");
    EXPECT_EQ(cfg.get("int"), 42);
    EXPECT_DOUBLE_EQ(cfg.get<double>("double", 0.0), 3.14);
    EXPECT_EQ(cfg.get("bool"), true);
    EXPECT_TRUE(cfg.get("null").is_null());
    EXPECT_TRUE(cfg.get("array").is_array());
    EXPECT_EQ(cfg.get("object.nested"), "value");
}

// =============================================================================
// Contains Tests (RULE D5-D6)
// =============================================================================

TEST(ConfigContains, ExistingKey) {
    Config cfg({{"key", "value"}});
    EXPECT_TRUE(cfg.contains("key"));
}

TEST(ConfigContains, MissingKey) {
    Config cfg({{"existing", "value"}});
    EXPECT_FALSE(cfg.contains("missing"));
}

TEST(ConfigContains, NestedExisting) {
    Config cfg({
        {"database", {{"host", "localhost"}}}
    });

    EXPECT_TRUE(cfg.contains("database"));
    EXPECT_TRUE(cfg.contains("database.host"));
    EXPECT_FALSE(cfg.contains("database.port"));
}

TEST(ConfigContains, TypeErrorOnNonContainer) {
    Config cfg({{"key", 42}});  // integer, not object

    // RULE D6: TypeError for traversing into non-container
    EXPECT_THROW(cfg.contains("key.child"), TypeError);
}

// =============================================================================
// Mandatory Validation Tests (RULE M1-M3)
// =============================================================================

TEST(ConfigMandatory, AllKeysPresent) {
    LoadOptions opts;
    opts.defaults = {
        {"database", {{"host", "localhost"}}},
        {"api", {{"key", "secret"}}}
    };
    opts.mandatory = {"database.host", "api.key"};

    // Should not throw
    EXPECT_NO_THROW(Config::load(opts));
}

TEST(ConfigMandatory, SingleMissingKey) {
    LoadOptions opts;
    opts.defaults = {{"existing", "value"}};
    opts.mandatory = {"missing"};

    EXPECT_THROW({
        try {
            Config::load(opts);
        } catch (const MissingMandatoryConfig& e) {
            EXPECT_THAT(e.missing_keys(), UnorderedElementsAre("missing"));
            throw;
        }
    }, MissingMandatoryConfig);
}

TEST(ConfigMandatory, MultipleMissingKeys) {
    LoadOptions opts;
    opts.defaults = {{"existing", "value"}};
    opts.mandatory = {"missing1", "missing2", "missing3"};

    // RULE M2: All missing keys collected
    EXPECT_THROW({
        try {
            Config::load(opts);
        } catch (const MissingMandatoryConfig& e) {
            EXPECT_EQ(e.missing_keys().size(), 3);
            throw;
        }
    }, MissingMandatoryConfig);
}

TEST(ConfigMandatory, PathIntoNonContainerIsMissing) {
    LoadOptions opts;
    opts.defaults = {{"key", 42}};  // integer
    opts.mandatory = {"key.child"};  // Can't traverse into integer

    // RULE M3: Path into non-container counts as missing
    EXPECT_THROW(Config::load(opts), MissingMandatoryConfig);
}

// =============================================================================
// Precedence Tests (RULE P1)
// =============================================================================

TEST(ConfigPrecedence, DefaultsOnly) {
    LoadOptions opts;
    opts.defaults = {
        {"key", "from_defaults"},
        {"only_default", "value"}
    };

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("key"), "from_defaults");
    EXPECT_EQ(cfg.get("only_default"), "value");
}

TEST(ConfigPrecedence, FileOverridesDefaults) {
    // Create temp JSON file
    TempFile config_file("test_config.json", R"({
        "key": "from_file",
        "only_file": "value"
    })");

    LoadOptions opts;
    opts.defaults = {{"key", "from_defaults"}, {"only_default", "value"}};
    opts.file_path = config_file.path();

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("key"), "from_file");  // File wins
    EXPECT_EQ(cfg.get("only_default"), "value");  // Preserved from defaults
    EXPECT_EQ(cfg.get("only_file"), "value");  // Added from file
}

TEST(ConfigPrecedence, EnvOverridesFile) {
    TempFile config_file("test_config2.json", R"({
        "database": {"host": "from_file"}
    })");

    EnvGuard env("TESTENV_DATABASE_HOST", "from_env");

    LoadOptions opts;
    opts.defaults = {{"database", {{"host", "from_defaults"}}}};
    opts.file_path = config_file.path();
    opts.prefix = "TESTENV";
    opts.load_dotenv_file = false;  // Don't search for .env

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("database.host"), "from_env");  // Env wins
}

TEST(ConfigPrecedence, OverridesHighestPriority) {
    TempFile config_file("test_config3.json", R"({
        "key": "from_file"
    })");

    EnvGuard env("TESTOV_KEY", "from_env");

    LoadOptions opts;
    opts.defaults = {{"key", "from_defaults"}};
    opts.file_path = config_file.path();
    opts.prefix = "TESTOV";
    opts.load_dotenv_file = false;
    opts.overrides = {{"key", "from_overrides"}};

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("key"), "from_overrides");  // Overrides win
}

TEST(ConfigPrecedence, DeepMergePreservesUnchanged) {
    TempFile config_file("test_config4.json", R"({
        "database": {"port": 5433}
    })");

    LoadOptions opts;
    opts.defaults = {
        {"database", {
            {"host", "localhost"},
            {"port", 5432},
            {"name", "mydb"}
        }}
    };
    opts.file_path = config_file.path();

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("database.host"), "localhost");  // From defaults
    EXPECT_EQ(cfg.get("database.port"), 5433);          // From file (overridden)
    EXPECT_EQ(cfg.get("database.name"), "mydb");        // From defaults
}

// =============================================================================
// File Loading Tests
// =============================================================================

TEST(ConfigLoad, JsonFile) {
    TempFile config_file("test.json", R"({
        "string": "hello",
        "number": 42,
        "nested": {"key": "value"}
    })");

    LoadOptions opts;
    opts.file_path = config_file.path();

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("string"), "hello");
    EXPECT_EQ(cfg.get("number"), 42);
    EXPECT_EQ(cfg.get("nested.key"), "value");
}

TEST(ConfigLoad, TomlFile) {
    TempFile config_file("test.toml", R"(
string = "hello"
number = 42

[nested]
key = "value"
)");

    LoadOptions opts;
    opts.file_path = config_file.path();

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("string"), "hello");
    EXPECT_EQ(cfg.get("number"), 42);
    EXPECT_EQ(cfg.get("nested.key"), "value");
}

TEST(ConfigLoad, MissingFileThrows) {
    LoadOptions opts;
    opts.file_path = "/nonexistent/path/config.json";

    EXPECT_THROW(Config::load(opts), FileNotFoundError);
}

TEST(ConfigLoad, EmptyFilePathNoFile) {
    LoadOptions opts;
    opts.file_path = "";  // Empty = no file
    opts.defaults = {{"key", "value"}};

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("key"), "value");  // Only defaults
}

TEST(ConfigLoad, InvalidJsonThrows) {
    TempFile config_file("invalid.json", "{ invalid json }");

    LoadOptions opts;
    opts.file_path = config_file.path();

    EXPECT_THROW(Config::load(opts), ConfigParseError);
}

// =============================================================================
// Environment Variable Tests
// =============================================================================

TEST(ConfigEnv, PrefixFiltering) {
    EnvGuard env1("MYPREFIX_DATABASE_HOST", "envhost");
    EnvGuard env2("OTHER_DATABASE_HOST", "other");  // Should be ignored

    LoadOptions opts;
    opts.defaults = {{"database", {{"host", "default"}}}};
    opts.prefix = "MYPREFIX";
    opts.load_dotenv_file = false;

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("database.host"), "envhost");
}

TEST(ConfigEnv, NulloptDisablesEnvLoading) {
    EnvGuard env("NOENV_KEY", "from_env");

    LoadOptions opts;
    opts.defaults = {{"key", "from_defaults"}};
    opts.prefix = std::nullopt;  // Disable env loading
    opts.load_dotenv_file = false;

    Config cfg = Config::load(opts);

    // Env should NOT be loaded
    EXPECT_EQ(cfg.get("key"), "from_defaults");
}

TEST(ConfigEnv, TypeParsing) {
    EnvGuard e1("TYPEP_BOOL_TRUE", "true");
    EnvGuard e2("TYPEP_BOOL_FALSE", "false");
    EnvGuard e3("TYPEP_INT", "42");
    EnvGuard e4("TYPEP_FLOAT", "3.14");
    EnvGuard e5("TYPEP_NULL", "null");
    EnvGuard e6("TYPEP_STRING", "hello");

    LoadOptions opts;
    opts.defaults = {
        {"bool", {{"true", false}, {"false", true}}},
        {"int", 0},
        {"float", 0.0},
        {"null", "not_null"},
        {"string", ""}
    };
    opts.prefix = "TYPEP";
    opts.load_dotenv_file = false;

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("bool.true"), true);
    EXPECT_EQ(cfg.get("bool.false"), false);
    EXPECT_EQ(cfg.get("int"), 42);
    EXPECT_DOUBLE_EQ(cfg.get<double>("float", 0.0), 3.14);
    EXPECT_TRUE(cfg.get("null").is_null());
    EXPECT_EQ(cfg.get("string"), "hello");
}

// =============================================================================
// Overrides Tests
// =============================================================================

TEST(ConfigOverrides, SimpleDotPath) {
    LoadOptions opts;
    opts.defaults = {{"key", "default"}};
    opts.overrides = {{"key", "override"}};

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("key"), "override");
}

TEST(ConfigOverrides, NestedDotPath) {
    LoadOptions opts;
    opts.defaults = {{"database", {{"host", "default"}}}};
    opts.overrides = {{"database.host", "override"}};

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("database.host"), "override");
}

TEST(ConfigOverrides, CreateNewKeys) {
    LoadOptions opts;
    opts.defaults = {{"existing", "value"}};
    opts.overrides = {
        {"new.nested.key", "created"},
        {"another", 42}
    };

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("new.nested.key"), "created");
    EXPECT_EQ(cfg.get("another"), 42);
}

TEST(ConfigOverrides, StringValueParsing) {
    LoadOptions opts;
    opts.overrides = {
        {"bool", "true"},
        {"int", "42"},
        {"float", "3.14"}
    };

    Config cfg = Config::load(opts);

    // String values in overrides should be parsed
    EXPECT_EQ(cfg.get("bool"), true);
    EXPECT_EQ(cfg.get("int"), 42);
}

// =============================================================================
// Serialization Tests
// =============================================================================

TEST(ConfigSerialization, ToJson) {
    Config cfg({
        {"string", "hello"},
        {"number", 42},
        {"nested", {{"key", "value"}}}
    });

    std::string json = cfg.to_json();

    EXPECT_THAT(json, HasSubstr("\"string\""));
    EXPECT_THAT(json, HasSubstr("\"hello\""));
    EXPECT_THAT(json, HasSubstr("42"));
}

TEST(ConfigSerialization, ToJsonCompact) {
    Config cfg({{"key", "value"}});

    std::string compact = cfg.to_json(-1);

    // Should not have newlines
    EXPECT_EQ(compact.find('\n'), std::string::npos);
}

TEST(ConfigSerialization, ToToml) {
    Config cfg({
        {"string", "hello"},
        {"number", 42},
        {"nested", {{"key", "value"}}}
    });

    std::string toml = cfg.to_toml();

    EXPECT_THAT(toml, HasSubstr("string"));
    EXPECT_THAT(toml, HasSubstr("hello"));
    EXPECT_THAT(toml, HasSubstr("42"));
}

// =============================================================================
// Merge Tests
// =============================================================================

TEST(ConfigMerge, MergeConfig) {
    Config cfg1({{"a", 1}, {"b", 2}});
    Config cfg2({{"b", 3}, {"c", 4}});

    cfg1.merge(cfg2);

    EXPECT_EQ(cfg1.get("a"), 1);  // Preserved
    EXPECT_EQ(cfg1.get("b"), 3);  // Overridden
    EXPECT_EQ(cfg1.get("c"), 4);  // Added
}

TEST(ConfigMerge, MergeValue) {
    Config cfg({{"a", 1}});
    Value val = {{"b", 2}};

    cfg.merge(val);

    EXPECT_EQ(cfg.get("a"), 1);
    EXPECT_EQ(cfg.get("b"), 2);
}

TEST(ConfigMerge, MergeNonObjectThrows) {
    Config cfg;
    Value arr = Value::array({1, 2, 3});

    EXPECT_THROW(cfg.merge(arr), TypeError);
}

TEST(ConfigMerge, DeepMerge) {
    Config cfg({
        {"database", {
            {"host", "localhost"},
            {"port", 5432}
        }}
    });

    Value override_val = {
        {"database", {{"port", 5433}}}
    };

    cfg.merge(override_val);

    EXPECT_EQ(cfg.get("database.host"), "localhost");  // Preserved
    EXPECT_EQ(cfg.get("database.port"), 5433);          // Overridden
}

// =============================================================================
// Integration Tests
// =============================================================================

TEST(ConfigIntegration, FullPipeline) {
    // Create config file
    TempFile config_file("integration.toml", R"(
[database]
host = "file.host"
port = 5432
name = "mydb"

[logging]
level = "INFO"
)");

    // Set environment variables
    EnvGuard env1("INTEG_DATABASE_PORT", "5433");
    EnvGuard env2("INTEG_LOGGING_LEVEL", "DEBUG");

    LoadOptions opts;
    opts.defaults = {
        {"database", {
            {"host", "default.host"},
            {"port", 3306},
            {"pool_size", 10}
        }},
        {"logging", {
            {"level", "WARNING"},
            {"format", "%(message)s"}
        }}
    };
    opts.file_path = config_file.path();
    opts.prefix = "INTEG";
    opts.load_dotenv_file = false;
    opts.overrides = {{"logging.format", "[%(level)s] %(message)s"}};
    opts.mandatory = {"database.host"};

    Config cfg = Config::load(opts);

    // Verify precedence chain
    EXPECT_EQ(cfg.get("database.host"), "file.host");       // From file
    EXPECT_EQ(cfg.get("database.port"), 5433);               // From env (overrides file)
    EXPECT_EQ(cfg.get("database.name"), "mydb");             // From file
    EXPECT_EQ(cfg.get("database.pool_size"), 10);            // From defaults
    EXPECT_EQ(cfg.get("logging.level"), "DEBUG");            // From env
    EXPECT_EQ(cfg.get("logging.format"), "[%(level)s] %(message)s");  // From overrides
}

TEST(ConfigIntegration, RealWorldScenario) {
    // Simulate a real application configuration
    TempFile config_file("app.json", R"({
        "server": {
            "host": "0.0.0.0",
            "port": 8080
        },
        "database": {
            "url": "postgres://localhost/app"
        },
        "features": {
            "new_ui": false
        }
    })");

    // Production environment overrides
    EnvGuard env1("APP_SERVER_PORT", "80");
    EnvGuard env2("APP_DATABASE_URL", "postgres://prod.db/app");
    EnvGuard env3("APP_FEATURES_NEW__UI", "true");  // __ -> _

    LoadOptions opts;
    opts.defaults = {
        {"server", {{"host", "127.0.0.1"}, {"port", 3000}}},
        {"database", {{"url", "sqlite:///app.db"}}},
        {"features", {{"new_ui", false}, {"beta", false}}}
    };
    opts.file_path = config_file.path();
    opts.prefix = "APP";
    opts.load_dotenv_file = false;
    opts.mandatory = {"server.host", "database.url"};

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("server.host"), "0.0.0.0");
    EXPECT_EQ(cfg.get("server.port"), 80);
    EXPECT_EQ(cfg.get("database.url"), "postgres://prod.db/app");
    EXPECT_EQ(cfg.get("features.new_ui"), true);
    EXPECT_EQ(cfg.get("features.beta"), false);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST(ConfigEdgeCases, EmptyConfig) {
    LoadOptions opts;
    Config cfg = Config::load(opts);

    EXPECT_TRUE(cfg.empty());
}

TEST(ConfigEdgeCases, EmptyStringKey) {
    Config cfg;
    cfg.set("", "value");

    // Empty string is a valid key
    EXPECT_EQ(cfg.get(""), "value");
}

TEST(ConfigEdgeCases, DotInKey) {
    // This tests that we're using dot as separator
    Config cfg;
    cfg.set("a.b", "value");

    // "a.b" should create nested structure
    EXPECT_TRUE(cfg.contains("a"));
    EXPECT_EQ(cfg.get("a.b"), "value");
}

TEST(ConfigEdgeCases, UnicodeValues) {
    Config cfg;
    cfg.set("greeting", "こんにちは");
    cfg.set("emoji", "🎉");

    EXPECT_EQ(cfg.get("greeting"), "こんにちは");
    EXPECT_EQ(cfg.get("emoji"), "🎉");
}

TEST(ConfigEdgeCases, LargeNumbers) {
    Config cfg;
    cfg.set("big_int", int64_t(9223372036854775807LL));
    cfg.set("big_float", 1.7976931348623157e+308);

    EXPECT_EQ(cfg.get<int64_t>("big_int", 0), 9223372036854775807LL);
}

TEST(ConfigEdgeCases, ArrayValues) {
    Config cfg;
    cfg.set("items", Value::array({1, 2, 3, 4, 5}));

    Value items = cfg.get("items");
    EXPECT_TRUE(items.is_array());
    EXPECT_EQ(items.size(), 5);
}

TEST(ConfigEdgeCases, NullValues) {
    Config cfg({{"key", nullptr}});

    EXPECT_TRUE(cfg.contains("key"));
    EXPECT_TRUE(cfg.get("key").is_null());
}

// =============================================================================
// Error Message Quality
// =============================================================================

TEST(ConfigErrors, KeyErrorContainsPath) {
    Config cfg;

    try {
        cfg.get("nonexistent.path");
        FAIL() << "Expected KeyError";
    } catch (const KeyError& e) {
        EXPECT_THAT(e.what(), HasSubstr("nonexistent.path"));
    }
}

TEST(ConfigErrors, MissingMandatoryListsAllKeys) {
    LoadOptions opts;
    opts.mandatory = {"key1", "key2", "key3"};

    try {
        Config::load(opts);
        FAIL() << "Expected MissingMandatoryConfig";
    } catch (const MissingMandatoryConfig& e) {
        std::string msg = e.what();
        EXPECT_THAT(msg, HasSubstr("key1"));
        EXPECT_THAT(msg, HasSubstr("key2"));
        EXPECT_THAT(msg, HasSubstr("key3"));
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
