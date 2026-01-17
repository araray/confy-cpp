# confy-cpp API Reference

Complete API documentation for the confy-cpp configuration management library.

**Version:** 1.0.0  
**C++ Standard:** C++17  
**Namespace:** `confy`

---

## Table of Contents

1. [Overview](#1-overview)
2. [Core Types](#2-core-types)
3. [Config Class](#3-config-class)
4. [LoadOptions Structure](#4-loadoptions-structure)
5. [Exception Classes](#5-exception-classes)
6. [DotPath Module](#6-dotpath-module)
7. [Parse Module](#7-parse-module)
8. [Merge Module](#8-merge-module)
9. [Loader Module](#9-loader-module)
10. [EnvMapper Module](#10-envmapper-module)
11. [Utility Functions](#11-utility-functions)
12. [Constants](#12-constants)
13. [Type Traits & Concepts](#13-type-traits--concepts)

---

## 1. Overview

### Header Organization

```cpp
#include <confy/Config.hpp>      // Config class, LoadOptions (includes all below)
#include <confy/Value.hpp>       // Value type alias
#include <confy/Errors.hpp>      // Exception hierarchy
#include <confy/DotPath.hpp>     // Dot-path utilities
#include <confy/Parse.hpp>       // String-to-value parsing
#include <confy/Merge.hpp>       // Deep merge utilities
#include <confy/Loader.hpp>      // File loading (JSON/TOML/.env)
#include <confy/EnvMapper.hpp>   // Environment variable mapping
```

### Namespace

All public symbols are in the `confy` namespace:

```cpp
namespace confy {
    // Types
    using Value = nlohmann::json;
    
    // Classes
    class Config;
    struct LoadOptions;
    
    // Exceptions
    class ConfigError;
    class MissingMandatoryConfig;
    class FileNotFoundError;
    class ConfigParseError;
    class KeyError;
    class TypeError;
    
    // Functions
    // ... (see individual modules)
}
```

---

## 2. Core Types

### Value

```cpp
// Defined in <confy/Value.hpp>

namespace confy {
    using Value = nlohmann::json;
}
```

**Description:**  
Type alias for `nlohmann::json`, providing a flexible JSON-like value container that supports:
- Null values
- Booleans
- Integers (signed/unsigned, up to 64-bit)
- Floating-point numbers (double precision)
- Strings (UTF-8)
- Arrays (heterogeneous)
- Objects (string keys, heterogeneous values)

**Construction:**

```cpp
// Null
Value null_val = nullptr;
Value null_val2;  // Default is null

// Boolean
Value bool_val = true;
Value bool_val2 = false;

// Numbers
Value int_val = 42;
Value int64_val = int64_t(9223372036854775807);
Value uint_val = uint32_t(4294967295);
Value float_val = 3.14159;
Value double_val = 2.718281828459045;

// String
Value str_val = "hello";
Value str_val2 = std::string("world");

// Array
Value arr_val = {1, 2, 3, 4, 5};
Value arr_val2 = Value::array();  // Empty array
Value arr_val3 = {"mixed", 42, true, nullptr};

// Object
Value obj_val = {
    {"name", "Alice"},
    {"age", 30},
    {"active", true}
};
Value obj_val2 = Value::object();  // Empty object
```

**Type Checking:**

```cpp
bool is_null() const;
bool is_boolean() const;
bool is_number() const;
bool is_number_integer() const;
bool is_number_unsigned() const;
bool is_number_float() const;
bool is_string() const;
bool is_array() const;
bool is_object() const;
bool is_primitive() const;      // null, bool, number, string
bool is_structured() const;     // array or object
```

**Type Conversion:**

```cpp
template<typename T>
T get() const;                  // Throws on type mismatch

template<typename T>
T get_to(T& v) const;          // Assigns to v, returns reference

template<typename T, typename U>
T value(const std::string& key, U&& default_val) const;  // For objects
```

**Element Access:**

```cpp
// Array access
Value& operator[](size_t idx);
const Value& operator[](size_t idx) const;
Value& at(size_t idx);
const Value& at(size_t idx) const;

// Object access
Value& operator[](const std::string& key);
const Value& operator[](const std::string& key) const;
Value& at(const std::string& key);
const Value& at(const std::string& key) const;

// Check existence (objects)
bool contains(const std::string& key) const;
size_t count(const std::string& key) const;

// Size
size_t size() const;
bool empty() const;
```

**Iteration:**

```cpp
// Arrays
for (const auto& element : array_value) { /* ... */ }

// Objects
for (auto& [key, value] : object_value.items()) { /* ... */ }

// Iterator access
iterator begin();
iterator end();
const_iterator begin() const;
const_iterator end() const;
```

**Serialization:**

```cpp
std::string dump(int indent = -1) const;  // To JSON string
// indent = -1: compact
// indent >= 0: pretty-print with that many spaces
```

**See Also:** [nlohmann/json documentation](https://json.nlohmann.me/)

---

## 3. Config Class

```cpp
// Defined in <confy/Config.hpp>

namespace confy {
    class Config;
}
```

### Synopsis

```cpp
class Config {
public:
    // Constructors
    Config();
    explicit Config(const Value& data);
    explicit Config(Value&& data);
    
    // Static factory
    static Config load(const LoadOptions& opts);
    
    // Value access
    template<typename T>
    T get(const std::string& path, const T& default_value) const;
    
    Value get(const std::string& path) const;
    std::optional<Value> get_optional(const std::string& path) const;
    
    // Existence check
    bool contains(const std::string& path) const;
    
    // Modification
    void set(const std::string& path, const Value& value, bool create_missing = true);
    
    // Merging
    void merge(const Config& other);
    void merge(const Value& other);
    
    // Serialization
    std::string to_json(int indent = 2) const;
    std::string to_toml() const;
    Value to_dict() const;
    
    // Raw access
    const Value& data() const;
    Value& data();
    
    // Utility
    bool empty() const;
    size_t size() const;
    
    // Validation
    void validate_mandatory(const std::vector<std::string>& keys) const;
};
```

### Constructors

#### Config()

```cpp
Config();
```

**Description:**  
Default constructor. Creates an empty configuration with an empty JSON object as the underlying data.

**Example:**
```cpp
confy::Config cfg;
assert(cfg.empty());
assert(cfg.data().is_object());
```

---

#### Config(const Value&)

```cpp
explicit Config(const Value& data);
```

**Description:**  
Constructs a Config from a copy of the provided Value.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `data` | `const Value&` | Initial configuration data (should be an object) |

**Example:**
```cpp
confy::Value initial = {
    {"server", {{"port", 8080}}}
};
confy::Config cfg(initial);
```

---

#### Config(Value&&)

```cpp
explicit Config(Value&& data);
```

**Description:**  
Constructs a Config by moving the provided Value (avoids copy).

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `data` | `Value&&` | Initial configuration data (moved) |

**Example:**
```cpp
confy::Config cfg(confy::Value{{"key", "value"}});
```

---

### Static Methods

#### Config::load

```cpp
static Config load(const LoadOptions& opts);
```

**Description:**  
Factory method that loads configuration from multiple sources according to the provided options. This is the primary way to create a fully-configured Config instance.

**Loading Order (Precedence):**
1. `opts.defaults` (lowest)
2. Config file (`opts.file_path`)
3. `.env` file (if `opts.load_dotenv_file`)
4. Environment variables (if `opts.prefix` has value)
5. `opts.overrides` (highest)

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `opts` | `const LoadOptions&` | Configuration loading options |

**Returns:**  
`Config` — Fully loaded and merged configuration.

**Throws:**
| Exception | Condition |
|-----------|-----------|
| `FileNotFoundError` | `opts.file_path` specified but file doesn't exist |
| `ConfigParseError` | Config file has syntax errors |
| `MissingMandatoryConfig` | One or more `opts.mandatory` keys missing after merge |

**Example:**
```cpp
confy::LoadOptions opts;
opts.defaults = {{"server", {{"port", 8080}}}};
opts.file_path = "config.toml";
opts.prefix = "MYAPP";
opts.mandatory = {"database.host"};

try {
    confy::Config cfg = confy::Config::load(opts);
} catch (const confy::MissingMandatoryConfig& e) {
    // Handle missing keys
}
```

---

### Instance Methods

#### get\<T\>(path, default_value)

```cpp
template<typename T>
T get(const std::string& path, const T& default_value) const;
```

**Description:**  
Retrieves the value at the specified dot-path, converted to type `T`. If the path doesn't exist, returns `default_value`.

**Template Parameters:**
| Name | Constraints | Description |
|------|-------------|-------------|
| `T` | Convertible from JSON | Target type for value conversion |

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `path` | `const std::string&` | Dot-separated path (e.g., `"database.host"`) |
| `default_value` | `const T&` | Value to return if path not found |

**Returns:**  
`T` — The value at path, or `default_value` if not found.

**Throws:**
| Exception | Condition |
|-----------|-----------|
| `TypeError` | Path traverses through a non-object/non-array value |
| `nlohmann::json::type_error` | Value exists but cannot be converted to `T` |

**Supported Types for T:**
- `bool`
- `int`, `int8_t`, `int16_t`, `int32_t`, `int64_t`
- `unsigned int`, `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`, `size_t`
- `float`, `double`
- `std::string`
- `std::vector<T>` (where T is supported)
- `std::map<std::string, T>`
- `confy::Value`

**Example:**
```cpp
std::string host = cfg.get<std::string>("database.host", "localhost");
int port = cfg.get<int>("database.port", 5432);
bool debug = cfg.get<bool>("debug", false);
auto tags = cfg.get<std::vector<std::string>>("tags", {});
```

---

#### get(path)

```cpp
Value get(const std::string& path) const;
```

**Description:**  
Retrieves the value at the specified dot-path. Throws if path doesn't exist.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `path` | `const std::string&` | Dot-separated path |

**Returns:**  
`Value` — The value at the specified path.

**Throws:**
| Exception | Condition |
|-----------|-----------|
| `KeyError` | Path segment doesn't exist |
| `TypeError` | Path traverses through a non-object/non-array |

**Example:**
```cpp
try {
    confy::Value db = cfg.get("database");
    std::string host = db["host"].get<std::string>();
} catch (const confy::KeyError& e) {
    std::cerr << "Key not found: " << e.path() << std::endl;
}
```

---

#### get_optional(path)

```cpp
std::optional<Value> get_optional(const std::string& path) const;
```

**Description:**  
Retrieves the value at the specified dot-path, returning `std::nullopt` if not found.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `path` | `const std::string&` | Dot-separated path |

**Returns:**  
`std::optional<Value>` — The value if found, `std::nullopt` otherwise.

**Throws:**
| Exception | Condition |
|-----------|-----------|
| `TypeError` | Path traverses through a non-object/non-array |

**Example:**
```cpp
if (auto ssl = cfg.get_optional("server.ssl")) {
    bool enabled = ssl->value("enabled", false);
}
```

---

#### contains(path)

```cpp
bool contains(const std::string& path) const;
```

**Description:**  
Checks whether a value exists at the specified dot-path.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `path` | `const std::string&` | Dot-separated path |

**Returns:**  
`bool` — `true` if path exists and resolves to a value, `false` otherwise.

**Throws:**
| Exception | Condition |
|-----------|-----------|
| `TypeError` | Path traverses through a non-object/non-array |

**Example:**
```cpp
if (cfg.contains("database.replica.host")) {
    // Configure replica
}
```

---

#### set(path, value, create_missing)

```cpp
void set(const std::string& path, const Value& value, bool create_missing = true);
```

**Description:**  
Sets a value at the specified dot-path.

**Parameters:**
| Name | Type | Default | Description |
|------|------|---------|-------------|
| `path` | `const std::string&` | — | Dot-separated path |
| `value` | `const Value&` | — | Value to set |
| `create_missing` | `bool` | `true` | Create intermediate objects if path doesn't exist |

**Throws:**
| Exception | Condition |
|-----------|-----------|
| `KeyError` | `create_missing=false` and path doesn't exist |
| `TypeError` | Path traverses through non-object (when `create_missing=false`) |

**Behavior:**
- When `create_missing=true`: Creates intermediate objects as needed, overwrites non-objects in path
- When `create_missing=false`: Throws if any segment doesn't exist

**Example:**
```cpp
cfg.set("server.port", 9000);
cfg.set("new.deeply.nested.key", "value");  // Creates all intermediate objects
cfg.set("existing.key", "updated", false);  // Throws if path doesn't exist
```

---

#### merge(const Config&)

```cpp
void merge(const Config& other);
```

**Description:**  
Deep-merges another Config into this one. Values from `other` override values in `this`.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `other` | `const Config&` | Configuration to merge from |

**Merge Rules:**
- Objects are merged recursively
- Arrays are replaced entirely
- Scalars are replaced
- `null` does not override existing values

**Example:**
```cpp
confy::Config base({{"a", 1}, {"b", {{"c", 2}}}});
confy::Config overlay({{"b", {{"d", 3}}}});
base.merge(overlay);
// Result: {"a": 1, "b": {"c": 2, "d": 3}}
```

---

#### merge(const Value&)

```cpp
void merge(const Value& other);
```

**Description:**  
Deep-merges a Value into this Config.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `other` | `const Value&` | Value to merge from (should be an object) |

---

#### to_json(indent)

```cpp
std::string to_json(int indent = 2) const;
```

**Description:**  
Serializes the configuration to a JSON string.

**Parameters:**
| Name | Type | Default | Description |
|------|------|---------|-------------|
| `indent` | `int` | `2` | Indentation level; `-1` for compact output |

**Returns:**  
`std::string` — JSON representation of the configuration.

**Example:**
```cpp
std::cout << cfg.to_json() << std::endl;      // Pretty-printed
std::cout << cfg.to_json(-1) << std::endl;    // Compact
std::cout << cfg.to_json(4) << std::endl;     // 4-space indent
```

---

#### to_toml()

```cpp
std::string to_toml() const;
```

**Description:**  
Serializes the configuration to a TOML string.

**Returns:**  
`std::string` — TOML representation of the configuration.

**Example:**
```cpp
std::ofstream file("config.toml");
file << cfg.to_toml();
```

---

#### to_dict()

```cpp
Value to_dict() const;
```

**Description:**  
Returns a copy of the underlying data as a Value.

**Returns:**  
`Value` — Copy of the configuration data.

---

#### data() const

```cpp
const Value& data() const;
```

**Description:**  
Returns a const reference to the underlying data.

**Returns:**  
`const Value&` — Reference to the configuration data.

---

#### data()

```cpp
Value& data();
```

**Description:**  
Returns a mutable reference to the underlying data.

**Returns:**  
`Value&` — Mutable reference to the configuration data.

**Warning:** Direct modification bypasses validation. Use with caution.

---

#### empty()

```cpp
bool empty() const;
```

**Description:**  
Checks if the configuration has no keys.

**Returns:**  
`bool` — `true` if no top-level keys exist.

---

#### size()

```cpp
size_t size() const;
```

**Description:**  
Returns the number of top-level keys.

**Returns:**  
`size_t` — Number of top-level keys.

---

#### validate_mandatory(keys)

```cpp
void validate_mandatory(const std::vector<std::string>& keys) const;
```

**Description:**  
Validates that all specified keys exist in the configuration.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `keys` | `const std::vector<std::string>&` | List of required dot-paths |

**Throws:**
| Exception | Condition |
|-----------|-----------|
| `MissingMandatoryConfig` | One or more keys are missing |

**Note:** Collects ALL missing keys before throwing (not just the first).

**Example:**
```cpp
try {
    cfg.validate_mandatory({"database.host", "api.key"});
} catch (const confy::MissingMandatoryConfig& e) {
    for (const auto& key : e.missing_keys()) {
        std::cerr << "Missing: " << key << std::endl;
    }
}
```

---

## 4. LoadOptions Structure

```cpp
// Defined in <confy/Config.hpp>

namespace confy {
    struct LoadOptions;
}
```

### Synopsis

```cpp
struct LoadOptions {
    std::string file_path;
    std::optional<std::string> prefix = std::nullopt;
    bool load_dotenv_file = true;
    std::string dotenv_path;
    Value defaults = Value::object();
    std::unordered_map<std::string, Value> overrides;
    std::vector<std::string> mandatory;
};
```

### Fields

#### file_path

```cpp
std::string file_path;
```

**Type:** `std::string`  
**Default:** `""` (empty)

**Description:**  
Path to the configuration file (JSON or TOML). Format is auto-detected by extension.

**Behavior:**
- Empty string: No file loaded
- Non-existent file: Throws `FileNotFoundError`
- Supported extensions: `.json`, `.toml` (case-insensitive)

**Example:**
```cpp
opts.file_path = "config.toml";
opts.file_path = "/etc/myapp/config.json";
opts.file_path = "";  // No file
```

---

#### prefix

```cpp
std::optional<std::string> prefix = std::nullopt;
```

**Type:** `std::optional<std::string>`  
**Default:** `std::nullopt`

**Description:**  
Environment variable prefix for filtering and transformation.

**Behavior:**
| Value | Behavior |
|-------|----------|
| `std::nullopt` | Environment variables disabled |
| `""` (empty) | Most env vars included (system vars excluded) |
| `"MYAPP"` | Only `MYAPP_*` variables included |

**Prefix Matching:**
- Case-insensitive matching
- Prefix + underscore required (e.g., `MYAPP_KEY` not `MYAPPKEY`)

**Example:**
```cpp
opts.prefix = "MYAPP";           // MYAPP_DATABASE_HOST → database.host
opts.prefix = "";                // DATABASE_HOST → database.host (if not system var)
opts.prefix = std::nullopt;      // Disable env vars
```

---

#### load_dotenv_file

```cpp
bool load_dotenv_file = true;
```

**Type:** `bool`  
**Default:** `true`

**Description:**  
Whether to load variables from a `.env` file.

**Behavior:**
- `true`: Search for and load `.env` file
- `false`: Skip `.env` loading entirely

---

#### dotenv_path

```cpp
std::string dotenv_path;
```

**Type:** `std::string`  
**Default:** `""` (empty)

**Description:**  
Explicit path to `.env` file.

**Behavior:**
- Empty: Search current directory and parents for `.env`
- Non-empty: Use specified path directly

**Example:**
```cpp
opts.dotenv_path = ".env.local";
opts.dotenv_path = "/app/.env";
```

---

#### defaults

```cpp
Value defaults = Value::object();
```

**Type:** `Value`  
**Default:** Empty object `{}`

**Description:**  
Default configuration values (lowest precedence).

**Example:**
```cpp
opts.defaults = {
    {"server", {
        {"host", "localhost"},
        {"port", 8080}
    }},
    {"debug", false}
};
```

---

#### overrides

```cpp
std::unordered_map<std::string, Value> overrides;
```

**Type:** `std::unordered_map<std::string, Value>`  
**Default:** Empty map

**Description:**  
Programmatic overrides (highest precedence). Keys are dot-paths.

**Example:**
```cpp
opts.overrides = {
    {"server.port", 9000},
    {"debug", true},
    {"new.key", "value"}
};
```

---

#### mandatory

```cpp
std::vector<std::string> mandatory;
```

**Type:** `std::vector<std::string>`  
**Default:** Empty vector

**Description:**  
List of required configuration keys (dot-paths). Validation occurs after all sources are merged.

**Example:**
```cpp
opts.mandatory = {
    "database.host",
    "database.name",
    "api.key"
};
```

---

## 5. Exception Classes

```cpp
// Defined in <confy/Errors.hpp>

namespace confy {
    class ConfigError;
    class MissingMandatoryConfig;
    class FileNotFoundError;
    class ConfigParseError;
    class KeyError;
    class TypeError;
}
```

### Hierarchy

```
std::runtime_error
    └── ConfigError
            ├── MissingMandatoryConfig
            ├── FileNotFoundError
            ├── ConfigParseError
            ├── KeyError
            └── TypeError
```

---

### ConfigError

```cpp
class ConfigError : public std::runtime_error {
public:
    explicit ConfigError(const std::string& message);
    const char* what() const noexcept override;
};
```

**Description:**  
Base class for all confy-specific exceptions.

**Example:**
```cpp
try {
    // confy operations
} catch (const confy::ConfigError& e) {
    // Catches any confy exception
    std::cerr << e.what() << std::endl;
}
```

---

### MissingMandatoryConfig

```cpp
class MissingMandatoryConfig : public ConfigError {
public:
    explicit MissingMandatoryConfig(const std::vector<std::string>& missing_keys);
    
    const std::vector<std::string>& missing_keys() const noexcept;
};
```

**Description:**  
Thrown when one or more mandatory configuration keys are missing after all sources are merged.

**Methods:**

| Method | Returns | Description |
|--------|---------|-------------|
| `missing_keys()` | `const std::vector<std::string>&` | List of all missing key paths |

**Example:**
```cpp
try {
    confy::Config cfg = confy::Config::load(opts);
} catch (const confy::MissingMandatoryConfig& e) {
    std::cerr << "Missing keys:\n";
    for (const auto& key : e.missing_keys()) {
        std::cerr << "  - " << key << "\n";
    }
}
```

---

### FileNotFoundError

```cpp
class FileNotFoundError : public ConfigError {
public:
    explicit FileNotFoundError(const std::string& path);
    
    const std::string& path() const noexcept;
};
```

**Description:**  
Thrown when a specified configuration file doesn't exist.

**Methods:**

| Method | Returns | Description |
|--------|---------|-------------|
| `path()` | `const std::string&` | Path to the missing file |

**Example:**
```cpp
try {
    confy::Config cfg = confy::Config::load(opts);
} catch (const confy::FileNotFoundError& e) {
    std::cerr << "Config file not found: " << e.path() << std::endl;
}
```

---

### ConfigParseError

```cpp
class ConfigParseError : public ConfigError {
public:
    ConfigParseError(const std::string& file, const std::string& details);
    
    const std::string& file() const noexcept;
    const std::string& details() const noexcept;
};
```

**Description:**  
Thrown when a configuration file has syntax errors.

**Methods:**

| Method | Returns | Description |
|--------|---------|-------------|
| `file()` | `const std::string&` | Path to the file with errors |
| `details()` | `const std::string&` | Parser error message |

**Example:**
```cpp
try {
    confy::Config cfg = confy::Config::load(opts);
} catch (const confy::ConfigParseError& e) {
    std::cerr << "Parse error in " << e.file() << ":\n";
    std::cerr << e.details() << std::endl;
}
```

---

### KeyError

```cpp
class KeyError : public ConfigError {
public:
    KeyError(const std::string& path, const std::string& segment);
    
    const std::string& path() const noexcept;
    const std::string& segment() const noexcept;
};
```

**Description:**  
Thrown when a dot-path segment doesn't exist during strict access.

**Methods:**

| Method | Returns | Description |
|--------|---------|-------------|
| `path()` | `const std::string&` | Full dot-path being accessed |
| `segment()` | `const std::string&` | Specific segment that was not found |

**Example:**
```cpp
try {
    confy::Value val = cfg.get("database.replica.host");
} catch (const confy::KeyError& e) {
    std::cerr << "Key '" << e.segment() << "' not found in path '" 
              << e.path() << "'" << std::endl;
}
```

---

### TypeError

```cpp
class TypeError : public ConfigError {
public:
    TypeError(const std::string& path, const std::string& expected, const std::string& actual);
    
    const std::string& path() const noexcept;
    const std::string& expected() const noexcept;
    const std::string& actual() const noexcept;
};
```

**Description:**  
Thrown when path traversal encounters a non-traversable type (e.g., trying to access `port.invalid` when `port` is an integer).

**Methods:**

| Method | Returns | Description |
|--------|---------|-------------|
| `path()` | `const std::string&` | Full dot-path being accessed |
| `expected()` | `const std::string&` | Expected type (e.g., "object" or "array") |
| `actual()` | `const std::string&` | Actual type encountered |

**Example:**
```cpp
try {
    cfg.get("server.port.invalid");  // port is integer
} catch (const confy::TypeError& e) {
    std::cerr << "Type error at '" << e.path() << "': "
              << "expected " << e.expected() 
              << ", got " << e.actual() << std::endl;
}
```

---

## 6. DotPath Module

```cpp
// Defined in <confy/DotPath.hpp>

namespace confy {
    std::vector<std::string> split_dot_path(const std::string& path);
    Value get_by_dot(const Value& data, const std::string& path);
    Value get_by_dot(const Value& data, const std::string& path, const Value& default_value);
    void set_by_dot(Value& data, const std::string& path, const Value& value, bool create_missing = true);
    bool contains_dot(const Value& data, const std::string& path);
}
```

### split_dot_path

```cpp
std::vector<std::string> split_dot_path(const std::string& path);
```

**Description:**  
Splits a dot-path string into individual segments.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `path` | `const std::string&` | Dot-separated path |

**Returns:**  
`std::vector<std::string>` — Vector of path segments.

**Behavior:**
- Empty path returns empty vector
- Empty segments are filtered out (`"a..b"` → `["a", "b"]`)
- Leading/trailing dots are ignored

**Example:**
```cpp
auto parts = confy::split_dot_path("database.connection.host");
// parts = {"database", "connection", "host"}

auto parts2 = confy::split_dot_path("a..b");
// parts2 = {"a", "b"}
```

---

### get_by_dot (strict)

```cpp
Value get_by_dot(const Value& data, const std::string& path);
```

**Description:**  
Retrieves a value at the specified dot-path. Throws if path doesn't exist.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `data` | `const Value&` | Source data |
| `path` | `const std::string&` | Dot-separated path |

**Returns:**  
`Value` — Value at the specified path.

**Throws:**
| Exception | Condition |
|-----------|-----------|
| `KeyError` | Path segment doesn't exist |
| `TypeError` | Path traverses non-object/non-array |

**Example:**
```cpp
confy::Value data = {{"db", {{"host", "localhost"}}}};
confy::Value host = confy::get_by_dot(data, "db.host");
// host = "localhost"
```

---

### get_by_dot (with default)

```cpp
Value get_by_dot(const Value& data, const std::string& path, const Value& default_value);
```

**Description:**  
Retrieves a value at the specified dot-path, returning default if not found.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `data` | `const Value&` | Source data |
| `path` | `const std::string&` | Dot-separated path |
| `default_value` | `const Value&` | Value to return if path not found |

**Returns:**  
`Value` — Value at path, or `default_value` if not found.

**Throws:**
| Exception | Condition |
|-----------|-----------|
| `TypeError` | Path traverses non-object/non-array |

**Note:** Returns default only for missing keys, NOT for type errors.

**Example:**
```cpp
confy::Value data = {{"db", {{"host", "localhost"}}}};
confy::Value port = confy::get_by_dot(data, "db.port", 5432);
// port = 5432 (default)
```

---

### set_by_dot

```cpp
void set_by_dot(Value& data, const std::string& path, const Value& value, bool create_missing = true);
```

**Description:**  
Sets a value at the specified dot-path.

**Parameters:**
| Name | Type | Default | Description |
|------|------|---------|-------------|
| `data` | `Value&` | — | Target data (modified in-place) |
| `path` | `const std::string&` | — | Dot-separated path |
| `value` | `const Value&` | — | Value to set |
| `create_missing` | `bool` | `true` | Create intermediate objects |

**Throws:**
| Exception | Condition |
|-----------|-----------|
| `KeyError` | `create_missing=false` and path doesn't exist |
| `TypeError` | `create_missing=false` and path traverses non-object |

**Example:**
```cpp
confy::Value data = confy::Value::object();
confy::set_by_dot(data, "database.host", "localhost");
// data = {"database": {"host": "localhost"}}
```

---

### contains_dot

```cpp
bool contains_dot(const Value& data, const std::string& path);
```

**Description:**  
Checks whether a value exists at the specified dot-path.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `data` | `const Value&` | Source data |
| `path` | `const std::string&` | Dot-separated path |

**Returns:**  
`bool` — `true` if path exists, `false` otherwise.

**Throws:**
| Exception | Condition |
|-----------|-----------|
| `TypeError` | Path traverses non-object/non-array |

**Example:**
```cpp
confy::Value data = {{"db", {{"host", "localhost"}}}};
bool has_host = confy::contains_dot(data, "db.host");    // true
bool has_port = confy::contains_dot(data, "db.port");    // false
```

---

## 7. Parse Module

```cpp
// Defined in <confy/Parse.hpp>

namespace confy {
    Value parse_value(const std::string& str);
}
```

### parse_value

```cpp
Value parse_value(const std::string& str);
```

**Description:**  
Parses a string into an appropriate Value type using automatic type inference.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `str` | `const std::string&` | String to parse |

**Returns:**  
`Value` — Parsed value with inferred type.

**Parsing Rules (in order):**

| Rule | Pattern | Result |
|------|---------|--------|
| T1 | `"true"`, `"false"` (case-insensitive) | `bool` |
| T2 | `"null"` (case-insensitive) | `null` |
| T3 | `^-?[0-9]+$` | `int64_t` |
| T4 | `^-?[0-9]+\.[0-9]+([eE][+-]?[0-9]+)?$` | `double` |
| T5 | Starts with `{` or `[` | Parsed JSON |
| T6 | Starts and ends with `"` | Unquoted string |
| T7 | Everything else | Raw string |

**Examples:**
```cpp
confy::Value v1 = confy::parse_value("true");      // bool: true
confy::Value v2 = confy::parse_value("TRUE");      // bool: true
confy::Value v3 = confy::parse_value("null");      // null
confy::Value v4 = confy::parse_value("42");        // int64_t: 42
confy::Value v5 = confy::parse_value("-123");      // int64_t: -123
confy::Value v6 = confy::parse_value("3.14");      // double: 3.14
confy::Value v7 = confy::parse_value("1.5e-3");    // double: 0.0015
confy::Value v8 = confy::parse_value("[1,2,3]");   // array: [1,2,3]
confy::Value v9 = confy::parse_value("{\"a\":1}"); // object: {"a":1}
confy::Value v10 = confy::parse_value("\"hello\""); // string: "hello"
confy::Value v11 = confy::parse_value("hello");    // string: "hello"
confy::Value v12 = confy::parse_value("/usr/bin"); // string: "/usr/bin"
```

---

## 8. Merge Module

```cpp
// Defined in <confy/Merge.hpp>

namespace confy {
    Value deep_merge(const Value& base, const Value& overlay);
    void deep_merge_into(Value& base, const Value& overlay);
}
```

### deep_merge

```cpp
Value deep_merge(const Value& base, const Value& overlay);
```

**Description:**  
Creates a new Value by deep-merging two Values. Values from `overlay` take precedence.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `base` | `const Value&` | Base configuration |
| `overlay` | `const Value&` | Configuration to merge on top |

**Returns:**  
`Value` — New merged configuration.

**Merge Rules:**
- Objects: Recursively merged (keys combined)
- Arrays: `overlay` replaces `base` entirely
- Scalars: `overlay` replaces `base`
- `null` in `overlay`: Does NOT override `base`

**Example:**
```cpp
confy::Value base = {
    {"a", 1},
    {"b", {{"c", 2}, {"d", 3}}}
};
confy::Value overlay = {
    {"b", {{"c", 4}, {"e", 5}}}
};
confy::Value result = confy::deep_merge(base, overlay);
// result = {"a": 1, "b": {"c": 4, "d": 3, "e": 5}}
```

---

### deep_merge_into

```cpp
void deep_merge_into(Value& base, const Value& overlay);
```

**Description:**  
Deep-merges `overlay` into `base` in-place.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `base` | `Value&` | Base configuration (modified in-place) |
| `overlay` | `const Value&` | Configuration to merge from |

**Example:**
```cpp
confy::Value config = {{"a", 1}};
confy::Value updates = {{"b", 2}};
confy::deep_merge_into(config, updates);
// config = {"a": 1, "b": 2}
```

---

## 9. Loader Module

```cpp
// Defined in <confy/Loader.hpp>

namespace confy {
    // File loading
    Value load_json_file(const std::string& path);
    Value load_toml_file(const std::string& path);
    Value load_config_file(const std::string& path, const Value& base_config = Value::object());
    
    // .env file handling
    std::unordered_map<std::string, std::string> parse_dotenv_file(const std::string& path);
    std::string find_dotenv(const std::string& start_dir = "");
    void load_dotenv_file(const std::string& path = "");
    
    // Environment variable manipulation
    void set_env_var(const std::string& name, const std::string& value);
    std::optional<std::string> get_env_var(const std::string& name);
    void unset_env_var(const std::string& name);
}
```

### load_json_file

```cpp
Value load_json_file(const std::string& path);
```

**Description:**  
Loads and parses a JSON configuration file.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `path` | `const std::string&` | Path to JSON file |

**Returns:**  
`Value` — Parsed JSON content.

**Throws:**
| Exception | Condition |
|-----------|-----------|
| `FileNotFoundError` | File doesn't exist |
| `ConfigParseError` | Invalid JSON syntax |

**Example:**
```cpp
confy::Value config = confy::load_json_file("config.json");
```

---

### load_toml_file

```cpp
Value load_toml_file(const std::string& path);
```

**Description:**  
Loads and parses a TOML configuration file.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `path` | `const std::string&` | Path to TOML file |

**Returns:**  
`Value` — Parsed TOML content as JSON-compatible Value.

**Throws:**
| Exception | Condition |
|-----------|-----------|
| `FileNotFoundError` | File doesn't exist |
| `ConfigParseError` | Invalid TOML syntax |

**Example:**
```cpp
confy::Value config = confy::load_toml_file("config.toml");
```

---

### load_config_file

```cpp
Value load_config_file(const std::string& path, const Value& base_config = Value::object());
```

**Description:**  
Loads a configuration file with format auto-detection and optional key promotion.

**Parameters:**
| Name | Type | Default | Description |
|------|------|---------|-------------|
| `path` | `const std::string&` | — | Path to config file |
| `base_config` | `const Value&` | `{}` | Base config for TOML key promotion |

**Returns:**  
`Value` — Parsed configuration.

**Throws:**
| Exception | Condition |
|-----------|-----------|
| `FileNotFoundError` | File doesn't exist |
| `ConfigParseError` | Invalid syntax |
| `ConfigError` | Unknown file extension |

**Format Detection:**
- `.json` → JSON parser
- `.toml` → TOML parser
- Other → Error

**TOML Key Promotion (F5):**  
When `base_config` is provided, keys in TOML sections that match root-level keys in `base_config` may be promoted to the root level.

**Example:**
```cpp
confy::Value defaults = {{"debug", false}};
confy::Value config = confy::load_config_file("config.toml", defaults);
```

---

### parse_dotenv_file

```cpp
std::unordered_map<std::string, std::string> parse_dotenv_file(const std::string& path);
```

**Description:**  
Parses a `.env` file without setting environment variables.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `path` | `const std::string&` | Path to .env file |

**Returns:**  
`std::unordered_map<std::string, std::string>` — Map of variable names to values.

**Supported Syntax:**
```bash
KEY=value
KEY="quoted value"
KEY='single quoted'
export KEY=value
# comment
```

**Example:**
```cpp
auto vars = confy::parse_dotenv_file(".env");
for (const auto& [name, value] : vars) {
    std::cout << name << "=" << value << std::endl;
}
```

---

### find_dotenv

```cpp
std::string find_dotenv(const std::string& start_dir = "");
```

**Description:**  
Searches for a `.env` file starting from the specified directory and walking up to parent directories.

**Parameters:**
| Name | Type | Default | Description |
|------|------|---------|-------------|
| `start_dir` | `const std::string&` | `""` | Starting directory (empty = current) |

**Returns:**  
`std::string` — Path to `.env` file, or empty string if not found.

**Example:**
```cpp
std::string env_path = confy::find_dotenv();
if (!env_path.empty()) {
    // Found .env file
}
```

---

### load_dotenv_file

```cpp
void load_dotenv_file(const std::string& path = "");
```

**Description:**  
Loads a `.env` file and sets environment variables. Does NOT override existing variables.

**Parameters:**
| Name | Type | Default | Description |
|------|------|---------|-------------|
| `path` | `const std::string&` | `""` | Path to .env file (empty = auto-search) |

**Behavior:**
- If `path` is empty, searches for `.env` in current/parent directories
- Variables are only set if they don't already exist in the environment
- Silently ignores if file not found

**Example:**
```cpp
confy::load_dotenv_file();                    // Auto-search
confy::load_dotenv_file(".env.local");        // Explicit path
```

---

### set_env_var

```cpp
void set_env_var(const std::string& name, const std::string& value);
```

**Description:**  
Sets an environment variable.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `name` | `const std::string&` | Variable name |
| `value` | `const std::string&` | Variable value |

**Platform Behavior:**
- POSIX: Uses `setenv()`
- Windows: Uses `SetEnvironmentVariableA()` / `_putenv_s()`

---

### get_env_var

```cpp
std::optional<std::string> get_env_var(const std::string& name);
```

**Description:**  
Gets an environment variable value.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `name` | `const std::string&` | Variable name |

**Returns:**  
`std::optional<std::string>` — Variable value, or `std::nullopt` if not set.

---

### unset_env_var

```cpp
void unset_env_var(const std::string& name);
```

**Description:**  
Removes an environment variable.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `name` | `const std::string&` | Variable name |

---

## 10. EnvMapper Module

```cpp
// Defined in <confy/EnvMapper.hpp>

namespace confy {
    // Environment collection
    std::unordered_map<std::string, std::string> collect_env_vars(
        const std::optional<std::string>& prefix);
    
    // Name transformation
    std::string transform_env_name(const std::string& env_name);
    std::string strip_prefix(const std::string& env_name, const std::string& prefix);
    
    // Remapping
    std::string remap_env_key(
        const std::string& transformed_key,
        const Value& base_structure);
    
    // High-level operations
    Value env_vars_to_nested(
        const std::unordered_map<std::string, std::string>& env_vars);
    
    Value remap_and_flatten_env_data(
        const Value& env_data,
        const Value& base_structure);
    
    Value load_env_vars(
        const std::optional<std::string>& prefix,
        const Value& base_structure);
}
```

### collect_env_vars

```cpp
std::unordered_map<std::string, std::string> collect_env_vars(
    const std::optional<std::string>& prefix);
```

**Description:**  
Collects environment variables matching the specified prefix.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `prefix` | `const std::optional<std::string>&` | Prefix filter |

**Returns:**  
`std::unordered_map<std::string, std::string>` — Map of matching variables.

**Behavior:**
| Prefix Value | Behavior |
|--------------|----------|
| `std::nullopt` | Returns empty map |
| `""` (empty) | Returns most vars (system vars excluded) |
| `"MYAPP"` | Returns only `MYAPP_*` variables |

**Example:**
```cpp
auto vars = confy::collect_env_vars("MYAPP");
// Returns: {"MYAPP_DATABASE_HOST": "localhost", ...}
```

---

### transform_env_name

```cpp
std::string transform_env_name(const std::string& env_name);
```

**Description:**  
Transforms an environment variable name to a dot-path.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `env_name` | `const std::string&` | Environment variable name (without prefix) |

**Returns:**  
`std::string` — Transformed dot-path.

**Transformation Rules:**
1. Convert to lowercase
2. Replace `__` with temporary marker
3. Replace `_` with `.`
4. Replace marker with `_`

**Examples:**
```cpp
confy::transform_env_name("DATABASE_HOST");        // "database.host"
confy::transform_env_name("FEATURE_FLAGS__BETA");  // "feature_flags.beta"
confy::transform_env_name("A__B_C");               // "a_b.c"
```

---

### strip_prefix

```cpp
std::string strip_prefix(const std::string& env_name, const std::string& prefix);
```

**Description:**  
Removes the prefix and following underscore from an environment variable name.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `env_name` | `const std::string&` | Full environment variable name |
| `prefix` | `const std::string&` | Prefix to strip |

**Returns:**  
`std::string` — Name with prefix removed.

**Example:**
```cpp
confy::strip_prefix("MYAPP_DATABASE_HOST", "MYAPP");  // "DATABASE_HOST"
```

---

### remap_env_key

```cpp
std::string remap_env_key(
    const std::string& transformed_key,
    const Value& base_structure);
```

**Description:**  
Remaps a transformed key to match the base configuration structure.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `transformed_key` | `const std::string&` | Key after `transform_env_name` |
| `base_structure` | `const Value&` | Base config for matching |

**Returns:**  
`std::string` — Remapped key, or original if no match.

**Example:**
```cpp
confy::Value base = {{"feature_flags", {{"beta", false}}}};

// Without base structure, FEATURE_FLAGS_BETA → feature.flags.beta
// With base structure, it remaps to → feature_flags.beta
std::string key = confy::remap_env_key("feature.flags.beta", base);
// key = "feature_flags.beta"
```

---

### env_vars_to_nested

```cpp
Value env_vars_to_nested(
    const std::unordered_map<std::string, std::string>& env_vars);
```

**Description:**  
Converts flat environment variables to a nested Value structure.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `env_vars` | `const std::unordered_map<std::string, std::string>&` | Environment variables |

**Returns:**  
`Value` — Nested configuration structure.

**Example:**
```cpp
std::unordered_map<std::string, std::string> vars = {
    {"database.host", "localhost"},
    {"database.port", "5432"}
};
confy::Value nested = confy::env_vars_to_nested(vars);
// nested = {"database": {"host": "localhost", "port": 5432}}
```

---

### remap_and_flatten_env_data

```cpp
Value remap_and_flatten_env_data(
    const Value& env_data,
    const Value& base_structure);
```

**Description:**  
Remaps environment-derived data against a base structure.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `env_data` | `const Value&` | Environment data (nested) |
| `base_structure` | `const Value&` | Base config for remapping |

**Returns:**  
`Value` — Remapped configuration.

---

### load_env_vars

```cpp
Value load_env_vars(
    const std::optional<std::string>& prefix,
    const Value& base_structure);
```

**Description:**  
High-level function that collects, transforms, and remaps environment variables.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `prefix` | `const std::optional<std::string>&` | Prefix filter |
| `base_structure` | `const Value&` | Base config for remapping |

**Returns:**  
`Value` — Configuration from environment variables.

**Example:**
```cpp
confy::Value defaults = {
    {"database", {{"host", "localhost"}}},
    {"feature_flags", {{"beta", false}}}
};
confy::Value env_config = confy::load_env_vars("MYAPP", defaults);
```

---

## 11. Utility Functions

```cpp
// Defined in <confy/Util.hpp>

namespace confy {
    std::vector<std::string> flatten_to_dotpaths(const Value& data, const std::string& prefix = "");
    Value overrides_dict_to_value(const std::unordered_map<std::string, Value>& overrides);
}
```

### flatten_to_dotpaths

```cpp
std::vector<std::string> flatten_to_dotpaths(const Value& data, const std::string& prefix = "");
```

**Description:**  
Flattens a nested configuration to a list of all dot-paths.

**Parameters:**
| Name | Type | Default | Description |
|------|------|---------|-------------|
| `data` | `const Value&` | — | Configuration to flatten |
| `prefix` | `const std::string&` | `""` | Path prefix |

**Returns:**  
`std::vector<std::string>` — List of all leaf paths.

**Example:**
```cpp
confy::Value data = {
    {"a", 1},
    {"b", {{"c", 2}, {"d", 3}}}
};
auto paths = confy::flatten_to_dotpaths(data);
// paths = {"a", "b.c", "b.d"}
```

---

### overrides_dict_to_value

```cpp
Value overrides_dict_to_value(const std::unordered_map<std::string, Value>& overrides);
```

**Description:**  
Converts a flat override map to a nested Value structure.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `overrides` | `const std::unordered_map<std::string, Value>&` | Flat override map |

**Returns:**  
`Value` — Nested configuration.

**Example:**
```cpp
std::unordered_map<std::string, confy::Value> overrides = {
    {"server.port", 9000},
    {"debug", true}
};
confy::Value nested = confy::overrides_dict_to_value(overrides);
// nested = {"server": {"port": 9000}, "debug": true}
```

---

## 12. Constants

### System Variable Prefixes

```cpp
// Defined in <confy/EnvMapper.hpp>

namespace confy {
    extern const std::vector<std::string> SYSTEM_VAR_PREFIXES;
}
```

**Description:**  
List of environment variable prefixes excluded when using empty prefix (`""`).

**Contains (120+ prefixes):**
```
PATH, HOME, USER, SHELL, TERM, LANG, LC_
PWD, OLDPWD, HOSTNAME, LOGNAME, DISPLAY
SSH_, GPG_, DBUS_, XDG_
JAVA_, PYTHON, PYTHON_, NODE_, NPM_, CONDA_, VIRTUAL_ENV
CMAKE_, CC, CXX, CFLAGS, CXXFLAGS, LDFLAGS
AWS_, GOOGLE_, GCP_, AZURE_, DOCKER_, KUBERNETES_
GITHUB_, GITLAB_, TRAVIS_, CI, JENKINS_
MAIL, EDITOR, VISUAL, PAGER
LS_COLORS, LESS, GREP_
... and more
```

---

## 13. Type Traits & Concepts

### Value Type Detection

```cpp
// Check if a type is convertible to/from Value
template<typename T>
constexpr bool is_json_compatible_v = /* implementation-defined */;
```

### Supported Conversion Types

| C++ Type | JSON Type | Notes |
|----------|-----------|-------|
| `bool` | boolean | |
| `int8_t`, `int16_t`, `int32_t`, `int64_t` | number (integer) | |
| `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t` | number (unsigned) | |
| `float`, `double` | number (floating) | |
| `std::string` | string | |
| `const char*` | string | |
| `std::nullptr_t` | null | |
| `std::vector<T>` | array | Where T is convertible |
| `std::array<T, N>` | array | Where T is convertible |
| `std::map<std::string, T>` | object | Where T is convertible |
| `std::unordered_map<std::string, T>` | object | Where T is convertible |

---

## Appendix A: Thread Safety

### Thread Safety Guarantees

| Operation | Thread Safety |
|-----------|---------------|
| `Config::load()` | Thread-safe (creates new instance) |
| `Config::get()` (const) | Thread-safe for concurrent reads |
| `Config::set()` | NOT thread-safe |
| `Config::merge()` | NOT thread-safe |
| `parse_value()` | Thread-safe (stateless) |
| `deep_merge()` | Thread-safe (creates new Value) |
| `load_json_file()` | Thread-safe (stateless) |
| `load_toml_file()` | Thread-safe (stateless) |
| `collect_env_vars()` | Thread-safe |
| `set_env_var()` | NOT thread-safe (modifies global state) |

### Recommended Patterns

```cpp
// Pattern 1: Load once, read many
confy::Config cfg = confy::Config::load(opts);  // Single-threaded
// After loading, concurrent reads are safe

// Pattern 2: Mutex for modifications
std::mutex cfg_mutex;
std::lock_guard<std::mutex> lock(cfg_mutex);
cfg.set("key", "value");

// Pattern 3: Reader-writer lock
std::shared_mutex cfg_mutex;
// Read: std::shared_lock<std::shared_mutex> lock(cfg_mutex);
// Write: std::unique_lock<std::shared_mutex> lock(cfg_mutex);
```

---

## Appendix B: Performance Characteristics

### Time Complexity

| Operation | Complexity | Notes |
|-----------|------------|-------|
| `get(path)` | O(d) | d = path depth |
| `set(path, value)` | O(d) | d = path depth |
| `contains(path)` | O(d) | d = path depth |
| `merge()` | O(n) | n = total keys |
| `to_json()` | O(n) | n = total elements |
| `to_toml()` | O(n) | n = total elements |
| `Config::load()` | O(n + e) | n = config size, e = env vars |

### Memory Usage

- `Value` uses `nlohmann::json` memory model
- Small string optimization for strings < 16 bytes (typical)
- Objects use `std::map` or `std::unordered_map` internally
- Arrays use `std::vector`

---

## Appendix C: Compiler Support

### Minimum Requirements

| Compiler | Version | Notes |
|----------|---------|-------|
| GCC | 11+ | Full C++17 support |
| Clang | 14+ | Full C++17 support |
| Apple Clang | 14+ | Xcode 14+ |
| MSVC | 2022 (19.30+) | Full C++17 support |

### Required Features

- `std::optional`
- `std::string_view`
- `std::filesystem`
- Structured bindings
- `if constexpr`
- Fold expressions

---

## Appendix D: Dependencies

| Library | Version | License | Purpose |
|---------|---------|---------|---------|
| nlohmann/json | 3.11.3 | MIT | JSON parsing, Value type |
| toml++ | 3.4.0 | MIT | TOML parsing |
| cxxopts | 3.2.0 | MIT | CLI argument parsing |
| GoogleTest | 1.14.0 | BSD-3 | Testing (dev only) |

---

## Appendix E: Error Codes Summary

| Exception | Code | Description |
|-----------|------|-------------|
| `MissingMandatoryConfig` | E001 | Required keys missing |
| `FileNotFoundError` | E002 | Config file not found |
| `ConfigParseError` | E003 | Syntax error in config |
| `KeyError` | E004 | Path segment not found |
| `TypeError` | E005 | Type mismatch in traversal |

---

*API Reference for confy-cpp v1.0.0*  
*Generated from source code and design specification.*
