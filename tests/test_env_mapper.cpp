/**
 * @file test_env_mapper.cpp
 * @brief Unit tests for EnvMapper functionality (GoogleTest)
 *
 * Tests aligned with actual EnvMapper.cpp implementation.
 *
 * @copyright (c) 2026. MIT License.
 */

#include <gtest/gtest.h>
#include "confy/EnvMapper.hpp"
#include "confy/Value.hpp"

#include <cstdlib>

using namespace confy;

// ============================================================================
// Test Utilities
// ============================================================================

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
// System Variable Detection Tests
// ============================================================================

TEST(EnvMapperSystem, DetectsSystemVars) {
    EXPECT_TRUE(is_system_variable("PATH"));
    EXPECT_TRUE(is_system_variable("HOME"));
    EXPECT_TRUE(is_system_variable("USER"));
    EXPECT_TRUE(is_system_variable("SHELL"));
    EXPECT_TRUE(is_system_variable("LANG"));
    EXPECT_TRUE(is_system_variable("LC_ALL"));
    EXPECT_TRUE(is_system_variable("PWD"));
    EXPECT_TRUE(is_system_variable("SSH_AUTH_SOCK"));
    EXPECT_TRUE(is_system_variable("PYTHONPATH"));
}

TEST(EnvMapperSystem, AcceptsNonSystemVars) {
    EXPECT_FALSE(is_system_variable("MYAPP_DATABASE_HOST"));
    EXPECT_FALSE(is_system_variable("CONFIG_VALUE"));
    EXPECT_FALSE(is_system_variable("CUSTOM_VAR"));
}

TEST(EnvMapperSystem, CaseInsensitive) {
    EXPECT_TRUE(is_system_variable("path"));
    EXPECT_TRUE(is_system_variable("Path"));
    EXPECT_TRUE(is_system_variable("PATH"));
}

// ============================================================================
// RULE E4: Underscore Transformation Tests
// ============================================================================

TEST(EnvMapperTransform, SingleUnderscoreToDot) {
    EXPECT_EQ(transform_env_name("DATABASE_HOST"), "database.host");
    EXPECT_EQ(transform_env_name("API_KEY"), "api.key");
    EXPECT_EQ(transform_env_name("A_B_C"), "a.b.c");
}

TEST(EnvMapperTransform, DoubleUnderscoreToSingle) {
    // __ becomes _ (via marker replacement)
    std::string result1 = transform_env_name("FEATURE__FLAGS");
    EXPECT_EQ(result1, "feature_flags");

    std::string result2 = transform_env_name("MY__VAR");
    EXPECT_EQ(result2, "my_var");
}

TEST(EnvMapperTransform, MixedUnderscores) {
    // Mix of __ and _
    // FEATURE_FLAGS__BETA:
    //   lowercase: feature_flags__beta
    //   __ -> marker: feature_flags[marker]beta
    //   _ -> .: feature.flags[marker]beta
    //   marker -> _: feature.flags_beta
    EXPECT_EQ(transform_env_name("FEATURE_FLAGS__BETA"), "feature.flags_beta");

    // A__B_C:
    //   lowercase: a__b_c
    //   __ -> marker: a[marker]b_c
    //   _ -> .: a[marker]b.c
    //   marker -> _: a_b.c
    EXPECT_EQ(transform_env_name("A__B_C"), "a_b.c");

    // A_B__C:
    //   lowercase: a_b__c
    //   __ -> marker: a_b[marker]c
    //   _ -> .: a.b[marker]c
    //   marker -> _: a.b_c
    EXPECT_EQ(transform_env_name("A_B__C"), "a.b_c");
}

TEST(EnvMapperTransform, Lowercase) {
    EXPECT_EQ(transform_env_name("UPPERCASE"), "uppercase");
    EXPECT_EQ(transform_env_name("MixedCase"), "mixedcase");
}

TEST(EnvMapperTransform, NoUnderscores) {
    EXPECT_EQ(transform_env_name("simple"), "simple");
    EXPECT_EQ(transform_env_name("SIMPLE"), "simple");
}

TEST(EnvMapperTransform, MultipleConsecutive) {
    // Three underscores: ___ = __ + _
    //   After __ -> marker: [marker]_
    //   After _ -> .: [marker].
    //   After marker -> _: _.
    EXPECT_EQ(transform_env_name("A___B"), "a_.b");

    // Four underscores: ____ = __ + __
    //   After __ -> marker (both): [marker][marker]
    //   After _ -> .: no change
    //   After marker -> _: __
    EXPECT_EQ(transform_env_name("A____B"), "a__b");
}

// ============================================================================
// Prefix Stripping Tests
// ============================================================================

TEST(EnvMapperPrefix, StripSimple) {
    EXPECT_EQ(strip_prefix("MYAPP_DATABASE_HOST", "MYAPP"), "DATABASE_HOST");
    EXPECT_EQ(strip_prefix("APP_KEY", "APP"), "KEY");
}

TEST(EnvMapperPrefix, CaseInsensitive) {
    EXPECT_EQ(strip_prefix("myapp_key", "MYAPP"), "key");
    EXPECT_EQ(strip_prefix("MYAPP_KEY", "myapp"), "KEY");
}

TEST(EnvMapperPrefix, NoMatch) {
    EXPECT_EQ(strip_prefix("OTHER_KEY", "MYAPP"), "");
    EXPECT_EQ(strip_prefix("MYAPPKEY", "MYAPP"), "");  // Missing underscore
}

TEST(EnvMapperPrefix, EmptyPrefix) {
    EXPECT_EQ(strip_prefix("ANY_KEY", ""), "ANY_KEY");
}

// ============================================================================
// RULE E1-E3: Prefix Filtering Tests
// ============================================================================

TEST(EnvMapperCollect, NulloptDisablesLoading) {
    auto vars = collect_env_vars(std::nullopt);
    EXPECT_TRUE(vars.empty());
}

TEST(EnvMapperCollect, EmptyPrefixFiltersSystem) {
    EnvGuard env("CONFY_TEST_VAR_12345", "test_value");

    auto vars = collect_env_vars(std::string(""));

    // Should not include system vars like PATH
    bool found_path = false;
    bool found_test = false;
    for (const auto& [name, value] : vars) {
        if (name == "PATH") found_path = true;
        if (name == "CONFY_TEST_VAR_12345") found_test = true;
    }
    EXPECT_FALSE(found_path);
    EXPECT_TRUE(found_test);
}

TEST(EnvMapperCollect, NonEmptyPrefixFilters) {
    EnvGuard env1("TESTPREFIX_KEY1", "value1");
    EnvGuard env2("TESTPREFIX_KEY2", "value2");
    EnvGuard env3("OTHER_KEY", "other_value");

    auto vars = collect_env_vars(std::string("TESTPREFIX"));

    bool found_key1 = false;
    bool found_key2 = false;
    bool found_other = false;

    for (const auto& [name, value] : vars) {
        if (name == "TESTPREFIX_KEY1") found_key1 = true;
        if (name == "TESTPREFIX_KEY2") found_key2 = true;
        if (name == "OTHER_KEY") found_other = true;
    }

    EXPECT_TRUE(found_key1);
    EXPECT_TRUE(found_key2);
    EXPECT_FALSE(found_other);
}

// ============================================================================
// RULE E5: Flatten Keys Tests
// ============================================================================

TEST(EnvMapperFlatten, SimpleObject) {
    Value data = {
        {"key1", "value1"},
        {"key2", "value2"}
    };

    auto keys = flatten_keys(data);

    EXPECT_TRUE(keys.count("key1") > 0);
    EXPECT_TRUE(keys.count("key2") > 0);
}

TEST(EnvMapperFlatten, NestedObject) {
    Value data = {
        {"database", {
            {"host", "localhost"},
            {"port", 5432}
        }}
    };

    auto keys = flatten_keys(data);

    EXPECT_TRUE(keys.count("database") > 0);
    EXPECT_TRUE(keys.count("database.host") > 0);
    EXPECT_TRUE(keys.count("database.port") > 0);
}

TEST(EnvMapperFlatten, DeepNested) {
    Value data = {
        {"a", {
            {"b", {
                {"c", "value"}
            }}
        }}
    };

    auto keys = flatten_keys(data);

    EXPECT_TRUE(keys.count("a") > 0);
    EXPECT_TRUE(keys.count("a.b") > 0);
    EXPECT_TRUE(keys.count("a.b.c") > 0);
}

// ============================================================================
// RULE E6-E7: Remapping Tests
// ============================================================================

TEST(EnvMapperRemap, ExactMatch) {
    std::set<std::string> base_keys = {"database.host", "database.port"};

    std::string result = remap_env_key("database.host", base_keys, std::string("APP"), false);
    EXPECT_EQ(result, "database.host");
}

TEST(EnvMapperRemap, UnderscoreKey) {
    std::set<std::string> base_keys = {"feature_flags", "feature_flags.beta"};

    // "feature.flags" should remap to "feature_flags" if it exists in base
    std::string result = remap_env_key("feature.flags", base_keys, std::string("APP"), false);
    EXPECT_EQ(result, "feature_flags");
}

TEST(EnvMapperRemap, NestedUnderscoreKey) {
    std::set<std::string> base_keys = {"feature_flags", "feature_flags.beta"};

    std::string result = remap_env_key("feature.flags.beta", base_keys, std::string("APP"), false);
    EXPECT_EQ(result, "feature_flags.beta");
}

// ============================================================================
// Full Pipeline Tests
// ============================================================================

TEST(EnvMapperPipeline, LoadEnvVars) {
    EnvGuard env("TESTPIPE_DATABASE_HOST", "env_host");
    EnvGuard env2("TESTPIPE_DATABASE_PORT", "5433");

    Value defaults = {
        {"database", {
            {"host", "default_host"},
            {"port", 5432}
        }}
    };

    Value result = load_env_vars(
        std::string("TESTPIPE"),
        defaults,
        defaults,
        Value::object(),
        false
    );

    EXPECT_TRUE(result.is_object());
}

TEST(EnvMapperPipeline, ValueParsing) {
    EnvGuard env1("TESTPARSE_NUMBER", "42");
    EnvGuard env2("TESTPARSE_BOOL", "true");
    EnvGuard env3("TESTPARSE_STRING", "hello");

    Value base = {
        {"number", 0},
        {"bool", false},
        {"string", ""}
    };

    Value result = load_env_vars(
        std::string("TESTPARSE"),
        base,
        base,
        Value::object(),
        false
    );

    EXPECT_TRUE(result.is_object());
}
