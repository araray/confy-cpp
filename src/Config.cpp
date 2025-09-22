#include "confy/Config.hpp"
#include "confy/Util.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <system_error>
#include <algorithm>

namespace confy {

Config Config::load(const LoadOptions& opts) {
    nlohmann::json merged = nlohmann::json::object();

    // 1) defaults
    deep_merge(merged, opts.defaults);

    // 2) file
    if (opts.file_path.has_value()) {
        nlohmann::json filej = read_file_any(*opts.file_path);
        deep_merge(merged, filej);
    }

    Config cfg(merged);

    // 3) env
    if (opts.prefix.has_value() && !opts.prefix->empty()) {
        cfg.apply_env_prefix(*opts.prefix);
    }

    // 4) overrides
    cfg.apply_overrides(opts.overrides);

    // 5) mandatory
    cfg.enforce_mandatory(opts.mandatory);

    return cfg;
}

const nlohmann::json& Config::at(const std::string& path) const {
    return get_by_dot(data_, path);
}

bool Config::contains(const std::string& path) const {
    return exists_by_dot(data_, path);
}

void Config::set(const std::string& path, const nlohmann::json& v) {
    set_by_dot(data_, path, v);
}

void Config::enforce_mandatory(const std::vector<std::string>& keys) const {
    std::vector<std::string> missing;
    for (const auto& k : keys) {
        if (!contains(k)) missing.push_back(k);
    }
    if (!missing.empty()) throw MissingMandatoryConfig(missing);
}

std::string Config::to_json_string(int indent) const {
    return data_.dump(indent);
}

static toml::node* json_to_toml_impl(const nlohmann::json& j) {
    using nlohmann::json;
    if (j.is_object()) {
        auto* tbl = new toml::table{};
        for (auto it = j.begin(); it != j.end(); ++it) {
            tbl->insert(it.key(), std::unique_ptr<toml::node>(json_to_toml_impl(it.value())));
        }
        return tbl;
    } else if (j.is_array()) {
        auto* arr = new toml::array{};
        for (const auto& v : j) {
            arr->push_back(std::unique_ptr<toml::node>(json_to_toml_impl(v)));
        }
        return arr;
    } else if (j.is_string()) {
        return new toml::value<std::string>(j.get<std::string>());
    } else if (j.is_boolean()) {
        return new toml::value<bool>(j.get<bool>());
    } else if (j.is_number_integer()) {
        return new toml::value<std::int64_t>(j.get<std::int64_t>());
    } else if (j.is_number_unsigned()) {
        return new toml::value<std::uint64_t>(j.get<std::uint64_t>());
    } else if (j.is_number_float()) {
        return new toml::value<double>(j.get<double>());
    } else if (j.is_null()) {
        // Represent null as empty string (TOML has no null). Alternative: omit key.
        return new toml::value<std::string>("");
    }
    // Fallback to string
    return new toml::value<std::string>(j.dump());
}

toml::node* Config::json_to_toml(const nlohmann::json& j) {
    return json_to_toml_impl(j);
}

static nlohmann::json toml_to_json_impl(const toml::node& n) {
    using nlohmann::json;
    if (auto v = n.as_string()) {
        return json(v->get());
    } else if (auto v = n.as_integer()) {
        return json(v->get());
    } else if (auto v = n.as_floating_point()) {
        return json(v->get());
    } else if (auto v = n.as_boolean()) {
        return json(v->get());
    } else if (auto v = n.as_array()) {
        json arr = json::array();
        for (const auto& elem : *v) arr.push_back(toml_to_json_impl(elem));
        return arr;
    } else if (auto v = n.as_table()) {
        json obj = json::object();
        for (const auto& [k, val] : *v) {
            obj[k] = toml_to_json_impl(val);
        }
        return obj;
    } else if (auto v = n.as_date()) {
        std::ostringstream oss; oss << *v; return json(oss.str());
    } else if (auto v = n.as_time()) {
        std::ostringstream oss; oss << *v; return json(oss.str());
    } else if (auto v = n.as_date_time()) {
        std::ostringstream oss; oss << *v; return json(oss.str());
    }
    return nlohmann::json();
}

nlohmann::json Config::toml_to_json(const toml::node& n) {
    return toml_to_json_impl(n);
}

std::string Config::to_toml_string() const {
    std::unique_ptr<toml::node> root(json_to_toml(data_));
    std::ostringstream oss;
    oss << *root;
    return oss.str();
}

std::string Config::ext_of(const std::string& path) {
    auto pos = path.find_last_of('.');
    if (pos == std::string::npos) return "";
    std::string ext = path.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

nlohmann::json Config::read_file_any(const std::string& file) {
    std::string ext = ext_of(file);
    if (ext == ".json") {
        std::ifstream ifs(file);
        if (!ifs) throw std::runtime_error("Failed to open JSON file: " + file);
        nlohmann::json j;
        ifs >> j;
        return j;
    } else if (ext == ".toml") {
        toml::table tbl = toml::parse_file(file);
        return toml_to_json(tbl);
    } else {
        throw std::runtime_error("Unsupported config file type: " + ext);
    }
}

void Config::write_file_json(const std::string& file, const nlohmann::json& j) {
    std::ofstream ofs(file);
    if (!ofs) throw std::runtime_error("Failed to open for write: " + file);
    ofs << std::setw(2) << j << "\n";
}

void Config::write_file_toml(const std::string& file, const nlohmann::json& j) {
    std::unique_ptr<toml::node> root(json_to_toml(j));
    std::ofstream ofs(file);
    if (!ofs) throw std::runtime_error("Failed to open for write: " + file);
    ofs << *root;
}

void Config::apply_env_prefix(const std::string& prefix) {
    // Python confy behavior: prefix is normalized to end with '_'
    std::string normalized = prefix;
    while (!normalized.empty() && normalized.back() == '_') normalized.pop_back();
    normalized += "_";

    for (const auto& [name, value] : enumerate_environment()) {
        // check startswith (case-sensitive match for simplicity)
        if (name.rfind(normalized, 0) == 0) {
            // remainder -> lower, underscores become dots
            std::string key = name.substr(normalized.size());
            for (auto& ch : key) ch = std::tolower(static_cast<unsigned char>(ch));
            std::replace(key.begin(), key.end(), '_', '.');

            if (key.empty()) continue;
            nlohmann::json v = parse_json_or_string(value);
            set_by_dot(data_, key, v);
        }
    }
}

void Config::apply_overrides(const std::map<std::string, nlohmann::json>& kv) {
    for (const auto& [k, v] : kv) {
        set_by_dot(data_, k, v);
    }
}

} // namespace confy
