/**
 * @file Parse.cpp
 * @brief Implementation of type parsing
 */

#include "confy/Parse.hpp"
#include <algorithm>
#include <cctype>
#include <regex>

namespace confy {

namespace {
    /**
     * @brief Convert string to lowercase
     */
    std::string to_lower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    /**
     * @brief Check if string matches regex pattern
     */
    bool matches_regex(const std::string& str, const std::string& pattern) {
        try {
            std::regex re(pattern);
            return std::regex_match(str, re);
        } catch (...) {
            return false;
        }
    }
}

Value parse_value(const std::string& str) {
    // Handle empty string
    if (str.empty()) {
        return ""; // T7: empty string stays as string
    }

    // T1: Boolean
    std::string lower = to_lower(str);
    if (lower == "true") {
        return true;
    }
    if (lower == "false") {
        return false;
    }

    // T2: Null
    if (lower == "null") {
        return nullptr;
    }

    // T3: Integer
    // Pattern: ^-?[0-9]+$
    if (matches_regex(str, "^-?[0-9]+$")) {
        try {
            // Try to parse as int64
            size_t pos = 0;
            long long val = std::stoll(str, &pos);
            // Ensure entire string was consumed
            if (pos == str.size()) {
                return static_cast<int64_t>(val);
            }
        } catch (...) {
            // Fall through to next rule
        }
    }

    // T4: Float
    // Pattern: ^-?[0-9]+\.[0-9]+([eE][+-]?[0-9]+)?$
    if (matches_regex(str, "^-?[0-9]+\\.[0-9]+([eE][+-]?[0-9]+)?$")) {
        try {
            size_t pos = 0;
            double val = std::stod(str, &pos);
            // Ensure entire string was consumed
            if (pos == str.size()) {
                return val;
            }
        } catch (...) {
            // Fall through to next rule
        }
    }

    // T5: JSON Compound (objects and arrays)
    if ((str.front() == '{' && str.back() == '}') ||
        (str.front() == '[' && str.back() == ']')) {
        try {
            return Value::parse(str);
        } catch (...) {
            // Parse failed, fall through to next rule
        }
    }

    // T6: Quoted String
    if (str.front() == '"' && str.back() == '"' && str.size() >= 2) {
        try {
            // Try to parse as JSON string (handles escapes)
            Value parsed = Value::parse(str);
            if (parsed.is_string()) {
                return parsed;
            }
        } catch (...) {
            // Parse failed, fall through
        }
    }

    // T7: Raw String (fallback)
    return str;
}

} // namespace confy
