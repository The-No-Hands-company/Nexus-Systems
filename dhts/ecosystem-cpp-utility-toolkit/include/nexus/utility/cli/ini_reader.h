#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <optional>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace nexus::utility::cli {

/// @brief INI-style configuration file reader/writer with typed access.
///
/// Supports [sections], key=value pairs, comments (# and ;), and typed
/// access (.get<int>(), .get<bool>(), etc.).
///
/// Usage:
///   IniReader ini;
///   ini.load("config.ini");
///   int port = ini.get<int>("server", "port", 8080);
///   std::string host = ini.get<std::string>("server", "host", "localhost");
class IniReader {
public:
    using Section = std::unordered_map<std::string, std::string>;

    IniReader() = default;

    /// @brief Load an INI file from disk
    bool load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        return parseStream(file);
    }

    /// @brief Parse INI content from a string
    bool parse(std::string_view content) {
        std::istringstream stream{std::string(content)};
        return parseStream(stream);
    }

    /// @brief Save to an INI file
    bool save(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        // Write global section first
        auto global_it = sections_.find("");
        if (global_it != sections_.end() && !global_it->second.empty()) {
            for (const auto& [key, value] : global_it->second) {
                file << key << " = " << value << "\n";
            }
            file << "\n";
        }

        for (const auto& [section_name, section] : sections_) {
            if (section_name.empty()) continue;
            file << "[" << section_name << "]\n";
            for (const auto& [key, value] : section) {
                file << key << " = " << value << "\n";
            }
            file << "\n";
        }
        return true;
    }

    // ── Typed Access ───────────────────────────────────────────────────────

    /// @brief Get a value from a section, with type conversion and default
    template <typename T>
    [[nodiscard]] T get(const std::string& section, const std::string& key,
                        T default_value = T{}) const {
        auto raw = getRaw(section, key);
        if (!raw.has_value()) return default_value;
        return convert<T>(*raw);
    }

    /// @brief Check if a key exists in a section
    [[nodiscard]] bool hasKey(const std::string& section, const std::string& key) const {
        auto it = sections_.find(section);
        if (it == sections_.end()) return false;
        return it->second.find(key) != it->second.end();
    }

    /// @brief Check if a section exists
    [[nodiscard]] bool hasSection(const std::string& section) const {
        return sections_.find(section) != sections_.end();
    }

    // ── Raw Access ─────────────────────────────────────────────────────────

    /// @brief Get raw string value
    [[nodiscard]] std::optional<std::string> getRaw(const std::string& section,
                                                     const std::string& key) const {
        auto it = sections_.find(section);
        if (it == sections_.end()) return std::nullopt;
        auto kit = it->second.find(key);
        if (kit == it->second.end()) return std::nullopt;
        return kit->second;
    }

    /// @brief Set a raw value
    void setRaw(const std::string& section, const std::string& key, std::string value) {
        sections_[section][key] = std::move(value);
    }

    /// @brief Set a typed value
    template <typename T>
    void set(const std::string& section, const std::string& key, const T& value) {
        sections_[section][key] = toString(value);
    }

    /// @brief Get all section names
    [[nodiscard]] std::vector<std::string> getSections() const {
        std::vector<std::string> names;
        names.reserve(sections_.size());
        for (const auto& [name, _] : sections_) {
            if (!name.empty()) names.push_back(name);
        }
        return names;
    }

    /// @brief Get all keys in a section
    [[nodiscard]] std::vector<std::string> getKeys(const std::string& section) const {
        std::vector<std::string> keys;
        auto it = sections_.find(section);
        if (it != sections_.end()) {
            keys.reserve(it->second.size());
            for (const auto& [key, _] : it->second) {
                keys.push_back(key);
            }
        }
        return keys;
    }

    /// @brief Clear all sections
    void clear() noexcept { sections_.clear(); }

    /// @brief Get section count
    [[nodiscard]] size_t sectionCount() const noexcept { return sections_.size(); }

private:
    std::unordered_map<std::string, Section> sections_;

    bool parseStream(std::istream& stream) {
        std::string line;
        std::string current_section;

        while (std::getline(stream, line)) {
            line = trim(line);

            if (line.empty() || line[0] == '#' || line[0] == ';') {
                continue;  // Skip comments and empty lines
            }

            if (line[0] == '[' && line.back() == ']') {
                current_section = line.substr(1, line.size() - 2);
                continue;
            }

            auto eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = trim(line.substr(0, eq_pos));
                std::string value = trim(line.substr(eq_pos + 1));
                sections_[current_section][key] = value;
            }
        }

        return true;
    }

    [[nodiscard]] static std::string trim(std::string s) {
        auto start = std::find_if_not(s.begin(), s.end(), [](unsigned char c) {
            return std::isspace(c);
        });
        auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) {
            return std::isspace(c);
        }).base();
        return start < end ? std::string(start, end) : std::string{};
    }

    template <typename T>
    [[nodiscard]] static T convert(const std::string& s) {
        if constexpr (std::is_same_v<T, std::string>) {
            return s;
        } else if constexpr (std::is_same_v<T, bool>) {
            std::string lower = s;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return lower == "true" || lower == "yes" || lower == "1" || lower == "on";
        } else if constexpr (std::is_arithmetic_v<T>) {
            T result{};
            std::istringstream iss(s);
            iss >> result;
            return result;
        }
        return T{};
    }

    template <typename T>
    [[nodiscard]] static std::string toString(const T& value) {
        if constexpr (std::is_same_v<T, std::string>) {
            return value;
        } else if constexpr (std::is_same_v<T, bool>) {
            return value ? "true" : "false";
        } else if constexpr (std::is_arithmetic_v<T>) {
            return std::to_string(value);
        }
        return {};
    }
};

} // namespace nexus::utility::cli
