#ifndef CONFY_EXCEPTIONS_HPP
#define CONFY_EXCEPTIONS_HPP

#include <stdexcept>
#include <string>
#include <vector>
#include <sstream>

namespace confy {

/**
 * @brief Thrown when one or more mandatory keys are missing.
 *
 * The `what()` message lists the missing keys separated by commas.
 */
class MissingMandatoryConfig : public std::runtime_error {
public:
    explicit MissingMandatoryConfig(std::vector<std::string> keys)
        : std::runtime_error(build_message(keys)), missing_keys_(std::move(keys)) {}

    const std::vector<std::string>& missing_keys() const noexcept { return missing_keys_; }

private:
    static std::string build_message(const std::vector<std::string>& keys) {
        std::ostringstream oss;
        oss << "Missing mandatory configuration keys: ";
        for (size_t i = 0; i < keys.size(); ++i) {
            if (i) oss << ", ";
            oss << keys[i];
        }
        return oss.str();
    }
    std::vector<std::string> missing_keys_;
};

} // namespace confy

#endif // CONFY_EXCEPTIONS_HPP
