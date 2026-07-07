#pragma once

#include <string>
#include <string_view>
#include <map>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <cctype>

namespace nexus::utility::serialization {

/// Simple YAML subset parser (scalars, sequences, mappings, no anchors/aliases)
class YamlHelper {
public:
    struct YamlValue;
    using YamlMap = std::map<std::string, YamlValue>;
    using YamlSeq = std::vector<YamlValue>;

    struct YamlValue {
        enum class Kind { Null, Scalar, Sequence, Mapping } kind = Kind::Null;
        std::string scalar;
        YamlSeq sequence;
        YamlMap mapping;

        static YamlValue null() { return {}; }
        static YamlValue makeScalar(std::string s) { YamlValue v; v.kind = Kind::Scalar; v.scalar = std::move(s); return v; }
        static YamlValue seq(YamlSeq s) { YamlValue v; v.kind = Kind::Sequence; v.sequence = std::move(s); return v; }
        static YamlValue map(YamlMap m) { YamlValue v; v.kind = Kind::Mapping; v.mapping = std::move(m); return v; }

        const YamlValue& operator[](const std::string& key) const { return mapping.at(key); }
        YamlValue& operator[](const std::string& key) { return mapping[key]; }
        const YamlValue& operator[](size_t i) const { return sequence[i]; }

        std::string asString() const { return scalar; }
        int asInt() const { return std::stoi(scalar); }
        double asDouble() const { return std::stod(scalar); }
        bool asBool() const {
            std::string lower;
            for (char c : scalar) lower += static_cast<char>(std::tolower(c));
            return lower == "true" || lower == "yes" || lower == "on";
        }

        bool isNull() const { return kind == Kind::Null; }
        bool isScalar() const { return kind == Kind::Scalar; }
        bool isSequence() const { return kind == Kind::Sequence; }
        bool isMapping() const { return kind == Kind::Mapping; }
    };

    struct Options {
        int indent = 2;
        bool use_flow_style = false;
    };

    YamlHelper() : opts_{} {}
    explicit YamlHelper(Options opts) : opts_(opts) {}

    YamlValue parse(std::string_view input) {
        lines_ = splitLines(input);
        line_idx_ = 0;
        return parseDocument(0);
    }

    std::string emit(const YamlValue& value) {
        buf_.str("");
        buf_.clear();
        emitValue(value, 0);
        return buf_.str();
    }

private:
    Options opts_;
    std::vector<std::string> lines_;
    size_t line_idx_ = 0;
    std::ostringstream buf_;

    static std::vector<std::string> splitLines(std::string_view sv) {
        std::vector<std::string> result;
        size_t start = 0;
        while (start < sv.size()) {
            size_t end = sv.find('\n', start);
            if (end == std::string_view::npos) end = sv.size();
            auto line = sv.substr(start, end - start);
            // remove \r
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            result.emplace_back(line);
            start = end + 1;
        }
        return result;
    }

    static int countIndent(const std::string& line) {
        int n = 0;
        for (char c : line) {
            if (c == ' ') ++n;
            else break;
        }
        return n;
    }

    static std::string_view trim(std::string_view s) {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
        return s;
    }

    std::string_view currentLine() {
        if (line_idx_ >= lines_.size()) return {};
        return lines_[line_idx_];
    }

    void advance() { ++line_idx_; }

    YamlValue parseDocument(int parent_indent) {
        // Skip empty/comment lines
        while (line_idx_ < lines_.size()) {
            auto line = trim(currentLine());
            if (line.empty() || line[0] == '#') { advance(); continue; }
            break;
        }
        if (line_idx_ >= lines_.size()) return YamlValue::null();

        auto raw = currentLine();
        int indent = countIndent(std::string(raw));
        if (indent < parent_indent) return YamlValue::null();

        auto content = trim(raw);
        if (content.empty()) { advance(); return YamlValue::null(); }

        // Sequence entry
        if (content.starts_with("- ")) {
            YamlSeq seq;
            while (line_idx_ < lines_.size()) {
                auto line = currentLine();
                int ci = countIndent(std::string(line));
                auto ct = trim(line);
                if (ci < indent || !ct.starts_with("- ")) break;
                advance();
                auto value_str = ct.substr(2);
                if (value_str.empty()) {
                    // Nested block
                    seq.push_back(parseDocument(indent + opts_.indent));
                } else if (value_str[0] == '{' || value_str[0] == '[') {
                    seq.push_back(parseInline(value_str));
                } else {
                    seq.push_back(YamlValue::makeScalar(std::string(value_str)));
                }
            }
            return YamlValue::seq(std::move(seq));
        }

        // Mapping
        auto colon = content.find(':');
        if (colon != std::string_view::npos) {
            YamlMap map;
            while (line_idx_ < lines_.size()) {
                auto line = currentLine();
                int ci = countIndent(std::string(line));
                auto ct = trim(line);
                if (ci < indent) break;
                auto c = ct.find(':');
                if (c == std::string_view::npos) break;
                advance();
                auto key = std::string(trim(ct.substr(0, c)));
                auto val = trim(ct.substr(c + 1));
                if (val.empty()) {
                    map[std::move(key)] = parseDocument(indent + opts_.indent);
                } else {
                    map[std::move(key)] = YamlValue::makeScalar(std::string(val));
                }
            }
            return YamlValue::map(std::move(map));
        }

        // Bare scalar
        advance();
        return YamlValue::makeScalar(std::string(content));
    }

    YamlValue parseInline(std::string_view sv) {
        // Simplified inline parsing
        return YamlValue::makeScalar(std::string(sv));
    }

    void emitValue(const YamlValue& v, int indent) {
        switch (v.kind) {
            case YamlValue::Kind::Null:
                buf_ << "~";
                break;
            case YamlValue::Kind::Scalar:
                buf_ << v.scalar;
                break;
            case YamlValue::Kind::Sequence:
                for (const auto& item : v.sequence) {
                    buf_ << std::string(indent, ' ') << "- ";
                    if (item.kind == YamlValue::Kind::Mapping || item.kind == YamlValue::Kind::Sequence) {
                        buf_ << '\n';
                        emitValue(item, indent + opts_.indent);
                    } else {
                        emitValue(item, 0);
                        buf_ << '\n';
                    }
                }
                break;
            case YamlValue::Kind::Mapping:
                for (const auto& [key, val] : v.mapping) {
                    buf_ << std::string(indent, ' ') << key << ':';
                    if (val.kind == YamlValue::Kind::Mapping || val.kind == YamlValue::Kind::Sequence) {
                        buf_ << '\n';
                        emitValue(val, indent + opts_.indent);
                    } else if (val.kind == YamlValue::Kind::Null) {
                        buf_ << '\n';
                    } else {
                        buf_ << ' ';
                        emitValue(val, 0);
                        buf_ << '\n';
                    }
                }
                break;
        }
    }
};

} // namespace nexus::utility::serialization
