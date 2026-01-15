/**
 * @file test_dotpath.cpp
 * @brief Unit tests for DotPath functionality (GoogleTest)
 *
 * Tests cover rules D1-D7 from the design specification:
 * - D1: parse_dot_path splits on '.'
 * - D2: get_by_dot traverses nested structures
 * - D3: set_by_dot creates/overwrites values
 * - D4: KeyError for missing segments
 * - D5: TypeError for non-container traversal
 * - D6: contains_dot checks existence
 * - D7: get_by_dot with default returns fallback
 */

#include <gtest/gtest.h>
#include "confy/DotPath.hpp"
#include "confy/Errors.hpp"

using namespace confy;
using json = nlohmann::json;

// ============================================================================
// D1: parse_dot_path tests
// ============================================================================

TEST(ParseDotPath, SplitsOnDots) {
    auto segments = parse_dot_path("a.b.c");
    ASSERT_EQ(segments.size(), 3u);
    EXPECT_EQ(segments[0], "a");
    EXPECT_EQ(segments[1], "b");
    EXPECT_EQ(segments[2], "c");
}

TEST(ParseDotPath, SingleSegment) {
    auto segments = parse_dot_path("single");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(segments[0], "single");
}

TEST(ParseDotPath, EmptyStringReturnsEmptyVector) {
    auto segments = parse_dot_path("");
    EXPECT_TRUE(segments.empty());
}

TEST(ParseDotPath, HandlesArrayIndices) {
    auto segments = parse_dot_path("users.0.name");
    ASSERT_EQ(segments.size(), 3u);
    EXPECT_EQ(segments[0], "users");
    EXPECT_EQ(segments[1], "0");
    EXPECT_EQ(segments[2], "name");
}

// ============================================================================
// D2: get_by_dot tests
// ============================================================================

TEST(GetByDot, RetrievesNestedValue) {
    Value data = {{"server", {{"host", "localhost"}, {"port", 8080}}}};

    auto result = get_by_dot(data, "server.host");
    EXPECT_EQ(result.get<std::string>(), "localhost");

    result = get_by_dot(data, "server.port");
    EXPECT_EQ(result.get<int>(), 8080);
}

TEST(GetByDot, RetrievesTopLevelValue) {
    Value data = {{"name", "test"}, {"count", 42}};

    EXPECT_EQ(get_by_dot(data, "name").get<std::string>(), "test");
    EXPECT_EQ(get_by_dot(data, "count").get<int>(), 42);
}

TEST(GetByDot, EmptyPathReturnsRoot) {
    Value data = {{"key", "value"}};
    auto result = get_by_dot(data, "");
    EXPECT_EQ(result, data);
}

TEST(GetByDot, AccessesArrayElements) {
    Value data = {{"items", json::array({"a", "b", "c"})}};

    EXPECT_EQ(get_by_dot(data, "items.0").get<std::string>(), "a");
    EXPECT_EQ(get_by_dot(data, "items.1").get<std::string>(), "b");
    EXPECT_EQ(get_by_dot(data, "items.2").get<std::string>(), "c");
}

TEST(GetByDot, DeeplyNestedAccess) {
    Value data = {{"a", {{"b", {{"c", {{"d", "deep"}}}}}}}};
    EXPECT_EQ(get_by_dot(data, "a.b.c.d").get<std::string>(), "deep");
}

// ============================================================================
// D3: set_by_dot tests
// ============================================================================

TEST(SetByDot, SetsExistingKey) {
    Value data = {{"server", {{"host", "old"}}}};
    set_by_dot(data, "server.host", "new");
    EXPECT_EQ(get_by_dot(data, "server.host").get<std::string>(), "new");
}

TEST(SetByDot, CreatesMissingIntermediates) {
    Value data = Value::object();
    set_by_dot(data, "a.b.c", "value", true);
    EXPECT_EQ(get_by_dot(data, "a.b.c").get<std::string>(), "value");
}

TEST(SetByDot, WithoutCreateThrowsOnMissingIntermediate) {
    Value data = Value::object();
    EXPECT_THROW(set_by_dot(data, "a.b.c", "value", false), KeyError);
}

TEST(SetByDot, EmptyPathReplacesRoot) {
    Value data = {{"old", "data"}};
    set_by_dot(data, "", "replaced", true);
    EXPECT_EQ(data.get<std::string>(), "replaced");
}

TEST(SetByDot, PreservesSiblingKeys) {
    Value data = {{"server", {{"host", "localhost"}, {"port", 8080}}}};
    set_by_dot(data, "server.host", "newhost");
    EXPECT_EQ(get_by_dot(data, "server.host").get<std::string>(), "newhost");
    EXPECT_EQ(get_by_dot(data, "server.port").get<int>(), 8080);
}

// ============================================================================
// D4: KeyError tests
// ============================================================================

TEST(GetByDotKeyError, ThrowsForMissingKey) {
    Value data = {{"server", {{"host", "localhost"}}}};
    EXPECT_THROW(get_by_dot(data, "server.missing"), KeyError);
}

TEST(GetByDotKeyError, ThrowsForMissingNestedPath) {
    Value data = {{"a", {{"b", "value"}}}};
    EXPECT_THROW(get_by_dot(data, "a.x.y"), KeyError);
}

TEST(GetByDotKeyError, ThrowsForArrayOutOfBounds) {
    Value data = {{"items", json::array({"a", "b"})}};
    EXPECT_THROW(get_by_dot(data, "items.5"), KeyError);
}

// ============================================================================
// D5: TypeError tests
// ============================================================================

TEST(GetByDotTypeError, ThrowsTraversingScalar) {
    Value data = {{"name", "string_value"}};
    EXPECT_THROW(get_by_dot(data, "name.nested"), TypeError);
}

TEST(GetByDotTypeError, ThrowsTraversingNumber) {
    Value data = {{"count", 42}};
    EXPECT_THROW(get_by_dot(data, "count.nested"), TypeError);
}

TEST(GetByDotTypeError, ThrowsTraversingBoolean) {
    Value data = {{"flag", true}};
    EXPECT_THROW(get_by_dot(data, "flag.nested"), TypeError);
}

TEST(GetByDotTypeError, ThrowsTraversingNull) {
    Value data = {{"nothing", nullptr}};
    EXPECT_THROW(get_by_dot(data, "nothing.nested"), TypeError);
}

// ============================================================================
// D6: contains_dot tests
// ============================================================================

TEST(ContainsDot, ReturnsTrueForExistingKey) {
    Value data = {{"server", {{"host", "localhost"}}}};
    EXPECT_TRUE(contains_dot(data, "server"));
    EXPECT_TRUE(contains_dot(data, "server.host"));
}

TEST(ContainsDot, ReturnsFalseForMissingKey) {
    Value data = {{"server", {{"host", "localhost"}}}};
    EXPECT_FALSE(contains_dot(data, "missing"));
    EXPECT_FALSE(contains_dot(data, "server.missing"));
}

TEST(ContainsDot, EmptyPathAlwaysExists) {
    Value data = {{"key", "value"}};
    EXPECT_TRUE(contains_dot(data, ""));

    Value empty_obj = Value::object();
    EXPECT_TRUE(contains_dot(empty_obj, ""));
}

TEST(ContainsDot, ThrowsTypeErrorForScalarTraversal) {
    Value data = {{"name", "value"}};
    EXPECT_THROW(contains_dot(data, "name.nested"), TypeError);
}

// ============================================================================
// D7: get_by_dot with default tests
// ============================================================================

TEST(GetByDotDefault, ReturnsValueWhenFound) {
    Value data = {{"server", {{"host", "localhost"}}}};
    Value default_val = "fallback";

    auto result = get_by_dot_default(data, "server.host", default_val);
    EXPECT_EQ(result.get<std::string>(), "localhost");
}

TEST(GetByDotDefault, ReturnsDefaultForMissingKey) {
    Value data = {{"server", {{"host", "localhost"}}}};
    Value default_val = "fallback";

    auto result = get_by_dot_default(data, "server.missing", default_val);
    EXPECT_EQ(result.get<std::string>(), "fallback");
}

TEST(GetByDotDefault, ReturnsDefaultForMissingNestedPath) {
    Value data = {{"a", {{"b", "value"}}}};
    Value default_val = 999;

    auto result = get_by_dot_default(data, "x.y.z", default_val);
    EXPECT_EQ(result.get<int>(), 999);
}

TEST(GetByDotDefault, StillThrowsTypeErrorForScalarTraversal) {
    Value data = {{"name", "value"}};
    Value default_val = "fallback";

    // TypeError is not suppressed by default - it's a structural error
    EXPECT_THROW(get_by_dot_default(data, "name.nested", default_val), TypeError);
}

// ============================================================================
// Complex/integration tests
// ============================================================================

TEST(DotPathComplex, NestedArrayAccess) {
    Value data = {
        {"users", json::array({
            {{"name", "alice"}, {"age", 30}},
            {{"name", "bob"}, {"age", 25}}
        })}
    };

    EXPECT_EQ(get_by_dot(data, "users.0.name").get<std::string>(), "alice");
    EXPECT_EQ(get_by_dot(data, "users.1.age").get<int>(), 25);
}

TEST(DotPathComplex, MixedObjectArrayAccess) {
    Value data = {
        {"config", {
            {"servers", json::array({
                {{"host", "server1"}},
                {{"host", "server2"}}
            })}
        }}
    };

    EXPECT_EQ(get_by_dot(data, "config.servers.0.host").get<std::string>(), "server1");
    EXPECT_EQ(get_by_dot(data, "config.servers.1.host").get<std::string>(), "server2");
}

TEST(DotPathComplex, SetCreatesNestedStructure) {
    Value data = Value::object();
    set_by_dot(data, "database.primary.host", "db.example.com", true);
    set_by_dot(data, "database.primary.port", 5432, true);
    set_by_dot(data, "database.replica.host", "replica.example.com", true);

    EXPECT_EQ(get_by_dot(data, "database.primary.host").get<std::string>(), "db.example.com");
    EXPECT_EQ(get_by_dot(data, "database.primary.port").get<int>(), 5432);
    EXPECT_EQ(get_by_dot(data, "database.replica.host").get<std::string>(), "replica.example.com");
}

TEST(DotPathErrors, KeyErrorContainsSegmentInfo) {
    Value data = {{"a", {{"b", "value"}}}};

    try {
        get_by_dot(data, "a.missing.key");
        FAIL() << "Expected KeyError";
    } catch (const KeyError& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("missing"), std::string::npos);
    }
}

TEST(DotPathErrors, TypeErrorContainsTypeInfo) {
    Value data = {{"name", "string_value"}};

    try {
        get_by_dot(data, "name.nested");
        FAIL() << "Expected TypeError";
    } catch (const TypeError& e) {
        std::string msg = e.what();
        // Should mention the path or type issue
        EXPECT_FALSE(msg.empty());
    }
}
