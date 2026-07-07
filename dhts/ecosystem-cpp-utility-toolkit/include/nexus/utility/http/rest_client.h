#pragma once

#include <string>
#include <string_view>
#include <map>
#include <vector>
#include <functional>
#include <optional>
#include <sstream>
#include "nexus/utility/http/http_client.h"

namespace nexus::utility::http {

/// REST API client built on top of HttpClient with resource-oriented interface
class RestClient {
public:
    explicit RestClient(const std::string& base_url) : base_url_(base_url) {
        // Remove trailing slash
        if (base_url_.ends_with('/')) base_url_.pop_back();
    }

    /// Set authentication
    void setAuthToken(const std::string& token) { auth_token_ = token; }
    void setApiKey(const std::string& key, const std::string& value) { api_key_ = {key, value}; }

    /// CRUD operations on resources
    struct ResourceResponse {
        int status = 0;
        std::string body;
        std::map<std::string, std::string> headers;

        bool ok() const { return status >= 200 && status < 300; }
    };

    /// GET a resource or collection
    ResourceResponse get(const std::string& path,
                          const std::map<std::string, std::string>& params = {}) {
        auto url = buildUrl(path, params);
        auto resp = http_.get(url);
        applyAuth();
        return {resp.status_code, resp.body, resp.headers};
    }

    /// POST to create a resource
    ResourceResponse post(const std::string& path, const std::string& body,
                           const std::string& content_type = "application/json") {
        auto url = fullUrl(path);
        auto resp = http_.post(url, body, content_type);
        applyAuth();
        return {resp.status_code, resp.body, resp.headers};
    }

    /// PUT to update a resource
    ResourceResponse put(const std::string& path, const std::string& body,
                          const std::string& content_type = "application/json") {
        auto url = fullUrl(path);
        auto resp = http_.put(url, body, content_type);
        applyAuth();
        return {resp.status_code, resp.body, resp.headers};
    }

    /// DELETE a resource
    ResourceResponse del(const std::string& path) {
        auto url = fullUrl(path);
        auto resp = http_.del(url);
        applyAuth();
        return {resp.status_code, resp.body, resp.headers};
    }

    /// PATCH a resource
    ResourceResponse patch(const std::string& path, const std::string& body) {
        auto url = fullUrl(path);
        HttpClient::Request req{HttpClient::Method::PATCH, url};
        req.body = body;
        req.headers["Content-Type"] = "application/json";
        auto resp = http_.execute(req);
        applyAuth();
        return {resp.status_code, resp.body, resp.headers};
    }

    /// Paginated GET (for list endpoints)
    struct PaginatedResponse {
        std::vector<std::string> items;
        std::optional<std::string> next_page_token;
        int total_count = 0;
    };

    /// Get the underlying HTTP client for custom configuration
    HttpClient& httpClient() { return http_; }

private:
    std::string base_url_;
    HttpClient http_;
    std::string auth_token_;
    std::optional<std::pair<std::string, std::string>> api_key_;

    std::string fullUrl(const std::string& path) const {
        if (path.starts_with('/')) return base_url_ + path;
        return base_url_ + "/" + path;
    }

    std::string buildUrl(const std::string& path,
                          const std::map<std::string, std::string>& params) const {
        std::string base = fullUrl(path);
        if (params.empty()) return base;
        std::ostringstream oss;
        oss << base;
        bool first = base.find('?') == std::string::npos;
        for (const auto& [k, v] : params) {
            oss << (first ? '?' : '&') << encode(k) << '=' << encode(v);
            first = false;
        }
        return oss.str();
    }

    void applyAuth() {
        if (!auth_token_.empty()) {
            http_.setBearerToken(auth_token_);
        }
        if (api_key_) {
            http_.setHeader(api_key_->first, api_key_->second);
        }
    }

    static std::string encode(const std::string& s) {
        static const char* hex = "0123456789ABCDEF";
        std::string r;
        for (char c : s) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~')
                r += c;
            else { r += '%'; r += hex[(c>>4)&0x0F]; r += hex[c&0x0F]; }
        }
        return r;
    }
};

} // namespace nexus::utility::http
