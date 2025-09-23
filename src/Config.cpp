#include "confy/Config.hpp"
#include "confy/Util.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <system_error>
#include <algorithm>
#include <memory>
#include <cstdint>
#include <limits>
#include <cctype>
#include <limits>

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

// ---- JSON -> TOML (value-based construction; no raw nodes) -----------------
namespace {
    using nlohmann::json;

    inline void insert_scalar(toml::table& tbl, const std::string& key, const json& v) {
        if (v.is_string()) {
            tbl.insert(key, v.get<std::string>());
        } else if (v.is_boolean()) {
            tbl.insert(key, v.get<bool>());
        } else if (v.is_number_integer()) {
            tbl.insert(key, static_cast<std::int64_t>(v.get<std::int64_t>()));
        } else if (v.is_number_unsigned()) {
            const auto u = v.get<std::uint64_t>();
            if (u <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                tbl.insert(key, static_cast<std::int64_t>(u));
            } else {
                // Oversize for TOML int; fall back to double
                tbl.insert(key, static_cast<double>(u));
            }
        } else if (v.is_number_float()) {
            tbl.insert(key, v.get<double>());
        } else if (v.is_null()) {
            // No TOML null: preserve as empty string (prior behavior)
            tbl.insert(key, std::string{""});
        } else {
            // Fallback for anything else: JSON text
            tbl.insert(key, v.dump());
        }
    }

    toml::array make_array_from_json(const json& a); // fwd
    toml::table make_table_from_json(const json& o); // fwd

    toml::array make_array_from_json(const json& a) {
        toml::array out;
        for (const auto& elem : a) {
            if (elem.is_object()) {
                out.push_back(make_table_from_json(elem));
            } else if (elem.is_array()) {
                out.push_back(make_array_from_json(elem));
            } else if (elem.is_string()) {
                out.push_back(elem.get<std::string>());
            } else if (elem.is_boolean()) {
                out.push_back(elem.get<bool>());
            } else if (elem.is_number_integer()) {
                out.push_back(static_cast<std::int64_t>(elem.get<std::int64_t>()));
            } else if (elem.is_number_unsigned()) {
                const auto u = elem.get<std::uint64_t>();
                if (u <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                    out.push_back(static_cast<std::int64_t>(u));
                else
                    out.push_back(static_cast<double>(u));
            } else if (elem.is_number_float()) {
                out.push_back(elem.get<double>());
            } else if (elem.is_null()) {
                out.push_back(std::string{""});
            } else {
                out.push_back(elem.dump());
            }
        }
        return out;
    }

    toml::table make_table_from_json(const json& o) {
        toml::table tbl;
        for (auto it = o.begin(); it != o.end(); ++it) {
            const auto& k = it.key();
            const auto& v = it.value();
            if (v.is_object()) {
                tbl.insert(k, make_table_from_json(v));
            } else if (v.is_array()) {
                tbl.insert(k, make_array_from_json(v));
            } else {
                insert_scalar(tbl, k, v);
            }
        }
        return tbl;
    }
} // namespace

toml::table Config::json_to_toml(const nlohmann::json& j) {
    // TOML requires a table at the root; if j isn't an object, wrap under "value".
    if (j.is_object()) return make_table_from_json(j);
    toml::table root;
    // keep behavior consistent with previous implementation
    if (j.is_array()) root.insert("value", make_array_from_json(j));
    else insert_scalar(root, "value", j);
    return root;
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

// Helper: stream a concrete toml++ node type (table/array/value) to an ostream.
static void stream_toml_node(std::ostream& os, const toml::node& n) {
    if (auto t = n.as_table()) { os << *t; return; }
    if (auto a = n.as_array()) { os << *a; return; }
    if (auto v = n.as_string()) { os << *v; return; }
    if (auto v = n.as_integer()) { os << *v; return; }
    if (auto v = n.as_floating_point()) { os << *v; return; }
    if (auto v = n.as_boolean()) { os << *v; return; }
    if (auto v = n.as_date()) { os << *v; return; }
    if (auto v = n.as_time()) { os << *v; return; }
    if (auto v = n.as_date_time()) { os << *v; return; }
    // Fallback: nothing matched (shouldn't happen)
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
    auto root = json_to_toml(data_);
    std::ostringstream oss;
    oss << root;
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
    auto root = json_to_toml(j);
    std::ofstream ofs(file);
    if (!ofs) throw std::runtime_error("Failed to open for write: " + file);
    ofs << root;
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
