/**
 * @file DotPath.cpp
 * @brief Implementation of dot-path utilities
 */

#include "confy/DotPath.hpp"
#include <sstream>
#include <algorithm>

namespace confy {

std::vector<std::string> split_dot_path(const std::string& path) {
    if (path.empty()) {
        return {};
    }

    std::vector<std::string> segments;
    std::string current;

    for (char c : path) {
        if (c == '.') {
            if (!current.empty()) {
                segments.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }

    // Add final segment
    if (!current.empty()) {
        segments.push_back(current);
    }

    return segments;
}

std::string join_dot_path(const std::vector<std::string>& segments) {
    if (segments.empty()) {
        return "";
    }

    std::ostringstream oss;
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) oss << '.';
        oss << segments[i];
    }
    return oss.str();
}

namespace {
    /**
     * @brief Check if segment represents an array index
     * @param segment The segment to check
     * @return true if segment is a valid non-negative integer
     */
    bool is_array_index(const std::string& segment) {
        if (segment.empty()) return false;
        // Must be all digits, no leading zeros except "0" itself
        if (segment[0] == '0' && segment.size() > 1) return false;
        return std::all_of(segment.begin(), segment.end(), ::isdigit);
    }

    /**
     * @brief Parse array index from segment
     * @param segment The segment string
     * @return The index value
     * @pre is_array_index(segment) must be true
     */
    size_t parse_array_index(const std::string& segment) {
        return std::stoull(segment);
    }
}

const Value* get_by_dot(const Value& data, const std::string& path) {
    const auto segments = split_dot_path(path);
    if (segments.empty()) {
        return &data; // Empty path returns root
    }

    const Value* current = &data;
    std::string partial_path;

    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& seg = segments[i];

        // Build partial path for error messages
        if (!partial_path.empty()) partial_path += '.';
        partial_path += seg;

        // Check if we can traverse into current
        if (!current->is_object() && !current->is_array()) {
            throw TypeError(
                path,
                "object or array",
                type_name(*current)
            );
        }

        if (current->is_object()) {
            // Object traversal
            if (!current->contains(seg)) {
                throw KeyError(path, seg);
            }
            current = &(*current)[seg];
        } else {
            // Array traversal
            if (!is_array_index(seg)) {
                throw KeyError(path, seg + " (not a valid array index)");
            }

            size_t idx = parse_array_index(seg);
            if (idx >= current->size()) {
                throw KeyError(path, seg + " (index out of range)");
            }
            current = &(*current)[idx];
        }
    }

    return current;
}

const Value* get_by_dot(const Value& data, const std::string& path,
                       const Value& default_val) {
    const auto segments = split_dot_path(path);
    if (segments.empty()) {
        return &data;
    }

    const Value* current = &data;

    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& seg = segments[i];

        // Check if we can traverse into current
        if (!current->is_object() && !current->is_array()) {
            // RULE D2: Still raise TypeError even with default
            throw TypeError(
                path,
                "object or array",
                type_name(*current)
            );
        }

        if (current->is_object()) {
            if (!current->contains(seg)) {
                // Key not found - return default
                return &default_val;
            }
            current = &(*current)[seg];
        } else {
            // Array traversal
            if (!is_array_index(seg)) {
                // Not a valid index - return default
                return &default_val;
            }

            size_t idx = parse_array_index(seg);
            if (idx >= current->size()) {
                // Out of range - return default
                return &default_val;
            }
            current = &(*current)[idx];
        }
    }

    return current;
}

void set_by_dot(Value& data, const std::string& path,
                const Value& value, bool create_missing) {
    const auto segments = split_dot_path(path);
    if (segments.empty()) {
        // Empty path: replace root
        data = value;
        return;
    }

    Value* current = &data;
    std::string partial_path;

    for (size_t i = 0; i < segments.size() - 1; ++i) {
        const auto& seg = segments[i];

        // Build partial path for error messages
        if (!partial_path.empty()) partial_path += '.';
        partial_path += seg;

        if (!current->is_object()) {
            if (!create_missing) {
                throw TypeError(
                    path,
                    "object",
                    type_name(*current)
                );
            }
            // RULE D4: Overwrite non-object with object
            *current = Value::object();
        }

        if (!current->contains(seg)) {
            if (!create_missing) {
                throw KeyError(path, seg);
            }
            // Create missing intermediate
            (*current)[seg] = Value::object();
        }

        current = &(*current)[seg];
    }

    // Set final value
    const auto& final_seg = segments.back();
    if (!current->is_object()) {
        if (!create_missing) {
            throw TypeError(
                path,
                "object",
                type_name(*current)
            );
        }
        *current = Value::object();
    }

    (*current)[final_seg] = value;
}

bool contains_dot(const Value& data, const std::string& path) {
    const auto segments = split_dot_path(path);
    if (segments.empty()) {
        return true; // Empty path always exists (root)
    }

    const Value* current = &data;

    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& seg = segments[i];

        // Check if we can traverse into current
        if (!current->is_object() && !current->is_array()) {
            // RULE D6: Raise error for invalid traversal
            throw TypeError(
                path,
                "object or array",
                type_name(*current)
            );
        }

        if (current->is_object()) {
            if (!current->contains(seg)) {
                // RULE D5: Return false for missing key (no error)
                return false;
            }
            current = &(*current)[seg];
        } else {
            // Array traversal
            if (!is_array_index(seg)) {
                return false; // Not a valid index
            }

            size_t idx = parse_array_index(seg);
            if (idx >= current->size()) {
                return false; // Out of range
            }
            current = &(*current)[idx];
        }
    }

    return true;
}

} // namespace confy
