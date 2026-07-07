#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <concepts>

namespace nexus::utility::string {

class StringUtils {
public:
    // --- Trimming ---

    static std::string trimLeft(std::string_view str) {
        auto it = std::find_if(str.begin(), str.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        });
        return std::string(it, str.end());
    }

    static std::string trimRight(std::string_view str) {
        auto it = std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        });
        return std::string(str.begin(), it.base());
    }

    static std::string trim(std::string_view str) {
        auto first = std::find_if(str.begin(), str.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        });
        if (first == str.end()) return ""; // All spaces

        auto last = std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        });
        return std::string(first, last.base());
    }

    // --- Splitting ---

    static std::vector<std::string> split(std::string_view str, std::string_view delimiter) {
        std::vector<std::string> tokens;
        if (delimiter.empty()) {
            tokens.emplace_back(str);
            return tokens;
        }

        size_t start = 0;
        size_t end = str.find(delimiter);
        while (end != std::string_view::npos) {
            tokens.emplace_back(str.substr(start, end - start));
            start = end + delimiter.length();
            end = str.find(delimiter, start);
        }
        tokens.emplace_back(str.substr(start));
        return tokens;
    }

    static std::vector<std::string> split(std::string_view str, char delimiter) {
        return split(str, std::string(1, delimiter));
    }

    // --- Joining ---
    
    template<typename Iterable>
    static std::string join(const Iterable& items, std::string_view delimiter) {
        std::ostringstream oss;
        auto it = std::begin(items);
        auto end = std::end(items);
        
        if (it != end) {
            oss << *it;
            ++it;
        }
        for (; it != end; ++it) {
            oss << delimiter << *it;
        }
        return oss.str();
    }

    // --- Inspection (Wrappers for convenience) ---

    static constexpr bool startsWith(std::string_view str, std::string_view prefix) {
        return str.starts_with(prefix); // C++20
    }

    static constexpr bool endsWith(std::string_view str, std::string_view suffix) {
        return str.ends_with(suffix); // C++20
    }

    static constexpr bool contains(std::string_view str, std::string_view sub) {
        return str.find(sub) != std::string_view::npos; // C++23 has contains() usually
    }

    // --- Case Conversion ---

    static std::string toUpper(std::string_view str) {
        std::string result(str);
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c){ 
            return std::toupper(c); 
        });
        return result;
    }

    static std::string toLower(std::string_view str) {
        std::string result(str);
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c){ 
            return std::tolower(c); 
        });
        return result;
    }
};

} // namespace nexus::utility::string
