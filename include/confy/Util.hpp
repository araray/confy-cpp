#ifndef CONFY_UTIL_HPP
#define CONFY_UTIL_HPP

#include <nlohmann/json.hpp>
#include <string>
#include <map>
#include <vector>
#include <optional>
#include <regex>
#include <cctype>

namespace confy {

// Merge b into a (recursively). Values in b take precedence.
void deep_merge(nlohmann::json& a, const nlohmann::json& b);

// Set a nested value by dot-notation, creating intermediate objects
void set_by_dot(nlohmann::json& obj, const std::string& path, const nlohmann::json& value);

// Get a nested value by dot-notation. Throws std::out_of_range if missing.
const nlohmann::json& get_by_dot(const nlohmann::json& obj, const std::string& path);

// Check existence of a nested key by dot-notation.
bool exists_by_dot(const nlohmann::json& obj, const std::string& path);

// Flatten nested object into map {"a.b.c": value, ...}
std::map<std::string, nlohmann::json> flatten(const nlohmann::json& obj);

// Simple pattern match selection used by `search`.
// Rules: if pattern contains * ? [ ] -> glob (case-insensitive)
// else if contains any of . + ^ $ ( ) { } | \ -> regex (case-sensitive by default, optional ignoreCase)
// else -> exact match (case-insensitive)
bool match_pattern(const std::string& pattern, const std::string& text, bool ignoreCase);

// Helpers
std::string to_lower(std::string s);
std::vector<std::string> split(const std::string& s, char delim);

// Parse an --overrides string: "k1:json, k2:json, ..."
std::map<std::string, nlohmann::json> parse_overrides(const std::string& s);

// Environment iteration: returns pairs (NAME, VALUE)
std::vector<std::pair<std::string, std::string>> enumerate_environment();

// Try parsing string as JSON, otherwise return it as a string.
nlohmann::json parse_json_or_string(const std::string& raw);

} // namespace confy

#endif // CONFY_UTIL_HPP
