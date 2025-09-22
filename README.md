# confy-cpp

A C++ port of **confy**: a minimal configuration library + CLI that merges **defaults → file → environment variables → overrides** and offers **dot-notation** access for nested keys. It mirrors the Python package’s behavior and CLI as closely as practical in C++ (see `USAGE.md`).

> Primary goals: JSON & TOML support, dot-path `get`/`set`, env-var overrides with prefix, mandatory key enforcement, and a handy CLI (`confy-cpp`).

## Features

- **File formats**: JSON & TOML (read/write)
- **Dot-notation** for deep reads (`get`) and writes (`set`)
- **Layered precedence**: `defaults → file → env (with prefix) → overrides`
- **Mandatory keys**: fail fast when required keys are absent
- **CLI**: `get`, `set`, `exists`, `search`, `dump`, `convert`

## Build

This project uses CMake and fetches header-only dependencies automatically:

- [nlohmann/json]
- [toml++]
- [cxxopts] (CLI)
- [Catch2] (tests)

```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
ctest --output-on-failure
```

The CLI binary will be at `build/bin/confy-cpp` (or `confy-cpp.exe` on Windows).

## Quickstart

```cpp
#include <confy/Config.hpp>
#include <iostream>

int main() {
    confy::LoadOptions opts;
    opts.file_path = "config.toml";
    opts.prefix    = "APP_CONF";  // use APP_CONF_* vars
    opts.defaults  = nlohmann::json{{"db", {{"host","localhost"}, {"port", 3306}}}};
    opts.mandatory = {"db.host", "db.port"};
    opts.overrides = {{"db.port", 5432}};  // final precedence

    auto cfg = confy::Config::load(opts);
    std::cout << cfg.get<std::string>("db.host", "missing") << "\n"; // prints resolved host
}
```

## CLI examples

```bash
# Inspect
confy-cpp -c config.toml -p APP_CONF get db.port
confy-cpp -c config.toml dump

# Edit in-place
confy-cpp -c config.toml set db.port 5432

# Search
confy-cpp -c config.toml search --key "db.*" -i

# Convert
confy-cpp -c config.json convert --to toml --out config.toml
```

See **USAGE.md** for all commands and options.

## License

MIT (see `LICENSE`).
