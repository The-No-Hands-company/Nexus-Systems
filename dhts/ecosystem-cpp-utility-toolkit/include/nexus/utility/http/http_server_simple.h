#pragma once

#include <string>
#include <optional>
#include <string_view>
#include <map>
#include <vector>
#include <functional>
#include <sstream>

namespace nexus::utility::http {

/// Simple embedded HTTP server for testing and development
class HttpServerSimple {
public:
    struct Request {
        std::string method;
        std::string path;
        std::string body;
        std::map<std::string, std::string> headers;
        std::map<std::string, std::string> query_params;
    };

    struct Response {
        int status_code = 200;
        std::string status_message = "OK";
        std::map<std::string, std::string> headers;
        std::string body;
    };

    using Handler = std::function<Response(const Request&)>;
    using Middleware = std::function<std::optional<Response>(const Request&)>;

    struct Route {
        std::string method;
        std::string path_pattern;
        Handler handler;
    };

    HttpServerSimple() {
        defaultHeaders_["Server"] = "Nexus-HttpServer/1.0";
        defaultHeaders_["Content-Type"] = "text/plain";
    }

    /// Register a route handler
    void route(const std::string& method, const std::string& path, Handler handler) {
        routes_.push_back({toUpper(method), path, std::move(handler)});
    }

    void get(const std::string& path, Handler handler)  { route("GET", path, std::move(handler)); }
    void post(const std::string& path, Handler handler) { route("POST", path, std::move(handler)); }
    void put(const std::string& path, Handler handler)  { route("PUT", path, std::move(handler)); }
    void del(const std::string& path, Handler handler)  { route("DELETE", path, std::move(handler)); }

    /// Add middleware
    void use(Middleware mw) { middlewares_.push_back(std::move(mw)); }

    /// Set default header for all responses
    void setDefaultHeader(const std::string& name, const std::string& value) {
        defaultHeaders_[name] = value;
    }

    /// Handle an incoming HTTP request (returns raw HTTP response string)
    std::string handleRequest(const std::string& raw_request) {
        auto req = parseRequest(raw_request);

        // Run middleware
        for (const auto& mw : middlewares_) {
            auto result = mw(req);
            if (result) {
                return formatResponse(*result);
            }
        }

        // Route dispatch
        for (const auto& route : routes_) {
            if (matchRoute(route, req)) {
                auto resp = route.handler(req);
                return formatResponse(resp);
            }
        }

        // 404
        Response not_found;
        not_found.status_code = 404;
        not_found.status_message = "Not Found";
        not_found.body = "404 - Route not found";
        return formatResponse(not_found);
    }

    /// Start listening on port (simplified)
    void listen(int port) {
        port_ = port;
        running_ = true;
    }

    void stop() { running_ = false; }
    bool isRunning() const { return running_; }
    int port() const { return port_; }

private:
    std::vector<Route> routes_;
    std::vector<Middleware> middlewares_;
    std::map<std::string, std::string> defaultHeaders_;
    int port_ = 8080;
    bool running_ = false;

    static Request parseRequest(const std::string& raw) {
        Request req;
        std::istringstream stream(raw);
        std::string line;

        // Request line
        if (std::getline(stream, line)) {
            std::istringstream line_stream(line);
            line_stream >> req.method >> req.path;
            // Parse query parameters
            auto qmark = req.path.find('?');
            if (qmark != std::string::npos) {
                auto query = req.path.substr(qmark + 1);
                req.path = req.path.substr(0, qmark);
                // Simple query parsing
                size_t pos = 0;
                while (pos < query.size()) {
                    auto amp = query.find('&', pos);
                    auto pair = query.substr(pos, amp == std::string::npos ? query.size() - pos : amp - pos);
                    auto eq = pair.find('=');
                    if (eq != std::string::npos) {
                        req.query_params[pair.substr(0, eq)] = pair.substr(eq + 1);
                    } else {
                        req.query_params[pair] = "";
                    }
                    if (amp == std::string::npos) break;
                    pos = amp + 1;
                }
            }
        }

        // Headers
        while (std::getline(stream, line) && !line.empty() && line != "\r") {
            if (line.back() == '\r') line.pop_back();
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                auto key = line.substr(0, colon);
                auto val = line.substr(colon + 1);
                // Trim value
                while (!val.empty() && val[0] == ' ') val.erase(0, 1);
                req.headers[key] = val;
            }
        }

        // Body
        std::string remaining;
        while (std::getline(stream, line)) {
            remaining += line + "\n";
        }
        req.body = remaining;

        return req;
    }

    static std::string formatResponse(const Response& resp) {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << resp.status_code << " " << resp.status_message << "\r\n";

        auto headers = resp.headers;
        headers["Content-Length"] = std::to_string(resp.body.size());
        for (const auto& [k, v] : headers) {
            oss << k << ": " << v << "\r\n";
        }
        oss << "\r\n";
        oss << resp.body;
        return oss.str();
    }

    static bool matchRoute(const Route& route, const Request& req) {
        return route.method == toUpper(req.method) && route.path_pattern == req.path;
    }

    static std::string toUpper(std::string s) {
        for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return s;
    }
};

} // namespace nexus::utility::http
