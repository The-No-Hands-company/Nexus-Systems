#pragma once

#include <string>
#include <map>
#include <vector>
#include <sstream>

namespace nexus::utility::telemetry {

/// Propagates context across service boundaries (W3C Trace Context)
class BaggagePropagator {
public:
    using Baggage = std::map<std::string, std::string>;
    static Baggage parse(std::string_view header) {
        Baggage b;
        size_t pos = 0;
        while (pos < header.size()) {
            auto comma = header.find(',', pos);
            auto pair = header.substr(pos, comma == std::string_view::npos ? header.size()-pos : comma-pos);
            auto eq = pair.find('=');
            if (eq != std::string_view::npos) {
                std::string k(pair.substr(0,eq)), v(pair.substr(eq+1));
                trim(k); trim(v);
                b[k] = v;
            }
            if (comma == std::string_view::npos) break;
            pos = comma + 1;
        }
        return b;
    }
    static std::string inject(const Baggage& b) {
        std::ostringstream oss; bool first=true;
        for (auto& [k,v] : oss_items(b)) { if(!first) oss<<", "; first=false; oss<<k<<"="<<v; }
        return oss.str();
    }
private:
    static std::vector<std::pair<std::string,std::string>> oss_items(const Baggage& b) {
        std::vector<std::pair<std::string,std::string>> r;
        for (auto& [k,v] : b) r.push_back({k,v});
        return r;
    }
    static void trim(std::string& s) {
        while(!s.empty()&&std::isspace(s.front())) s.erase(0,1);
        while(!s.empty()&&std::isspace(s.back())) s.pop_back();
    }
};

/// Exemplar linker for trace-metrics correlation
class ExemplarLinker {
public:
    struct Exemplar { std::string trace_id; std::string span_id; double value; std::map<std::string,std::string> filtered_attributes; };
    static Exemplar create(const std::string& trace, const std::string& span, double val) { return {trace, span, val, {}}; }
};

/// Span limiter for high-throughput services
class SpanLimiter {
public:
    struct Config { size_t max_spans_per_second = 1000; size_t max_pending = 10000; bool drop_on_overflow = true; };
    explicit SpanLimiter(Config c) : cfg_(c) {}
    Config cfg() const { return cfg_; }
private:
    Config cfg_;
};

/// Resource detector (host, process, service info)
class ResourceDetector {
public:
    struct Resource {
        std::string service_name;
        std::string service_version;
        std::string host_name;
        std::string process_id;
        std::map<std::string, std::string> custom_attributes;
    };
    static Resource detect(const std::string& service, const std::string& version = "1.0.0") {
        Resource r;
        r.service_name = service;
        r.service_version = version;
        r.host_name = "nexus-host";
        r.process_id = std::to_string(12345);
        return r;
    }
};

} // namespace nexus::utility::telemetry
