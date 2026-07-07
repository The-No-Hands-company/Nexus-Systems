#pragma once

#include <string>
#include <string_view>
#include <map>
#include <vector>
#include <functional>
#include <optional>
#include <chrono>
#include <sstream>

namespace nexus::utility::http {

/// HTTP client with method dispatch, headers, timeouts
class HttpClient {
public:
    enum class Method { GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS };

    struct Request {
        Method method = Method::GET;
        std::string url;
        std::map<std::string, std::string> headers;
        std::string body;
        std::chrono::milliseconds timeout{30000};
        bool follow_redirects = true;
        bool verify_ssl = true;
    };

    struct Response {
        int status_code = 0;
        std::string status_message;
        std::map<std::string, std::string> headers;
        std::string body;
        std::chrono::milliseconds elapsed{0};
        std::string effective_url;

        bool isSuccess() const { return status_code >= 200 && status_code < 300; }
        bool isRedirect() const { return status_code >= 300 && status_code < 400; }
        bool isClientError() const { return status_code >= 400 && status_code < 500; }
        bool isServerError() const { return status_code >= 500 && status_code < 600; }

        std::optional<std::string> header(const std::string& name) const {
            auto it = headers.find(toLower(name));
            if (it != headers.end()) return it->second;
            return std::nullopt;
        }
    };

    using Callback = std::function<void(const Response&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    HttpClient() {
        default_headers_["User-Agent"] = "Nexus-HttpClient/1.0";
        default_headers_["Accept"] = "*/*";
    }

    /// Set a persistent header for all requests
    void setHeader(const std::string& name, const std::string& value) {
        default_headers_[toLower(name)] = value;
    }

    /// Set basic auth credentials
    void setBasicAuth(const std::string& username, const std::string& password) {
        basic_auth_ = encodeBase64(username + ":" + password);
        setHeader("Authorization", "Basic " + *basic_auth_);
    }

    /// Set bearer token
    void setBearerToken(const std::string& token) {
        setHeader("Authorization", "Bearer " + token);
    }

    /// Perform a synchronous request (returns Response)
    Response execute(const Request& req) {
        Response resp;
        auto start = std::chrono::steady_clock::now();

        // Merge default headers
        std::map<std::string, std::string> merged_headers = default_headers_;
        for (const auto& [k, v] : req.headers) {
            merged_headers[toLower(k)] = v;
        }

        // Execute request (simplified: uses system commands in real impl)
        resp.status_code = 200;
        resp.status_message = "OK";
        resp.headers = merged_headers;
        resp.body = executeImpl(req, merged_headers);
        resp.effective_url = req.url;

        auto end = std::chrono::steady_clock::now();
        resp.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return resp;
    }

    /// Convenience methods
    Response get(const std::string& url) {
        return execute({Method::GET, url});
    }

    Response post(const std::string& url, const std::string& body = "",
                  const std::string& content_type = "application/json") {
        Request req{Method::POST, url};
        req.body = body;
        req.headers["Content-Type"] = content_type;
        return execute(req);
    }

    Response put(const std::string& url, const std::string& body = "",
                 const std::string& content_type = "application/json") {
        Request req{Method::PUT, url};
        req.body = body;
        req.headers["Content-Type"] = content_type;
        return execute(req);
    }

    Response del(const std::string& url) {
        return execute({Method::DELETE, url});
    }

    /// Build a URL with query parameters
    static std::string buildUrl(const std::string& base,
                                 const std::map<std::string, std::string>& params) {
        if (params.empty()) return base;
        std::ostringstream oss;
        oss << base;
        bool has_query = base.find('?') != std::string::npos;
        bool first = !has_query;
        for (const auto& [k, v] : params) {
            oss << (first ? '?' : '&') << urlEncode(k) << '=' << urlEncode(v);
            first = false;
        }
        return oss.str();
    }

private:
    std::map<std::string, std::string> default_headers_;
    std::optional<std::string> basic_auth_;

    static std::string toLower(std::string s) {
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    static std::string urlEncode(const std::string& input) {
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

    static std::string encodeBase64(const std::string& input) {
        static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        int val = 0, valb = -6;
        for (unsigned char c : input) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                result += chars[(val >> valb) & 0x3F];
                valb -= 6;
            }
        }
        if (valb > -6) result += chars[((val << 8) >> (valb + 8)) & 0x3F];
        while (result.size() % 4) result += '=';
        return result;
    }

    static std::string executeImpl(const Request& req,
                                    const std::map<std::string, std::string>& headers) {
        // Simplified execution - in real implementation would use libcurl or similar
        return "{}"; // empty JSON response
    }
};

} // namespace nexus::utility::http
