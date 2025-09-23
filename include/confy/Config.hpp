#ifndef CONFY_CONFIG_HPP
#define CONFY_CONFIG_HPP

#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>
#include <string>
#include <map>
#include <vector>
#include <optional>
#include "confy/Exceptions.hpp"

namespace confy {

/**
 * @brief Options for constructing a Config from multiple sources.
 */
struct LoadOptions {
    std::optional<std::string> file_path;
    std::optional<std::string> prefix; // Environment variable prefix, e.g. "APP_CONF"
    std::map<std::string, nlohmann::json> overrides; // final precedence
    nlohmann::json defaults = nlohmann::json::object();
    std::vector<std::string> mandatory;
};

/**
 * @brief Core configuration container with dot-notation helpers.
 *
 * Internally uses nlohmann::json to represent a hierarchical tree.
 * Supports JSON/TOML IO and layered merging.
 */
class Config {
public:
    Config() = default;
    explicit Config(nlohmann::json data) : data_(std::move(data)) {}

    // Load using the precedence: defaults -> file -> env (prefix) -> overrides
    static Config load(const LoadOptions& opts);

    // Access the underlying tree
    const nlohmann::json& data() const noexcept { return data_; }
    nlohmann::json& data() noexcept { return data_; }

    // Dot helpers
    const nlohmann::json& at(const std::string& path) const;
    bool contains(const std::string& path) const;
    void set(const std::string& path, const nlohmann::json& v);

    template <typename T>
    T get(const std::string& path, const T& fallback) const {
        if (!contains(path)) return fallback;
        try {
            return at(path).get<T>();
        } catch (...) {
            return fallback;
        }
    }

    // Enforcement
    void enforce_mandatory(const std::vector<std::string>& keys) const;

    // Serialization
    std::string to_json_string(int indent = 2) const;
    std::string to_toml_string() const;

    // ENV / Overrides
    void apply_env_prefix(const std::string& prefix);
    void apply_overrides(const std::map<std::string, nlohmann::json>& kv);

    // File IO
    static nlohmann::json read_file_any(const std::string& file);
    static void write_file_json(const std::string& file, const nlohmann::json& j);
    static void write_file_toml(const std::string& file, const nlohmann::json& j);

private:
    nlohmann::json data_ = nlohmann::json::object();

    // Conversions between json <-> toml::node
    static nlohmann::json toml_to_json(const toml::node& n);
    // Build a TOML table by value from a JSON object.
    static toml::table json_to_toml(const nlohmann::json& j);
    // Helper to detect file extension
    static std::string ext_of(const std::string& path);
};

} // namespace confy

#endif // CONFY_CONFIG_HPP
