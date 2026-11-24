/**
 * @file Schema.cpp
 * @brief Schema validation (C-04)
 */
#include "confy/Schema.hpp"
#include "confy/DotPath.hpp"

#include <algorithm>
#include <map>
#include <regex>

namespace confy {

namespace {

const char* type_name(KeySpec::Type t) {
    switch (t) {
        case KeySpec::Type::any: return "any";
        case KeySpec::Type::string: return "string";
        case KeySpec::Type::integer: return "integer";
        case KeySpec::Type::number: return "number";
        case KeySpec::Type::boolean: return "boolean";
        case KeySpec::Type::array: return "array";
        case KeySpec::Type::object: return "object";
    }
    return "?";
}

bool has_type(const Value& v, KeySpec::Type t) {
    switch (t) {
        case KeySpec::Type::any: return true;
        case KeySpec::Type::string: return v.is_string();
        case KeySpec::Type::integer: return v.is_number_integer();
        case KeySpec::Type::number: return v.is_number();
        case KeySpec::Type::boolean: return v.is_boolean();
        case KeySpec::Type::array: return v.is_array();
        case KeySpec::Type::object: return v.is_object();
    }
    return false;
}

const Value* find(const Value& root, const std::string& path) {
    const Value* cur = &root;
    std::size_t i = 0;
    while (i <= path.size()) {
        const auto j = std::min(path.find('.', i), path.size());
        const std::string part = path.substr(i, j - i);
        if (!cur->is_object() || !cur->contains(part)) return nullptr;
        cur = &(*cur)[part];
        i = j + 1;
    }
    return cur;
}

void walk(const Value& v, const std::string& prefix, const std::map<std::string, const KeySpec*>& specs,
          const std::set<std::string>& parents, std::vector<std::string>& unknown) {
    if (!v.is_object()) return;
    for (auto it = v.begin(); it != v.end(); ++it) {
        const std::string p = prefix.empty() ? it.key() : prefix + "." + it.key();
        auto s = specs.find(p);
        if (s != specs.end()) {
            if (it.value().is_object() && !s->second->open && s->second->type == KeySpec::Type::object) walk(it.value(), p, specs, parents, unknown);
            continue;
        }
        if (parents.count(p) && it.value().is_object()) { walk(it.value(), p, specs, parents, unknown); continue; }
        unknown.push_back(p);
    }
}

std::size_t distance(const std::string& a, const std::string& b) {
    std::vector<std::size_t> row(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j) row[j] = j;
    for (std::size_t i = 1; i <= a.size(); ++i) {
        std::size_t prev = row[0];
        row[0] = i;
        for (std::size_t j = 1; j <= b.size(); ++j) {
            const std::size_t cur = row[j];
            row[j] = std::min({row[j] + 1, row[j - 1] + 1, prev + (a[i - 1] == b[j - 1] ? 0 : 1)});
            prev = cur;
        }
    }
    return row[b.size()];
}

}  // namespace

std::set<std::string> Schema::string_keys() const {
    std::set<std::string> out;
    for (const auto& k : keys)
        if (k.type == KeySpec::Type::string) out.insert(k.path);
    return out;
}

std::vector<Violation> validate(const Value& data, const Schema& schema) {
    std::vector<Violation> out;
    std::map<std::string, const KeySpec*> specs;
    std::set<std::string> parents;
    for (const auto& k : schema.keys) {
        specs[k.path] = &k;
        for (auto d = k.path.find('.'); d != std::string::npos; d = k.path.find('.', d + 1)) parents.insert(k.path.substr(0, d));
    }
    for (const auto& [path, k] : specs) {
        const Value* v = find(data, path);
        if (!v) {
            if (k->required) out.push_back({Violation::Kind::missing_required, path, "required key is missing", true, {}});
            continue;
        }
        if (k->renamed_to) {
            out.push_back({Violation::Kind::renamed, path, "renamed to " + *k->renamed_to, false, *k->renamed_to});
            continue;
        }
        if (k->deprecated) out.push_back({Violation::Kind::deprecated, path, *k->deprecated, false, {}});
        if (!has_type(*v, k->type)) {
            out.push_back({Violation::Kind::wrong_type, path, std::string("expected ") + type_name(k->type) + ", got " + v->type_name(), true, {}});
            continue;
        }
        if (!k->one_of.empty() && std::find(k->one_of.begin(), k->one_of.end(), *v) == k->one_of.end()) {
            std::string allowed;
            for (const auto& a : k->one_of) allowed += (allowed.empty() ? "" : ", ") + a.dump();
            out.push_back({Violation::Kind::not_in_enum, path, v->dump() + " is not one of " + allowed, true, {}});
        }
        if (k->min || k->max) {
            std::optional<double> x;
            if (v->is_number()) x = v->get<double>();
            else if (v->is_string()) x = static_cast<double>(v->get<std::string>().size());
            else if (v->is_array()) x = static_cast<double>(v->size());
            if (x && ((k->min && *x < *k->min) || (k->max && *x > *k->max))) {
                std::string range = "[" + (k->min ? Value(*k->min).dump() : std::string("-inf")) + ", " + (k->max ? Value(*k->max).dump() : std::string("inf")) + "]";
                out.push_back({Violation::Kind::out_of_range, path, (v->is_number() ? v->dump() : "length " + Value(*x).dump()) + " is outside " + range, true, {}});
            }
        }
        if (k->pattern && v->is_string()) {
            try {
                if (!std::regex_match(v->get<std::string>(), std::regex(*k->pattern)))
                    out.push_back({Violation::Kind::pattern_mismatch, path, v->dump() + " does not match " + *k->pattern, true, {}});
            } catch (const std::regex_error&) {
                out.push_back({Violation::Kind::pattern_mismatch, path, "schema pattern is not a valid regular expression: " + *k->pattern, true, {}});
            }
        }
    }
    if (schema.unknown != Schema::Unknown::allow) {
        std::vector<std::string> unknown;
        walk(data, "", specs, parents, unknown);
        for (const auto& u : unknown) {
            std::string best;
            std::size_t bd = 3;
            for (const auto& [p, k] : specs) {
                const auto d = distance(u, p);
                if (d < bd) bd = d, best = p;
            }
            out.push_back({Violation::Kind::unknown_key, u, "unknown key" + (best.empty() ? std::string() : " (did you mean " + best + "?)"),
                           schema.unknown == Schema::Unknown::error, best});
        }
    }
    return out;
}

Value apply_renames(const Value& data, const Schema& schema) {
    Value out = data;
    for (const auto& k : schema.keys) {
        if (!k.renamed_to) continue;
        const Value* v = find(out, k.path);
        if (!v) continue;
        const Value moved = *v;
        if (!find(out, *k.renamed_to)) set_by_dot(out, *k.renamed_to, moved, true);
        // remove the old leaf
        const auto d = k.path.rfind('.');
        if (d == std::string::npos) out.erase(k.path);
        else {
            Value* parent = &out;
            std::size_t i = 0;
            const std::string pp = k.path.substr(0, d);
            while (i <= pp.size()) {
                const auto j = std::min(pp.find('.', i), pp.size());
                parent = &(*parent)[pp.substr(i, j - i)];
                i = j + 1;
            }
            parent->erase(k.path.substr(d + 1));
        }
    }
    return out;
}

}  // namespace confy
