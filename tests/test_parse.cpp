/**
 * @file test_parse.cpp
 * @brief Tests for type parsing using Google Test
 *
 * Validates RULES T1-T7 from CONFY_DESIGN_SPECIFICATION.md
 */

#include <gtest/gtest.h>
#include "confy/Parse.hpp"

using namespace confy;

// ============================================================================
// RULE T1: Boolean
// ============================================================================

TEST(ParseValue, BooleanTrue) {
    EXPECT_EQ(parse_value("true"), true);
    EXPECT_EQ(parse_value("TRUE"), true);
    EXPECT_EQ(parse_value("True"), true);
}

TEST(ParseValue, BooleanFalse) {
    EXPECT_EQ(parse_value("false"), false);
    EXPECT_EQ(parse_value("FALSE"), false);
    EXPECT_EQ(parse_value("False"), false);
}

// ============================================================================
// RULE T2: Null
// ============================================================================

TEST(ParseValue, Null) {
    EXPECT_TRUE(parse_value("null").is_null());
    EXPECT_TRUE(parse_value("NULL").is_null());
    EXPECT_TRUE(parse_value("Null").is_null());
}

// ============================================================================
// RULE T3: Integer
// ============================================================================

TEST(ParseValue, IntegerPositive) {
    auto result = parse_value("42");
    EXPECT_TRUE(result.is_number_integer());
    EXPECT_EQ(result, 42);
}

TEST(ParseValue, IntegerNegative) {
    auto result = parse_value("-17");
    EXPECT_TRUE(result.is_number_integer());
    EXPECT_EQ(result, -17);
}

TEST(ParseValue, IntegerZero) {
    auto result = parse_value("0");
    EXPECT_TRUE(result.is_number_integer());
    EXPECT_EQ(result, 0);
}

TEST(ParseValue, LeadingZerosNotInteger) {
    auto result = parse_value("01");
    EXPECT_TRUE(result.is_string());
    EXPECT_EQ(result, "01");
}

// ============================================================================
// RULE T4: Float
// ============================================================================

TEST(ParseValue, FloatSimple) {
    auto result = parse_value("3.14");
    EXPECT_TRUE(result.is_number_float());
    EXPECT_DOUBLE_EQ(result, 3.14);
}

TEST(ParseValue, FloatNegative) {
    auto result = parse_value("-2.5");
    EXPECT_TRUE(result.is_number_float());
    EXPECT_DOUBLE_EQ(result, -2.5);
}

TEST(ParseValue, FloatScientific) {
    auto result = parse_value("1.5e10");
    EXPECT_TRUE(result.is_number_float());
    EXPECT_DOUBLE_EQ(result, 1.5e10);
}

// ============================================================================
// RULE T5: JSON Compound
// ============================================================================

TEST(ParseValue, JSONObject) {
    auto result = parse_value(R"({"key":"value"})");
    EXPECT_TRUE(result.is_object());
    EXPECT_EQ(result["key"], "value");
}

TEST(ParseValue, JSONArray) {
    auto result = parse_value("[1,2,3]");
    EXPECT_TRUE(result.is_array());
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
}

TEST(ParseValue, MalformedJSONFallsThrough) {
    auto result = parse_value("{invalid}");
    EXPECT_TRUE(result.is_string());
    EXPECT_EQ(result, "{invalid}");
}

// ============================================================================
// RULE T6: Quoted String
// ============================================================================

TEST(ParseValue, QuotedString) {
    auto result = parse_value(R"("hello")");
    EXPECT_TRUE(result.is_string());
    EXPECT_EQ(result, "hello");
}

TEST(ParseValue, QuotedStringWithSpaces) {
    auto result = parse_value(R"("hello world")");
    EXPECT_TRUE(result.is_string());
    EXPECT_EQ(result, "hello world");
}

TEST(ParseValue, QuotedStringWithEscapes) {
    auto result = parse_value(R"("line1\nline2")");
    EXPECT_TRUE(result.is_string());
    EXPECT_EQ(result, "line1\nline2");
}

// ============================================================================
// RULE T7: Raw String (fallback)
// ============================================================================

TEST(ParseValue, RawString) {
    auto result = parse_value("hello");
    EXPECT_TRUE(result.is_string());
    EXPECT_EQ(result, "hello");
}

TEST(ParseValue, EmptyString) {
    auto result = parse_value("");
    EXPECT_TRUE(result.is_string());
    EXPECT_EQ(result, "");
}

TEST(ParseValue, StringWithSpaces) {
    auto result = parse_value("hello world");
    EXPECT_TRUE(result.is_string());
    EXPECT_EQ(result, "hello world");
}

// ============================================================================
// Real-world examples
// ============================================================================

TEST(ParseValue, DatabasePort) {
    auto result = parse_value("5432");
    EXPECT_TRUE(result.is_number_integer());
    EXPECT_EQ(result, 5432);
}

TEST(ParseValue, DebugFlag) {
    auto result = parse_value("true");
    EXPECT_TRUE(result.is_boolean());
    EXPECT_EQ(result, true);
}

TEST(ParseValue, LogLevel) {
    auto result = parse_value("DEBUG");
    EXPECT_TRUE(result.is_string());
    EXPECT_EQ(result, "DEBUG");
}

TEST(ParseValue, ConnectionString) {
    auto result = parse_value("postgres://user:pass@localhost:5432/db");
    EXPECT_TRUE(result.is_string());
    EXPECT_EQ(result, "postgres://user:pass@localhost:5432/db");
}
