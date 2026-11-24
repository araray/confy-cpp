// C-04: schema validation.
#include <gtest/gtest.h>
#include "confy/Schema.hpp"

using confy::KeySpec;
using confy::Schema;
using confy::Value;
using confy::Violation;

namespace {
Schema sample() {
    Schema s;
    KeySpec grade{"detect.min_grade", KeySpec::Type::string};
    grade.one_of = {Value("A"), Value("B"), Value("C")};
    KeySpec depth{"limits.max_depth", KeySpec::Type::integer};
    depth.min = 1;
    depth.max = 64;
    KeySpec mask{"redact.mask_char", KeySpec::Type::string};
    mask.min = 1;
    mask.max = 4;
    KeySpec name{"name", KeySpec::Type::string};
    name.required = true;
    name.pattern = "[a-z][a-z0-9_]*";
    KeySpec old{"limits.depth", KeySpec::Type::integer};
    old.renamed_to = "limits.max_depth";
    KeySpec legacy{"legacy", KeySpec::Type::boolean};
    legacy.deprecated = "no effect since 2.0";
    KeySpec extra{"extra", KeySpec::Type::object};
    extra.open = true;
    s.keys = {grade, depth, mask, name, old, legacy, extra};
    return s;
}
Value parse(const char* j) { return Value::parse(j); }
bool has(const std::vector<Violation>& vs, Violation::Kind k, const std::string& path) {
    for (auto& v : vs)
        if (v.kind == k && v.path == path) return true;
    return false;
}
}  // namespace

TEST(Schema, ValidDataHasNoViolations) {
    auto vs = confy::validate(parse(R"({"detect":{"min_grade":"B"},"limits":{"max_depth":4},"name":"abc","extra":{"anything":{"x":1}}})"), sample());
    EXPECT_TRUE(vs.empty()) << vs[0].path << ": " << vs[0].message;
}

TEST(Schema, EveryViolationIsReported) {
    auto vs = confy::validate(parse(R"({"detect":{"min_grade":"D","min_grad":"A"},"limits":{"max_depth":0,"depth":3},"redact":{"mask_char":"#####"},"legacy":true,"name":"9x"})"), sample());
    EXPECT_TRUE(has(vs, Violation::Kind::not_in_enum, "detect.min_grade"));
    EXPECT_TRUE(has(vs, Violation::Kind::unknown_key, "detect.min_grad"));
    EXPECT_TRUE(has(vs, Violation::Kind::out_of_range, "limits.max_depth"));
    EXPECT_TRUE(has(vs, Violation::Kind::out_of_range, "redact.mask_char"));
    EXPECT_TRUE(has(vs, Violation::Kind::renamed, "limits.depth"));
    EXPECT_TRUE(has(vs, Violation::Kind::deprecated, "legacy"));
    EXPECT_TRUE(has(vs, Violation::Kind::pattern_mismatch, "name"));
    for (auto& v : vs) {
        if (v.kind == Violation::Kind::unknown_key) {
            EXPECT_EQ(v.suggestion, "detect.min_grade");
        }
        if (v.kind == Violation::Kind::deprecated || v.kind == Violation::Kind::renamed) {
            EXPECT_FALSE(v.error);
        }
    }
}

TEST(Schema, TypesRequiredAndPolicy) {
    auto vs = confy::validate(parse(R"({"limits":{"max_depth":"4"}})"), sample());
    EXPECT_TRUE(has(vs, Violation::Kind::wrong_type, "limits.max_depth"));
    EXPECT_TRUE(has(vs, Violation::Kind::missing_required, "name"));
    auto s = sample();
    s.unknown = Schema::Unknown::warn;
    vs = confy::validate(parse(R"({"name":"a","zzz":1})"), s);
    ASSERT_EQ(vs.size(), 1u);
    EXPECT_FALSE(vs[0].error);
    s.unknown = Schema::Unknown::allow;
    EXPECT_TRUE(confy::validate(parse(R"({"name":"a","zzz":1})"), s).empty());
}

TEST(Schema, RenamesAndStringKeys) {
    auto moved = confy::apply_renames(parse(R"({"limits":{"depth":3},"name":"a"})"), sample());
    EXPECT_EQ(moved["limits"]["max_depth"].get<long long>(), 3);
    EXPECT_FALSE(moved["limits"].contains("depth"));
    auto kept = confy::apply_renames(parse(R"({"limits":{"depth":3,"max_depth":9}})"), sample());
    EXPECT_EQ(kept["limits"]["max_depth"].get<long long>(), 9);  // explicit new value wins
    EXPECT_EQ(sample().string_keys(), (std::set<std::string>{"detect.min_grade", "redact.mask_char", "name"}));
}
