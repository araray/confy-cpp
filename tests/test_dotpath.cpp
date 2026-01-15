/**
 * @file test_dotpath.cpp
 * @brief Unit tests for DotPath functionality (GoogleTest)
 *
 * Tests cover rules D1-D7 from the design specification:
 * - D1: get_by_dot traverses nested structures
 * - D2: get_by_dot with default returns fallback on missing keys
 * - D3: set_by_dot creates/overwrites values
 * - D4: KeyError for missing segments (without default)
 * - D5: TypeError for non-container traversal
 * - D6: contains_dot checks existence
 * - D7: join_dot_path joins segments
 */

#include <gtest/gtest.h>
#include "confy/DotPath.hpp"
#include "confy/Errors.hpp"

using namespace confy;

// ============================================================================
// get_by_dot - basic retrieval (no default)
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
// get_by_dot - errors without default (RULE D1/D4)
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
// get_by_dot - with default (RULE D2)
// ============================================================================

class GetByDotDefaultTest : public ::testing::Test {
protected:
    Value data = {
        {"server", {{"host", "localhost"}, {"port", 8080}}}
    };
    Value default_str = "fallback";
    Value default_int = 999;
};

TEST_F(GetByDotDefaultTest, ReturnsValueWhenFound) {
    auto* result = get_by_dot(data, "server.host", default_str);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->get<std::string>(), "localhost");
}

TEST_F(GetByDotDefaultTest, ReturnsDefaultForMissingKey) {
    auto* result = get_by_dot(data, "server.missing", default_str);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->get<std::string>(), "fallback");
}

TEST_F(GetByDotDefaultTest, ReturnsDefaultForMissingNestedKey) {
    auto* result = get_by_dot(data, "x.y.z", default_int);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->get<int>(), 999);
}

TEST_F(GetByDotDefaultTest, StillRaisesTypeErrorForNonContainer) {
    // TypeError is not suppressed by default - it's a structural error
    Value scalar_data = {{"name", "value"}};
    EXPECT_THROW(get_by_dot(scalar_data, "name.nested", default_str), TypeError);
}

// ============================================================================
// set_by_dot - without create_missing (RULE D3)
// ============================================================================

class SetByDotWithoutCreateTest : public ::testing::Test {
protected:
    Value data;
    void SetUp() override {
        data = {{"server", {{"host", "localhost"}}}};
    }
};

TEST_F(SetByDotWithoutCreateTest, SetExistingKey) {
    set_by_dot(data, "server.host", "newhost", false);
    auto* result = get_by_dot(data, "server.host");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "newhost");
}

TEST_F(SetByDotWithoutCreateTest, RaisesErrorForMissingIntermediate) {
    EXPECT_THROW(set_by_dot(data, "db.port", 5432, false), KeyError);
}

TEST_F(SetByDotWithoutCreateTest, RaisesTypeErrorForNonObjectIntermediate) {
    Value scalar_data = {{"name", "value"}};
    EXPECT_THROW(set_by_dot(scalar_data, "name.nested", "x", false), TypeError);
}

// ============================================================================
// set_by_dot - with create_missing (RULE D4)
// ============================================================================

class SetByDotWithCreateTest : public ::testing::Test {
protected:
    Value data;
    void SetUp() override {
        data = Value::object();
    }
};

TEST_F(SetByDotWithCreateTest, CreatesMissingIntermediates) {
    set_by_dot(data, "a.b.c", "value", true);
    auto* result = get_by_dot(data, "a.b.c");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "value");
}

TEST_F(SetByDotWithCreateTest, OverwritesNonObjectIntermediate) {
    data = {{"name", "scalar"}};
    set_by_dot(data, "name.nested", "value", true);
    auto* result = get_by_dot(data, "name.nested");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "value");
}

TEST_F(SetByDotWithCreateTest, PreservesExistingStructure) {
    data = {{"server", {{"host", "localhost"}, {"port", 8080}}}};
    set_by_dot(data, "server.timeout", 30, true);

    auto* host = get_by_dot(data, "server.host");
    auto* port = get_by_dot(data, "server.port");
    auto* timeout = get_by_dot(data, "server.timeout");

    ASSERT_NE(host, nullptr);
    ASSERT_NE(port, nullptr);
    ASSERT_NE(timeout, nullptr);
    EXPECT_EQ(*host, "localhost");
    EXPECT_EQ(*port, 8080);
    EXPECT_EQ(*timeout, 30);
}

TEST_F(SetByDotWithCreateTest, EmptyPathReplacesRoot) {
    set_by_dot(data, "", "replaced", true);
    EXPECT_EQ(data, "replaced");
}

// ============================================================================
// contains_dot (RULE D5/D6)
// ============================================================================

class ContainsDotTest : public ::testing::Test {
protected:
    Value data = {
        {"server", {{"host", "localhost"}, {"port", 8080}}},
        {"items", {1, 2, 3}}
    };
};

TEST_F(ContainsDotTest, ReturnsTrueForExistingKey) {
    EXPECT_TRUE(contains_dot(data, "server"));
    EXPECT_TRUE(contains_dot(data, "server.host"));
    EXPECT_TRUE(contains_dot(data, "server.port"));
}

TEST_F(ContainsDotTest, ReturnsFalseForMissingKey) {
    EXPECT_FALSE(contains_dot(data, "missing"));
    EXPECT_FALSE(contains_dot(data, "server.missing"));
}

TEST_F(ContainsDotTest, EmptyPathAlwaysExists) {
    EXPECT_TRUE(contains_dot(data, ""));
    Value empty_obj = Value::object();
    EXPECT_TRUE(contains_dot(empty_obj, ""));
}

// ============================================================================
// contains_dot - TypeError cases (RULE D6)
// ============================================================================

class ContainsDotErrorTest : public ::testing::Test {
protected:
    Value data = {{"name", "value"}, {"count", 42}};
};

TEST_F(ContainsDotErrorTest, RaisesTypeErrorForScalarTraversal) {
    EXPECT_THROW(contains_dot(data, "count.sub"), TypeError);
}

TEST_F(ContainsDotErrorTest, RaisesTypeErrorForStringTraversal) {
    EXPECT_THROW(contains_dot(data, "name.nested"), TypeError);
}

// ============================================================================
// Complex/integration tests
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

// ============================================================================
// join_dot_path tests (RULE D7)
// ============================================================================

TEST(JoinDotPath, EmptyVector) {
    std::vector<std::string> segments;
    EXPECT_EQ(join_dot_path(segments), "");
}

TEST(JoinDotPath, SingleSegment) {
    std::vector<std::string> segments = {"single"};
    EXPECT_EQ(join_dot_path(segments), "single");
}

TEST(JoinDotPath, MultipleSegments) {
    std::vector<std::string> segments = {"a", "b", "c"};
    EXPECT_EQ(join_dot_path(segments), "a.b.c");
}

TEST(JoinDotPath, WithNumericSegments) {
    std::vector<std::string> segments = {"users", "0", "name"};
    EXPECT_EQ(join_dot_path(segments), "users.0.name");
}
