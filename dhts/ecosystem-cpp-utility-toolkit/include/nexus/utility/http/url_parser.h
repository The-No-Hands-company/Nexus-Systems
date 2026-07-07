#pragma once

#include <string>
#include <string_view>
#include <map>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <cstdint>

namespace nexus::utility::http {

/// URL parser and builder with full RFC 3986 support
class UrlParser {
public:
    struct Url {
        std::string scheme;
        std::string userinfo;
        std::string host;
        int port = 0;
        std::string path;
        std::string query;
        std::string fragment;

        std::string toString() const {
            std::ostringstream oss;
            if (!scheme.empty()) oss << scheme << "://";
            if (!userinfo.empty()) oss << userinfo << '@';
            oss << host;
            if (port > 0 && !isDefaultPort()) oss << ':' << port;
            if (!path.empty()) {
                if (path[0] != '/') oss << '/';
                oss << path;
            } else {
                oss << '/';
            }
            if (!query.empty()) oss << '?' << query;
            if (!fragment.empty()) oss << '#' << fragment;
            return oss.str();
        }

        std::string hostPort() const {
            if (port > 0 && !isDefaultPort()) return host + ":" + std::to_string(port);
            return host;
        }

    private:
        bool isDefaultPort() const {
            return (scheme == "http" && port == 80) ||
                   (scheme == "https" && port == 443);
        }
    };

    static Url parse(std::string_view url_str) {
        Url url;
        auto sv = url_str;
        size_t pos = 0;

        // Scheme
        auto scheme_end = sv.find("://");
        if (scheme_end != std::string_view::npos) {
            url.scheme = sv.substr(0, scheme_end);
            pos = scheme_end + 3;
        }

        // Authority (userinfo@host:port)
        auto path_start = sv.find('/', pos);
        auto query_start = sv.find('?', pos);
        auto frag_start = sv.find('#', pos);

        size_t authority_end = sv.size();
        if (path_start != std::string_view::npos) authority_end = path_start;
        if (query_start != std::string_view::npos && query_start < authority_end) authority_end = query_start;
        if (frag_start != std::string_view::npos && frag_start < authority_end) authority_end = frag_start;

        std::string_view authority = sv.substr(pos, authority_end - pos);

        // Parse authority
        auto at_pos = authority.find('@');
        if (at_pos != std::string_view::npos) {
            url.userinfo = authority.substr(0, at_pos);
            authority = authority.substr(at_pos + 1);
        }

        auto colon_pos = authority.rfind(':');
        if (colon_pos != std::string_view::npos && colon_pos > authority.rfind(']')) {
            // Check if it's IPv6 bracket
            size_t bracket_end = authority.find(']');
            bool is_ipv6 = bracket_end != std::string_view::npos;
            if (!is_ipv6 || colon_pos > bracket_end) {
                url.host = authority.substr(0, colon_pos);
                auto port_sv = authority.substr(colon_pos + 1);
                url.port = std::stoi(std::string(port_sv));
            } else {
                url.host = authority;
            }
        } else {
            url.host = authority;
        }

        // Remove brackets from IPv6 host
        if (url.host.starts_with('[') && url.host.ends_with(']')) {
            url.host = url.host.substr(1, url.host.size() - 2);
        }

        // Path
        if (path_start != std::string_view::npos) {
            size_t path_end = sv.size();
            if (query_start != std::string_view::npos && query_start < path_end) path_end = query_start;
            if (frag_start != std::string_view::npos && frag_start < path_end) path_end = frag_start;
            url.path = sv.substr(path_start, path_end - path_start);
        }

        // Query
        if (query_start != std::string_view::npos) {
            size_t query_end = sv.size();
            if (frag_start != std::string_view::npos && frag_start < query_end) query_end = frag_start;
            url.query = sv.substr(query_start + 1, query_end - query_start - 1);
        }

        // Fragment
        if (frag_start != std::string_view::npos) {
            url.fragment = sv.substr(frag_start + 1);
        }

        return url;
    }

    /// Parse query string into key-value pairs
    static std::map<std::string, std::string> parseQuery(std::string_view query) {
        std::map<std::string, std::string> params;
        while (!query.empty()) {
            auto amp = query.find('&');
            auto pair = query.substr(0, amp);
            auto eq = pair.find('=');
            if (eq != std::string_view::npos) {
                params[std::string(percentDecode(pair.substr(0, eq)))] =
                    std::string(percentDecode(pair.substr(eq + 1)));
            } else {
                params[std::string(percentDecode(pair))] = "";
            }
            if (amp == std::string_view::npos) break;
            query = query.substr(amp + 1);
        }
        return params;
    }

    /// Build a query string from key-value pairs
    static std::string buildQuery(const std::map<std::string, std::string>& params) {
        std::ostringstream oss;
        bool first = true;
        for (const auto& [k, v] : params) {
            if (!first) oss << '&';
            first = false;
            oss << percentEncode(k) << '=' << percentEncode(v);
        }
        return oss.str();
    }

    /// Percent-encode a string
    static std::string percentEncode(std::string_view input) {
        static const char* hex = "0123456789ABCDEF";
        std::string result;
        for (char c : input) {
            if (std::isalnum(static_cast<unsigned char>(c)) ||
                c == '-' || c == '_' || c == '.' || c == '~') {
                result += c;
            } else {
                result += '%';
                result += hex[(c >> 4) & 0x0F];
                result += hex[c & 0x0F];
            }
        }
        return result;
    }

    /// Percent-decode a string
    static std::string percentDecode(std::string_view input) {
        std::string result;
        for (size_t i = 0; i < input.size(); ++i) {
            if (input[i] == '%' && i + 2 < input.size()) {
                auto hex = input.substr(i + 1, 2);
                result += static_cast<char>(std::stoi(std::string(hex), nullptr, 16));
                i += 2;
            } else if (input[i] == '+') {
                result += ' ';
            } else {
                result += input[i];
            }
        }
        return result;
    }
};

} // namespace nexus::utility::http
