/**
 * @file test_util.cpp
 * @brief Tests for utility functions
 */

#include <gtest/gtest.h>
#include "confy/Util.hpp"

using namespace confy;

TEST(FlattenToDotpaths, SimpleObject) {
    Value data = {{"a", 1}, {"b", 2}};
    auto result = flatten_to_dotpaths(data);

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].first, "a");
    EXPECT_EQ(result[0].second, 1);
    EXPECT_EQ(result[1].first, "b");
    EXPECT_EQ(result[1].second, 2);
}

TEST(FlattenToDotpaths, NestedObject) {
    Value data = {{"a", {{"b", 1}}}};
    auto result = flatten_to_dotpaths(data);

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].first, "a.b");
    EXPECT_EQ(result[0].second, 1);
}

TEST(OverridesDictToValue, Simple) {
    std::unordered_map<std::string, Value> overrides = {
        {"a.b", 1},
        {"c.d", 2}
    };

    auto result = overrides_dict_to_value(overrides);

    EXPECT_EQ(result["a"]["b"], 1);
    EXPECT_EQ(result["c"]["d"], 2);
}
