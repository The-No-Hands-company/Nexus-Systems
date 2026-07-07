#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <any>
#include <iostream>
#include <type_traits>

namespace nexus::utility::cli {

/// @brief Modern type-safe command-line argument parser with chaining API.
///
/// Usage:
///   ArgParser parser("myapp");
///   parser.addFlag("-v", "--verbose", "Enable verbose output")
///         .addOption<int>("-p", "--port", "Port number", 8080)
///         .addOption<std::string>("-n", "--name", "Your name", "world");
///   parser.parse(argc, argv);
///   bool verbose = parser.get<bool>("--verbose");
///   int port = parser.get<int>("--port");
class ArgParser {
public:
    explicit ArgParser(std::string program_name, std::string description = "")
        : program_name_(std::move(program_name)), description_(std::move(description)) {}

    /// @brief Add a boolean flag (no value). Returns *this for chaining.
    ArgParser& addFlag(const std::string& short_name, const std::string& long_name,
                       const std::string& description) {
        addArg(short_name, long_name, description, true, false);
        return *this;
    }

    /// @brief Add a typed option with a default value. Returns *this for chaining.
    template <typename T>
    ArgParser& addOption(const std::string& short_name, const std::string& long_name,
                         const std::string& description, T default_value = T{}) {
        addArg(short_name, long_name, description, false, default_value);
        return *this;
    }

    /// @brief Parse argc/argv. Throws on unknown options or missing values.
    void parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                printHelp();
                std::exit(0);
            }

            if (arg.starts_with("--")) {
                parseLongOption(arg, argc, argv, i);
            } else if (arg.starts_with("-") && arg.size() > 1 && arg[1] != '-') {
                parseShortOption(arg, argc, argv, i);
            } else {
                positional_.push_back(arg);
            }
        }
    }

    /// @brief Get a parsed value by long name (e.g., "--port").
    /// Returns the parsed value or the registered default.
    /// Throws if the name is unknown.
    template <typename T>
    [[nodiscard]] T get(const std::string& raw_name) const {
        std::string name = normalizeName(raw_name);
        auto val_it = values_.find(name);
        if (val_it != values_.end()) {
            return convertValue<T>(val_it->second);
        }

        auto arg_it = short_to_long_.find(name);
        if (arg_it != short_to_long_.end()) {
            return get<T>(arg_it->second);
        }

        auto arg = findArg(name);
        if (arg) {
            return convertValue<T>(arg->default_value);
        }

        throw std::runtime_error("Unknown argument: " + name);
    }

    /// @brief Check if a flag was set on the command line.
    [[nodiscard]] bool isSet(const std::string& raw_name) const {
        std::string name = normalizeName(raw_name);
        auto val_it = values_.find(name);
        if (val_it != values_.end()) return true;
        auto arg_it = short_to_long_.find(name);
        return arg_it != short_to_long_.end() && values_.count(arg_it->second) > 0;
    }

    [[nodiscard]] const std::vector<std::string>& getPositional() const noexcept {
        return positional_;
    }

    [[nodiscard]] size_t positionalCount() const noexcept { return positional_.size(); }

    void printHelp() const {
        std::cout << program_name_;
        if (!description_.empty()) std::cout << " - " << description_;
        std::cout << "\n\nUsage: " << program_name_ << " [options] [arguments]\n\n";
        if (!arguments_.empty()) std::cout << "Options:\n";
        for (const auto& [long_name, arg] : arguments_) {
            std::cout << "  ";
            if (!arg.short_name.empty()) std::cout << arg.short_name << ", ";
            std::cout << long_name;
            if (!arg.is_flag) std::cout << " <value>";
            std::cout << "\n      " << arg.description;
            if (!arg.is_flag && arg.default_value.has_value()) {
                std::cout << " (default: " << anyToString(arg.default_value) << ")";
            }
            std::cout << "\n";
        }
    }

private:
    struct Argument {
        std::string short_name;
        std::string description;
        bool is_flag = false;
        std::any default_value;
    };

    void addArg(const std::string& short_name, const std::string& long_name,
                const std::string& description, bool is_flag, std::any default_val) {
        // Strip -- prefix from long name for storage
        std::string key = long_name.starts_with("--") ? long_name.substr(2) : long_name;
        Argument arg;
        arg.short_name = short_name;
        arg.description = description;
        arg.is_flag = is_flag;
        arg.default_value = std::move(default_val);
        arguments_[key] = std::move(arg);
        if (!short_name.empty()) {
            // Strip - prefix from short name
            std::string short_key = short_name.starts_with("-") ? short_name.substr(1) : short_name;
            short_to_long_[short_key] = key;
        }
    }

    [[nodiscard]] static std::string normalizeName(const std::string& name) noexcept {
        if (name.starts_with("--")) return name.substr(2);
        if (name.starts_with("-")) return name.substr(1);
        return name;
    }

    void parseLongOption(const std::string& arg, int argc, char* argv[], int& i) {
        std::string name = arg.substr(2);

        // Handle --name=value syntax
        std::string value;
        auto eq_pos = name.find('=');
        if (eq_pos != std::string::npos) {
            value = name.substr(eq_pos + 1);
            name = name.substr(0, eq_pos);
        }

        auto it = arguments_.find(name);
        if (it == arguments_.end()) {
            throw std::runtime_error("Unknown option: " + arg);
        }

        if (it->second.is_flag) {
            if (!value.empty()) {
                throw std::runtime_error("Flag does not accept a value: " + arg);
            }
            values_[name] = true;
        } else {
            if (value.empty()) {
                if (i + 1 >= argc) {
                    throw std::runtime_error("Option requires value: " + arg);
                }
                value = argv[++i];
            }
            values_[name] = value;
        }
    }

    void parseShortOption(const std::string& arg, int argc, char* argv[], int& i) {
        std::string short_name = arg.substr(1);
        auto it = short_to_long_.find(short_name);
        if (it == short_to_long_.end()) {
            throw std::runtime_error("Unknown option: " + arg);
        }

        const auto& arg_info = arguments_[it->second];
        if (arg_info.is_flag) {
            values_[it->second] = true;
            // Handle combined short flags like -vxf
            for (size_t j = 1; j < short_name.size(); ++j) {
                std::string single(1, short_name[j]);
                auto sit = short_to_long_.find(single);
                if (sit != short_to_long_.end() && arguments_[sit->second].is_flag) {
                    values_[sit->second] = true;
                }
            }
        } else {
            std::string value;
            if (arg.size() > 2) {
                // -p8080 style
                value = arg.substr(2);
            } else if (i + 1 < argc) {
                value = argv[++i];
            } else {
                throw std::runtime_error("Option requires value: " + arg);
            }
            values_[it->second] = value;
        }
    }

    [[nodiscard]] const Argument* findArg(const std::string& name) const {
        auto it = arguments_.find(name);
        if (it != arguments_.end()) return &it->second;
        auto sti = short_to_long_.find(name);
        if (sti != short_to_long_.end()) {
            auto it2 = arguments_.find(sti->second);
            if (it2 != arguments_.end()) return &it2->second;
        }
        return nullptr;
    }

    template <typename T>
    [[nodiscard]] static T convertValue(const std::any& val) {
        if (auto* p = std::any_cast<T>(&val)) {
            return *p;
        }
        if constexpr (std::is_same_v<T, std::string>) {
            if (auto* p = std::any_cast<const bool>(&val)) {
                return *p ? "true" : "false";
            }
        }
        if (auto* p = std::any_cast<const std::string>(&val)) {
            if constexpr (std::is_same_v<T, std::string>) {
                return *p;
            } else if constexpr (std::is_arithmetic_v<T>) {
                T result{};
                std::istringstream iss(*p);
                iss >> result;
                if (iss.fail()) {
                    throw std::runtime_error("Cannot convert value '" + *p + "' to requested type");
                }
                return result;
            }
        }
        throw std::runtime_error("Type conversion failed for argument");
    }

    [[nodiscard]] static std::string anyToString(const std::any& val) {
        if (auto* p = std::any_cast<const int>(&val)) return std::to_string(*p);
        if (auto* p = std::any_cast<const double>(&val)) return std::to_string(*p);
        if (auto* p = std::any_cast<const bool>(&val)) return *p ? "true" : "false";
        if (auto* p = std::any_cast<const std::string>(&val)) return *p;
        if (auto* p = std::any_cast<const char*>(&val)) return *p;
        return "<unknown>";
    }

    std::string program_name_;
    std::string description_;
    std::unordered_map<std::string, Argument> arguments_;     // long_name → Argument
    std::unordered_map<std::string, std::string> short_to_long_; // short → long
    std::unordered_map<std::string, std::any> values_;        // long_name → parsed value
    std::vector<std::string> positional_;
};

} // namespace nexus::utility::cli
