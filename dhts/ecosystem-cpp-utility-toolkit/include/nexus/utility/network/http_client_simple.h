#pragma once

#include <nexus/utility/network/socket_wrapper.h>
#include <nexus/utility/network/url_parser.h>
#include <string>
#include <unordered_map>
#include <sstream>
#include <optional>

namespace nexus::utility::network {

/**
 * @brief Simple HTTP/1.1 client (plain HTTP, no TLS).
 */
class HttpClientSimple {
public:
    struct Response {
        int status_code;
        std::string status_message;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
    };

    /**
     * @brief Performs an HTTP GET request.
     */
    [[nodiscard]] static std::optional<Response> get(const std::string& url) {
        auto parsed = UrlParser::parse(url);
        if (!parsed) {
            return std::nullopt;
        }

        if (parsed->scheme != "http") {
            return std::nullopt; // Only plain HTTP supported
        }

        try {
            SocketWrapper socket(SocketWrapper::Type::TCP);
            if (!socket.connect(parsed->host, parsed->port)) {
                return std::nullopt;
            }

            // Build HTTP request
            std::ostringstream request;
            request << "GET " << parsed->path;
            if (!parsed->query.empty()) {
                request << "?" << parsed->query;
            }
            request << " HTTP/1.1\r\n";
            request << "Host: " << parsed->host << "\r\n";
            request << "Connection: close\r\n";
            request << "\r\n";

            socket.send(request.str());
            return receiveResponse(socket);
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Performs an HTTP POST request.
     */
    [[nodiscard]] static std::optional<Response> post(const std::string& url, const std::string& body,
                                       const std::string& content_type = "application/x-www-form-urlencoded") {
        auto parsed = UrlParser::parse(url);
        if (!parsed || parsed->scheme != "http") {
            return std::nullopt;
        }

        try {
            SocketWrapper socket(SocketWrapper::Type::TCP);
            if (!socket.connect(parsed->host, parsed->port)) {
                return std::nullopt;
            }

            // Build HTTP request
            std::ostringstream request;
            request << "POST " << parsed->path << " HTTP/1.1\r\n";
            request << "Host: " << parsed->host << "\r\n";
            request << "Content-Type: " << content_type << "\r\n";
            request << "Content-Length: " << body.length() << "\r\n";
            request << "Connection: close\r\n";
            request << "\r\n";
            request << body;

            socket.send(request.str());
            return receiveResponse(socket);
        } catch (...) {
            return std::nullopt;
        }
    }

private:
    static std::optional<Response> receiveResponse(SocketWrapper& socket) {
        std::string raw_response;

        while (true) {
            auto result = socket.receive();
            if (!result.has_value()) {
                return std::nullopt;  // Error
            }
            if (result->empty()) {
                break;  // Clean EOF
            }
            raw_response += *result;
        }

        if (raw_response.empty()) {
            return std::nullopt;
        }

        return parseResponse(raw_response);
    }

    static std::optional<Response> parseResponse(const std::string& raw) {
        Response response;

        // Split headers and body
        size_t body_start = raw.find("\r\n\r\n");
        if (body_start == std::string::npos) {
            return std::nullopt;
        }

        std::string headers_section = raw.substr(0, body_start);
        response.body = raw.substr(body_start + 4);

        // Parse status line
        std::istringstream iss(headers_section);
        std::string status_line;
        std::getline(iss, status_line);

        std::istringstream status_iss(status_line);
        std::string http_version;
        status_iss >> http_version >> response.status_code;
        std::getline(status_iss, response.status_message);
        
        // Trim status message
        if (!response.status_message.empty() && response.status_message[0] == ' ') {
            response.status_message = response.status_message.substr(1);
        }
        if (!response.status_message.empty() && response.status_message.back() == '\r') {
            response.status_message.pop_back();
        }

        // Parse headers
        std::string header_line;
        while (std::getline(iss, header_line) && header_line != "\r") {
            size_t colon = header_line.find(':');
            if (colon != std::string::npos) {
                std::string key = header_line.substr(0, colon);
                std::string value = header_line.substr(colon + 1);
                
                // Trim whitespace
                if (!value.empty() && value[0] == ' ') {
                    value = value.substr(1);
                }
                if (!value.empty() && value.back() == '\r') {
                    value.pop_back();
                }
                
                response.headers[key] = value;
            }
        }

        return response;
    }
};

} // namespace nexus::utility::network
