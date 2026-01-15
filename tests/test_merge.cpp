/**
 * @file test_merge.cpp
 * @brief Tests for deep merge using Google Test
 *
 * Validates RULES P2 and P3 from CONFY_DESIGN_SPECIFICATION.md
 */

#include <gtest/gtest.h>
#include "confy/Merge.hpp"

using namespace confy;

// ============================================================================
// Simple merges
// ============================================================================

TEST(DeepMerge, BothEmpty) {
    Value base = Value::object();
    Value override = Value::object();
    auto result = deep_merge(base, override);
    EXPECT_TRUE(result.is_object());
    EXPECT_TRUE(result.empty());
}

TEST(DeepMerge, OverrideAddsNewKeys) {
    Value base = {{"a", 1}};
    Value override = {{"b", 2}};
    auto result = deep_merge(base, override);
    EXPECT_EQ(result["a"], 1);
    EXPECT_EQ(result["b"], 2);
}

TEST(DeepMerge, OverrideReplacesValues) {
    Value base = {{"a", 1}, {"b", 2}};
    Value override = {{"b", 3}, {"c", 4}};
    auto result = deep_merge(base, override);
    EXPECT_EQ(result["a"], 1);
    EXPECT_EQ(result["b"], 3);  // Replaced
    EXPECT_EQ(result["c"], 4);
}

// ============================================================================
// RULE P2: Recursive object merge
// ============================================================================

TEST(DeepMerge, NestedObjectsMerged) {
    Value base = {{"db", {{"host", "a"}, {"port", 1}}}};
    Value override = {{"db", {{"port", 2}}}};
    auto result = deep_merge(base, override);

    EXPECT_EQ(result["db"]["host"], "a");  // Preserved
    EXPECT_EQ(result["db"]["port"], 2);    // Replaced
}

TEST(DeepMerge, DeeplyNestedMerge) {
    Value base = {
        {"level1", {
            {"level2", {
                {"level3", {
                    {"keep", "this"},
                    {"replace", "old"}
                }}
            }}
        }}
    };
    Value override = {
        {"level1", {
            {"level2", {
                {"level3", {
                    {"replace", "new"},
                    {"add", "extra"}
                }}
            }}
        }}
    };
    auto result = deep_merge(base, override);

    EXPECT_EQ(result["level1"]["level2"]["level3"]["keep"], "this");
    EXPECT_EQ(result["level1"]["level2"]["level3"]["replace"], "new");
    EXPECT_EQ(result["level1"]["level2"]["level3"]["add"], "extra");
}

// ============================================================================
// RULE P3: Scalar replaces object
// ============================================================================

TEST(DeepMerge, StringReplacesObject) {
    Value base = {{"db", {{"host", "a"}}}};
    Value override = {{"db", "string"}};
    auto result = deep_merge(base, override);

    EXPECT_EQ(result["db"], "string");
    EXPECT_TRUE(result["db"].is_string());
}

TEST(DeepMerge, NumberReplacesObject) {
    Value base = {{"config", {{"nested", "data"}}}};
    Value override = {{"config", 42}};
    auto result = deep_merge(base, override);

    EXPECT_EQ(result["config"], 42);
    EXPECT_TRUE(result["config"].is_number());
}

TEST(DeepMerge, ArrayReplacesObject) {
    Value base = {{"items", {{"key", "value"}}}};
    Value override = {{"items", {1, 2, 3}}};
    auto result = deep_merge(base, override);

    EXPECT_TRUE(result["items"].is_array());
    EXPECT_EQ(result["items"].size(), 3);
}

// ============================================================================
// RULE P3: Object replaces scalar
// ============================================================================

TEST(DeepMerge, ObjectReplacesString) {
    Value base = {{"db", "string"}};
    Value override = {{"db", {{"host", "localhost"}}}};
    auto result = deep_merge(base, override);

    EXPECT_TRUE(result["db"].is_object());
    EXPECT_EQ(result["db"]["host"], "localhost");
}

TEST(DeepMerge, ObjectReplacesNumber) {
    Value base = {{"port", 5432}};
    Value override = {{"port", {{"value", 8080}}}};
    auto result = deep_merge(base, override);

    EXPECT_TRUE(result["port"].is_object());
    EXPECT_EQ(result["port"]["value"], 8080);
}

// ============================================================================
// deep_merge_all tests
// ============================================================================

TEST(DeepMergeAll, ThreeSources) {
    Value s1 = {{"a", 1}, {"b", 2}};
    Value s2 = {{"b", 3}, {"c", 4}};
    Value s3 = {{"c", 5}, {"d", 6}};

    auto result = deep_merge_all({s1, s2, s3});

    EXPECT_EQ(result["a"], 1);  // From s1
    EXPECT_EQ(result["b"], 3);  // From s2 (overrides s1)
    EXPECT_EQ(result["c"], 5);  // From s3 (overrides s2)
    EXPECT_EQ(result["d"], 6);  // From s3
}

TEST(DeepMergeAll, EmptySources) {
    auto result = deep_merge_all({});
    EXPECT_TRUE(result.is_object());
    EXPECT_TRUE(result.empty());
}

TEST(DeepMergeAll, SingleSource) {
    Value s1 = {{"a", 1}};
    auto result = deep_merge_all({s1});
    EXPECT_EQ(result, s1);
}

// ============================================================================
// Real-world scenario
// ============================================================================

TEST(DeepMerge, ConfigurationPrecedenceChain) {
    Value defaults = {
        {"database", {
            {"host", "localhost"},
            {"port", 5432},
            {"pool_size", 10}
        }},
        {"logging", {
            {"level", "INFO"}
        }}
    };

    Value config_file = {
        {"database", {
            {"host", "prod.db"},
            {"pool_size", 50}
        }}
    };

    Value env_overrides = {
        {"database", {
            {"port", 5433}
        }},
        {"logging", {
            {"level", "DEBUG"}
        }}
    };

    auto result = deep_merge(defaults, config_file);
    result = deep_merge(result, env_overrides);

    EXPECT_EQ(result["database"]["host"], "prod.db");      // From file
    EXPECT_EQ(result["database"]["port"], 5433);           // From env
    EXPECT_EQ(result["database"]["pool_size"], 50);        // From file
    EXPECT_EQ(result["logging"]["level"], "DEBUG");        // From env
}
