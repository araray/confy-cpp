#include "confy/Util.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#if defined(_WIN32)
  #include <windows.h>
  #include <cstdlib>
#else
  #include <unistd.h>
  extern char **environ;
#endif

namespace confy {

void deep_merge(nlohmann::json& a, const nlohmann::json& b) {
    if (!a.is_object() || !b.is_object()) {
        a = b;
        return;
    }
    for (auto it = b.begin(); it != b.end(); ++it) {
        const auto& key = it.key();
        const auto& bv  = it.value();
        if (a.contains(key) && a[key].is_object() && bv.is_object()) {
            deep_merge(a[key], bv);
        } else {
            a[key] = bv;
        }
    }
}

static const nlohmann::json& get_obj_child(const nlohmann::json& obj, const std::string& key) {
    if (!obj.is_object())
        throw std::out_of_range("Attempt to access child on non-object");
    auto it = obj.find(key);
    if (it == obj.end())
        throw std::out_of_range("Missing key: " + key);
    return *it;
}

const nlohmann::json& get_by_dot(const nlohmann::json& obj, const std::string& path) {
    const nlohmann::json* cur = &obj;
    std::string token;
    std::istringstream iss(path);
    while (std::getline(iss, token, '.')) {
        cur = &get_obj_child(*cur, token);
    }
    return *cur;
}

bool exists_by_dot(const nlohmann::json& obj, const std::string& path) {
    try {
        (void)get_by_dot(obj, path);
        return true;
    } catch (...) {
        return false;
    }
}

void set_by_dot(nlohmann::json& obj, const std::string& path, const nlohmann::json& value) {
    nlohmann::json* cur = &obj;
    std::string token;
    std::istringstream iss(path);
    std::vector<std::string> parts;
    while (std::getline(iss, token, '.')) parts.push_back(token);
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        auto& p = parts[i];
        if (!(*cur).contains(p) || !(*cur)[p].is_object()) {
            (*cur)[p] = nlohmann::json::object();
        }
        cur = &(*cur)[p];
    }
    (*cur)[parts.back()] = value;
}

std::map<std::string, nlohmann::json> flatten(const nlohmann::json& obj) {
    std::map<std::string, nlohmann::json> out;
    std::vector<std::pair<std::string, const nlohmann::json*>> stack;
    stack.emplace_back("", &obj);

    while (!stack.empty()) {
        auto [prefix, node] = stack.back();
        stack.pop_back();
        if (node->is_object()) {
            for (auto it = node->begin(); it != node->end(); ++it) {
                std::string key = prefix.empty() ? it.key() : prefix + "." + it.key();
                stack.emplace_back(key, &(*it));
            }
        } else {
            out[prefix] = *node;
        }
    }
    return out;
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::string tok;
    std::istringstream iss(s);
    while (std::getline(iss, tok, delim)) {
        if (!tok.empty()) parts.push_back(tok);
    }
    return parts;
}

bool match_pattern(const std::string& pattern, const std::string& text, bool ignoreCase) {
    if (pattern.empty()) return true;

    auto is_glob = [](const std::string& p){
        return p.find_first_of("*?[]") != std::string::npos;
    };
    auto is_regex = [](const std::string& p){
        return p.find_first_of(".+^$(){}|\\") != std::string::npos;
    };

    if (is_glob(pattern)) {
        // translate glob to regex, case-insensitive
        std::string rx = "^";
        for (char c : pattern) {
            switch (c) {
                case '*': rx += ".*"; break;
                case '?': rx += "."; break;
                case '.': case '+': case '^': case '$': case '(': case ')':
                case '{': case '}': case '|': case '\\': case '[': case ']':
                    rx += '\\'; rx += c; break;
                default: rx += std::tolower(static_cast<unsigned char>(c)); break;
            }
        }
        rx += "$";
        std::string t = to_lower(text);
        try {
            return std::regex_search(t, std::regex(rx));
        } catch (...) { return false; }
    }

    if (is_regex(pattern)) {
        try {
            auto flags = ignoreCase ? std::regex_constants::icase : std::regex_constants::ECMAScript;
            return std::regex_search(text, std::regex(pattern, flags));
        } catch (...) {
            return false;
        }
    }

    // exact, case-insensitive
    std::string a = to_lower(pattern);
    std::string b = to_lower(text);
    return a == b;
}

std::map<std::string, nlohmann::json> parse_overrides(const std::string& s) {
    std::map<std::string, nlohmann::json> out;
    if (s.empty()) return out;
    // split by commas, but allow JSON commas inside values by simple heuristic:
    // we split on commas that are not inside braces/brackets/quotes (basic state machine).
    int depth = 0;
    bool in_str = false;
    char str_ch = '\0';
    std::string buf;
    auto flush = [&](){
        std::string pair = buf;
        buf.clear();
        auto pos = pair.find(':');
        if (pos == std::string::npos) return;
        std::string k = pair.substr(0, pos);
        std::string v = pair.substr(pos+1);
        // trim
        auto trim = [](std::string& x){
            size_t i=0, j=x.size();
            while (i<j && std::isspace(static_cast<unsigned char>(x[i]))) ++i;
            while (j>i && std::isspace(static_cast<unsigned char>(x[j-1]))) --j;
            x = x.substr(i, j-i);
        };
        trim(k); trim(v);
        if (k.empty()) return;
        out[k] = parse_json_or_string(v);
    };
    for (size_t i=0;i<s.size();++i){
        char c = s[i];
        if (in_str) {
            buf += c;
            if (c == str_ch && (i==0 || s[i-1] != '\\')) in_str = false;
            continue;
        }
        if (c=='"' || c=='\'') { in_str = true; str_ch = c; buf += c; continue; }
        if (c=='{' || c=='[') { depth++; buf += c; continue; }
        if (c=='}' || c==']') { depth--; buf += c; continue; }
        if (c==',' and depth==0) { flush(); continue; }
        buf += c;
    }
    if (!buf.empty()) flush();
    return out;
}

std::vector<std::pair<std::string, std::string>> enumerate_environment() {
    std::vector<std::pair<std::string, std::string>> envs;
#if defined(_WIN32)
    LPTCH env = GetEnvironmentStringsA();
    if (!env) return envs;
    for (LPSTR var = (LPSTR)env; *var != '\0'; var += strlen(var) + 1) {
        std::string entry(var);
        auto pos = entry.find('=');
        if (pos == std::string::npos) continue;
        envs.emplace_back(entry.substr(0,pos), entry.substr(pos+1));
    }
    FreeEnvironmentStringsA(env);
#else
    if (environ) {
        for (char **env = environ; *env; ++env) {
            std::string entry(*env);
            auto pos = entry.find('=');
            if (pos == std::string::npos) continue;
            envs.emplace_back(entry.substr(0,pos), entry.substr(pos+1));
        }
    }
#endif
    return envs;
}

nlohmann::json parse_json_or_string(const std::string& raw) {
    // Attempt JSON parse
    try {
        return nlohmann::json::parse(raw);
    } catch (...) {
        // If it's a quoted word without surrounding quotes, still treat raw as string
        return nlohmann::json(raw);
    }
}

} // namespace confy
