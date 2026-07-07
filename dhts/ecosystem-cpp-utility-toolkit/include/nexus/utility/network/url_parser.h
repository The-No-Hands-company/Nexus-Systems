#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <optional>
#include <unordered_map>
#include <regex>
#include <algorithm>
#include <cctype>

namespace nexus::utility::network {

/**
 * @brief URL parser and encoder/decoder.
 */
class UrlParser {
public:
    struct ParsedUrl {
        std::string scheme;
        std::string host;
        int port = 0;
        std::string path;
        std::string query;
        std::string fragment;
        std::unordered_map<std::string, std::string> query_params;
    };

    /**
     * @brief Parses a URL string.
     */
    [[nodiscard]] static std::optional<ParsedUrl> parse(const std::string& url) {
        ParsedUrl result;
        std::string remaining = url;

        // Parse scheme
        size_t scheme_end = remaining.find("://");
        if (scheme_end != std::string::npos) {
            result.scheme = remaining.substr(0, scheme_end);
            remaining = remaining.substr(scheme_end + 3);
        }

        // Parse fragment
        size_t fragment_pos = remaining.find('#');
        if (fragment_pos != std::string::npos) {
            result.fragment = remaining.substr(fragment_pos + 1);
            remaining = remaining.substr(0, fragment_pos);
        }

        // Parse query
        size_t query_pos = remaining.find('?');
        if (query_pos != std::string::npos) {
            result.query = remaining.substr(query_pos + 1);
            result.query_params = parseQueryParams(result.query);
            remaining = remaining.substr(0, query_pos);
        }

        // Parse host and port
        size_t path_pos = remaining.find('/');
        std::string host_port;
        if (path_pos != std::string::npos) {
            host_port = remaining.substr(0, path_pos);
            result.path = remaining.substr(path_pos);
        } else {
            host_port = remaining;
            result.path = "/";
        }

        // Split host and port
        size_t port_pos = host_port.find(':');
        if (port_pos != std::string::npos) {
            result.host = host_port.substr(0, port_pos);
            try {
                result.port = std::stoi(host_port.substr(port_pos + 1));
            } catch (...) {
                return std::nullopt;
            }
        } else {
            result.host = host_port;
            // Default ports
            if (result.scheme == "http") result.port = 80;
            else if (result.scheme == "https") result.port = 443;
        }

        return result;
    }

    /**
     * @brief URL encodes a string.
     */
    [[nodiscard]] static std::string encode(const std::string& str) noexcept {
        std::ostringstream encoded;
        for (unsigned char c : str) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded << c;
            } else {
                encoded << '%' << std::hex << std::uppercase
                        << std::setw(2) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(c));
                encoded << std::dec << std::nouppercase;
            }
        }
        return encoded.str();
    }

    /**
     * @brief URL decodes a string.
     */
    [[nodiscard]] static std::string decode(const std::string& str) noexcept {
        std::ostringstream decoded;
        for (size_t i = 0; i < str.length(); ++i) {
            if (str[i] == '%' && i + 2 < str.length()) {
                int value;
                std::istringstream iss(str.substr(i + 1, 2));
                if (iss >> std::hex >> value) {
                    decoded << static_cast<char>(value);
                    i += 2;
                } else {
                    decoded << str[i];
                }
            } else if (str[i] == '+') {
                decoded << ' ';
            } else {
                decoded << str[i];
            }
        }
        return decoded.str();
    }

private:
    static std::unordered_map<std::string, std::string> parseQueryParams(const std::string& query) noexcept {
        std::unordered_map<std::string, std::string> params;
        std::istringstream iss(query);
        std::string pair;

        while (std::getline(iss, pair, '&')) {
            size_t eq_pos = pair.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = decode(pair.substr(0, eq_pos));
                std::string value = decode(pair.substr(eq_pos + 1));
                params[key] = value;
            }
        }

        return params;
    }
};

} // namespace nexus::utility::network
