/**
 * @file Config.hpp
 * @brief Main configuration class (Phase 3)
 *
 * This is a stub header. Full implementation will be added in Phase 3.
 */

#ifndef CONFY_CONFIG_HPP
#define CONFY_CONFIG_HPP

#include "confy/Value.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace confy {

/**
 * @brief Configuration loading options (Phase 3)
 */
struct LoadOptions {
    std::string file_path;
    std::optional<std::string> prefix = std::nullopt;
    bool load_dotenv_file = true;
    std::string dotenv_path;
    Value defaults = Value::object();
    std::unordered_map<std::string, Value> overrides;
    std::vector<std::string> mandatory;
};

/**
 * @brief Configuration container (Phase 3)
 */
class Config {
public:
    // Phase 3: Full API implementation

private:
    Value data_;
};

} // namespace confy

#endif // CONFY_CONFIG_HPP
