/**
 * @file Errors.hpp
 * @brief Exception types for confy configuration errors
 *
 * Defines the error taxonomy specified in CONFY_DESIGN_SPECIFICATION.md:
 * - ConfigError: Base class
 * - MissingMandatoryConfig: Mandatory keys absent
 * - FileNotFoundError: Config file not found
 * - ConfigParseError: JSON/TOML syntax errors
 * - KeyError: Dot-path segment not found
 * - TypeError: Traversal into non-container
 */

#ifndef CONFY_ERRORS_HPP
#define CONFY_ERRORS_HPP

#include <stdexcept>
#include <string>
#include <vector>
#include <sstream>

namespace confy {

/**
 * @brief Base class for all confy exceptions
 */
class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Mandatory configuration keys are missing after merge
 *
 * Contains the list of all missing mandatory keys.
 */
class MissingMandatoryConfig : public ConfigError {
public:
    /**
     * @brief Construct with list of missing keys
     * @param keys Dot-paths of missing mandatory keys
     */
    explicit MissingMandatoryConfig(std::vector<std::string> keys)
        : ConfigError(format_message(keys))
        , missing_keys_(std::move(keys))
    {}

    /**
     * @brief Get the list of missing keys
     * @return Vector of dot-path strings
     */
    const std::vector<std::string>& missing_keys() const noexcept {
        return missing_keys_;
    }

private:
    std::vector<std::string> missing_keys_;

    static std::string format_message(const std::vector<std::string>& keys) {
        std::ostringstream oss;
        oss << "Missing mandatory configuration keys: [";
        for (size_t i = 0; i < keys.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "'" << keys[i] << "'";
        }
        oss << "]";
        return oss.str();
    }
};

/**
 * @brief Configuration file not found
 */
class FileNotFoundError : public ConfigError {
public:
    /**
     * @brief Construct with file path
     * @param path Path to the missing file
     */
    explicit FileNotFoundError(std::string path)
        : ConfigError("Configuration file not found: " + path)
        , path_(std::move(path))
    {}

    /**
     * @brief Get the file path that was not found
     */
    const std::string& path() const noexcept {
        return path_;
    }

private:
    std::string path_;
};

/**
 * @brief Configuration file parse error (JSON/TOML syntax)
 */
class ConfigParseError : public ConfigError {
public:
    /**
     * @brief Construct with file path and error details
     * @param file Path to the file with parse error
     * @param details Detailed error message from parser
     */
    ConfigParseError(std::string file, std::string details)
        : ConfigError("Parse error in '" + file + "': " + details)
        , file_(std::move(file))
        , details_(std::move(details))
    {}

    /**
     * @brief Get the file path with parse error
     */
    const std::string& file() const noexcept {
        return file_;
    }

    /**
     * @brief Get detailed error message
     */
    const std::string& details() const noexcept {
        return details_;
    }

private:
    std::string file_;
    std::string details_;
};

/**
 * @brief Key not found during dot-path traversal
 *
 * Raised when a segment in a dot-path does not exist.
 */
class KeyError : public ConfigError {
public:
    /**
     * @brief Construct with full path and failing segment
     * @param path Full dot-path being accessed (e.g., "database.host")
     * @param segment The specific segment that doesn't exist (e.g., "host")
     */
    KeyError(std::string path, std::string segment)
        : ConfigError("Key not found: '" + segment + "' in path '" + path + "'")
        , path_(std::move(path))
        , segment_(std::move(segment))
    {}

    /**
     * @brief Get the full dot-path that was being accessed
     */
    const std::string& path() const noexcept {
        return path_;
    }

    /**
     * @brief Get the specific segment that was not found
     */
    const std::string& segment() const noexcept {
        return segment_;
    }

private:
    std::string path_;
    std::string segment_;
};

/**
 * @brief Type mismatch during dot-path traversal
 *
 * Raised when attempting to traverse into a non-container type
 * (e.g., trying to access "scalar_value.sub_key").
 */
class TypeError : public ConfigError {
public:
    /**
     * @brief Construct with path, expected type, and actual type
     * @param path Full dot-path being accessed
     * @param expected Expected type (e.g., "object")
     * @param actual Actual type encountered (e.g., "integer")
     */
    TypeError(std::string path, std::string expected, std::string actual)
        : ConfigError("Cannot traverse into " + actual +
                      " (expected " + expected + ") at path '" + path + "'")
        , path_(std::move(path))
        , expected_(std::move(expected))
        , actual_(std::move(actual))
    {}

    /**
     * @brief Get the full dot-path that was being accessed
     */
    const std::string& path() const noexcept {
        return path_;
    }

    /**
     * @brief Get the expected type
     */
    const std::string& expected() const noexcept {
        return expected_;
    }

    /**
     * @brief Get the actual type encountered
     */
    const std::string& actual() const noexcept {
        return actual_;
    }

private:
    std::string path_;
    std::string expected_;
    std::string actual_;
};

} // namespace confy

#endif // CONFY_ERRORS_HPP
