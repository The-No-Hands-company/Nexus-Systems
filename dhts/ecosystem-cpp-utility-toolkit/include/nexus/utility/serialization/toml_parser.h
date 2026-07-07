#pragma once

#include <string>
#include <string_view>
#include <map>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <charconv>

namespace nexus::utility::serialization {

/// TOML configuration file parser (v1.0 subset)
class TomlParser {
public:
    struct TomlValue;
    using TomlTable = std::map<std::string, TomlValue>;
    using TomlArray = std::vector<TomlValue>;

    struct TomlValue {
        enum class Kind { String, Integer, Float, Boolean, Array, Table, DateTime } kind;
        std::string string_val;
        int64_t int_val = 0;
        double float_val = 0.0;
        bool bool_val = false;
        TomlArray array_val;
        TomlTable table_val;

        static TomlValue stringVal(std::string s)     { TomlValue v; v.kind = Kind::String; v.string_val = std::move(s); return v; }
        static TomlValue intVal(int64_t i)             { TomlValue v; v.kind = Kind::Integer; v.int_val = i; return v; }
        static TomlValue floatVal(double d)            { TomlValue v; v.kind = Kind::Float; v.float_val = d; return v; }
        static TomlValue boolVal(bool b)               { TomlValue v; v.kind = Kind::Boolean; v.bool_val = b; return v; }
        static TomlValue arrayVal(TomlArray a)         { TomlValue v; v.kind = Kind::Array; v.array_val = std::move(a); return v; }
        static TomlValue tableVal(TomlTable t)         { TomlValue v; v.kind = Kind::Table; v.table_val = std::move(t); return v; }

        const TomlValue& operator[](const std::string& key) const { return table_val.at(key); }
        TomlValue& operator[](const std::string& key) { return table_val[key]; }

        std::string asString() const { return string_val; }
        int64_t asInt() const { return int_val; }
        double asFloat() const { return float_val; }
        bool asBool() const { return bool_val; }
        const TomlArray& asArray() const { return array_val; }
        const TomlTable& asTable() const { return table_val; }
    };

    TomlTable parse(std::string_view input) {
        pos_ = 0;
        input_ = input;
        TomlTable root;
        parseTable(root, "");
        return root;
    }

    std::string emit(const TomlTable& table) {
        buf_.str("");
        buf_.clear();
        emitTable(table, "");
        return buf_.str();
    }

private:
    std::string_view input_;
    size_t pos_ = 0;
    std::ostringstream buf_;

    void skipWhitespace() {
        while (pos_ < input_.size() && (input_[pos_] == ' ' || input_[pos_] == '\t' ||
               input_[pos_] == '\n' || input_[pos_] == '\r')) ++pos_;
    }

    void skipLine() {
        while (pos_ < input_.size() && input_[pos_] != '\n') ++pos_;
        if (pos_ < input_.size()) ++pos_;
    }

    char peek() const { return pos_ < input_.size() ? input_[pos_] : '\0'; }

    void parseTable(TomlTable& table, const std::string& prefix) {
        while (pos_ < input_.size()) {
            skipWhitespace();
            if (pos_ >= input_.size()) break;

            char c = peek();
            if (c == '#') { skipLine(); continue; }
            if (c == '[') {
                throw std::runtime_error(
                    "TOML table headers ([section]) are not yet supported. "
                    "Use flat key-value TOML without sections.");
            }

            // Key = value
            auto key = parseKey();
            if (key.empty()) { skipLine(); continue; }
            skipWhitespace();
            if (peek() != '=') { skipLine(); continue; }
            ++pos_; // consume '='
            skipWhitespace();
            auto value = parseValue();
            table[std::move(key)] = std::move(value);
            skipWhitespace();
        }
    }

    std::string parseKey() {
        std::string key;
        if (peek() == '"') {
            key = parseBasicString();
        } else {
            while (pos_ < input_.size() && (std::isalnum(static_cast<unsigned char>(input_[pos_])) ||
                   input_[pos_] == '_' || input_[pos_] == '-')) {
                key += input_[pos_++];
            }
        }
        return key;
    }

    std::string parseBasicString() {
        if (peek() != '"') return {};
        ++pos_;
        std::string result;
        while (pos_ < input_.size() && peek() != '"') {
            if (peek() == '\\') {
                ++pos_;
                switch (consume()) {
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    default: result += input_[pos_ - 1];
                }
            } else {
                result += input_[pos_++];
            }
        }
        if (pos_ < input_.size()) ++pos_; // closing quote
        return result;
    }

    char consume() { return pos_ < input_.size() ? input_[pos_++] : '\0'; }

    TomlValue parseValue() {
        skipWhitespace();
        char c = peek();

        if (c == '"') return TomlValue::stringVal(parseBasicString());
        if (c == 't' || c == 'f') {
            if (input_.substr(pos_, 4) == "true")  { pos_ += 4; return TomlValue::boolVal(true); }
            if (input_.substr(pos_, 5) == "false") { pos_ += 5; return TomlValue::boolVal(false); }
        }
        if (c == '[') return parseTomlArray();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseTomlNumber();

        throw std::runtime_error(std::string("TOML: Unexpected character '") + c + "' at " + std::to_string(pos_));
    }

    TomlValue parseTomlNumber() {
        size_t start = pos_;
        if (peek() == '-' || peek() == '+') consume();
        bool is_float = false;
        while (pos_ < input_.size() && (std::isdigit(static_cast<unsigned char>(input_[pos_])) || input_[pos_] == '_')) consume();
        if (peek() == '.') { is_float = true; consume();
            while (pos_ < input_.size() && (std::isdigit(static_cast<unsigned char>(input_[pos_])) || input_[pos_] == '_')) consume();
        }
        if (peek() == 'e' || peek() == 'E') { is_float = true; consume();
            if (peek() == '+' || peek() == '-') consume();
            while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) consume();
        }
        std::string num(input_.substr(start, pos_ - start));
        // Remove underscores
        std::string clean;
        for (char ch : num) if (ch != '_') clean += ch;

        if (is_float) {
            double d;
            auto [ptr, ec] = std::from_chars(clean.data(), clean.data() + clean.size(), d);
            if (ec == std::errc{}) return TomlValue::floatVal(d);
        }
        int64_t i;
        auto [ptr, ec] = std::from_chars(clean.data(), clean.data() + clean.size(), i);
        if (ec == std::errc{}) return TomlValue::intVal(i);

        throw std::runtime_error("TOML: Invalid number at " + std::to_string(start));
    }

    TomlValue parseTomlArray() {
        if (peek() != '[') throw std::runtime_error("TOML: Expected '['");
        consume();
        TomlArray arr;
        skipWhitespace();
        if (peek() != ']') {
            arr.push_back(parseValue());
            skipWhitespace();
            while (peek() == ',') {
                consume();
                skipWhitespace();
                arr.push_back(parseValue());
                skipWhitespace();
            }
        }
        if (peek() != ']') throw std::runtime_error("TOML: Expected ']'");
        consume();
        return TomlValue::arrayVal(std::move(arr));
    }

    void emitTable(const TomlTable& table, const std::string& prefix) {
        for (const auto& [key, val] : table) {
            if (!prefix.empty()) buf_ << prefix << '.';
            buf_ << key << " = ";
            emitValue(val);
            buf_ << '\n';
        }
    }

    void emitValue(const TomlValue& val) {
        switch (val.kind) {
            case TomlValue::Kind::String:  buf_ << '"' << escape(val.string_val) << '"'; break;
            case TomlValue::Kind::Integer: buf_ << val.int_val; break;
            case TomlValue::Kind::Float:   buf_ << val.float_val; break;
            case TomlValue::Kind::Boolean: buf_ << (val.bool_val ? "true" : "false"); break;
            case TomlValue::Kind::Array:
                buf_ << "[ ";
                for (size_t i = 0; i < val.array_val.size(); ++i) {
                    if (i > 0) buf_ << ", ";
                    emitValue(val.array_val[i]);
                }
                buf_ << " ]";
                break;
            case TomlValue::Kind::Table:
                buf_ << "{ ";
                {
                    size_t idx = 0;
                    for (const auto& [k, v] : val.table_val) {
                        if (idx++ > 0) buf_ << ", ";
                        buf_ << k << " = ";
                        emitValue(v);
                    }
                }
                buf_ << "}";
                break;
            default: buf_ << "\"\"";
        }
    }

    static std::string escape(const std::string& s) {
        std::string r;
        for (char c : s) {
            if (c == '"') r += "\\\"";
            else if (c == '\\') r += "\\\\";
            else if (c == '\n') r += "\\n";
            else if (c == '\t') r += "\\t";
            else r += c;
        }
        return r;
    }
};

} // namespace nexus::utility::serialization
