# confy-cpp

**Unified Configuration Management for C++**

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/the-standard)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)]()

A minimal, flexible configuration management library that provides a unified, predictable way to manage application configuration across multiple sources. This is the C++ port of [confy](https://github.com/araray/confy) (Python), designed for **100% behavioral parity**.

---

## ✨ Features

- **Layered Precedence** — Well-defined override order: defaults → file → .env → environment → overrides
- **Dot-Notation Access** — Intuitive nested access via `cfg.get("database.host")`
- **Multiple Formats** — Native support for JSON and TOML configuration files
- **Environment Integration** — Seamless override via environment variables with prefix filtering
- **Smart Env Mapping** — Automatic underscore transformation (`DATABASE_HOST` → `database.host`)
- **Validation** — Mandatory key enforcement with actionable error messages
- **CLI Tool** — Command-line inspection, mutation, and format conversion
- **Cross-Platform** — Linux, macOS, Windows (x86_64 and ARM64)
- **Modern C++17** — Clean, well-documented codebase

---

## 📦 Installation

### Prerequisites

- **C++17 compiler**: GCC 11+, Clang 14+, MSVC 2022, or Apple Clang 14+
- **CMake 3.20+**

### Build from Source

```bash
# Clone the repository
git clone https://github.com/araray/confy-cpp.git
cd confy-cpp

# Create build directory
mkdir build && cd build

# Configure (dependencies are fetched automatically)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
cmake --build . -j$(nproc)

# Run tests
ctest --output-on-failure

# (Optional) Install
sudo cmake --install .
```

### Dependencies

All dependencies are automatically fetched via CMake's FetchContent:

| Library | Version | Purpose |
|---------|---------|---------|
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | JSON parsing and value model |
| [toml++](https://github.com/marzer/tomlplusplus) | 3.4.0 | TOML parsing |
| [cxxopts](https://github.com/jarro2783/cxxopts) | 3.2.0 | CLI argument parsing |
| [GoogleTest](https://github.com/google/googletest) | 1.14.0 | Testing framework |

---

## 🚀 Quick Start

### Library Usage

```cpp
#include <confy/Config.hpp>
#include <iostream>

int main() {
    // Define loading options
    confy::LoadOptions opts;
    
    // Set defaults (lowest precedence)
    opts.defaults = {
        {"database", {
            {"host", "localhost"},
            {"port", 5432}
        }},
        {"logging", {
            {"level", "INFO"}
        }}
    };
    
    // Load from TOML file
    opts.file_path = "config.toml";
    
    // Enable environment variable overrides with prefix
    opts.prefix = "MYAPP";  // Matches MYAPP_DATABASE_HOST, etc.
    
    // Load .env file (default: true)
    opts.load_dotenv_file = true;
    
    // Require certain keys to exist
    opts.mandatory = {"database.host"};
    
    // Load configuration
    try {
        confy::Config cfg = confy::Config::load(opts);
        
        // Access values with dot-notation
        std::string host = cfg.get<std::string>("database.host", "localhost");
        int port = cfg.get<int>("database.port", 5432);
        
        std::cout << "Connecting to " << host << ":" << port << std::endl;
        
        // Check if key exists
        if (cfg.contains("logging.file")) {
            std::string logfile = cfg.get<std::string>("logging.file");
            std::cout << "Logging to: " << logfile << std::endl;
        }
        
        // Serialize to JSON or TOML
        std::cout << cfg.to_json(2) << std::endl;
        
    } catch (const confy::MissingMandatoryConfig& e) {
        std::cerr << "Missing required config: " << e.what() << std::endl;
        return 1;
    } catch (const confy::FileNotFoundError& e) {
        std::cerr << "Config file not found: " << e.path() << std::endl;
        return 1;
    }
    
    return 0;
}
```

### CLI Usage

```bash
# Get a value
confy-cpp -c config.toml get database.host

# Set a value (modifies file in-place)
confy-cpp -c config.toml set database.port 5433

# Check if key exists (exit code 0 = exists, 1 = missing)
confy-cpp -c config.toml exists database.ssl.enabled

# Search for keys/values
confy-cpp -c config.toml search --key "database.*"
confy-cpp -c config.toml search --val "localhost" -i

# Dump entire config as JSON
confy-cpp -c config.toml dump

# Convert between formats
confy-cpp -c config.toml convert --to json --out config.json

# Use environment variable prefix
confy-cpp -c config.toml -p MYAPP dump

# Specify overrides
confy-cpp -c config.toml --overrides "database.port:5433,debug:true" dump
```

---

## 📋 Configuration Sources & Precedence

Configuration is loaded from multiple sources in this order (later sources override earlier):

```
┌─────────────────────────────────────────────────────────────┐
│  1. DEFAULTS (lowest precedence)                            │
│     Hardcoded fallback values in your application           │
├─────────────────────────────────────────────────────────────┤
│  2. CONFIG FILE                                             │
│     JSON or TOML file (auto-detected by extension)          │
├─────────────────────────────────────────────────────────────┤
│  3. .ENV FILE                                               │
│     Loaded into environment (does NOT override existing)    │
├─────────────────────────────────────────────────────────────┤
│  4. ENVIRONMENT VARIABLES                                   │
│     Filtered by prefix, transformed with underscore rules   │
├─────────────────────────────────────────────────────────────┤
│  5. OVERRIDES (highest precedence)                          │
│     Explicit overrides passed to Config::load()             │
└─────────────────────────────────────────────────────────────┘
```

### Environment Variable Mapping

Environment variables are transformed to dot-paths using these rules:

| Environment Variable | Transformed Path | Rule |
|---------------------|------------------|------|
| `MYAPP_DATABASE_HOST` | `database.host` | Single `_` → `.` |
| `MYAPP_FEATURE_FLAGS__BETA` | `feature_flags.beta` | Double `__` → single `_` |
| `MYAPP_A__B__C_D` | `a_b_c.d` | Combined rules |

**Prefix Filtering:**
- `prefix = "MYAPP"` → Only `MYAPP_*` variables are considered
- `prefix = ""` → Most variables included (120+ system prefixes excluded)
- `prefix = std::nullopt` → Environment loading disabled entirely

---

## 📖 API Reference

### LoadOptions

```cpp
struct LoadOptions {
    std::string file_path;                              // Config file path (empty = none)
    std::optional<std::string> prefix = std::nullopt;   // Env var prefix
    bool load_dotenv_file = true;                       // Load .env file
    std::string dotenv_path;                            // Explicit .env path
    Value defaults = Value::object();                   // Default values
    std::unordered_map<std::string, Value> overrides;   // Final overrides
    std::vector<std::string> mandatory;                 // Required keys
};
```

### Config Class

```cpp
class Config {
public:
    // Load from multiple sources
    static Config load(const LoadOptions& opts);
    
    // Value access (dot-notation)
    template<typename T>
    T get(const std::string& path, const T& default_val) const;
    
    Value get(const std::string& path) const;                    // Throws if missing
    std::optional<Value> get_optional(const std::string& path) const;
    
    void set(const std::string& path, const Value& value, 
             bool create_missing = true);
    
    bool contains(const std::string& path) const;
    
    // Serialization
    std::string to_json(int indent = 2) const;
    std::string to_toml() const;
    
    // Raw data access
    const Value& data() const;
    Value& data();
    
    // Merging
    void merge(const Config& other);
    void merge(const Value& other);
};
```

### Exception Types

| Exception | When Thrown |
|-----------|-------------|
| `ConfigError` | Base class for all confy exceptions |
| `MissingMandatoryConfig` | Required keys missing after merge |
| `FileNotFoundError` | Config file doesn't exist |
| `ConfigParseError` | JSON/TOML syntax error |
| `KeyError` | Dot-path segment not found (strict get) |
| `TypeError` | Traversal into non-container type |

---

## 📁 Project Structure

```
confy-cpp/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── ROADMAP.md                  # Development plan
│
├── include/confy/              # Public API headers
│   ├── Config.hpp              # Main configuration class
│   ├── DotPath.hpp             # Dot-path utilities
│   ├── EnvMapper.hpp           # Environment variable mapping
│   ├── Errors.hpp              # Exception types
│   ├── Loader.hpp              # File loading (JSON/TOML/.env)
│   ├── Merge.hpp               # Deep merge utilities
│   ├── Parse.hpp               # Type parsing
│   └── Value.hpp               # Value type (nlohmann::json wrapper)
│
├── src/                        # Implementation
│   ├── Config.cpp
│   ├── DotPath.cpp
│   ├── EnvMapper.cpp
│   ├── Loader.cpp
│   ├── Merge.cpp
│   ├── Parse.cpp
│   ├── Util.cpp
│   └── cli_main.cpp            # CLI tool entry point
│
└── tests/                      # Test suite (GoogleTest)
    ├── test_main.cpp
    ├── test_dotpath.cpp        # 200+ tests
    ├── test_parse.cpp          # 150+ tests
    ├── test_merge.cpp          # 100+ tests
    ├── test_env_mapper.cpp
    ├── test_loader.cpp
    ├── test_config.cpp
    └── test_cli.cpp
```

---

## 🧪 Testing

The test suite contains **1000+ test cases** covering all behavioral rules from the design specification.

```bash
# Run all tests
cd build
ctest --output-on-failure

# Run with verbose output
ctest -V

# Run specific test
./confy_tests --gtest_filter="ConfigPrecedence.*"

# List all tests
./confy_tests --gtest_list_tests
```

### Parity Testing

To verify C++ implementation matches Python behavior:

```bash
# Generate golden test files from Python
python -m confy.golden_generator tests/golden/

# Run C++ against golden files
./confy_tests --gtest_filter="GoldenParity.*"
```

---

## 🔧 Configuration Examples

### JSON Configuration

```json
{
  "database": {
    "host": "localhost",
    "port": 5432,
    "ssl": {
      "enabled": true,
      "cert_path": "/etc/ssl/certs/db.pem"
    }
  },
  "logging": {
    "level": "INFO",
    "handlers": ["console", "file"]
  },
  "feature_flags": {
    "beta_features": false
  }
}
```

### TOML Configuration

```toml
[database]
host = "localhost"
port = 5432

[database.ssl]
enabled = true
cert_path = "/etc/ssl/certs/db.pem"

[logging]
level = "INFO"
handlers = ["console", "file"]

[feature_flags]
beta_features = false
```

### .env File

```bash
# Database overrides
MYAPP_DATABASE_HOST=db.production.example.com
MYAPP_DATABASE_PORT=5433

# Feature flags (double underscore preserves underscore in key)
MYAPP_FEATURE_FLAGS__BETA_FEATURES=true

# Logging
MYAPP_LOGGING_LEVEL=DEBUG
```

---

## 🏗️ Development Status

| Phase | Status | Description |
|-------|--------|-------------|
| **Phase 1** | ✅ Complete | Core infrastructure (Errors, Value, DotPath, Parse, Merge) |
| **Phase 2** | ✅ Complete | Source loaders (EnvMapper, Loader) |
| **Phase 3** | ✅ Complete | Config class with full precedence |
| **Phase 4** | ✅ Complete | CLI tool (get, set, exists, search, dump, convert) |
| **Phase 5** | 🟡 Partial | Polish & release (docs, CI/CD, packaging) |

### Behavioral Rules Implemented

All 27 rules from `CONFY_DESIGN_SPECIFICATION.md`:

- **D1-D6**: Dot-path access semantics
- **E1-E7**: Environment variable mapping
- **F1-F8**: File format behavior
- **M1-M3**: Mandatory key validation
- **P1-P4**: Precedence ordering
- **T1-T7**: Type parsing rules

---

## 🤝 Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Follow existing code style and patterns
4. Add tests for new functionality
5. Ensure all tests pass (`ctest --output-on-failure`)
6. Commit with clear messages (`git commit -m 'Add amazing feature'`)
7. Push to your branch (`git push origin feature/amazing-feature`)
8. Open a Pull Request

### Code Style

- C++17 standard
- 4-space indentation
- `snake_case` for functions and variables
- `PascalCase` for classes and types
- Comprehensive documentation (Doxygen-style comments)

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🔗 References

- **Python Implementation**: [github.com/araray/confy](https://github.com/araray/confy)
- **Design Specification**: [CONFY_DESIGN_SPECIFICATION.md](CONFY_DESIGN_SPECIFICATION.md)
- **Development Roadmap**: [ROADMAP.md](ROADMAP.md)

### Dependencies Documentation

- [nlohmann/json](https://json.nlohmann.me/) — JSON for Modern C++
- [toml++](https://marzer.github.io/tomlplusplus/) — TOML parser for C++17
- [cxxopts](https://github.com/jarro2783/cxxopts) — Lightweight C++ option parser
- [GoogleTest](https://google.github.io/googletest/) — Google Testing Framework

---

<p align="center">
  <i>Configuration should be simple to define, predictable in behavior, and consistent across language boundaries.</i>
</p>
