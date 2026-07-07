#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdlib>
#include <sstream>

namespace nexus::utility::system {

/**
 * @brief Environment variable management utilities.
 */
class EnvironmentVars {
public:
    /**
     * @brief Gets an environment variable value.
     */
    static std::optional<std::string> get(const std::string& name) {
        const char* value = std::getenv(name.c_str());
        if (value) {
            return std::string(value);
        }
        return std::nullopt;
    }

    /**
     * @brief Sets an environment variable.
     */
    static bool set(const std::string& name, const std::string& value, bool overwrite = true) {
        return setenv(name.c_str(), value.c_str(), overwrite ? 1 : 0) == 0;
    }

    /**
     * @brief Unsets an environment variable.
     */
    static bool unset(const std::string& name) {
        return unsetenv(name.c_str()) == 0;
    }

    /**
     * @brief Checks if an environment variable exists.
     */
    static bool has(const std::string& name) {
        return std::getenv(name.c_str()) != nullptr;
    }

    /**
     * @brief Parses a PATH-like variable into components.
     */
    static std::vector<std::string> parsePath(const std::string& name, char delimiter = ':') {
        auto value = get(name);
        if (!value) {
            return {};
        }

        std::vector<std::string> paths;
        std::istringstream iss(*value);
        std::string path;
        
        while (std::getline(iss, path, delimiter)) {
            if (!path.empty()) {
                paths.push_back(path);
            }
        }
        
        return paths;
    }

    /**
     * @brief RAII-based temporary environment variable change.
     */
    class ScopedEnv {
    public:
        ScopedEnv(const std::string& name, const std::string& value) 
            : name_(name), had_value_(EnvironmentVars::has(name)) {
            if (had_value_) {
                old_value_ = *EnvironmentVars::get(name);
            }
            EnvironmentVars::set(name, value);
        }

        ~ScopedEnv() {
            if (had_value_) {
                EnvironmentVars::set(name_, old_value_);
            } else {
                EnvironmentVars::unset(name_);
            }
        }

        // Non-copyable, non-movable
        ScopedEnv(const ScopedEnv&) = delete;
        ScopedEnv& operator=(const ScopedEnv&) = delete;

    private:
        std::string name_;
        bool had_value_;
        std::string old_value_;
    };
};

} // namespace nexus::utility::system
