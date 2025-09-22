#include <cxxopts.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include "confy/Config.hpp"
#include "confy/Util.hpp"
#include "confy/Exceptions.hpp"

using nlohmann::json;
using namespace confy;

int main(int argc, char** argv) {
    try {
        cxxopts::Options options("confy-cpp", "Inspect & mutate JSON/TOML configs via dot-notation");
        options.positional_help("COMMAND [ARGS]");

        // Global options
        options.add_options()
            ("c,config", "Path to JSON/TOML config", cxxopts::value<std::string>())
            ("p,prefix", "Env-var prefix for overrides", cxxopts::value<std::string>())
            ("overrides", "Comma-separated dot:key,JSON_value pairs", cxxopts::value<std::string>()->default_value(""))
            ("defaults", "Path to JSON file containing defaults", cxxopts::value<std::string>())
            ("mandatory", "Comma-separated list of mandatory dot-keys", cxxopts::value<std::string>()->default_value(""))
            ("h,help", "Show help");

        // Command + sub-options captured as positional strings
        options.add_options()
            ("command", "Subcommand", cxxopts::value<std::vector<std::string>>());

        options.parse_positional({"command"});

        auto result = options.parse(argc, argv);
        if (result.count("help") || !result.count("command")) {
            std::cout << options.help({"", "Group"}) << "\n";
            std::cout << "Commands: get KEY | set KEY VALUE | exists KEY | search [--key PAT] [--val PAT] [-i] | dump | convert [--to json|toml] [--out FILE]\n";
            return 0;
        }

        // Prepare LoadOptions
        LoadOptions load;
        if (result.count("config")) load.file_path = result["config"].as<std::string>();
        if (result.count("prefix")) load.prefix = result["prefix"].as<std::string>();
        if (result.count("defaults")) {
            std::ifstream ifs(result["defaults"].as<std::string>());
            if (!ifs) {
                std::cerr << "Error: Could not open defaults file\n";
                return 1;
            }
            json d; ifs >> d; load.defaults = d;
        }
        if (result.count("overrides")) {
            load.overrides = parse_overrides(result["overrides"].as<std::string>());
        }
        if (result.count("mandatory")) {
            auto toks = split(result["mandatory"].as<std::string>(), ',');
            load.mandatory = toks;
        }

        // Parse subcommand
        auto cmdv = result["command"].as<std::vector<std::string>>();
        if (cmdv.empty()) { std::cerr << "Error: missing command\n"; return 1; }
        std::string cmd = cmdv[0];

        // For commands that operate on the final merged config, load it now
        Config cfg = Config::load(load);

        // Helpers to require extra args
        auto expect_args = [&](size_t want) {
            if (cmdv.size() < want) {
                std::cerr << "Error: insufficient arguments for command '"<<cmd<<"'\n";
                std::exit(1);
            }
        };

        // GET
        if (cmd == "get") {
            expect_args(2);
            const std::string key = cmdv[1];
            try {
                const json& v = cfg.at(key);
                std::cout << v.dump(2) << "\n";
            } catch (const std::exception& ex) {
                std::cerr << "Key not found: " << key << "\n";
                return 1;
            }
            return 0;
        }

        // SET (requires --config)
        if (cmd == "set") {
            expect_args(3);
            if (!load.file_path.has_value()) {
                std::cerr << "Error: --config must be provided for `set`\n";
                return 1;
            }
            const std::string key = cmdv[1];
            const std::string raw = cmdv[2];
            json parsed = parse_json_or_string(raw);

            // Load the file directly and write back in its native format
            auto src = Config::read_file_any(*load.file_path);
            set_by_dot(src, key, parsed);
            std::string ext = *load.file_path;
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext.size() >= 5 && ext.substr(ext.size()-5) == ".json") {
                Config::write_file_json(*load.file_path, src);
            } else if (ext.size() >= 5 && ext.substr(ext.size()-5) == ".toml") {
                Config::write_file_toml(*load.file_path, src);
            } else {
                std::cerr << "Error: Unsupported file type for set\n";
                return 1;
            }
            std::cout << "Set " << key << " = " << parsed.dump() << " in " << *load.file_path << "\n";
            return 0;
        }

        // EXISTS
        if (cmd == "exists") {
            expect_args(2);
            const std::string key = cmdv[1];
            bool ok = cfg.contains(key);
            std::cout << (ok ? "true" : "false") << "\n";
            return ok ? 0 : 1;
        }

        // SEARCH
        if (cmd == "search") {
            // Re-parse options for this subcommand: allow flags after "search"
            // naive scan for --key / --val / -i
            std::string keypat, valpat;
            bool ignoreCase = false;
            for (size_t i=1; i<cmdv.size(); ++i) {
                if (cmdv[i] == "--key" && i+1 < cmdv.size()) { keypat = cmdv[++i]; continue; }
                if (cmdv[i] == "--val" && i+1 < cmdv.size()) { valpat = cmdv[++i]; continue; }
                if (cmdv[i] == "-i" || cmdv[i] == "--ignore-case") { ignoreCase = true; continue; }
            }
            if (keypat.empty() && valpat.empty()) {
                std::cerr << "Error: supply --key or --val\n";
                return 1;
            }
            auto fl = flatten(cfg.data());
            json found = json::object();
            for (const auto& [k, v] : fl) {
                bool km = keypat.empty() ? true : match_pattern(keypat, k, ignoreCase);
                bool vm = valpat.empty() ? true : match_pattern(valpat, v.dump(), ignoreCase);
                if (km && vm) found[k] = v;
            }
            if (found.empty()) {
                std::cout << "No matches\n";
                return 1;
            }
            std::cout << found.dump(2) << "\n";
            return 0;
        }

        // DUMP
        if (cmd == "dump") {
            std::cout << cfg.to_json_string(2) << "\n";
            return 0;
        }

        // CONVERT
        if (cmd == "convert") {
            // defaults to JSON
            std::string to = "json";
            std::string out;
            // parse simple flags
            for (size_t i=1; i<cmdv.size(); ++i) {
                if (cmdv[i] == "--to" && i+1 < cmdv.size()) { to = cmdv[++i]; continue; }
                if (cmdv[i] == "--out" && i+1 < cmdv.size()) { out = cmdv[++i]; continue; }
            }
            std::string text;
            if (to == "toml") text = cfg.to_toml_string();
            else text = cfg.to_json_string(2);

            if (!out.empty()) {
                std::ofstream ofs(out);
                if (!ofs) { std::cerr << "Error: cannot write to " << out << "\n"; return 1; }
                ofs << text;
                std::cout << "Wrote " << (to=="toml"?"TOML":"JSON") << " to " << out << "\n";
            } else {
                std::cout << text << "\n";
            }
            return 0;
        }

        std::cerr << "Unknown command: " << cmd << "\n";
        return 1;

    } catch (const MissingMandatoryConfig& mmc) {
        std::cerr << "Error: " << mmc.what() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
