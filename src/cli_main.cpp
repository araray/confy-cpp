/**
 * @file cli_main.cpp
 * @brief CLI tool entry point (Phase 4)
 *
 * Command-line interface for confy-cpp.
 * Provides: get, set, exists, search, dump, convert commands.
 *
 * Phase 4 implementation - currently a stub.
 */

#include <iostream>
#include <string>

// Phase 4: Include cxxopts for CLI parsing
// #include <cxxopts.hpp>
// #include "confy/Config.hpp"

void print_usage() {
    std::cout << "confy-cpp - Configuration Management CLI\n\n";
    std::cout << "Usage: confy-cpp [OPTIONS] COMMAND [ARGS]\n\n";
    std::cout << "Global Options:\n";
    std::cout << "  -c, --config PATH      Path to config file (JSON/TOML)\n";
    std::cout << "  -p, --prefix TEXT      Environment variable prefix\n";
    std::cout << "  --overrides TEXT       Comma-separated overrides\n";
    std::cout << "  --defaults PATH        Path to defaults file\n";
    std::cout << "  --mandatory TEXT       Comma-separated mandatory keys\n";
    std::cout << "  -h, --help             Show this help\n\n";
    std::cout << "Commands:\n";
    std::cout << "  get KEY                Get value at dot-path\n";
    std::cout << "  set KEY VALUE          Set value in config file\n";
    std::cout << "  exists KEY             Check if key exists\n";
    std::cout << "  search [OPTIONS]       Search keys/values\n";
    std::cout << "  dump                   Print entire config\n";
    std::cout << "  convert --to FORMAT    Convert to JSON/TOML\n\n";
    std::cout << "Phase 4 - Full implementation coming soon\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];

    if (command == "--help" || command == "-h") {
        print_usage();
        return 0;
    }

    std::cout << "confy-cpp: Command '" << command << "' not yet implemented\n";
    std::cout << "Phase 4 implementation in progress\n";
    std::cout << "Currently available: libconfy with core utilities (Phase 1 complete)\n\n";
    std::cout << "Use --help for available commands\n";

    return 1;
}
