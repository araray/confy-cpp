# confy-cpp Usage Guide

A comprehensive guide to using confy-cpp for unified configuration management in C++ applications.

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Getting Started](#2-getting-started)
3. [Configuration Sources](#3-configuration-sources)
4. [The Config Class API](#4-the-config-class-api)
5. [Dot-Path Notation](#5-dot-path-notation)
6. [Type Parsing Rules](#6-type-parsing-rules)
7. [Environment Variable Mapping](#7-environment-variable-mapping)
8. [Error Handling](#8-error-handling)
9. [Real-World Scenarios](#9-real-world-scenarios)
10. [CLI Tool Reference](#10-cli-tool-reference)
11. [Best Practices](#11-best-practices)
12. [Troubleshooting](#12-troubleshooting)
13. [Migration from Python confy](#13-migration-from-python-confy)

---

## 1. Introduction

### What is confy-cpp?

confy-cpp is a configuration management library that unifies multiple configuration sources into a single, easy-to-use interface. It allows you to:

- Define sensible defaults in code
- Override with configuration files (JSON or TOML)
- Further override with `.env` files
- Apply environment variable overrides
- Make final runtime overrides programmatically

All of this happens transparently, with a well-defined precedence order that ensures predictable behavior.

### Core Philosophy

> **Configuration should be simple to define, predictable in behavior, and consistent across language boundaries.**

confy-cpp is designed to be a drop-in replacement for the Python confy library, with identical behavior. This means you can share configuration files and environment variable conventions between Python and C++ components of your system.

### When to Use confy-cpp

✅ **Good fit:**
- Applications with complex configuration needs
- Microservices that need environment-based configuration
- CLI tools that support config files
- Systems with shared Python/C++ components
- 12-factor apps that configure via environment

❌ **Consider alternatives for:**
- Simple applications with few settings
- Real-time systems where config parsing latency matters
- Embedded systems with severe memory constraints

---

## 2. Getting Started

### Minimal Example

The simplest possible usage:

```cpp
#include <confy/Config.hpp>
#include <iostream>

int main() {
    confy::LoadOptions opts;
    opts.defaults = {{"message", "Hello, World!"}};
    
    confy::Config cfg = confy::Config::load(opts);
    
    std::cout << cfg.get<std::string>("message") << std::endl;
    return 0;
}
```

### Loading from a File

```cpp
#include <confy/Config.hpp>
#include <iostream>

int main() {
    confy::LoadOptions opts;
    
    // Defaults (fallback values)
    opts.defaults = {
        {"server", {
            {"host", "127.0.0.1"},
            {"port", 8080}
        }}
    };
    
    // Load from TOML file (overrides defaults)
    opts.file_path = "config.toml";
    
    confy::Config cfg = confy::Config::load(opts);
    
    std::string host = cfg.get<std::string>("server.host");
    int port = cfg.get<int>("server.port");
    
    std::cout << "Server: " << host << ":" << port << std::endl;
    return 0;
}
```

**config.toml:**
```toml
[server]
host = "0.0.0.0"
# port not specified, will use default 8080
```

**Output:**
```
Server: 0.0.0.0:8080
```

### Adding Environment Variables

```cpp
#include <confy/Config.hpp>
#include <iostream>

int main() {
    confy::LoadOptions opts;
    
    opts.defaults = {
        {"database", {
            {"host", "localhost"},
            {"port", 5432},
            {"name", "myapp"}
        }}
    };
    
    opts.file_path = "config.toml";
    
    // Enable environment variable overrides
    // Variables like MYAPP_DATABASE_HOST will override database.host
    opts.prefix = "MYAPP";
    
    confy::Config cfg = confy::Config::load(opts);
    
    std::cout << "Database: " 
              << cfg.get<std::string>("database.host") << ":"
              << cfg.get<int>("database.port") << "/"
              << cfg.get<std::string>("database.name") << std::endl;
    
    return 0;
}
```

**Running with environment override:**
```bash
MYAPP_DATABASE_HOST=db.prod.example.com ./myapp
# Output: Database: db.prod.example.com:5432/myapp
```

---

## 3. Configuration Sources

confy-cpp loads configuration from five sources, merged in this order:

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│   DEFAULTS ──► FILE ──► .ENV ──► ENVIRONMENT ──► OVERRIDES     │
│                                                                 │
│   (lowest)                                        (highest)     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

Each source can override values from previous sources. Let's explore each one.

### 3.1 Defaults

Defaults are hardcoded values that serve as fallbacks when no other source provides a value.

```cpp
confy::LoadOptions opts;

// Simple defaults
opts.defaults = {
    {"debug", false},
    {"log_level", "INFO"},
    {"max_connections", 100}
};

// Nested defaults
opts.defaults = {
    {"database", {
        {"host", "localhost"},
        {"port", 5432},
        {"pool", {
            {"min_size", 5},
            {"max_size", 20},
            {"timeout_ms", 30000}
        }}
    }},
    {"cache", {
        {"enabled", true},
        {"ttl_seconds", 3600}
    }}
};

// All JSON types are supported
opts.defaults = {
    {"string_val", "hello"},
    {"int_val", 42},
    {"float_val", 3.14159},
    {"bool_val", true},
    {"null_val", nullptr},
    {"array_val", {1, 2, 3, 4, 5}},
    {"object_val", {{"nested", "value"}}}
};
```

**Best Practice:** Always provide sensible defaults for all configuration keys your application uses. This makes your application more robust and self-documenting.

### 3.2 Configuration Files

confy-cpp supports JSON and TOML configuration files. The format is auto-detected by file extension.

#### JSON Files (.json)

```json
{
    "server": {
        "host": "0.0.0.0",
        "port": 8080,
        "workers": 4
    },
    "database": {
        "url": "postgres://localhost/myapp",
        "pool_size": 10
    },
    "features": {
        "enable_cache": true,
        "enable_metrics": true
    }
}
```

#### TOML Files (.toml)

```toml
[server]
host = "0.0.0.0"
port = 8080
workers = 4

[database]
url = "postgres://localhost/myapp"
pool_size = 10

[features]
enable_cache = true
enable_metrics = true
```

#### Loading Config Files

```cpp
confy::LoadOptions opts;
opts.defaults = {/* ... */};

// Load JSON
opts.file_path = "config.json";

// Or load TOML
opts.file_path = "config.toml";

// File format is auto-detected by extension
// Supported: .json, .toml (case-insensitive)

confy::Config cfg = confy::Config::load(opts);
```

#### Handling Missing Files

```cpp
// If file doesn't exist, FileNotFoundError is thrown
opts.file_path = "nonexistent.toml";

try {
    confy::Config cfg = confy::Config::load(opts);
} catch (const confy::FileNotFoundError& e) {
    std::cerr << "Config file not found: " << e.path() << std::endl;
}

// To make the file optional, check existence first
#include <filesystem>

opts.file_path = "";  // Start with no file
if (std::filesystem::exists("config.toml")) {
    opts.file_path = "config.toml";
}
```

### 3.3 .env Files

The `.env` file is a simple key-value format commonly used for local development and deployment configuration.

**.env file format:**
```bash
# Comments start with #
DATABASE_HOST=localhost
DATABASE_PORT=5432

# Quoted values (single or double quotes)
DATABASE_PASSWORD="my secret password"
API_KEY='sk-12345'

# Export prefix is optional and ignored
export REDIS_URL=redis://localhost:6379

# Empty values are allowed
OPTIONAL_FEATURE=

# Escape sequences in double quotes
MULTILINE="line1\nline2"
```

#### How .env Loading Works

1. confy-cpp searches for `.env` in the current directory (and parent directories)
2. Variables are loaded into the process environment
3. **Important:** `.env` does NOT override existing environment variables

```cpp
confy::LoadOptions opts;
opts.defaults = {/* ... */};

// Enable .env loading (default is true)
opts.load_dotenv_file = true;

// Or specify explicit path
opts.dotenv_path = "/path/to/.env";

// To disable .env loading
opts.load_dotenv_file = false;
```

#### .env Precedence Example

```cpp
// .env file contains: DATABASE_HOST=from_dotenv

// Before running:
// $ export DATABASE_HOST=from_shell

confy::LoadOptions opts;
opts.defaults = {{"database", {{"host", "from_defaults"}}}};
opts.prefix = "DATABASE";
opts.load_dotenv_file = true;

confy::Config cfg = confy::Config::load(opts);

// Result: cfg.get("database.host") == "from_shell"
// The shell environment variable takes precedence over .env
```

### 3.4 Environment Variables

Environment variables provide runtime configuration, ideal for containerized deployments and 12-factor apps.

#### Prefix Filtering

```cpp
confy::LoadOptions opts;
opts.defaults = {
    {"database", {{"host", "localhost"}, {"port", 5432}}},
    {"cache", {{"enabled", true}}}
};

// Option 1: Specific prefix (recommended)
opts.prefix = "MYAPP";
// Only MYAPP_* variables are considered:
// MYAPP_DATABASE_HOST → database.host
// MYAPP_CACHE_ENABLED → cache.enabled
// OTHER_VAR → ignored

// Option 2: Empty prefix (include most vars)
opts.prefix = "";
// Most variables included, but 120+ system prefixes excluded:
// PATH, HOME, USER, SHELL, etc. → ignored
// DATABASE_HOST → database.host

// Option 3: Disable environment loading
opts.prefix = std::nullopt;
// No environment variables are loaded
```

#### Underscore Transformation Rules

Environment variable names are transformed to dot-paths:

| Rule | Pattern | Example |
|------|---------|---------|
| Lowercase | `ABC` → `abc` | `HOST` → `host` |
| Single `_` → `.` | `A_B` → `a.b` | `DATABASE_HOST` → `database.host` |
| Double `__` → `_` | `A__B` → `a_b` | `FEATURE__FLAGS` → `feature_flags` |
| Combined | `A__B_C` → `a_b.c` | `FEATURE__FLAGS_ENABLED` → `feature_flags.enabled` |

**Examples:**
```bash
# Environment variables:
export MYAPP_DATABASE_HOST=db.example.com
export MYAPP_DATABASE_PORT=5432
export MYAPP_FEATURE_FLAGS__BETA=true
export MYAPP_FEATURE_FLAGS__EXPERIMENTAL_UI=false

# Result in config:
# database.host = "db.example.com"
# database.port = 5432
# feature_flags.beta = true
# feature_flags.experimental_ui = false
```

### 3.5 Overrides

Programmatic overrides have the highest precedence, useful for CLI arguments or runtime modifications.

```cpp
confy::LoadOptions opts;
opts.defaults = {
    {"server", {{"port", 8080}}},
    {"debug", false}
};
opts.file_path = "config.toml";
opts.prefix = "MYAPP";

// Overrides always win
opts.overrides = {
    {"server.port", 9000},      // Override nested value
    {"debug", true},             // Override simple value
    {"new.key", "added"}         // Add new keys
};

confy::Config cfg = confy::Config::load(opts);
// server.port will be 9000, regardless of file or env
```

#### Using Overrides with CLI Arguments

```cpp
#include <confy/Config.hpp>
#include <cxxopts.hpp>

int main(int argc, char* argv[]) {
    cxxopts::Options options("myapp", "My Application");
    options.add_options()
        ("p,port", "Server port", cxxopts::value<int>())
        ("d,debug", "Enable debug mode")
        ("c,config", "Config file", cxxopts::value<std::string>());
    
    auto result = options.parse(argc, argv);
    
    confy::LoadOptions opts;
    opts.defaults = {
        {"server", {{"port", 8080}}},
        {"debug", false}
    };
    
    if (result.count("config")) {
        opts.file_path = result["config"].as<std::string>();
    }
    
    opts.prefix = "MYAPP";
    
    // CLI arguments become overrides
    if (result.count("port")) {
        opts.overrides["server.port"] = result["port"].as<int>();
    }
    if (result.count("debug")) {
        opts.overrides["debug"] = true;
    }
    
    confy::Config cfg = confy::Config::load(opts);
    
    // Now config respects: defaults < file < env < CLI args
    return 0;
}
```

---

## 4. The Config Class API

### 4.1 Loading Configuration

#### Basic Load

```cpp
confy::LoadOptions opts;
opts.defaults = {{"key", "value"}};

confy::Config cfg = confy::Config::load(opts);
```

#### Full Options Example

```cpp
confy::LoadOptions opts;

// 1. Defaults (lowest precedence)
opts.defaults = {
    {"app", {
        {"name", "MyApp"},
        {"version", "1.0.0"}
    }},
    {"server", {
        {"host", "localhost"},
        {"port", 8080}
    }},
    {"database", {
        {"host", "localhost"},
        {"port", 5432},
        {"name", "myapp_dev"}
    }},
    {"logging", {
        {"level", "INFO"},
        {"format", "json"}
    }}
};

// 2. Config file (overrides defaults)
opts.file_path = "config.toml";

// 3. .env file settings
opts.load_dotenv_file = true;
opts.dotenv_path = ".env.local";  // Optional: specific path

// 4. Environment variable prefix
opts.prefix = "MYAPP";

// 5. Mandatory keys (throws if missing)
opts.mandatory = {
    "database.host",
    "database.name"
};

// 6. Overrides (highest precedence)
opts.overrides = {
    {"app.version", "1.0.1-beta"}
};

// Load!
try {
    confy::Config cfg = confy::Config::load(opts);
    // Use cfg...
} catch (const confy::MissingMandatoryConfig& e) {
    std::cerr << "Missing required config: " << e.what() << std::endl;
    for (const auto& key : e.missing_keys()) {
        std::cerr << "  - " << key << std::endl;
    }
    return 1;
}
```

### 4.2 Reading Values

#### get\<T\>(path, default) — Safe Access with Default

Returns the value at path, or default if not found:

```cpp
confy::Config cfg = /* ... */;

// String with default
std::string host = cfg.get<std::string>("server.host", "localhost");

// Integer with default
int port = cfg.get<int>("server.port", 8080);

// Boolean with default
bool debug = cfg.get<bool>("debug", false);

// Double with default
double rate = cfg.get<double>("rate_limit.requests_per_second", 100.0);

// If path doesn't exist, default is returned
std::string missing = cfg.get<std::string>("nonexistent.key", "fallback");
// missing == "fallback"
```

#### get(path) — Strict Access

Returns the value at path, throws `KeyError` if not found:

```cpp
confy::Config cfg = /* ... */;

try {
    confy::Value host = cfg.get("server.host");
    std::cout << "Host: " << host.get<std::string>() << std::endl;
    
    confy::Value port = cfg.get("server.port");
    std::cout << "Port: " << port.get<int>() << std::endl;
    
} catch (const confy::KeyError& e) {
    std::cerr << "Missing key: " << e.path() << std::endl;
}
```

#### get_optional(path) — Optional Access

Returns `std::optional<Value>`:

```cpp
confy::Config cfg = /* ... */;

auto maybe_ssl = cfg.get_optional("server.ssl.enabled");
if (maybe_ssl.has_value()) {
    bool ssl_enabled = maybe_ssl->get<bool>();
    std::cout << "SSL: " << (ssl_enabled ? "enabled" : "disabled") << std::endl;
} else {
    std::cout << "SSL not configured" << std::endl;
}

// With value_or pattern
bool debug = cfg.get_optional("debug")
    .value_or(confy::Value(false))
    .get<bool>();
```

#### Accessing Nested Values

```cpp
confy::Config cfg = /* ... */;

// Dot notation works at any depth
std::string cert = cfg.get<std::string>("server.ssl.certificates.primary.path", "");

// Access arrays by index
confy::Value handlers = cfg.get("logging.handlers");
if (handlers.is_array()) {
    for (const auto& handler : handlers) {
        std::cout << "Handler: " << handler.get<std::string>() << std::endl;
    }
}

// Access array elements by index (as string)
std::string first_handler = cfg.get<std::string>("logging.handlers.0", "console");
```

#### Working with the Value Type

The `Value` type is an alias for `nlohmann::json`:

```cpp
confy::Config cfg = /* ... */;

confy::Value db = cfg.get("database");

// Type checking
if (db.is_object()) {
    // Iterate over keys
    for (auto& [key, value] : db.items()) {
        std::cout << key << " = " << value << std::endl;
    }
}

// Array operations
confy::Value ports = cfg.get("allowed_ports");
if (ports.is_array()) {
    std::cout << "Allowed ports: ";
    for (size_t i = 0; i < ports.size(); ++i) {
        std::cout << ports[i].get<int>() << " ";
    }
    std::cout << std::endl;
}

// Type conversion
std::string host = db["host"].get<std::string>();
int port = db["port"].get<int>();
bool ssl = db.value("ssl_enabled", false);  // nlohmann::json method

// Check for null
if (cfg.get("optional_field").is_null()) {
    std::cout << "Field is null" << std::endl;
}
```

### 4.3 Writing Values

#### set(path, value, create_missing)

```cpp
confy::Config cfg = /* ... */;

// Set existing value
cfg.set("server.port", 9000);

// Create nested path (create_missing defaults to true)
cfg.set("new.nested.value", "created");

// Set array
cfg.set("allowed_ips", confy::Value({"192.168.1.1", "10.0.0.1"}));

// Set object
cfg.set("new_section", confy::Value({
    {"key1", "value1"},
    {"key2", 42}
}));

// With create_missing = false (throws if path doesn't exist)
try {
    cfg.set("nonexistent.path", "value", false);
} catch (const confy::KeyError& e) {
    std::cerr << "Cannot set: path doesn't exist" << std::endl;
}
```

### 4.4 Checking Existence

#### contains(path)

```cpp
confy::Config cfg = /* ... */;

// Simple check
if (cfg.contains("database.host")) {
    std::cout << "Database host is configured" << std::endl;
}

// Check before access
if (cfg.contains("optional_feature.enabled")) {
    bool enabled = cfg.get<bool>("optional_feature.enabled");
    // Use enabled...
}

// Note: contains returns false for missing keys, but throws
// TypeError if you try to traverse into a non-object
try {
    // If "port" is an integer, this throws TypeError
    bool exists = cfg.contains("server.port.invalid");
} catch (const confy::TypeError& e) {
    std::cerr << "Cannot traverse: " << e.what() << std::endl;
}
```

### 4.5 Serialization

#### to_json(indent)

```cpp
confy::Config cfg = /* ... */;

// Pretty-printed JSON (default indent = 2)
std::string json_pretty = cfg.to_json();
std::cout << json_pretty << std::endl;

// Custom indentation
std::string json_4space = cfg.to_json(4);

// Compact JSON (no whitespace)
std::string json_compact = cfg.to_json(-1);

// Save to file
std::ofstream out("config_dump.json");
out << cfg.to_json(2);
```

#### to_toml()

```cpp
confy::Config cfg = /* ... */;

// Convert to TOML string
std::string toml_str = cfg.to_toml();
std::cout << toml_str << std::endl;

// Save to file
std::ofstream out("config_dump.toml");
out << cfg.to_toml();
```

### 4.6 Merging Configurations

```cpp
// Merge another Config
confy::Config base = /* ... */;
confy::Config overlay = /* ... */;

base.merge(overlay);  // overlay values override base

// Merge a Value object
confy::Value extra = {
    {"new_section", {
        {"key", "value"}
    }}
};
base.merge(extra);

// Merge rules:
// - Objects are merged recursively
// - Non-objects (scalars, arrays) are replaced entirely
```

### 4.7 Raw Data Access

```cpp
confy::Config cfg = /* ... */;

// Get underlying Value (const)
const confy::Value& data = cfg.data();

// Get underlying Value (mutable) — use with caution!
confy::Value& mutable_data = cfg.data();
mutable_data["injected"] = "value";

// Convert to plain dictionary
confy::Value dict = cfg.to_dict();  // Returns a copy
```

---

## 5. Dot-Path Notation

Dot-path notation allows access to nested configuration values using a simple string syntax.

### Basic Syntax

```
"key"                    → root level key
"parent.child"           → nested key
"a.b.c.d"               → deeply nested key
"array.0"               → first element of array
"array.0.property"      → property of first array element
```

### Examples

```cpp
confy::Config cfg({
    {"server", {
        {"host", "localhost"},
        {"port", 8080},
        {"ssl", {
            {"enabled", true},
            {"cert", "/path/to/cert.pem"}
        }}
    }},
    {"handlers", {
        {{"type", "console"}, {"level", "DEBUG"}},
        {{"type", "file"}, {"level", "INFO"}, {"path", "/var/log/app.log"}}
    }}
});

// Simple access
cfg.get<std::string>("server.host");        // "localhost"
cfg.get<int>("server.port");                // 8080

// Deep access
cfg.get<bool>("server.ssl.enabled");        // true
cfg.get<std::string>("server.ssl.cert");    // "/path/to/cert.pem"

// Array access
cfg.get<std::string>("handlers.0.type");    // "console"
cfg.get<std::string>("handlers.1.path");    // "/var/log/app.log"
```

### Path Behavior Rules

| Operation | Missing Key | Path Through Non-Object |
|-----------|-------------|------------------------|
| `get(path)` | Throws `KeyError` | Throws `TypeError` |
| `get(path, default)` | Returns default | Throws `TypeError` |
| `contains(path)` | Returns `false` | Throws `TypeError` |
| `set(path, val, true)` | Creates path | Overwrites with object |
| `set(path, val, false)` | Throws `KeyError` | Throws `TypeError` |

**Example of TypeError:**
```cpp
confy::Config cfg({{"port", 8080}});  // port is an integer

// Trying to access "port.invalid" throws TypeError
// because we can't traverse "into" an integer
try {
    cfg.get("port.invalid");
} catch (const confy::TypeError& e) {
    // "Cannot traverse into integer (expected object) at path 'port.invalid'"
}
```

---

## 6. Type Parsing Rules

When values come from environment variables or string overrides, confy-cpp automatically parses them into appropriate types.

### Parsing Order (First Match Wins)

| Rule | Pattern | Input | Result |
|------|---------|-------|--------|
| T1 | Boolean | `"true"`, `"false"` (case-insensitive) | `true`, `false` |
| T2 | Null | `"null"` (case-insensitive) | `null` |
| T3 | Integer | Matches `^-?[0-9]+$` | `int64_t` |
| T4 | Float | Matches `^-?[0-9]+\.[0-9]+([eE][+-]?[0-9]+)?$` | `double` |
| T5 | JSON | Starts with `{` or `[` | Parsed JSON |
| T6 | Quoted | Starts and ends with `"` | Unquoted string |
| T7 | String | Everything else | Raw string |

### Examples

```bash
# Environment variables and their parsed types:

export MYAPP_DEBUG=true              # → true (boolean)
export MYAPP_VERBOSE=FALSE           # → false (boolean)
export MYAPP_EMPTY=null              # → null
export MYAPP_PORT=8080               # → 8080 (integer)
export MYAPP_NEGATIVE=-42            # → -42 (integer)
export MYAPP_RATE=3.14               # → 3.14 (double)
export MYAPP_SCIENTIFIC=1.5e-3       # → 0.0015 (double)
export MYAPP_TAGS='["a","b","c"]'    # → ["a","b","c"] (array)
export MYAPP_EXTRA='{"key":"val"}'   # → {"key":"val"} (object)
export MYAPP_QUOTED='"keep quotes"'  # → "keep quotes" (string, quotes removed)
export MYAPP_NAME=myapp              # → "myapp" (string)
export MYAPP_PATH=/usr/bin           # → "/usr/bin" (string)
```

### Manual Parsing

You can also use the parse function directly:

```cpp
#include <confy/Parse.hpp>

confy::Value v1 = confy::parse_value("true");      // boolean true
confy::Value v2 = confy::parse_value("42");        // integer 42
confy::Value v3 = confy::parse_value("3.14");      // double 3.14
confy::Value v4 = confy::parse_value("[1,2,3]");   // array
confy::Value v5 = confy::parse_value("hello");     // string "hello"
```

---

## 7. Environment Variable Mapping

### The Mapping Pipeline

```
MYAPP_DATABASE_HOST=db.example.com
        │
        ▼
┌───────────────────────┐
│ 1. Prefix Filtering   │  MYAPP_DATABASE_HOST matches MYAPP_*
└───────────────────────┘
        │
        ▼
┌───────────────────────┐
│ 2. Strip Prefix       │  DATABASE_HOST
└───────────────────────┘
        │
        ▼
┌───────────────────────┐
│ 3. Lowercase          │  database_host
└───────────────────────┘
        │
        ▼
┌───────────────────────┐
│ 4. __ → marker        │  database_host (no change)
└───────────────────────┘
        │
        ▼
┌───────────────────────┐
│ 5. _ → .              │  database.host
└───────────────────────┘
        │
        ▼
┌───────────────────────┐
│ 6. marker → _         │  database.host (no change)
└───────────────────────┘
        │
        ▼
┌───────────────────────┐
│ 7. Remap to Base Keys │  database.host (verified against defaults)
└───────────────────────┘
        │
        ▼
┌───────────────────────┐
│ 8. Parse Value        │  "db.example.com" (string)
└───────────────────────┘
```

### Handling Underscores in Key Names

If your config has keys with underscores, use double underscores in env vars:

```cpp
// Config structure:
opts.defaults = {
    {"feature_flags", {
        {"beta_feature", false},
        {"experimental", false}
    }}
};

// Environment variables:
// MYAPP_FEATURE_FLAGS__BETA_FEATURE=true    → feature_flags.beta_feature
// MYAPP_FEATURE_FLAGS__EXPERIMENTAL=true    → feature_flags.experimental
```

### Prefix Options Explained

```cpp
// Option 1: Named prefix (recommended)
opts.prefix = "MYAPP";
// - Only MYAPP_* variables considered
// - MYAPP_ stripped before transformation
// - Clear namespace, no conflicts

// Option 2: Empty prefix
opts.prefix = "";
// - Most environment variables considered
// - System variables excluded (PATH, HOME, USER, etc.)
// - 120+ prefixes in exclusion list
// - Use with caution in shared environments

// Option 3: Disabled
opts.prefix = std::nullopt;
// - No environment variables loaded
// - Useful for testing or isolated configs
```

### System Variable Exclusion List

When using empty prefix (`""`), these prefixes are excluded:

```
PATH, HOME, USER, SHELL, TERM, LANG, LC_*, PWD, HOSTNAME,
SSH_*, GPG_*, DBUS_*, DISPLAY, XDG_*,
JAVA_*, PYTHON*, NODE_*, NPM_*, CONDA_*, VIRTUAL_ENV,
CMAKE_*, CC, CXX, CFLAGS, LDFLAGS,
AWS_*, GOOGLE_*, DOCKER_*, KUBERNETES_*,
GITHUB_*, GITLAB_*, TRAVIS_*, CI,
... (120+ total)
```

### Remapping Algorithm

confy-cpp includes smart remapping to match env-derived paths to your config structure:

```cpp
opts.defaults = {
    {"feature_flags", {      // Note: underscore in key name
        {"beta", false}
    }}
};

// Without remapping:
// MYAPP_FEATURE_FLAGS_BETA → feature.flags.beta (WRONG!)

// With remapping:
// MYAPP_FEATURE_FLAGS_BETA → feature_flags.beta (CORRECT!)
// confy recognizes "feature_flags" as a known key and adjusts
```

---

## 8. Error Handling

### Exception Hierarchy

```
std::runtime_error
    └── ConfigError (base)
            ├── MissingMandatoryConfig
            ├── FileNotFoundError
            ├── ConfigParseError
            ├── KeyError
            └── TypeError
```

### MissingMandatoryConfig

Thrown when required keys are missing after all sources are merged:

```cpp
opts.mandatory = {"database.host", "database.password", "api.key"};

try {
    confy::Config cfg = confy::Config::load(opts);
} catch (const confy::MissingMandatoryConfig& e) {
    std::cerr << "Configuration error: " << e.what() << std::endl;
    
    // Get list of all missing keys
    for (const auto& key : e.missing_keys()) {
        std::cerr << "  Missing: " << key << std::endl;
    }
    
    return 1;
}
```

**Output:**
```
Configuration error: Missing mandatory configuration keys: ['database.password', 'api.key']
  Missing: database.password
  Missing: api.key
```

### FileNotFoundError

Thrown when a specified config file doesn't exist:

```cpp
opts.file_path = "nonexistent.toml";

try {
    confy::Config cfg = confy::Config::load(opts);
} catch (const confy::FileNotFoundError& e) {
    std::cerr << "Config file not found: " << e.path() << std::endl;
    
    // Optionally fall back to defaults only
    opts.file_path = "";
    confy::Config cfg = confy::Config::load(opts);
}
```

### ConfigParseError

Thrown when a config file has syntax errors:

```cpp
try {
    confy::Config cfg = confy::Config::load(opts);
} catch (const confy::ConfigParseError& e) {
    std::cerr << "Parse error in " << e.file() << std::endl;
    std::cerr << "Details: " << e.details() << std::endl;
    return 1;
}
```

**Output:**
```
Parse error in config.toml
Details: line 15, column 8: expected value, got '}'
```

### KeyError

Thrown when accessing a missing key with strict `get()`:

```cpp
try {
    confy::Value secret = cfg.get("api.secret_key");
} catch (const confy::KeyError& e) {
    std::cerr << "Key not found: " << e.segment() 
              << " in path: " << e.path() << std::endl;
}
```

### TypeError

Thrown when traversing into a non-container:

```cpp
// cfg has: {"port": 8080}  (port is an integer)

try {
    cfg.get("port.invalid");  // Can't traverse into integer
} catch (const confy::TypeError& e) {
    std::cerr << "Type error at " << e.path() << std::endl;
    std::cerr << "Expected: " << e.expected() << std::endl;
    std::cerr << "Got: " << e.actual() << std::endl;
}
```

### Comprehensive Error Handling Pattern

```cpp
#include <confy/Config.hpp>
#include <iostream>

int main() {
    confy::LoadOptions opts;
    opts.defaults = {/* ... */};
    opts.file_path = "config.toml";
    opts.prefix = "MYAPP";
    opts.mandatory = {"database.host", "api.key"};
    
    try {
        confy::Config cfg = confy::Config::load(opts);
        
        // Application logic...
        run_application(cfg);
        
    } catch (const confy::MissingMandatoryConfig& e) {
        std::cerr << "ERROR: Missing required configuration:\n";
        for (const auto& key : e.missing_keys()) {
            std::cerr << "  - " << key << "\n";
        }
        std::cerr << "\nPlease set these in your config file or environment.\n";
        return 1;
        
    } catch (const confy::FileNotFoundError& e) {
        std::cerr << "ERROR: Configuration file not found: " << e.path() << "\n";
        std::cerr << "Create the file or specify a different path with --config\n";
        return 1;
        
    } catch (const confy::ConfigParseError& e) {
        std::cerr << "ERROR: Invalid configuration file: " << e.file() << "\n";
        std::cerr << "Parse error: " << e.details() << "\n";
        return 1;
        
    } catch (const confy::ConfigError& e) {
        // Catch-all for other config errors
        std::cerr << "ERROR: Configuration error: " << e.what() << "\n";
        return 1;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Unexpected error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
```

---

## 9. Real-World Scenarios

### 9.1 Web Server Configuration

```cpp
#include <confy/Config.hpp>

struct ServerConfig {
    std::string host;
    int port;
    int workers;
    bool ssl_enabled;
    std::string ssl_cert;
    std::string ssl_key;
    int request_timeout_ms;
    size_t max_body_size;
};

ServerConfig load_server_config(const std::string& config_file) {
    confy::LoadOptions opts;
    
    opts.defaults = {
        {"server", {
            {"host", "0.0.0.0"},
            {"port", 8080},
            {"workers", 4},
            {"ssl", {
                {"enabled", false},
                {"cert", ""},
                {"key", ""}
            }},
            {"limits", {
                {"request_timeout_ms", 30000},
                {"max_body_size", 10485760}  // 10MB
            }}
        }}
    };
    
    opts.file_path = config_file;
    opts.prefix = "SERVER";
    
    // SSL cert and key are required if SSL is enabled
    // We'll check this after loading
    
    confy::Config cfg = confy::Config::load(opts);
    
    ServerConfig sc;
    sc.host = cfg.get<std::string>("server.host");
    sc.port = cfg.get<int>("server.port");
    sc.workers = cfg.get<int>("server.workers");
    sc.ssl_enabled = cfg.get<bool>("server.ssl.enabled");
    sc.ssl_cert = cfg.get<std::string>("server.ssl.cert", "");
    sc.ssl_key = cfg.get<std::string>("server.ssl.key", "");
    sc.request_timeout_ms = cfg.get<int>("server.limits.request_timeout_ms");
    sc.max_body_size = cfg.get<size_t>("server.limits.max_body_size");
    
    // Validate SSL configuration
    if (sc.ssl_enabled) {
        if (sc.ssl_cert.empty() || sc.ssl_key.empty()) {
            throw std::runtime_error(
                "SSL enabled but cert/key not provided. "
                "Set server.ssl.cert and server.ssl.key"
            );
        }
    }
    
    return sc;
}
```

### 9.2 Database Connection Pool

```cpp
#include <confy/Config.hpp>
#include <memory>

class DatabasePool {
public:
    struct Config {
        std::string host;
        int port;
        std::string database;
        std::string username;
        std::string password;
        int pool_min;
        int pool_max;
        int connection_timeout_ms;
        int idle_timeout_ms;
        bool ssl_required;
    };
    
    static Config load_config() {
        confy::LoadOptions opts;
        
        opts.defaults = {
            {"database", {
                {"host", "localhost"},
                {"port", 5432},
                {"name", "app"},
                {"pool", {
                    {"min", 2},
                    {"max", 10},
                    {"connection_timeout_ms", 5000},
                    {"idle_timeout_ms", 600000}
                }},
                {"ssl", {
                    {"required", false}
                }}
            }}
        };
        
        opts.file_path = "database.toml";
        opts.prefix = "DB";
        
        // Credentials are mandatory
        opts.mandatory = {
            "database.username",
            "database.password"
        };
        
        confy::Config cfg = confy::Config::load(opts);
        
        Config c;
        c.host = cfg.get<std::string>("database.host");
        c.port = cfg.get<int>("database.port");
        c.database = cfg.get<std::string>("database.name");
        c.username = cfg.get<std::string>("database.username");
        c.password = cfg.get<std::string>("database.password");
        c.pool_min = cfg.get<int>("database.pool.min");
        c.pool_max = cfg.get<int>("database.pool.max");
        c.connection_timeout_ms = cfg.get<int>("database.pool.connection_timeout_ms");
        c.idle_timeout_ms = cfg.get<int>("database.pool.idle_timeout_ms");
        c.ssl_required = cfg.get<bool>("database.ssl.required");
        
        return c;
    }
};
```

**Usage:**
```bash
# Development (.env)
DB_DATABASE_USERNAME=dev_user
DB_DATABASE_PASSWORD=dev_password

# Production (environment)
export DB_DATABASE_HOST=db.prod.internal
export DB_DATABASE_USERNAME=prod_user
export DB_DATABASE_PASSWORD=$VAULT_DB_PASSWORD
export DB_DATABASE_SSL__REQUIRED=true
export DB_DATABASE_POOL__MAX=50
```

### 9.3 Multi-Environment Application

```cpp
#include <confy/Config.hpp>
#include <filesystem>

namespace fs = std::filesystem;

class AppConfig {
public:
    enum class Environment { Development, Staging, Production };
    
    static confy::Config load(Environment env) {
        confy::LoadOptions opts;
        
        // Base defaults for all environments
        opts.defaults = {
            {"app", {
                {"name", "MyApp"},
                {"debug", false},
                {"log_level", "INFO"}
            }},
            {"api", {
                {"rate_limit", 100},
                {"timeout_ms", 5000}
            }}
        };
        
        // Environment-specific config file
        std::string env_name;
        switch (env) {
            case Environment::Development:
                env_name = "development";
                opts.defaults["app"]["debug"] = true;
                opts.defaults["app"]["log_level"] = "DEBUG";
                break;
            case Environment::Staging:
                env_name = "staging";
                break;
            case Environment::Production:
                env_name = "production";
                opts.defaults["api"]["rate_limit"] = 1000;
                break;
        }
        
        // Load base config first, then environment-specific
        std::string base_config = "config/base.toml";
        std::string env_config = "config/" + env_name + ".toml";
        
        // Load base config
        if (fs::exists(base_config)) {
            opts.file_path = base_config;
        }
        
        confy::Config cfg = confy::Config::load(opts);
        
        // Merge environment-specific config
        if (fs::exists(env_config)) {
            confy::LoadOptions env_opts;
            env_opts.file_path = env_config;
            confy::Config env_cfg = confy::Config::load(env_opts);
            cfg.merge(env_cfg);
        }
        
        // Environment variables always override (same prefix for all envs)
        opts.file_path = "";
        opts.prefix = "MYAPP";
        opts.defaults = cfg.data();  // Use current config as base
        
        return confy::Config::load(opts);
    }
    
    static Environment detect_environment() {
        const char* env = std::getenv("MYAPP_ENV");
        if (env) {
            std::string e(env);
            if (e == "production" || e == "prod") return Environment::Production;
            if (e == "staging" || e == "stage") return Environment::Staging;
        }
        return Environment::Development;
    }
};

// Usage:
int main() {
    auto env = AppConfig::detect_environment();
    confy::Config cfg = AppConfig::load(env);
    
    std::cout << "Running in " 
              << (cfg.get<bool>("app.debug") ? "debug" : "production")
              << " mode" << std::endl;
    
    return 0;
}
```

### 9.4 Feature Flags System

```cpp
#include <confy/Config.hpp>
#include <unordered_set>

class FeatureFlags {
public:
    FeatureFlags(const confy::Config& cfg) {
        // Load all feature flags
        if (cfg.contains("features")) {
            confy::Value features = cfg.get("features");
            if (features.is_object()) {
                for (auto& [name, value] : features.items()) {
                    if (value.is_boolean() && value.get<bool>()) {
                        enabled_features_.insert(name);
                    }
                }
            }
        }
        
        // Load percentage rollouts
        if (cfg.contains("rollouts")) {
            confy::Value rollouts = cfg.get("rollouts");
            if (rollouts.is_object()) {
                for (auto& [name, value] : rollouts.items()) {
                    if (value.is_number()) {
                        rollout_percentages_[name] = value.get<int>();
                    }
                }
            }
        }
    }
    
    bool is_enabled(const std::string& feature) const {
        return enabled_features_.count(feature) > 0;
    }
    
    bool is_enabled_for_user(const std::string& feature, uint64_t user_id) const {
        auto it = rollout_percentages_.find(feature);
        if (it == rollout_percentages_.end()) {
            return is_enabled(feature);
        }
        
        // Simple percentage-based rollout
        return (user_id % 100) < static_cast<uint64_t>(it->second);
    }
    
private:
    std::unordered_set<std::string> enabled_features_;
    std::unordered_map<std::string, int> rollout_percentages_;
};

// config.toml:
// [features]
// dark_mode = true
// new_checkout = false
// beta_dashboard = true
//
// [rollouts]
// experimental_search = 10  # 10% of users
// new_recommendations = 50  # 50% of users

// Environment override:
// MYAPP_FEATURES__DARK_MODE=false
// MYAPP_ROLLOUTS__EXPERIMENTAL_SEARCH=100
```

### 9.5 Microservice Configuration

```cpp
#include <confy/Config.hpp>

struct MicroserviceConfig {
    // Service identity
    std::string service_name;
    std::string service_version;
    std::string instance_id;
    
    // Server
    std::string bind_address;
    int port;
    
    // Service discovery
    std::string consul_address;
    std::string consul_token;
    
    // Tracing
    std::string jaeger_endpoint;
    double sample_rate;
    
    // Metrics
    int metrics_port;
    std::string metrics_path;
    
    static MicroserviceConfig load() {
        confy::LoadOptions opts;
        
        opts.defaults = {
            {"service", {
                {"name", "unknown"},
                {"version", "0.0.0"},
                {"instance_id", generate_instance_id()}
            }},
            {"server", {
                {"bind", "0.0.0.0"},
                {"port", 8080}
            }},
            {"consul", {
                {"address", "localhost:8500"},
                {"token", ""}
            }},
            {"tracing", {
                {"jaeger_endpoint", ""},
                {"sample_rate", 0.1}
            }},
            {"metrics", {
                {"port", 9090},
                {"path", "/metrics"}
            }}
        };
        
        // Try to load config file
        for (const auto& path : {
            "config.toml",
            "/etc/myservice/config.toml",
            "/app/config.toml"
        }) {
            if (std::filesystem::exists(path)) {
                opts.file_path = path;
                break;
            }
        }
        
        // Service-specific prefix
        opts.prefix = "SVC";
        
        // Consul token is required in production
        if (std::getenv("SVC_PRODUCTION")) {
            opts.mandatory = {"consul.token"};
        }
        
        confy::Config cfg = confy::Config::load(opts);
        
        MicroserviceConfig mc;
        mc.service_name = cfg.get<std::string>("service.name");
        mc.service_version = cfg.get<std::string>("service.version");
        mc.instance_id = cfg.get<std::string>("service.instance_id");
        mc.bind_address = cfg.get<std::string>("server.bind");
        mc.port = cfg.get<int>("server.port");
        mc.consul_address = cfg.get<std::string>("consul.address");
        mc.consul_token = cfg.get<std::string>("consul.token", "");
        mc.jaeger_endpoint = cfg.get<std::string>("tracing.jaeger_endpoint", "");
        mc.sample_rate = cfg.get<double>("tracing.sample_rate");
        mc.metrics_port = cfg.get<int>("metrics.port");
        mc.metrics_path = cfg.get<std::string>("metrics.path");
        
        return mc;
    }
    
private:
    static std::string generate_instance_id() {
        // Generate unique instance ID
        return "instance-" + std::to_string(std::time(nullptr));
    }
};
```

### 9.6 Configuration Singleton Pattern

```cpp
#include <confy/Config.hpp>
#include <mutex>
#include <memory>

class AppSettings {
public:
    // Get the singleton instance
    static AppSettings& instance() {
        static AppSettings instance;
        return instance;
    }
    
    // Initialize (call once at startup)
    void initialize(const confy::LoadOptions& opts) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_) {
            throw std::runtime_error("AppSettings already initialized");
        }
        config_ = std::make_unique<confy::Config>(confy::Config::load(opts));
        initialized_ = true;
    }
    
    // Type-safe accessors
    template<typename T>
    T get(const std::string& path, const T& default_val) const {
        check_initialized();
        return config_->get<T>(path, default_val);
    }
    
    bool contains(const std::string& path) const {
        check_initialized();
        return config_->contains(path);
    }
    
    // Direct config access
    const confy::Config& config() const {
        check_initialized();
        return *config_;
    }
    
private:
    AppSettings() = default;
    AppSettings(const AppSettings&) = delete;
    AppSettings& operator=(const AppSettings&) = delete;
    
    void check_initialized() const {
        if (!initialized_) {
            throw std::runtime_error("AppSettings not initialized");
        }
    }
    
    std::unique_ptr<confy::Config> config_;
    mutable std::mutex mutex_;
    bool initialized_ = false;
};

// Usage:
int main() {
    // Initialize once at startup
    confy::LoadOptions opts;
    opts.defaults = {/* ... */};
    opts.file_path = "config.toml";
    opts.prefix = "MYAPP";
    
    AppSettings::instance().initialize(opts);
    
    // Access from anywhere
    int port = AppSettings::instance().get<int>("server.port", 8080);
    std::string host = AppSettings::instance().get<std::string>("server.host", "localhost");
    
    return 0;
}
```

### 9.7 Thread-Safe Configuration with Reader-Writer Lock

```cpp
#include <confy/Config.hpp>
#include <shared_mutex>

class ThreadSafeConfig {
public:
    ThreadSafeConfig(const confy::LoadOptions& opts)
        : config_(confy::Config::load(opts)) {}
    
    // Read operations (shared lock)
    template<typename T>
    T get(const std::string& path, const T& default_val) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return config_.get<T>(path, default_val);
    }
    
    bool contains(const std::string& path) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return config_.contains(path);
    }
    
    // Write operations (exclusive lock)
    void set(const std::string& path, const confy::Value& value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        config_.set(path, value);
    }
    
    void reload(const confy::LoadOptions& opts) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        config_ = confy::Config::load(opts);
    }
    
    // Snapshot for consistent multi-key reads
    confy::Config snapshot() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return config_;  // Copy
    }
    
private:
    confy::Config config_;
    mutable std::shared_mutex mutex_;
};

// Usage:
void worker_thread(ThreadSafeConfig& cfg) {
    // Safe concurrent reads
    int port = cfg.get<int>("server.port", 8080);
    std::string host = cfg.get<std::string>("server.host", "localhost");
    
    // For multiple related reads, use snapshot
    auto snapshot = cfg.snapshot();
    std::string db_host = snapshot.get<std::string>("database.host");
    int db_port = snapshot.get<int>("database.port");
    // db_host and db_port are guaranteed consistent
}
```

---

## 10. CLI Tool Reference

confy-cpp includes a command-line tool for inspecting and modifying configuration.

### Basic Usage

```bash
confy-cpp [OPTIONS] COMMAND [ARGS...]
```

### Global Options

| Option | Description |
|--------|-------------|
| `-c, --config FILE` | Path to config file (JSON or TOML) |
| `-p, --prefix PREFIX` | Environment variable prefix |
| `--defaults FILE` | Path to defaults JSON file |
| `--overrides KEY:VAL,...` | Comma-separated overrides |
| `--mandatory KEY,...` | Comma-separated required keys |
| `--dotenv-path FILE` | Explicit .env file path |
| `--no-dotenv` | Disable .env loading |
| `-h, --help` | Show help message |

### Commands

#### `get KEY` — Get Configuration Value

```bash
# Get a simple value
confy-cpp -c config.toml get server.port
# Output: 8080

# Get nested value
confy-cpp -c config.toml get database.connection.pool.max_size
# Output: 20

# Get with environment override
MYAPP_SERVER_PORT=9000 confy-cpp -c config.toml -p MYAPP get server.port
# Output: 9000

# Output is JSON-formatted
confy-cpp -c config.toml get database
# Output:
# {
#   "host": "localhost",
#   "port": 5432,
#   "name": "myapp"
# }
```

#### `set KEY VALUE` — Set Configuration Value

```bash
# Set a simple value
confy-cpp -c config.toml set server.port 9000

# Set a string value
confy-cpp -c config.toml set server.host "0.0.0.0"

# Set a boolean
confy-cpp -c config.toml set debug true

# Set a nested value (creates path if needed)
confy-cpp -c config.toml set new.nested.key "value"

# Set a JSON value
confy-cpp -c config.toml set allowed_ips '["10.0.0.1", "192.168.1.1"]'
```

#### `exists KEY` — Check Key Existence

```bash
# Returns exit code 0 if exists, 1 if not
confy-cpp -c config.toml exists server.port
echo $?  # 0

confy-cpp -c config.toml exists nonexistent.key
echo $?  # 1

# Use in scripts
if confy-cpp -c config.toml exists database.ssl.enabled; then
    echo "SSL configuration found"
fi
```

#### `search` — Search Configuration

```bash
# Search by key pattern
confy-cpp -c config.toml search --key "database.*"

# Search by value pattern
confy-cpp -c config.toml search --val "localhost"

# Case-insensitive search
confy-cpp -c config.toml search --key "*.host" -i

# Combined key and value search
confy-cpp -c config.toml search --key "*.port" --val "5432"
```

#### `dump` — Print Entire Configuration

```bash
# Pretty-printed JSON output
confy-cpp -c config.toml dump

# With environment overrides
MYAPP_DEBUG=true confy-cpp -c config.toml -p MYAPP dump

# Pipe to other tools
confy-cpp -c config.toml dump | jq '.database'
```

#### `convert` — Convert Between Formats

```bash
# Convert TOML to JSON
confy-cpp -c config.toml convert --to json

# Convert and save to file
confy-cpp -c config.toml convert --to json --out config.json

# Convert JSON to TOML
confy-cpp -c config.json convert --to toml --out config.toml
```

### CLI Examples

```bash
# Development workflow
confy-cpp -c config.toml dump                     # View current config
confy-cpp -c config.toml get database.host        # Check specific value
confy-cpp -c config.toml set debug true           # Enable debug mode

# Production deployment
export MYAPP_DATABASE_HOST=db.prod.internal
export MYAPP_DATABASE_PASSWORD=secret
confy-cpp -c config.toml -p MYAPP dump            # Verify merged config

# Configuration validation
confy-cpp -c config.toml --mandatory "api.key,database.password" dump
# Exits with error if mandatory keys missing

# Search and discovery
confy-cpp -c config.toml search --key "*timeout*"  # Find all timeout settings
confy-cpp -c config.toml search --val "true"       # Find all enabled flags

# Format conversion for deployment
confy-cpp -c config.toml convert --to json --out /etc/myapp/config.json
```

---

## 11. Best Practices

### 11.1 Always Provide Defaults

```cpp
// ❌ Bad: No defaults
confy::LoadOptions opts;
opts.file_path = "config.toml";  // What if file is missing?

// ✅ Good: Complete defaults
confy::LoadOptions opts;
opts.defaults = {
    {"server", {
        {"host", "localhost"},
        {"port", 8080}
    }},
    {"logging", {
        {"level", "INFO"}
    }}
};
opts.file_path = "config.toml";
```

### 11.2 Use Mandatory Keys for Critical Configuration

```cpp
// ❌ Bad: Silently using empty credentials
std::string password = cfg.get<std::string>("database.password", "");

// ✅ Good: Fail fast if credentials missing
opts.mandatory = {"database.password", "api.key"};
// Application won't start without these
```

### 11.3 Use Specific Prefixes

```cpp
// ❌ Bad: Empty prefix can pick up unrelated vars
opts.prefix = "";

// ✅ Good: Specific prefix avoids conflicts
opts.prefix = "MYAPP";
```

### 11.4 Validate Configuration After Loading

```cpp
confy::Config cfg = confy::Config::load(opts);

// Validate constraints
int port = cfg.get<int>("server.port");
if (port < 1 || port > 65535) {
    throw std::runtime_error("Invalid port: " + std::to_string(port));
}

int pool_min = cfg.get<int>("database.pool.min");
int pool_max = cfg.get<int>("database.pool.max");
if (pool_min > pool_max) {
    throw std::runtime_error("pool.min cannot be greater than pool.max");
}
```

### 11.5 Document Your Configuration

```cpp
// Create a configuration schema/documentation
/*
 * Configuration Reference
 * =======================
 * 
 * server.host (string, default: "localhost")
 *   The address to bind the server to.
 *   Environment: MYAPP_SERVER_HOST
 * 
 * server.port (int, default: 8080)
 *   The port to listen on. Must be 1-65535.
 *   Environment: MYAPP_SERVER_PORT
 * 
 * database.pool.max (int, default: 10)
 *   Maximum database connections.
 *   Environment: MYAPP_DATABASE_POOL__MAX
 */
```

### 11.6 Use Configuration Structs

```cpp
// ❌ Bad: Scattered get() calls throughout code
void start_server(const confy::Config& cfg) {
    auto host = cfg.get<std::string>("server.host");
    // ... 100 lines later ...
    auto port = cfg.get<int>("server.port");
}

// ✅ Good: Centralized configuration struct
struct ServerConfig {
    std::string host;
    int port;
    int workers;
    
    static ServerConfig from_config(const confy::Config& cfg) {
        return {
            .host = cfg.get<std::string>("server.host", "localhost"),
            .port = cfg.get<int>("server.port", 8080),
            .workers = cfg.get<int>("server.workers", 4)
        };
    }
};

void start_server(const ServerConfig& cfg) {
    // Clean, type-safe access
}
```

### 11.7 Handle Configuration Errors Gracefully

```cpp
int main() {
    try {
        confy::Config cfg = confy::Config::load(opts);
        return run_application(cfg);
        
    } catch (const confy::MissingMandatoryConfig& e) {
        std::cerr << "Missing required configuration:\n";
        for (const auto& key : e.missing_keys()) {
            std::cerr << "  - " << key << "\n";
        }
        std::cerr << "\nSee documentation for required settings.\n";
        return 1;
        
    } catch (const confy::ConfigError& e) {
        std::cerr << "Configuration error: " << e.what() << "\n";
        return 1;
    }
}
```

### 11.8 Keep Secrets Out of Config Files

```cpp
// ❌ Bad: Secrets in config.toml
// [database]
// password = "supersecret123"

// ✅ Good: Secrets from environment
// config.toml contains no secrets
// Production: export MYAPP_DATABASE_PASSWORD=supersecret123
// Development: Use .env file (not committed)

opts.mandatory = {"database.password"};  // Enforces that it must be set
```

### 11.9 Use Environment-Specific Configurations

```bash
# Directory structure:
config/
├── base.toml           # Shared settings
├── development.toml    # Dev overrides
├── staging.toml        # Staging overrides
└── production.toml     # Prod overrides
```

```cpp
std::string env = std::getenv("APP_ENV") ?: "development";
std::string config_file = "config/" + env + ".toml";
```

---

## 12. Troubleshooting

### Problem: Environment variable not being picked up

**Symptoms:** Value from environment variable is ignored.

**Checklist:**
1. Is the prefix correct?
   ```cpp
   opts.prefix = "MYAPP";  // Expects MYAPP_*
   ```

2. Is the variable name correct?
   ```bash
   # For config key "database.host":
   export MYAPP_DATABASE_HOST=value  # ✓ Correct
   export MYAPP_database_host=value  # ✗ Wrong case
   ```

3. Are you using double underscores for keys with underscores?
   ```bash
   # For config key "feature_flags.enabled":
   export MYAPP_FEATURE_FLAGS__ENABLED=true  # ✓ Correct
   export MYAPP_FEATURE_FLAGS_ENABLED=true   # ✗ Wrong (becomes feature.flags.enabled)
   ```

4. Debug by dumping the config:
   ```bash
   confy-cpp -c config.toml -p MYAPP dump | grep -i database
   ```

### Problem: .env file not loading

**Symptoms:** Variables in .env are not being applied.

**Checklist:**
1. Is .env loading enabled?
   ```cpp
   opts.load_dotenv_file = true;  // Default
   ```

2. Is the .env file in the right location?
   ```bash
   # confy searches current directory and parents
   ls -la .env
   ```

3. Remember: .env doesn't override existing environment variables
   ```bash
   # If DATABASE_HOST is already set in shell, .env won't override it
   unset DATABASE_HOST  # Then try again
   ```

4. Check .env syntax:
   ```bash
   # Valid formats:
   KEY=value
   KEY="quoted value"
   export KEY=value
   
   # Invalid:
   KEY = value    # Spaces around =
   KEY: value     # Wrong separator
   ```

### Problem: TOML file not loading correctly

**Symptoms:** Values from TOML file are missing or wrong.

**Checklist:**
1. Is the file extension correct?
   ```cpp
   opts.file_path = "config.toml";  // ✓
   opts.file_path = "config.conf";  // ✗ Unknown extension
   ```

2. Is the TOML syntax valid?
   ```bash
   # Online validator: https://www.toml-lint.com/
   ```

3. Are you using correct TOML types?
   ```toml
   # Correct:
   port = 8080        # Integer
   host = "localhost" # String (must be quoted!)
   enabled = true     # Boolean
   
   # Wrong:
   host = localhost   # Error: unquoted string
   ```

### Problem: Type conversion errors

**Symptoms:** `get<T>()` throws or returns unexpected values.

**Checklist:**
1. Match the type to the value:
   ```cpp
   // If config has: port = 8080
   cfg.get<int>("port");        // ✓
   cfg.get<std::string>("port"); // May work but not ideal
   
   // If config has: port = "8080" (string)
   cfg.get<std::string>("port"); // ✓
   cfg.get<int>("port");         // ✗ Will throw
   ```

2. Use the default-value form for safety:
   ```cpp
   int port = cfg.get<int>("port", 8080);  // Safe: returns 8080 if missing
   ```

### Problem: Mandatory key validation fails unexpectedly

**Symptoms:** `MissingMandatoryConfig` thrown even though key seems present.

**Checklist:**
1. Check the exact path:
   ```cpp
   opts.mandatory = {"database.host"};  // Dot notation
   // NOT: {"database", "host"}  // This checks for two separate keys
   ```

2. Check for typos:
   ```cpp
   opts.mandatory = {"databse.host"};  // Typo!
   ```

3. Verify the key exists in merged config:
   ```bash
   confy-cpp -c config.toml -p MYAPP dump | grep -i database
   ```

---

## 13. Migration from Python confy

confy-cpp is designed for 100% behavioral parity with Python confy. Here's how to migrate:

### API Comparison

| Python | C++ |
|--------|-----|
| `Config(file_path=...)` | `LoadOptions opts; opts.file_path = ...` |
| `cfg.get("key", default)` | `cfg.get<T>("key", default)` |
| `cfg["key"]` | `cfg.get("key")` |
| `"key" in cfg` | `cfg.contains("key")` |
| `cfg.as_dict()` | `cfg.to_dict()` or `cfg.data()` |

### Python to C++ Example

**Python:**
```python
from confy import Config

cfg = Config(
    file_path="config.toml",
    prefix="MYAPP",
    defaults={"server": {"port": 8080}},
    mandatory=["database.host"]
)

host = cfg.get("database.host")
port = cfg.get("server.port", 8080)
```

**C++:**
```cpp
#include <confy/Config.hpp>

confy::LoadOptions opts;
opts.file_path = "config.toml";
opts.prefix = "MYAPP";
opts.defaults = {{"server", {{"port", 8080}}}};
opts.mandatory = {"database.host"};

confy::Config cfg = confy::Config::load(opts);

std::string host = cfg.get<std::string>("database.host");
int port = cfg.get<int>("server.port", 8080);
```

### Shared Configuration Files

Both implementations read the same JSON/TOML files identically:

```toml
# config.toml - works with both Python and C++
[server]
host = "0.0.0.0"
port = 8080

[database]
host = "localhost"
port = 5432
```

### Shared Environment Variables

Both implementations use identical environment variable mapping:

```bash
# These work identically in Python and C++
export MYAPP_SERVER_PORT=9000
export MYAPP_DATABASE_HOST=db.example.com
export MYAPP_FEATURE_FLAGS__BETA=true
```

### Testing Parity

To verify your Python and C++ configurations match:

```bash
# Python
python -c "from confy import Config; import json; \
    cfg = Config(file_path='config.toml', prefix='MYAPP'); \
    print(json.dumps(cfg.as_dict(), sort_keys=True, indent=2))"

# C++
confy-cpp -c config.toml -p MYAPP dump

# Compare outputs - they should be identical
```

---

## Appendix: Quick Reference

### LoadOptions Fields

```cpp
struct LoadOptions {
    std::string file_path;                              // Config file
    std::optional<std::string> prefix = std::nullopt;   // Env prefix
    bool load_dotenv_file = true;                       // Load .env
    std::string dotenv_path;                            // .env location
    Value defaults = Value::object();                   // Defaults
    std::unordered_map<std::string, Value> overrides;   // Overrides
    std::vector<std::string> mandatory;                 // Required keys
};
```

### Config Methods

```cpp
// Loading
static Config load(const LoadOptions& opts);

// Reading
template<typename T> T get(const std::string& path, const T& default_val) const;
Value get(const std::string& path) const;
std::optional<Value> get_optional(const std::string& path) const;
bool contains(const std::string& path) const;

// Writing
void set(const std::string& path, const Value& value, bool create_missing = true);
void merge(const Config& other);
void merge(const Value& other);

// Serialization
std::string to_json(int indent = 2) const;
std::string to_toml() const;
Value to_dict() const;
const Value& data() const;
```

### Exception Types

```cpp
ConfigError              // Base class
├── MissingMandatoryConfig  // Required keys missing
├── FileNotFoundError       // Config file not found
├── ConfigParseError        // JSON/TOML syntax error
├── KeyError                // Path segment not found
└── TypeError               // Traverse into non-object
```

### Environment Variable Mapping

```
MYAPP_DATABASE_HOST       → database.host
MYAPP_FEATURE_FLAGS__BETA → feature_flags.beta
```

### Precedence (Low → High)

```
defaults → file → .env → environment → overrides
```

---

*For more information, see the [README](README.md), [API documentation](docs/api.md), or the [design specification](CONFY_DESIGN_SPECIFICATION.md).*
