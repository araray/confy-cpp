/**
 * @file test_dotpath.cpp
 * @brief Tests for dot-path utilities using Google Test
 *
 * Validates RULES D1-D6 from CONFY_DESIGN_SPECIFICATION.md
 */

#include <gtest/gtest.h>
#include "confy/DotPath.hpp"
#include "confy/Errors.hpp"

using namespace confy;

// ============================================================================
// split_dot_path tests
// ============================================================================

TEST(SplitDotPath, EmptyPath) {
    auto result = split_dot_path("");
    EXPECT_TRUE(result.empty());
}

TEST(SplitDotPath, SingleSegment) {
    auto result = split_dot_path("key");
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "key");
}

TEST(SplitDotPath, TwoSegments) {
    auto result = split_dot_path("a.b");
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
}

TEST(SplitDotPath, NestedPath) {
    auto result = split_dot_path("database.connection.host");
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "database");
    EXPECT_EQ(result[1], "connection");
    EXPECT_EQ(result[2], "host");
}

TEST(SplitDotPath, ArrayIndex) {
    auto result = split_dot_path("handlers.0.type");
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "handlers");
    EXPECT_EQ(result[1], "0");
    EXPECT_EQ(result[2], "type");
}

// ============================================================================
// join_dot_path tests
// ============================================================================

TEST(JoinDotPath, EmptySegments) {
    auto result = join_dot_path({});
    EXPECT_EQ(result, "");
}

TEST(JoinDotPath, SingleSegment) {
    auto result = join_dot_path({"key"});
    EXPECT_EQ(result, "key");
}

TEST(JoinDotPath, MultipleSegments) {
    auto result = join_dot_path({"a", "b", "c"});
    EXPECT_EQ(result, "a.b.c");
}

// ============================================================================
// get_by_dot - simple access tests
// ============================================================================

class GetByDotTest : public ::testing::Test {
protected:
    Value data = {
        {"simple", "value"},
        {"nested", {
            {"key", 42},
            {"deep", {
                {"path", true}
            }}
        }},
        {"array", {1, 2, 3}}
    };
};

TEST_F(GetByDotTest, SimpleKey) {
    auto* result = get_by_dot(data, "simple");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "value");
}

TEST_F(GetByDotTest, NestedKey) {
    auto* result = get_by_dot(data, "nested.key");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, 42);
}

TEST_F(GetByDotTest, DeeplyNested) {
    auto* result = get_by_dot(data, "nested.deep.path");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, true);
}

TEST_F(GetByDotTest, EmptyPathReturnsRoot) {
    auto* result = get_by_dot(data, "");
    EXPECT_EQ(result, &data);
}

TEST_F(GetByDotTest, ArrayAccess) {
    auto* result = get_by_dot(data, "array.1");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, 2);
}

// ============================================================================
// get_by_dot - RULE D1: errors without default
// ============================================================================

class GetByDotErrorTest : public ::testing::Test {
protected:
    Value data = {
        {"db", {{"host", "localhost"}}},
        {"scalar", 42}
    };
};

TEST_F(GetByDotErrorTest, MissingKeyRaisesKeyError) {
    EXPECT_THROW(get_by_dot(data, "missing"), KeyError);
}

TEST_F(GetByDotErrorTest, MissingNestedKeyRaisesKeyError) {
    EXPECT_THROW(get_by_dot(data, "db.port"), KeyError);
}

TEST_F(GetByDotErrorTest, TraverseIntoScalarRaisesTypeError) {
    EXPECT_THROW(get_by_dot(data, "scalar.sub"), TypeError);
}

TEST_F(GetByDotErrorTest, ArrayOutOfBoundsRaisesKeyError) {
    Value arr_data = {{"items", {1, 2, 3}}};
    EXPECT_THROW(get_by_dot(arr_data, "items.10"), KeyError);
}

// ============================================================================
// get_by_dot - RULE D2: with default
// ============================================================================

class GetByDotDefaultTest : public ::testing::Test {
protected:
    Value data = {
        {"db", {{"host", "localhost"}}},
        {"scalar", 42}
    };
    Value default_val = "default";
};

TEST_F(GetByDotDefaultTest, ReturnsDefaultForMissingKey) {
    auto* result = get_by_dot(data, "missing", default_val);
    EXPECT_EQ(result, &default_val);
}

TEST_F(GetByDotDefaultTest, ReturnsDefaultForMissingNestedKey) {
    auto* result = get_by_dot(data, "db.port", default_val);
    EXPECT_EQ(result, &default_val);
}

TEST_F(GetByDotDefaultTest, ReturnsValueWhenFound) {
    auto* result = get_by_dot(data, "db.host", default_val);
    EXPECT_EQ(*result, "localhost");
}

TEST_F(GetByDotDefaultTest, StillRaisesTypeErrorForNonContainer) {
    EXPECT_THROW(get_by_dot(data, "scalar.sub", default_val), TypeError);
}

// ============================================================================
// set_by_dot - RULE D3: without create_missing
// ============================================================================

TEST(SetByDotWithoutCreate, SetExistingKey) {
    Value data = {{"db", {{"host", "old"}}}};
    set_by_dot(data, "db.host", "new", false);
    EXPECT_EQ(data["db"]["host"], "new");
}

TEST(SetByDotWithoutCreate, RaisesErrorForMissingIntermediate) {
    Value data = Value::object();
    EXPECT_THROW(set_by_dot(data, "missing.key", "value", false), KeyError);
}

TEST(SetByDotWithoutCreate, RaisesTypeErrorForNonObjectIntermediate) {
    Value data = {{"scalar", 42}};
    EXPECT_THROW(set_by_dot(data, "scalar.key", "value", false), TypeError);
}

// ============================================================================
// set_by_dot - RULE D4: with create_missing
// ============================================================================

TEST(SetByDotWithCreate, CreatesMissingIntermediates) {
    Value data = Value::object();
    set_by_dot(data, "a.b.c", 123, true);
    EXPECT_EQ(data["a"]["b"]["c"], 123);
}

TEST(SetByDotWithCreate, OverwritesNonObjectIntermediate) {
    Value data = {{"path", "scalar"}};
    set_by_dot(data, "path.key", "value", true);
    EXPECT_EQ(data["path"]["key"], "value");
}

TEST(SetByDotWithCreate, PreservesExistingStructure) {
    Value data = {{"db", {{"host", "old"}, {"port", 5432}}}};
    set_by_dot(data, "db.user", "admin", true);
    EXPECT_EQ(data["db"]["host"], "old");
    EXPECT_EQ(data["db"]["port"], 5432);
    EXPECT_EQ(data["db"]["user"], "admin");
}

TEST(SetByDotWithCreate, EmptyPathReplacesRoot) {
    Value data = {{"old", "data"}};
    set_by_dot(data, "", Value({{"new", "data"}}), true);
    EXPECT_EQ(data["new"], "data");
    EXPECT_FALSE(data.contains("old"));
}

// ============================================================================
// contains_dot - RULE D5: basic existence check
// ============================================================================

class ContainsDotTest : public ::testing::Test {
protected:
    Value data = {
        {"simple", "value"},
        {"nested", {{"key", 42}}},
        {"array", {1, 2, 3}}
    };
};

TEST_F(ContainsDotTest, ReturnsTrueForExistingKey) {
    EXPECT_TRUE(contains_dot(data, "simple"));
    EXPECT_TRUE(contains_dot(data, "nested.key"));
    EXPECT_TRUE(contains_dot(data, "array.1"));
}

TEST_F(ContainsDotTest, ReturnsFalseForMissingKey) {
    EXPECT_FALSE(contains_dot(data, "missing"));
    EXPECT_FALSE(contains_dot(data, "nested.missing"));
    EXPECT_FALSE(contains_dot(data, "array.10"));
}

TEST_F(ContainsDotTest, EmptyPathAlwaysExists) {
    EXPECT_TRUE(contains_dot(data, ""));
}

// ============================================================================
// contains_dot - RULE D6: TypeError before final segment
// ============================================================================

TEST(ContainsDotError, RaisesTypeErrorForScalarTraversal) {
    Value data = {{"scalar", 42}};
    EXPECT_THROW(contains_dot(data, "scalar.sub"), TypeError);
}

TEST(ContainsDotError, RaisesTypeErrorForStringTraversal) {
    Value data = {{"string", "hello"}};
    EXPECT_THROW(contains_dot(data, "string.sub"), TypeError);
}

// ============================================================================
// Complex scenarios
// ============================================================================

TEST(DotPathComplex, NestedArrayAccess) {
    Value data = {
        {"matrix", {
            {1, 2, 3},
            {4, 5, 6},
            {7, 8, 9}
        }}
    };

    auto* result = get_by_dot(data, "matrix.1.2");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, 6);
}

TEST(DotPathComplex, MixedObjectArrayAccess) {
    Value data = {
        {"items", {
            {{"id", 1}, {"name", "first"}},
            {{"id", 2}, {"name", "second"}}
        }}
    };

    auto* result = get_by_dot(data, "items.1.name");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "second");
}

TEST(DotPathComplex, SetCreatesNestedStructure) {
    Value data = Value::object();
    set_by_dot(data, "level1.level2.level3.value", "deep", true);

    EXPECT_TRUE(data["level1"].is_object());
    EXPECT_TRUE(data["level1"]["level2"].is_object());
    EXPECT_TRUE(data["level1"]["level2"]["level3"].is_object());
    EXPECT_EQ(data["level1"]["level2"]["level3"]["value"], "deep");
}

// ============================================================================
// Error message tests
// ============================================================================

TEST(DotPathErrors, KeyErrorContainsSegmentInfo) {
    Value data = {{"db", {{"host", "localhost"}}}};

    try {
        get_by_dot(data, "db.missing");
        FAIL() << "Should have thrown KeyError";
    } catch (const KeyError& e) {
        EXPECT_EQ(e.path(), "db.missing");
        EXPECT_EQ(e.segment(), "missing");
        std::string msg = e.what();
        EXPECT_NE(msg.find("missing"), std::string::npos);
        EXPECT_NE(msg.find("db.missing"), std::string::npos);
    }
}

TEST(DotPathErrors, TypeErrorContainsTypeInfo) {
    Value data = {{"db", {{"host", "localhost"}}}};

    try {
        get_by_dot(data, "db.host.sub");
        FAIL() << "Should have thrown TypeError";
    } catch (const TypeError& e) {
        EXPECT_EQ(e.path(), "db.host.sub");
        EXPECT_EQ(e.actual(), "string");
        std::string msg = e.what();
        EXPECT_NE(msg.find("string"), std::string::npos);
        EXPECT_NE(msg.find("db.host.sub"), std::string::npos);
    }
}
