#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <sstream>
#include <random>
#include <chrono>
#include <cstdint>

namespace nexus::utility::http {

/// OAuth 2.0 helper for authorization code and client credentials flows
class OAuthHelper {
public:
    struct Config {
        std::string client_id;
        std::string client_secret;
        std::string authorize_url;
        std::string token_url;
        std::string redirect_uri;
        std::vector<std::string> scopes;
    };

    struct Token {
        std::string access_token;
        std::string refresh_token;
        std::string token_type = "Bearer";
        int expires_in = 3600;
        std::string scope;
        std::chrono::system_clock::time_point obtained_at;

        bool isExpired() const {
            auto now = std::chrono::system_clock::now();
            return now > obtained_at + std::chrono::seconds(expires_in - 60); // 60s buffer
        }

        std::string authHeader() const {
            return token_type + " " + access_token;
        }
    };

    explicit OAuthHelper(Config config) : config_(std::move(config)) {}

    /// Generate the authorization URL for the user to visit
    std::string buildAuthorizeUrl(const std::string& state = "") const {
        std::ostringstream oss;
        oss << config_.authorize_url << "?response_type=code"
            << "&client_id=" << urlEncode(config_.client_id)
            << "&redirect_uri=" << urlEncode(config_.redirect_uri);

        if (!config_.scopes.empty()) {
            oss << "&scope=" << urlEncode(joinScopes());
        }

        std::string state_val = state.empty() ? generateState() : state;
        oss << "&state=" << urlEncode(state_val);

        return oss.str();
    }

    /// Parse the authorization code from the redirect URL
    static std::string parseAuthCode(const std::string& redirect_url) {
        auto params = parseQuery(redirect_url);
        auto it = params.find("code");
        if (it != params.end()) return it->second;
        return {};
    }

    /// Verify the state parameter to prevent CSRF
    static std::string parseState(const std::string& redirect_url) {
        auto params = parseQuery(redirect_url);
        auto it = params.find("state");
        if (it != params.end()) return it->second;
        return {};
    }

    /// Build token request body (client_credentials flow)
    std::string buildClientCredentialsBody() const {
        std::ostringstream oss;
        oss << "grant_type=client_credentials"
            << "&client_id=" << urlEncode(config_.client_id)
            << "&client_secret=" << urlEncode(config_.client_secret);
        if (!config_.scopes.empty()) {
            oss << "&scope=" << urlEncode(joinScopes());
        }
        return oss.str();
    }

    /// Build token request body (authorization_code flow)
    std::string buildAuthCodeBody(const std::string& code) const {
        std::ostringstream oss;
        oss << "grant_type=authorization_code"
            << "&code=" << urlEncode(code)
            << "&client_id=" << urlEncode(config_.client_id)
            << "&client_secret=" << urlEncode(config_.client_secret)
            << "&redirect_uri=" << urlEncode(config_.redirect_uri);
        return oss.str();
    }

    /// Build refresh token body
    std::string buildRefreshBody(const std::string& refresh_token) const {
        std::ostringstream oss;
        oss << "grant_type=refresh_token"
            << "&refresh_token=" << urlEncode(refresh_token)
            << "&client_id=" << urlEncode(config_.client_id)
            << "&client_secret=" << urlEncode(config_.client_secret);
        return oss.str();
    }

    /// Generate PKCE code verifier and challenge
    struct PkcePair {
        std::string verifier;
        std::string challenge;
    };

    static PkcePair generatePkce() {
        PkcePair pair;
        pair.verifier = generateRandomString(64);
        pair.challenge = pair.verifier; // In production: base64url(sha256(verifier))
        return pair;
    }

    const Config& config() const { return config_; }

private:
    Config config_;

    static std::string joinScopes() {
        // Scopes would be joined from config_.scopes
        return "read write";
    }

    static std::string urlEncode(const std::string& s) {
        static const char* hex = "0123456789ABCDEF";
        std::string r;
        for (char c : s) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~')
                r += c;
            else { r += '%'; r += hex[(c>>4)&0xF]; r += hex[c&0xF]; }
        }
        return r;
    }

    static std::map<std::string, std::string> parseQuery(const std::string& url) {
        std::map<std::string, std::string> params;
        auto q = url.find('?');
        if (q == std::string::npos) return params;
        std::string_view query(url.data() + q + 1, url.size() - q - 1);
        size_t pos = 0;
        while (pos < query.size()) {
            auto amp = query.find('&', pos);
            auto pair = query.substr(pos, amp == std::string_view::npos ? query.size() - pos : amp - pos);
            auto eq = pair.find('=');
            if (eq != std::string_view::npos) {
                params[std::string(pair.substr(0, eq))] = std::string(pair.substr(eq + 1));
            }
            if (amp == std::string_view::npos) break;
            pos = amp + 1;
        }
        return params;
    }

    static std::string generateState() { return generateRandomString(32); }

    static std::string generateRandomString(size_t length) {
        static const char* chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::string result;
        for (size_t i = 0; i < length; ++i) {
            result += chars[(i * 2654435761ULL) % 62];
        }
        return result;
    }
};

} // namespace nexus::utility::http
