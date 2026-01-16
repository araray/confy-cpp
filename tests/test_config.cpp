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

#include "confy/Config.hpp"
#include "confy/Errors.hpp"
#include "confy/Value.hpp"
#include "confy/Loader.hpp"

#include <fstream>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;
using namespace confy;

// ============================================================================
// Test Utilities
// ============================================================================

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
// Construction Tests
// ============================================================================

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
    EXPECT_EQ(cfg.get("database.host"), "localhost");
    EXPECT_EQ(cfg.get("database.port"), 5432);
}

TEST(ConfigConstruction, ConstructFromNonObjectThrows) {
    Value arr = Value::array({1, 2, 3});
    EXPECT_THROW(Config cfg(arr), TypeError);
}

TEST(ConfigConstruction, CopyConstruction) {
    Config cfg1(Value{{"key", "value"}});
    Config cfg2(cfg1);

    EXPECT_EQ(cfg2.get("key"), "value");
}

TEST(ConfigConstruction, MoveConstruction) {
    Config cfg1(Value{{"key", "value"}});
    Config cfg2(std::move(cfg1));

    EXPECT_EQ(cfg2.get("key"), "value");
}

// ============================================================================
// Get Tests (RULE D1-D2)
// ============================================================================

TEST(ConfigGet, SimpleKey) {
    Config cfg(Value{{"key", "value"}});
    EXPECT_EQ(cfg.get("key"), "value");
}

TEST(ConfigGet, NestedKey) {
    Config cfg(Value{
        {"database", {{"host", "localhost"}}}
    });
    EXPECT_EQ(cfg.get("database.host"), "localhost");
}

TEST(ConfigGet, MissingKeyThrows) {
    Config cfg(Value{{"existing", "value"}});
    EXPECT_THROW(cfg.get("missing"), KeyError);
}

TEST(ConfigGet, WithDefaultValue) {
    Config cfg(Value{{"existing", "value"}});

    EXPECT_EQ(cfg.get<std::string>("missing", "default"), "default");
    EXPECT_EQ(cfg.get<int>("missing_int", 42), 42);
}

TEST(ConfigGet, TypeConversion) {
    Config cfg(Value{
        {"string", "hello"},
        {"integer", 42},
        {"floating", 3.14},
        {"boolean", true}
    });

    EXPECT_EQ(cfg.get<std::string>("string", ""), "hello");
    EXPECT_EQ(cfg.get<int>("integer", 0), 42);
    EXPECT_DOUBLE_EQ(cfg.get<double>("floating", 0.0), 3.14);
    EXPECT_EQ(cfg.get<bool>("boolean", false), true);
}

TEST(ConfigGet, AllTypes) {
    Config cfg(Value{
        {"null_val", nullptr},
        {"array", {1, 2, 3}},
        {"object", {{"nested", "value"}}}
    });

    EXPECT_TRUE(cfg.get("null_val").is_null());
    EXPECT_TRUE(cfg.get("array").is_array());
    EXPECT_EQ(cfg.get("object.nested"), "value");
}

// ============================================================================
// Contains Tests (RULE D5-D6)
// ============================================================================

TEST(ConfigContains, ExistingKey) {
    Config cfg(Value{{"key", "value"}});
    EXPECT_TRUE(cfg.contains("key"));
}

TEST(ConfigContains, MissingKey) {
    Config cfg(Value{{"existing", "value"}});
    EXPECT_FALSE(cfg.contains("missing"));
}

TEST(ConfigContains, NestedExisting) {
    Config cfg(Value{
        {"database", {{"host", "localhost"}}}
    });

    EXPECT_TRUE(cfg.contains("database"));
    EXPECT_TRUE(cfg.contains("database.host"));
    EXPECT_FALSE(cfg.contains("database.port"));
}

TEST(ConfigContains, TypeErrorOnNonContainer) {
    Config cfg(Value{{"key", 42}});
    EXPECT_THROW(cfg.contains("key.child"), TypeError);
}

// ============================================================================
// Mandatory Validation Tests (RULE M1-M3)
// ============================================================================

TEST(ConfigMandatory, AllKeysPresent) {
    LoadOptions opts;
    opts.defaults = {
        {"database", {{"host", "localhost"}}},
        {"api", {{"key", "secret"}}}
    };
    opts.mandatory = {"database.host", "api.key"};

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
            ASSERT_EQ(e.missing_keys().size(), 1);
            EXPECT_EQ(e.missing_keys()[0], "missing");
            throw;
        }
    }, MissingMandatoryConfig);
}

TEST(ConfigMandatory, MultipleMissingKeys) {
    LoadOptions opts;
    opts.defaults = {{"existing", "value"}};
    opts.mandatory = {"missing1", "missing2", "missing3"};

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
    opts.defaults = {{"key", 42}};
    opts.mandatory = {"key.child"};

    EXPECT_THROW(Config::load(opts), MissingMandatoryConfig);
}

// ============================================================================
// Precedence Tests (RULE P1)
// ============================================================================

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
    TempFile config_file("test_config_prec.json", R"({
        "key": "from_file",
        "only_file": "value"
    })");

    LoadOptions opts;
    opts.defaults = {{"key", "from_defaults"}, {"only_default", "value"}};
    opts.file_path = config_file.path();

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("key"), "from_file");
    EXPECT_EQ(cfg.get("only_default"), "value");
    EXPECT_EQ(cfg.get("only_file"), "value");
}

TEST(ConfigPrecedence, EnvOverridesFile) {
    TempFile config_file("test_config_env.json", R"({
        "database": {"host": "from_file"}
    })");

    EnvGuard env("TESTENV_DATABASE_HOST", "from_env");

    LoadOptions opts;
    opts.defaults = {{"database", {{"host", "from_defaults"}}}};
    opts.file_path = config_file.path();
    opts.prefix = "TESTENV";
    opts.load_dotenv_file = false;

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("database.host"), "from_env");
}

TEST(ConfigPrecedence, OverridesHighestPriority) {
    TempFile config_file("test_config_over.json", R"({
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

    EXPECT_EQ(cfg.get("key"), "from_overrides");
}

TEST(ConfigPrecedence, DeepMergePreservesUnchanged) {
    TempFile config_file("test_config_deep.json", R"({
        "database": {"port": 5433}
    })");

    LoadOptions opts;
    opts.defaults = {
        {"database", {
            {"host", "localhost"},
            {"port", 5432}
        }}
    };
    opts.file_path = config_file.path();

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("database.host"), "localhost");
    EXPECT_EQ(cfg.get("database.port"), 5433);
}

// ============================================================================
// Serialization Tests
// ============================================================================

TEST(ConfigSerialization, ToJson) {
    Config cfg(Value{
        {"string", "hello"},
        {"number", 42},
        {"nested", {{"key", "value"}}}
    });

    std::string json = cfg.to_json();

    EXPECT_NE(json.find("\"string\""), std::string::npos);
    EXPECT_NE(json.find("\"hello\""), std::string::npos);
    EXPECT_NE(json.find("42"), std::string::npos);
}

TEST(ConfigSerialization, ToJsonCompact) {
    Config cfg(Value{{"key", "value"}});

    std::string compact = cfg.to_json(-1);

    EXPECT_EQ(compact.find('\n'), std::string::npos);
}

TEST(ConfigSerialization, ToToml) {
    Config cfg(Value{
        {"string", "hello"},
        {"number", 42},
        {"nested", {{"key", "value"}}}
    });

    std::string toml = cfg.to_toml();

    EXPECT_NE(toml.find("string"), std::string::npos);
    EXPECT_NE(toml.find("hello"), std::string::npos);
    EXPECT_NE(toml.find("42"), std::string::npos);
}

// ============================================================================
// Merge Tests
// ============================================================================

TEST(ConfigMerge, MergeConfig) {
    Config cfg1({{"a", 1}, {"b", 2}});
    Config cfg2({{"b", 3}, {"c", 4}});

    cfg1.merge(cfg2);

    EXPECT_EQ(cfg1.get("a"), 1);
    EXPECT_EQ(cfg1.get("b"), 3);
    EXPECT_EQ(cfg1.get("c"), 4);
}

TEST(ConfigMerge, MergeValue) {
    Config cfg(Value{{"a", 1}});
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
    Config cfg(Value{
        {"database", {
            {"host", "localhost"},
            {"port", 5432}
        }}
    });

    Value override_val = {
        {"database", {{"port", 5433}}}
    };

    cfg.merge(override_val);

    EXPECT_EQ(cfg.get("database.host"), "localhost");
    EXPECT_EQ(cfg.get("database.port"), 5433);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(ConfigIntegration, FullPipeline) {
    TempFile config_file("test_integration.json", R"({
        "server": {
            "host": "0.0.0.0",
            "port": 8080
        }
    })");

    EnvGuard env("INTTEST_SERVER_HOST", "127.0.0.1");

    LoadOptions opts;
    opts.defaults = {
        {"server", {
            {"host", "localhost"},
            {"port", 3000},
            {"debug", false}
        }}
    };
    opts.file_path = config_file.path();
    opts.prefix = "INTTEST";
    opts.load_dotenv_file = false;
    opts.overrides = {{"server.debug", true}};
    opts.mandatory = {"server.host", "server.port"};

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("server.host"), "127.0.0.1");
    EXPECT_EQ(cfg.get("server.port"), 8080);
    EXPECT_EQ(cfg.get("server.debug"), true);
}

TEST(ConfigIntegration, TomlFile) {
    TempFile config_file("test_integration.toml", R"(
[database]
host = "localhost"
port = 5432
)");

    LoadOptions opts;
    opts.file_path = config_file.path();

    Config cfg = Config::load(opts);

    EXPECT_EQ(cfg.get("database.host"), "localhost");
    EXPECT_EQ(cfg.get("database.port"), 5432);
}
