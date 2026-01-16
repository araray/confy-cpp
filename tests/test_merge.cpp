/**
 * @file test_merge.cpp
 * @brief Unit tests for deep merge functionality (GoogleTest)
 *
 * Tests covering merge rules P2-P3:
 * - P2: Object + Object = recursive merge
 * - P3: Non-object + any = replacement
 *
 * @copyright (c) 2026. MIT License.
 */

#include <gtest/gtest.h>
#include "confy/Merge.hpp"
#include "confy/Value.hpp"

using namespace confy;

// ============================================================================
// Basic Merge Tests
// ============================================================================

TEST(MergeBasic, EmptyObjects) {
    Value a = Value::object();
    Value b = Value::object();

    Value result = deep_merge(a, b);
    EXPECT_TRUE(result.is_object());
    EXPECT_TRUE(result.empty());
}

TEST(MergeBasic, MergeIntoEmpty) {
    Value a = Value::object();
    Value b = {{"key", "value"}};

    Value result = deep_merge(a, b);
    EXPECT_EQ(result["key"], "value");
}

TEST(MergeBasic, MergeEmptyInto) {
    Value a = {{"key", "value"}};
    Value b = Value::object();

    Value result = deep_merge(a, b);
    EXPECT_EQ(result["key"], "value");
}

TEST(MergeBasic, NonOverlappingKeys) {
    Value a = {{"key1", "value1"}};
    Value b = {{"key2", "value2"}};

    Value result = deep_merge(a, b);
    EXPECT_EQ(result["key1"], "value1");
    EXPECT_EQ(result["key2"], "value2");
}

// ============================================================================
// Scalar Replacement Tests (RULE P3)
// ============================================================================

TEST(MergeScalar, StringReplacement) {
    Value a = {{"key", "old"}};
    Value b = {{"key", "new"}};

    Value result = deep_merge(a, b);
    EXPECT_EQ(result["key"], "new");
}

TEST(MergeScalar, IntegerReplacement) {
    Value a = {{"num", 1}};
    Value b = {{"num", 2}};

    Value result = deep_merge(a, b);
    EXPECT_EQ(result["num"], 2);
}

TEST(MergeScalar, BooleanReplacement) {
    Value a = {{"flag", true}};
    Value b = {{"flag", false}};

    Value result = deep_merge(a, b);
    EXPECT_EQ(result["flag"], false);
}

TEST(MergeScalar, NullReplacement) {
    Value a = {{"val", "something"}};
    Value b = {{"val", nullptr}};

    Value result = deep_merge(a, b);
    EXPECT_TRUE(result["val"].is_null());
}

TEST(MergeScalar, TypeChange) {
    Value a = {{"val", "string"}};
    Value b = {{"val", 42}};

    Value result = deep_merge(a, b);
    EXPECT_EQ(result["val"], 42);
}

// ============================================================================
// Array Replacement Tests
// ============================================================================

TEST(MergeArray, ArrayReplacement) {
    // Arrays are replaced, not merged
    Value a = {{"arr", {1, 2, 3}}};
    Value b = {{"arr", {4, 5}}};

    Value result = deep_merge(a, b);
    EXPECT_EQ(result["arr"].size(), 2);
    EXPECT_EQ(result["arr"][0], 4);
    EXPECT_EQ(result["arr"][1], 5);
}

TEST(MergeArray, ArrayToScalar) {
    Value a = {{"val", {1, 2, 3}}};
    Value b = {{"val", "scalar"}};

    Value result = deep_merge(a, b);
    EXPECT_EQ(result["val"], "scalar");
}

TEST(MergeArray, ScalarToArray) {
    Value a = {{"val", "scalar"}};
    Value b = {{"val", {1, 2, 3}}};

    Value result = deep_merge(a, b);
    EXPECT_TRUE(result["val"].is_array());
    EXPECT_EQ(result["val"].size(), 3);
}

// ============================================================================
// Recursive Object Merge Tests (RULE P2)
// ============================================================================

TEST(MergeNested, SimpleNested) {
    Value a = {
        {"outer", {{"inner1", "a"}}}
    };
    Value b = {
        {"outer", {{"inner2", "b"}}}
    };

    Value result = deep_merge(a, b);
    EXPECT_EQ(result["outer"]["inner1"], "a");
    EXPECT_EQ(result["outer"]["inner2"], "b");
}

TEST(MergeNested, DeepNested) {
    Value a = {
        {"level1", {
            {"level2", {
                {"level3", "a"}
            }}
        }}
    };
    Value b = {
        {"level1", {
            {"level2", {
                {"other", "b"}
            }}
        }}
    };

    Value result = deep_merge(a, b);
    EXPECT_EQ(result["level1"]["level2"]["level3"], "a");
    EXPECT_EQ(result["level1"]["level2"]["other"], "b");
}

TEST(MergeNested, PartialOverride) {
    Value a = {
        {"database", {
            {"host", "localhost"},
            {"port", 5432},
            {"user", "admin"}
        }}
    };
    Value b = {
        {"database", {
            {"port", 5433}  // Only override port
        }}
    };

    Value result = deep_merge(a, b);
    EXPECT_EQ(result["database"]["host"], "localhost");  // Preserved
    EXPECT_EQ(result["database"]["port"], 5433);          // Overridden
    EXPECT_EQ(result["database"]["user"], "admin");       // Preserved
}

TEST(MergeNested, ObjectToScalar) {
    Value a = {
        {"key", {{"nested", "value"}}}
    };
    Value b = {
        {"key", "scalar"}
    };

    // Object replaced by scalar
    Value result = deep_merge(a, b);
    EXPECT_EQ(result["key"], "scalar");
}

TEST(MergeNested, ScalarToObject) {
    Value a = {
        {"key", "scalar"}
    };
    Value b = {
        {"key", {{"nested", "value"}}}
    };

    // Scalar replaced by object
    Value result = deep_merge(a, b);
    EXPECT_TRUE(result["key"].is_object());
    EXPECT_EQ(result["key"]["nested"], "value");
}

// ============================================================================
// Multi-Source Merge Tests
// ============================================================================

TEST(MergeMulti, ThreeSources) {
    Value a = {{"key1", "a"}, {"shared", "a"}};
    Value b = {{"key2", "b"}, {"shared", "b"}};
    Value c = {{"key3", "c"}, {"shared", "c"}};

    Value result = deep_merge(deep_merge(a, b), c);

    EXPECT_EQ(result["key1"], "a");
    EXPECT_EQ(result["key2"], "b");
    EXPECT_EQ(result["key3"], "c");
    EXPECT_EQ(result["shared"], "c");  // Last one wins
}

TEST(MergeMulti, ChainedNestedMerge) {
    Value defaults = {
        {"database", {
            {"host", "localhost"},
            {"port", 5432}
        }},
        {"logging", {
            {"level", "INFO"}
        }}
    };

    Value file_config = {
        {"database", {
            {"port", 5433}
        }},
        {"cache", {
            {"enabled", true}
        }}
    };

    Value env_override = {
        {"database", {
            {"host", "prod.example.com"}
        }}
    };

    Value result = deep_merge(deep_merge(defaults, file_config), env_override);

    EXPECT_EQ(result["database"]["host"], "prod.example.com");  // From env
    EXPECT_EQ(result["database"]["port"], 5433);                 // From file
    EXPECT_EQ(result["logging"]["level"], "INFO");               // From defaults
    EXPECT_EQ(result["cache"]["enabled"], true);                 // From file
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(MergeEdgeCases, NullBase) {
    Value a = nullptr;
    Value b = {{"key", "value"}};

    Value result = deep_merge(a, b);
    // null is replaced by object
    EXPECT_TRUE(result.is_object());
    EXPECT_EQ(result["key"], "value");
}

TEST(MergeEdgeCases, NullOverride) {
    Value a = {{"key", "value"}};
    Value b = nullptr;

    Value result = deep_merge(a, b);
    // object is replaced by null
    EXPECT_TRUE(result.is_null());
}

TEST(MergeEdgeCases, EmptyStringKey) {
    Value a = {{"", "empty_key_a"}};
    Value b = {{"", "empty_key_b"}};

    Value result = deep_merge(a, b);
    EXPECT_EQ(result[""], "empty_key_b");
}

TEST(MergeEdgeCases, SpecialCharacterKeys) {
    Value a = {{"key.with.dots", "a"}};
    Value b = {{"key.with.dots", "b"}};

    Value result = deep_merge(a, b);
    EXPECT_EQ(result["key.with.dots"], "b");
}

// ============================================================================
// Real-World Configuration Scenarios
// ============================================================================

TEST(MergeRealWorld, DatabaseConfig) {
    Value defaults = {
        {"database", {
            {"host", "localhost"},
            {"port", 5432},
            {"name", "myapp"},
            {"pool", {
                {"min", 5},
                {"max", 20}
            }}
        }}
    };

    Value production = {
        {"database", {
            {"host", "prod-db.example.com"},
            {"pool", {
                {"max", 50}
            }}
        }}
    };

    Value result = deep_merge(defaults, production);

    EXPECT_EQ(result["database"]["host"], "prod-db.example.com");
    EXPECT_EQ(result["database"]["port"], 5432);
    EXPECT_EQ(result["database"]["name"], "myapp");
    EXPECT_EQ(result["database"]["pool"]["min"], 5);
    EXPECT_EQ(result["database"]["pool"]["max"], 50);
}

TEST(MergeRealWorld, FeatureFlags) {
    Value base = {
        {"features", {
            {"new_ui", false},
            {"beta_api", false},
            {"analytics", true}
        }}
    };

    Value overrides = {
        {"features", {
            {"beta_api", true}
        }}
    };

    Value result = deep_merge(base, overrides);

    EXPECT_EQ(result["features"]["new_ui"], false);
    EXPECT_EQ(result["features"]["beta_api"], true);
    EXPECT_EQ(result["features"]["analytics"], true);
}
