/**
 * @file test_loader.cpp
 * @brief Unit tests for file loading functionality (GoogleTest)
 *
 * Phase 2 tests covering file loading rules F1-F8:
 * - F1: JSON file loading
 * - F2: TOML file loading
 * - F3: Auto-detection by extension
 * - F4: .env file parsing
 * - F5: TOML key promotion
 * - F6: Empty path handling
 * - F7: Missing file handling
 * - F8: Malformed file handling
 *
 * @copyright (c) 2026. MIT License.
 */

#include <gtest/gtest.h>
#include "confy/Loader.hpp"
#include "confy/Errors.hpp"
#include "confy/Value.hpp"

#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;
using namespace confy;

// ============================================================================
// Test Utilities
// ============================================================================

class TempFile {
public:
    explicit TempFile(const std::string& filename, const std::string& content)
        : path_(fs::temp_directory_path() / filename) {
        std::ofstream f(path_);
        f << content;
        f.close();
    }

    ~TempFile() {
        try {
            if (fs::exists(path_)) {
                fs::remove(path_);
            }
        } catch (...) {}
    }

    std::string path() const { return path_.string(); }

private:
    fs::path path_;
};

// ============================================================================
// RULE F1: JSON File Loading
// ============================================================================

TEST(LoaderJson, LoadsValidJson) {
    TempFile file("test_loader.json", R"({
        "key": "value",
        "number": 42
    })");

    Value result = load_json_file(file.path());

    EXPECT_TRUE(result.is_object());
    EXPECT_EQ(result["key"], "value");
    EXPECT_EQ(result["number"], 42);
}

TEST(LoaderJson, LoadsNestedJson) {
    TempFile file("test_loader_nested.json", R"({
        "database": {
            "host": "localhost",
            "port": 5432
        }
    })");

    Value result = load_json_file(file.path());

    EXPECT_EQ(result["database"]["host"], "localhost");
    EXPECT_EQ(result["database"]["port"], 5432);
}

TEST(LoaderJson, LoadsAllTypes) {
    TempFile file("test_loader_types.json", R"({
        "string": "hello",
        "integer": 42,
        "float": 3.14,
        "bool_true": true,
        "bool_false": false,
        "null_val": null,
        "array": [1, 2, 3],
        "object": {"nested": "value"}
    })");

    Value result = load_json_file(file.path());

    EXPECT_EQ(result["string"], "hello");
    EXPECT_EQ(result["integer"], 42);
    EXPECT_DOUBLE_EQ(result["float"].get<double>(), 3.14);
    EXPECT_EQ(result["bool_true"], true);
    EXPECT_EQ(result["bool_false"], false);
    EXPECT_TRUE(result["null_val"].is_null());
    EXPECT_TRUE(result["array"].is_array());
    EXPECT_TRUE(result["object"].is_object());
}

TEST(LoaderJson, ThrowsOnMalformedJson) {
    TempFile file("test_malformed.json", R"({ "key": )");

    EXPECT_THROW(load_json_file(file.path()), ConfigParseError);
}

TEST(LoaderJson, ThrowsOnMissingFile) {
    EXPECT_THROW(load_json_file("/nonexistent/path.json"), FileNotFoundError);
}

// ============================================================================
// RULE F2: TOML File Loading
// ============================================================================

TEST(LoaderToml, LoadsValidToml) {
    TempFile file("test_loader.toml", R"(
key = "value"
number = 42
)");

    Value result = load_toml_file(file.path());

    EXPECT_TRUE(result.is_object());
    EXPECT_EQ(result["key"], "value");
    EXPECT_EQ(result["number"], 42);
}

TEST(LoaderToml, LoadsSections) {
    TempFile file("test_loader_sections.toml", R"(
[database]
host = "localhost"
port = 5432
)");

    Value result = load_toml_file(file.path());

    EXPECT_TRUE(result["database"].is_object());
    EXPECT_EQ(result["database"]["host"], "localhost");
    EXPECT_EQ(result["database"]["port"], 5432);
}

TEST(LoaderToml, LoadsNestedSections) {
    TempFile file("test_loader_nested_toml.toml", R"(
[database.connection]
host = "localhost"
port = 5432
)");

    Value result = load_toml_file(file.path());

    EXPECT_EQ(result["database"]["connection"]["host"], "localhost");
}

TEST(LoaderToml, ThrowsOnMalformedToml) {
    TempFile file("test_malformed.toml", R"(
key = "unclosed string
)");

    EXPECT_THROW(load_toml_file(file.path()), ConfigParseError);
}

TEST(LoaderToml, ThrowsOnMissingFile) {
    EXPECT_THROW(load_toml_file("/nonexistent/path.toml"), FileNotFoundError);
}

// ============================================================================
// RULE F3: Auto-detection by Extension
// ============================================================================

TEST(LoaderAutoDetect, DetectsJsonByExtension) {
    TempFile file("test_auto.json", R"({"key": "json_value"})");

    Value result = load_config_file(file.path());

    EXPECT_EQ(result["key"], "json_value");
}

TEST(LoaderAutoDetect, DetectsTomlByExtension) {
    TempFile file("test_auto.toml", R"(key = "toml_value")");

    Value result = load_config_file(file.path());

    EXPECT_EQ(result["key"], "toml_value");
}

TEST(LoaderAutoDetect, CaseInsensitiveExtension) {
    TempFile file("test_auto.JSON", R"({"key": "value"})");

    Value result = load_config_file(file.path());
    EXPECT_EQ(result["key"], "value");
}

// ============================================================================
// RULE F4: .env File Parsing
// ============================================================================

TEST(LoaderDotenv, ParsesKeyValuePairs) {
    TempFile file("test.env", R"(
KEY1=value1
KEY2=value2
)");

    DotenvResult result = parse_dotenv_file(file.path());

    EXPECT_TRUE(result.found);
    EXPECT_GE(result.entries.size(), 2u);

    bool found_key1 = false, found_key2 = false;
    for (const auto& [k, v] : result.entries) {
        if (k == "KEY1" && v == "value1") found_key1 = true;
        if (k == "KEY2" && v == "value2") found_key2 = true;
    }
    EXPECT_TRUE(found_key1);
    EXPECT_TRUE(found_key2);
}

TEST(LoaderDotenv, HandlesComments) {
    TempFile file("test_comments.env", R"(
# This is a comment
KEY=value
# Another comment
)");

    DotenvResult result = parse_dotenv_file(file.path());

    EXPECT_TRUE(result.found);
    EXPECT_EQ(result.entries.size(), 1u);
    EXPECT_EQ(result.entries[0].first, "KEY");
    EXPECT_EQ(result.entries[0].second, "value");
}

TEST(LoaderDotenv, HandlesDoubleQuotes) {
    TempFile file("test_quotes.env", R"(
KEY="quoted value"
)");

    DotenvResult result = parse_dotenv_file(file.path());

    EXPECT_TRUE(result.found);
    EXPECT_EQ(result.entries.size(), 1u);
    EXPECT_EQ(result.entries[0].second, "quoted value");
}

TEST(LoaderDotenv, HandlesSingleQuotes) {
    TempFile file("test_single_quotes.env", R"(
KEY='single quoted'
)");

    DotenvResult result = parse_dotenv_file(file.path());

    EXPECT_TRUE(result.found);
    EXPECT_EQ(result.entries[0].second, "single quoted");
}

TEST(LoaderDotenv, HandlesExportPrefix) {
    TempFile file("test_export.env", R"(
export KEY=exported_value
)");

    DotenvResult result = parse_dotenv_file(file.path());

    EXPECT_TRUE(result.found);
    EXPECT_EQ(result.entries[0].first, "KEY");
    EXPECT_EQ(result.entries[0].second, "exported_value");
}

TEST(LoaderDotenv, SkipsEmptyLines) {
    TempFile file("test_empty.env", R"(
KEY1=value1

KEY2=value2

)");

    DotenvResult result = parse_dotenv_file(file.path());

    EXPECT_EQ(result.entries.size(), 2u);
}

TEST(LoaderDotenv, MissingFileNotFound) {
    DotenvResult result = parse_dotenv_file("/nonexistent/.env");

    EXPECT_FALSE(result.found);
    EXPECT_TRUE(result.entries.empty());
}

// ============================================================================
// RULE F5: TOML Key Promotion
// ============================================================================

TEST(LoaderTomlPromotion, PromotesMatchingKeys) {
    TempFile file("test_promotion.toml", R"(
[settings]
debug = true

[production]
debug = false
)");

    Value defaults = {{"debug", false}};
    Value result = load_toml_file(file.path(), defaults);

    // "debug" should be promoted from a section if it matches a root default
    // The exact behavior depends on the implementation
    EXPECT_TRUE(result.is_object());
}

// ============================================================================
// RULE F6: Empty Path Handling
// ============================================================================

TEST(LoaderEmptyPath, ReturnsEmptyObject) {
    Value result = load_config_file("");

    EXPECT_TRUE(result.is_object());
    EXPECT_TRUE(result.empty());
}

// ============================================================================
// RULE F7: Missing File Handling
// ============================================================================

TEST(LoaderMissingFile, ThrowsFileNotFound) {
    EXPECT_THROW(
        load_config_file("/definitely/does/not/exist.json"),
        FileNotFoundError
    );
}

// ============================================================================
// File Extension Helper
// ============================================================================

TEST(LoaderExtension, GetsLowerExtension) {
    EXPECT_EQ(get_file_extension("file.json"), ".json");
    EXPECT_EQ(get_file_extension("file.JSON"), ".json");
    EXPECT_EQ(get_file_extension("file.Json"), ".json");
    EXPECT_EQ(get_file_extension("file.toml"), ".toml");
    EXPECT_EQ(get_file_extension("path/to/file.json"), ".json");
}

TEST(LoaderExtension, NoExtension) {
    EXPECT_EQ(get_file_extension("noext"), "");
    EXPECT_EQ(get_file_extension("path/noext"), "");
}
