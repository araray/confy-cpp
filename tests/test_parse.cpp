/**
 * @file test_parse.cpp
 * @brief Unit tests for type parsing (GoogleTest)
 *
 * Tests covering parsing rules T1-T7:
 * - T1: Boolean parsing (true, false, yes, no, on, off)
 * - T2: Null parsing (null, none, nil)
 * - T3: Integer parsing
 * - T4: Float parsing
 * - T5: JSON compound parsing (arrays, objects)
 * - T6: Quoted string parsing
 * - T7: Raw string fallback
 *
 * @copyright (c) 2026. MIT License.
 */

#include <gtest/gtest.h>
#include "confy/Parse.hpp"
#include "confy/Value.hpp"

using namespace confy;

// ============================================================================
// RULE T1: Boolean Parsing
// ============================================================================

TEST(ParseBoolean, TrueValues) {
    EXPECT_EQ(parse_value("true"), true);
    EXPECT_EQ(parse_value("True"), true);
    EXPECT_EQ(parse_value("TRUE"), true);
    EXPECT_EQ(parse_value("yes"), true);
    EXPECT_EQ(parse_value("Yes"), true);
    EXPECT_EQ(parse_value("YES"), true);
    EXPECT_EQ(parse_value("on"), true);
    EXPECT_EQ(parse_value("On"), true);
    EXPECT_EQ(parse_value("ON"), true);
    EXPECT_EQ(parse_value("1"), 1);  // Note: "1" parses as integer, not boolean
}

TEST(ParseBoolean, FalseValues) {
    EXPECT_EQ(parse_value("false"), false);
    EXPECT_EQ(parse_value("False"), false);
    EXPECT_EQ(parse_value("FALSE"), false);
    EXPECT_EQ(parse_value("no"), false);
    EXPECT_EQ(parse_value("No"), false);
    EXPECT_EQ(parse_value("NO"), false);
    EXPECT_EQ(parse_value("off"), false);
    EXPECT_EQ(parse_value("Off"), false);
    EXPECT_EQ(parse_value("OFF"), false);
    EXPECT_EQ(parse_value("0"), 0);  // Note: "0" parses as integer, not boolean
}

// ============================================================================
// RULE T2: Null Parsing
// ============================================================================

TEST(ParseNull, NullValues) {
    EXPECT_TRUE(parse_value("null").is_null());
    EXPECT_TRUE(parse_value("Null").is_null());
    EXPECT_TRUE(parse_value("NULL").is_null());
    EXPECT_TRUE(parse_value("none").is_null());
    EXPECT_TRUE(parse_value("None").is_null());
    EXPECT_TRUE(parse_value("NONE").is_null());
    EXPECT_TRUE(parse_value("nil").is_null());
    EXPECT_TRUE(parse_value("Nil").is_null());
    EXPECT_TRUE(parse_value("NIL").is_null());
}

// ============================================================================
// RULE T3: Integer Parsing
// ============================================================================

TEST(ParseInteger, PositiveIntegers) {
    EXPECT_EQ(parse_value("0"), 0);
    EXPECT_EQ(parse_value("1"), 1);
    EXPECT_EQ(parse_value("42"), 42);
    EXPECT_EQ(parse_value("12345"), 12345);
    EXPECT_EQ(parse_value("999999999"), 999999999);
}

TEST(ParseInteger, NegativeIntegers) {
    EXPECT_EQ(parse_value("-1"), -1);
    EXPECT_EQ(parse_value("-42"), -42);
    EXPECT_EQ(parse_value("-12345"), -12345);
}

TEST(ParseInteger, LeadingZeros) {
    // Leading zeros should still parse as integers
    EXPECT_EQ(parse_value("007"), 7);
    EXPECT_EQ(parse_value("00123"), 123);
}

// ============================================================================
// RULE T4: Float Parsing
// ============================================================================

TEST(ParseFloat, SimpleFloats) {
    EXPECT_DOUBLE_EQ(parse_value("3.14").get<double>(), 3.14);
    EXPECT_DOUBLE_EQ(parse_value("0.5").get<double>(), 0.5);
    EXPECT_DOUBLE_EQ(parse_value("-3.14").get<double>(), -3.14);
}

TEST(ParseFloat, ScientificNotation) {
    EXPECT_DOUBLE_EQ(parse_value("1e10").get<double>(), 1e10);
    EXPECT_DOUBLE_EQ(parse_value("1E10").get<double>(), 1e10);
    EXPECT_DOUBLE_EQ(parse_value("1.5e-3").get<double>(), 1.5e-3);
    EXPECT_DOUBLE_EQ(parse_value("-2.5E+4").get<double>(), -2.5e4);
}

TEST(ParseFloat, SpecialValues) {
    // Leading decimal
    EXPECT_DOUBLE_EQ(parse_value(".5").get<double>(), 0.5);
    // Trailing decimal
    EXPECT_DOUBLE_EQ(parse_value("5.").get<double>(), 5.0);
}

// ============================================================================
// RULE T5: JSON Compound Parsing
// ============================================================================

TEST(ParseJson, Arrays) {
    Value arr = parse_value("[1, 2, 3]");
    EXPECT_TRUE(arr.is_array());
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
}

TEST(ParseJson, NestedArrays) {
    Value arr = parse_value("[[1, 2], [3, 4]]");
    EXPECT_TRUE(arr.is_array());
    EXPECT_EQ(arr.size(), 2);
    EXPECT_TRUE(arr[0].is_array());
}

TEST(ParseJson, Objects) {
    Value obj = parse_value(R"({"key": "value"})");
    EXPECT_TRUE(obj.is_object());
    EXPECT_EQ(obj["key"], "value");
}

TEST(ParseJson, NestedObjects) {
    Value obj = parse_value(R"({"outer": {"inner": 42}})");
    EXPECT_TRUE(obj.is_object());
    EXPECT_EQ(obj["outer"]["inner"], 42);
}

TEST(ParseJson, MixedTypes) {
    Value obj = parse_value(R"({"str": "hello", "num": 42, "arr": [1, 2]})");
    EXPECT_EQ(obj["str"], "hello");
    EXPECT_EQ(obj["num"], 42);
    EXPECT_TRUE(obj["arr"].is_array());
}

// ============================================================================
// RULE T6: Quoted String Parsing
// ============================================================================

TEST(ParseString, DoubleQuoted) {
    EXPECT_EQ(parse_value("\"hello\""), "hello");
    EXPECT_EQ(parse_value("\"hello world\""), "hello world");
    EXPECT_EQ(parse_value("\"\""), "");
}

TEST(ParseString, SingleQuoted) {
    EXPECT_EQ(parse_value("'hello'"), "hello");
    EXPECT_EQ(parse_value("'hello world'"), "hello world");
    EXPECT_EQ(parse_value("''"), "");
}

TEST(ParseString, EscapeSequences) {
    EXPECT_EQ(parse_value("\"hello\\nworld\""), "hello\nworld");
    EXPECT_EQ(parse_value("\"tab\\there\""), "tab\there");
    EXPECT_EQ(parse_value("\"quote\\\"here\""), "quote\"here");
    EXPECT_EQ(parse_value("\"back\\\\slash\""), "back\\slash");
}

TEST(ParseString, PreservesQuotedNumbers) {
    // Quoted numbers should remain strings
    EXPECT_EQ(parse_value("\"42\""), "42");
    EXPECT_EQ(parse_value("\"3.14\""), "3.14");
    EXPECT_EQ(parse_value("\"true\""), "true");
}

// ============================================================================
// RULE T7: Raw String Fallback
// ============================================================================

TEST(ParseRawString, UnquotedStrings) {
    EXPECT_EQ(parse_value("hello"), "hello");
    EXPECT_EQ(parse_value("hello_world"), "hello_world");
    EXPECT_EQ(parse_value("path/to/file"), "path/to/file");
}

TEST(ParseRawString, StringsWithSpaces) {
    // Unquoted strings with spaces
    EXPECT_EQ(parse_value("hello world"), "hello world");
}

TEST(ParseRawString, MalformedJson) {
    // Malformed JSON should fall back to string
    EXPECT_EQ(parse_value("[incomplete"), "[incomplete");
    EXPECT_EQ(parse_value("{bad:json}"), "{bad:json}");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(ParseEdgeCases, EmptyString) {
    EXPECT_EQ(parse_value(""), "");
}

TEST(ParseEdgeCases, WhitespaceOnly) {
    EXPECT_EQ(parse_value("   "), "   ");
}

TEST(ParseEdgeCases, WhitespaceAround) {
    // Whitespace should be preserved in raw strings
    EXPECT_EQ(parse_value("  hello  "), "  hello  ");
}

TEST(ParseEdgeCases, VeryLongInteger) {
    // Should handle large integers
    Value v = parse_value("9223372036854775807");
    EXPECT_TRUE(v.is_number());
}

TEST(ParseEdgeCases, NumericPrefix) {
    // Strings starting with numbers but containing letters
    EXPECT_EQ(parse_value("123abc"), "123abc");
    EXPECT_EQ(parse_value("3.14abc"), "3.14abc");
}
