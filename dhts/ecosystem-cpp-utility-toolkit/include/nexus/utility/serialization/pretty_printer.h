#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <tuple>
#include <type_traits>
#include <concepts>
#include <iomanip>

namespace nexus::utility::serialization {

/// Pretty printer for C++ containers and custom types
class PrettyPrinter {
public:
    struct Options {
        int indent = 2;
        int max_width = 120;
        bool color = false;
        bool compact_single_line = true;
        char key_value_separator = ':';
    };

    PrettyPrinter() : opts_{} {}
    explicit PrettyPrinter(Options opts) : opts_(opts) {}

    template<typename T>
    std::string format(const T& value) {
        buf_.str("");
        buf_.clear();
        current_indent_ = 0;
        formatValue(value);
        return buf_.str();
    }

private:
    Options opts_;
    std::ostringstream buf_;
    int current_indent_ = 0;

    void writeIndent() {
        buf_ << std::string(current_indent_ * opts_.indent, ' ');
    }

    // ---- Fundamental types ----
    void formatValue(bool v)   { buf_ << (v ? "true" : "false"); }
    void formatValue(int v)    { buf_ << v; }
    void formatValue(long v)   { buf_ << v; }
    void formatValue(long long v) { buf_ << v; }
    void formatValue(unsigned v)  { buf_ << v << 'u'; }
    void formatValue(unsigned long v) { buf_ << v << "ul"; }
    void formatValue(unsigned long long v) { buf_ << v << "ull"; }
    void formatValue(float v)  { buf_ << std::fixed << std::setprecision(6) << v << 'f'; }
    void formatValue(double v) { buf_ << std::fixed << std::setprecision(6) << v; }
    void formatValue(const std::string& v) { buf_ << '"' << escape(v) << '"'; }
    void formatValue(const char* v) { formatValue(std::string(v)); }
    void formatValue(std::nullptr_t) { buf_ << "null"; }

    // ---- Optional ----
    template<typename T>
    void formatValue(const std::optional<T>& v) {
        if (v) { formatValue(*v); }
        else   { buf_ << "nullopt"; }
    }

    // ---- Pair ----
    template<typename A, typename B>
    void formatValue(const std::pair<A, B>& p) {
        buf_ << '(';
        formatValue(p.first);
        buf_ << ", ";
        formatValue(p.second);
        buf_ << ')';
    }

    // ---- Tuple ----
    template<typename... Ts>
    void formatValue(const std::tuple<Ts...>& t) {
        buf_ << '(';
        std::apply([this](const auto&... args) {
            size_t i = 0;
            ((formatValue(args), (++i < sizeof...(Ts) ? (void)(buf_ << ", ") : (void)0)), ...);
        }, t);
        buf_ << ')';
    }

    // ---- Sequential containers ----
    template<typename T>
    void formatValue(const std::vector<T>& v) { formatSequence(v, '[', ']'); }

    template<typename T>
    void formatValue(const std::set<T>& s) { formatSequence(s, '{', '}'); }

    template<typename T>
    void formatValue(const std::unordered_set<T>& s) { formatSequence(s, '{', '}'); }

    // ---- Associative containers ----
    template<typename K, typename V>
    void formatValue(const std::map<K, V>& m) { formatMap(m); }

    template<typename K, typename V>
    void formatValue(const std::unordered_map<K, V>& m) { formatMap(m); }

private:
    std::string escape(const std::string& s) {
        std::string result;
        for (char c : s) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:   result += c;
            }
        }
        return result;
    }

    template<typename Container>
    void formatSequence(const Container& c, char open, char close) {
        if (c.empty()) { buf_ << open << close; return; }
        if (opts_.compact_single_line && c.size() <= 5) {
            buf_ << open << ' ';
            bool first = true;
            for (const auto& item : c) {
                if (!first) buf_ << ", ";
                first = false;
                formatValue(item);
            }
            buf_ << ' ' << close;
        } else {
            buf_ << open << '\n';
            current_indent_++;
            for (const auto& item : c) {
                writeIndent();
                formatValue(item);
                buf_ << ",\n";
            }
            current_indent_--;
            writeIndent();
            buf_ << close;
        }
    }

    template<typename MapType>
    void formatMap(const MapType& m) {
        if (m.empty()) { buf_ << "{}"; return; }
        buf_ << "{\n";
        current_indent_++;
        for (const auto& [key, val] : m) {
            writeIndent();
            formatValue(key);
            buf_ << " => ";
            formatValue(val);
            buf_ << ",\n";
        }
        current_indent_--;
        writeIndent();
        buf_ << '}';
    }
};

} // namespace nexus::utility::serialization
