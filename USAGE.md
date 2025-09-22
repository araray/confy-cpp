# confy-cpp – Usage

The CLI mirrors the Python `confy` tool’s ergonomics.

## Global options

```
confy-cpp [OPTIONS] COMMAND ...

Options:
  -c, --config <PATH>     Path to JSON or TOML file
  -p, --prefix <TEXT>     Env-var prefix for overrides (e.g., APP_CONF)
      --overrides <TEXT>  Comma-separated dot:key,JSON_value pairs
      --defaults <PATH>   Path to JSON file with defaults
      --mandatory <TEXT>  Comma-separated list of mandatory dot-keys
  -h, --help              Show help
```

## Subcommands

- `get <KEY>` – Print the JSON value of `KEY`
- `set <KEY> <VALUE>` – Set `KEY` to JSON-parsed `VALUE` in the source file
- `exists <KEY>` – Print `true`/`false`, exit 0/1
- `search [--key PAT] [--val PAT] [-i]` – Glob/regex/exact
- `dump` – Pretty-print merged config as JSON
- `convert [--to json|toml] [--out FILE]` – Convert merged config

### Examples

```bash
confy-cpp -c cfg.toml -p APP_CONF get db.host
confy-cpp -c cfg.toml set db.port 5432
confy-cpp -c cfg.toml exists db.host && echo "ok"
confy-cpp -c cfg.toml search --key 'auth.*' --val 'enabled' -i
confy-cpp -c cfg.json convert --to toml --out cfg.toml
```

## Notes

- Precedence is `defaults → file → env → overrides`.
- Environment overrides: with `--prefix APP_CONF`, `APP_CONF_DB_HOST=...` maps to `db.host` (lowercased; single underscores become dots).
- JSON parsing for values: numbers, booleans, arrays/objects are preserved when you supply valid JSON; otherwise values are treated as strings.
