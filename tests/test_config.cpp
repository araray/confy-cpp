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

// ============================================================================
// C-06: TOML key promotion can be switched off
// ============================================================================

TEST(ConfigLoad, TomlKeyPromotionIsOnByDefault) {
    TempFile file("confy_promotion_on.toml", "[section]\ndebug = true\n");
    LoadOptions opts;
    opts.file_path = file.path();
    opts.prefix = std::nullopt;
    opts.load_dotenv_file = false;
    opts.defaults = {{"debug", false}};
    Config cfg = Config::load(opts);
    EXPECT_TRUE(cfg.get<bool>("debug", false));        // promoted to the root
    EXPECT_FALSE(cfg.contains("section.debug"));
}

TEST(ConfigLoad, TomlKeyPromotionCanBeDisabled) {
    TempFile file("confy_promotion_off.toml", "[section]\ndebug = true\n");
    LoadOptions opts;
    opts.file_path = file.path();
    opts.prefix = std::nullopt;
    opts.load_dotenv_file = false;
    opts.defaults = {{"debug", false}};
    opts.toml_key_promotion = false;
    Config cfg = Config::load(opts);
    EXPECT_FALSE(cfg.get<bool>("debug", true));        // the default stays
    EXPECT_TRUE(cfg.get<bool>("section.debug", false)); // read as written
}

// ============================================================================
// C-01, C-02, C-11, C-12 (Sigilo phase 4)
// ============================================================================

TEST(ConfigLoad, SeveralFilesMergeInOrder) {
    TempFile sys("confy_c01_sys.json", R"({"db": {"host": "sys", "port": 1}, "a": 1})");
    TempFile user("confy_c01_user.toml", "[db]\nport = 2\n");
    TempFile expl("confy_c01_explicit.json", R"({"db": {"host": "explicit"}})");
    LoadOptions opts;
    opts.file_path = sys.path();
    opts.file_paths = {user.path(), expl.path()};
    opts.prefix = std::nullopt;
    opts.load_dotenv_file = false;
    Config cfg = Config::load(opts);
    EXPECT_EQ(cfg.get<std::string>("db.host", ""), "explicit");
    EXPECT_EQ(cfg.get<int>("db.port", 0), 2);
    EXPECT_EQ(cfg.get<int>("a", 0), 1);
    opts.file_paths.push_back((fs::temp_directory_path() / "confy_c01_missing.json").string());
    EXPECT_THROW(Config::load(opts), FileNotFoundError);
}

TEST(ConfigLoad, DotenvOverlayLeavesTheEnvironmentAlone) {
    TempFile env("confy_c02.env", "C02APP_DB_HOST=from-dotenv\nC02APP_DB_PORT=7000\nC02APP_ONLY_DOTENV=yes\n");
    ::unsetenv("C02APP_ONLY_DOTENV");
    EnvGuard g("C02APP_DB_PORT", "8000");  // the environment wins over .env (RULE P4)
    LoadOptions opts;
    opts.prefix = "C02APP";
    opts.load_dotenv_file = true;
    opts.dotenv_path = env.path();
    opts.dotenv_mode = LoadOptions::DotenvMode::overlay;
    opts.defaults = {{"db", {{"host", "default"}, {"port", 1}}}, {"only", {{"dotenv", "no"}}}};
    Config cfg = Config::load(opts);
    EXPECT_EQ(cfg.get<std::string>("db.host", ""), "from-dotenv");
    EXPECT_EQ(cfg.get<int>("db.port", 0), 8000);
    EXPECT_EQ(std::getenv("C02APP_ONLY_DOTENV"), nullptr);  // no setenv
    EXPECT_EQ(std::getenv("C02APP_DB_HOST"), nullptr);
}

TEST(ConfigLoad, DefaultDotenvIsSearchedInParentDirectories) {
    fs::path root = fs::temp_directory_path() / "confy_c02_tree";
    fs::create_directories(root / "a" / "b");
    { std::ofstream(root / ".env") << "C02P_NAME=parent\n"; }
    auto cwd = fs::current_path();
    fs::current_path(root / "a" / "b");
    LoadOptions opts;
    opts.prefix = "C02P";
    opts.load_dotenv_file = true;
    opts.dotenv_mode = LoadOptions::DotenvMode::overlay;
    opts.defaults = {{"name", "none"}};
    Config cfg = Config::load(opts);
    fs::current_path(cwd);
    fs::remove_all(root);
    EXPECT_EQ(cfg.get<std::string>("name", ""), "parent");
}

TEST(ConfigLoad, EmptyPrefixCanBeRejected) {
    LoadOptions opts;
    opts.prefix = "";
    opts.load_dotenv_file = false;
    opts.reject_empty_prefix = true;
    EXPECT_THROW(Config::load(opts), ConfigError);
    opts.prefix = "SOMEAPP";
    EXPECT_NO_THROW(Config::load(opts));
}

TEST(ConfigSerialization, RedactedOutput) {
    Config cfg(Value{{"db", {{"host", "h"}, {"password", "s3cret"}}}, {"token", "abc"}});
    std::string j = cfg.to_json_redacted({"db.password", "token", "no.such"});
    EXPECT_EQ(j.find("s3cret"), std::string::npos);
    EXPECT_EQ(j.find("abc"), std::string::npos);
    EXPECT_NE(j.find("\"h\""), std::string::npos);
    EXPECT_NE(j.find("***"), std::string::npos);
    std::string t = cfg.to_toml_redacted({"db.password"}, "<hidden>");
    EXPECT_EQ(t.find("s3cret"), std::string::npos);
    EXPECT_NE(t.find("<hidden>"), std::string::npos);
    EXPECT_NE(cfg.to_json().find("s3cret"), std::string::npos);  // the config itself is unchanged
}

// C-07: try_load returns structured errors instead of throwing.
TEST(ConfigTryLoad, ReportsEachKindOfFailure) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "confy_try_load";
    fs::create_directories(dir);

    confy::LoadOptions missing;
    missing.file_path = (dir / "does_not_exist.toml").string();
    missing.load_dotenv_file = false;
    auto r1 = confy::Config::try_load(missing);
    EXPECT_FALSE(r1.ok());
    ASSERT_TRUE(r1.error.has_value());
    EXPECT_EQ(r1.error->kind, confy::LoadError::Kind::file_not_found);
    EXPECT_NE(r1.error->file.find("does_not_exist.toml"), std::string::npos);

    const fs::path bad = dir / "bad.toml";
    { std::ofstream(bad) << "[section\nkey = = 1\n"; }
    confy::LoadOptions parse;
    parse.file_path = bad.string();
    parse.load_dotenv_file = false;
    auto r2 = confy::Config::try_load(parse);
    ASSERT_TRUE(r2.error.has_value());
    EXPECT_EQ(r2.error->kind, confy::LoadError::Kind::parse);
    EXPECT_FALSE(r2.error->message.empty());

    confy::LoadOptions mand;
    mand.load_dotenv_file = false;
    mand.mandatory = {"db.host"};
    auto r3 = confy::Config::try_load(mand);
    ASSERT_TRUE(r3.error.has_value());
    EXPECT_EQ(r3.error->kind, confy::LoadError::Kind::missing_mandatory);
    EXPECT_EQ(r3.error->key, "db.host");

    const fs::path good = dir / "good.toml";
    { std::ofstream(good) << "[db]\nhost = \"localhost\"\n"; }
    confy::LoadOptions ok;
    ok.file_path = good.string();
    ok.load_dotenv_file = false;
    ok.mandatory = {"db.host"};
    auto r4 = confy::Config::try_load(ok);
    ASSERT_TRUE(r4.ok());
    EXPECT_FALSE(r4.error.has_value());
    EXPECT_EQ(r4.config->get<std::string>("db.host", std::string()), "localhost");
    fs::remove_all(dir);
}

// C-05: values of string keys are not parsed (the "007" problem).
TEST(ConfigStringKeys, EnvironmentAndOverridesKeepStrings) {
    ::setenv("CFYT_REDACT_MASK__CHAR", "007", 1);
    ::setenv("CFYT_LIMITS_DEPTH", "007", 1);
    confy::LoadOptions o;
    o.load_dotenv_file = false;
    o.prefix = std::string("CFYT");
    o.defaults = confy::Value::object();
    o.defaults["redact"] = confy::Value::object();
    o.defaults["redact"]["mask_char"] = "*";
    o.defaults["limits"] = confy::Value::object();
    o.defaults["limits"]["depth"] = 4;
    o.defaults["name"] = "x";
    o.string_keys = {"redact.mask_char", "name"};
    o.overrides["name"] = confy::Value("0012");
    auto c = confy::Config::load(o);
    ::unsetenv("CFYT_REDACT_MASK__CHAR");
    ::unsetenv("CFYT_LIMITS_DEPTH");
    EXPECT_EQ(c.data()["redact"]["mask_char"].get<std::string>(), "007");  // kept
    EXPECT_EQ(c.data()["name"].get<std::string>(), "0012");                // kept
    EXPECT_EQ(c.data()["limits"]["depth"].get<long long>(), 7);           // other keys parse as before
    // Without string keys the old behaviour stays.
    ::setenv("CFYT_REDACT_MASK__CHAR", "007", 1);
    confy::LoadOptions p = o;
    p.string_keys.clear();
    p.overrides.clear();
    auto d = confy::Config::load(p);
    ::unsetenv("CFYT_REDACT_MASK__CHAR");
    EXPECT_TRUE(d.data()["redact"]["mask_char"].is_number());
}

// C-03: provenance of each leaf.
TEST(ConfigProvenance, EachLayerIsRecorded) {
    ::setenv("CFYP_LIMITS_MAX__DEPTH", "9", 1);
    confy::LoadOptions o;
    o.load_dotenv_file = false;
    o.prefix = std::string("CFYP");
    o.defaults = confy::Value::object();
    o.defaults["limits"] = confy::Value::object();
    o.defaults["limits"]["max_depth"] = 4;
    o.defaults["limits"]["width"] = 2;
    o.defaults["name"] = "x";
    o.defaults["mode"] = "a";
    o.overrides["mode"] = confy::Value("b");
    o.track_provenance = true;
    auto c = confy::Config::load(o);
    ::unsetenv("CFYP_LIMITS_MAX__DEPTH");
    EXPECT_EQ(c.source_of("limits.width").value_or(""), "defaults");
    EXPECT_EQ(c.source_of("limits.max_depth").value_or(""), "env:CFYP_LIMITS_MAX__DEPTH");
    EXPECT_EQ(c.source_of("mode").value_or(""), "override");
    EXPECT_FALSE(c.source_of("absent").has_value());
    o.track_provenance = false;
    EXPECT_TRUE(confy::Config::load(o).provenance().empty());
}
