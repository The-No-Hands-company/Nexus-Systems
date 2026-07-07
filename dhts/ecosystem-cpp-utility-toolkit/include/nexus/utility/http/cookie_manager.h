#pragma once

#include <string>
#include <string_view>
#include <map>
#include <vector>
#include <sstream>
#include <ctime>
#include <chrono>
#include <optional>

namespace nexus::utility::http {

/// Cookie parsing and management (RFC 6265)
class CookieManager {
public:
    struct Cookie {
        std::string name;
        std::string value;
        std::string domain;
        std::string path = "/";
        std::optional<std::chrono::system_clock::time_point> expires;
        int max_age = 0;
        bool secure = false;
        bool http_only = false;
        std::string same_site;  // "Strict", "Lax", "None"

        bool isExpired() const {
            if (expires) return std::chrono::system_clock::now() > *expires;
            return false;
        }
    };

    /// Parse a Set-Cookie header value
    static Cookie parseSetCookie(const std::string& header) {
        Cookie cookie;
        auto parts = split(header, ';');
        bool first = true;

        for (const auto& part : parts) {
            auto trimmed = trim(part);
            auto eq = trimmed.find('=');

            if (first) {
                if (eq != std::string::npos) {
                    cookie.name = trim(trimmed.substr(0, eq));
                    cookie.value = trim(trimmed.substr(eq + 1));
                }
                first = false;
                continue;
            }

            auto key = toLower(eq != std::string::npos ? trim(trimmed.substr(0, eq)) : trimmed);
            auto val = eq != std::string::npos ? trim(trimmed.substr(eq + 1)) : "";

            if (key == "domain") cookie.domain = val;
            else if (key == "path") cookie.path = val;
            else if (key == "secure") cookie.secure = true;
            else if (key == "httponly") cookie.http_only = true;
            else if (key == "max-age") cookie.max_age = std::stoi(val);
            else if (key == "samesite") cookie.same_site = val;
            else if (key == "expires") {
                // Simplified: store as max_age from now
                // In production, parse RFC 1123 date
                cookie.max_age = 3600; // 1 hour default
            }
        }

        return cookie;
    }

    /// Build a Cookie header value from stored cookies
    static std::string buildCookieHeader(const std::vector<Cookie>& cookies) {
        std::ostringstream oss;
        bool first = true;
        for (const auto& c : cookies) {
            if (c.isExpired()) continue;
            if (!first) oss << "; ";
            first = false;
            oss << c.name << '=' << c.value;
        }
        return oss.str();
    }

private:
    static std::vector<std::string> split(const std::string& s, char delim) {
        std::vector<std::string> result;
        size_t start = 0;
        while (start <= s.size()) {
            size_t end = s.find(delim, start);
            if (end == std::string::npos) end = s.size();
            result.push_back(s.substr(start, end - start));
            start = end + 1;
        }
        return result;
    }

    static std::string trim(const std::string& s) {
        size_t start = 0, end = s.size();
        while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
        while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
        return s.substr(start, end - start);
    }

    static std::string toLower(std::string s) {
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }
};

} // namespace nexus::utility::http
