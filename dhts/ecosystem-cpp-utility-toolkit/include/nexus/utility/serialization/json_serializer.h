#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <cstdint>
#include <charconv>
#include <cmath>
#include <limits>

namespace nexus::utility::serialization {

/// JSON value type supporting null, bool, number, string, array, object
struct JsonValue;
using JsonNull = std::nullptr_t;
using JsonObject = std::map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

struct JsonValue {
    std::variant<JsonNull, bool, double, std::string, JsonArray, JsonObject> value;

    JsonValue() : value(nullptr) {}
    JsonValue(JsonNull) : value(nullptr) {}
    explicit JsonValue(bool b) : value(b) {}
    explicit JsonValue(int n) : value(static_cast<double>(n)) {}
    explicit JsonValue(double d) : value(d) {}
    explicit JsonValue(std::string s) : value(std::move(s)) {}
    explicit JsonValue(const char* s) : value(std::string(s)) {}
    explicit JsonValue(JsonArray a) : value(std::move(a)) {}
    explicit JsonValue(JsonObject o) : value(std::move(o)) {}

    bool isNull()    const { return std::holds_alternative<JsonNull>(value); }
    bool isBool()    const { return std::holds_alternative<bool>(value); }
    bool isNumber()  const { return std::holds_alternative<double>(value); }
    bool isString()  const { return std::holds_alternative<std::string>(value); }
    bool isArray()   const { return std::holds_alternative<JsonArray>(value); }
    bool isObject()  const { return std::holds_alternative<JsonObject>(value); }

    bool asBool()    const { return checkedGet<bool>("bool"); }
    double asDouble() const { return checkedGet<double>("number"); }
    int asInt()      const {
        double d = checkedGet<double>("number");
        return static_cast<int>(d);
    }
    const std::string& asString() const { return std::get<std::string>(value); }
    const JsonArray& asArray()   const { return std::get<JsonArray>(value); }
    const JsonObject& asObject()  const { return std::get<JsonObject>(value); }

    JsonValue& operator[](size_t index) {
        auto& arr = std::get<JsonArray>(value);
        if (index >= arr.size()) {
            throw std::out_of_range("JsonValue: array index " +
                                    std::to_string(index) + " out of range (size=" +
                                    std::to_string(arr.size()) + ")");
        }
        return arr[index];
    }
    const JsonValue& operator[](size_t index) const {
        const auto& arr = std::get<JsonArray>(value);
        if (index >= arr.size()) {
            throw std::out_of_range("JsonValue: array index " +
                                    std::to_string(index) + " out of range (size=" +
                                    std::to_string(arr.size()) + ")");
        }
        return arr[index];
    }
    JsonValue& operator[](const std::string& key) {
        return std::get<JsonObject>(value)[key];
    }
    const JsonValue& operator[](const std::string& key) const {
        return std::get<JsonObject>(value).at(key);
    }

private:
    template <typename T>
    T checkedGet(const char* expected_type) const {
        if (!std::holds_alternative<T>(value)) {
            throw std::domain_error(
                std::string("JsonValue: expected ") + expected_type +
                " but value is " + typeName());
        }
        return std::get<T>(value);
    }

    const char* typeName() const {
        if (isNull())   return "null";
        if (isBool())   return "bool";
        if (isNumber()) return "number";
        if (isString()) return "string";
        if (isArray())  return "array";
        if (isObject()) return "object";
        return "unknown";
    }
};

/// JSON serializer/deserializer with pretty-printing support
class JsonSerializer {
public:
    struct Options {
        bool pretty_print = false;
        int indent_width = 2;
        bool sort_keys = false;
        bool escape_unicode = false;
    };

    JsonSerializer() : options_{} {}
    explicit JsonSerializer(Options opts) : options_(opts) {}

    /// Parse JSON from string, returns parsed value or throws on error
    JsonValue parse(std::string_view input) {
        pos_ = 0;
        input_ = input;
        skipWhitespace();
        auto result = parseValue();
        skipWhitespace();
        if (pos_ < input_.size()) {
            throw std::runtime_error("Unexpected trailing content at position " + std::to_string(pos_));
        }
        return result;
    }

    /// Serialize a JSON value to string
    std::string serialize(const JsonValue& val) {
        output_.str("");
        output_.clear();
        serializeValue(val, 0);
        return output_.str();
    }

    /// Convenience: parse then serialize (round-trip validation)
    std::string normalize(std::string_view input) {
        return serialize(parse(input));
    }

private:
    Options options_;
    std::string_view input_;
    size_t pos_ = 0;
    std::ostringstream output_;

    void skipWhitespace() {
        while (pos_ < input_.size() && (input_[pos_] == ' ' || input_[pos_] == '\t' ||
               input_[pos_] == '\n' || input_[pos_] == '\r')) {
            ++pos_;
        }
    }

    char peek() const { return pos_ < input_.size() ? input_[pos_] : '\0'; }
    char consume() { return pos_ < input_.size() ? input_[pos_++] : '\0'; }

    void expect(char c) {
        if (peek() != c) {
            throw std::runtime_error(
                std::string("Expected '") + c + "' at position " + std::to_string(pos_));
        }
        consume();
    }

    JsonValue parseValue() {
        char c = peek();
        if (c == '"') return parseString();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') return parseNull();
        return parseNumber();
    }

    JsonValue parseNull() {
        if (input_.substr(pos_, 4) == "null") { pos_ += 4; return JsonValue(); }
        throw std::runtime_error("Expected null at " + std::to_string(pos_));
    }

    JsonValue parseBool() {
        if (input_.substr(pos_, 4) == "true")  { pos_ += 4; return JsonValue(true); }
        if (input_.substr(pos_, 5) == "false") { pos_ += 5; return JsonValue(false); }
        throw std::runtime_error("Expected bool at " + std::to_string(pos_));
    }

    JsonValue parseNumber() {
        size_t start = pos_;
        if (peek() == '-') consume();
        while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) consume();
        if (peek() == '.') { consume(); while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) consume(); }
        if (peek() == 'e' || peek() == 'E') {
            consume();
            if (peek() == '+' || peek() == '-') consume();
            while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) consume();
        }
        std::string num(input_.substr(start, pos_ - start));
        double val;
        auto [ptr, ec] = std::from_chars(num.data(), num.data() + num.size(), val);
        if (ec != std::errc{}) throw std::runtime_error("Invalid number at " + std::to_string(start));
        return JsonValue(val);
    }

    std::string parseStringContent() {
        std::string result;
        while (pos_ < input_.size() && peek() != '"') {
            if (peek() == '\\') {
                consume();
                switch (consume()) {
                    case '"':  result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/':  result += '/'; break;
                    case 'b':  result += '\b'; break;
                    case 'f':  result += '\f'; break;
                    case 'n':  result += '\n'; break;
                    case 'r':  result += '\r'; break;
                    case 't':  result += '\t'; break;
                    case 'u': {
                        std::string hex(4, '\0');
                        for (int i = 0; i < 4; ++i) hex[i] = consume();
                        uint16_t codepoint = static_cast<uint16_t>(std::stoul(hex, nullptr, 16));
                        if (codepoint < 0x80) {
                            result += static_cast<char>(codepoint);
                        } else if (codepoint < 0x800) {
                            result += static_cast<char>(0xC0 | (codepoint >> 6));
                            result += static_cast<char>(0x80 | (codepoint & 0x3F));
                        } else {
                            result += static_cast<char>(0xE0 | (codepoint >> 12));
                            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                            result += static_cast<char>(0x80 | (codepoint & 0x3F));
                        }
                        break;
                    }
                    default: throw std::runtime_error("Invalid escape at " + std::to_string(pos_));
                }
            } else {
                result += consume();
            }
        }
        return result;
    }

    JsonValue parseString() {
        expect('"');
        std::string s = parseStringContent();
        expect('"');
        return JsonValue(std::move(s));
    }

    JsonValue parseArray() {
        expect('[');
        skipWhitespace();
        JsonArray arr;
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
        expect(']');
        return JsonValue(std::move(arr));
    }

    JsonValue parseObject() {
        expect('{');
        skipWhitespace();
        JsonObject obj;
        if (peek() != '}') {
            auto key = parseString().asString();
            skipWhitespace();
            expect(':');
            skipWhitespace();
            obj[std::move(key)] = parseValue();
            skipWhitespace();
            while (peek() == ',') {
                consume();
                skipWhitespace();
                auto k = parseString().asString();
                skipWhitespace();
                expect(':');
                skipWhitespace();
                obj[std::move(k)] = parseValue();
                skipWhitespace();
            }
        }
        expect('}');
        return JsonValue(std::move(obj));
    }

    void indent(int level) {
        if (options_.pretty_print) {
            output_ << '\n' << std::string(level * options_.indent_width, ' ');
        }
    }

    void serializeValue(const JsonValue& val, int level) {
        if (val.isNull())      { output_ << "null"; }
        else if (val.isBool()) { output_ << (val.asBool() ? "true" : "false"); }
        else if (val.isNumber()) {
            double d = val.asDouble();
            if (std::isnan(d)) {
                output_ << "null";  // JSON does not support NaN
            } else if (std::isinf(d)) {
                output_ << "null";  // JSON does not support Infinity
            } else {
                double int_part;
                if (std::modf(d, &int_part) == 0.0 &&
                    int_part >= static_cast<double>(std::numeric_limits<int64_t>::min()) &&
                    int_part <= static_cast<double>(std::numeric_limits<int64_t>::max())) {
                    output_ << static_cast<int64_t>(int_part);
                } else {
                    output_ << d;
                }
            }
        }
        else if (val.isString()) { serializeString(val.asString()); }
        else if (val.isArray())  { serializeArray(val.asArray(), level); }
        else if (val.isObject()) { serializeObject(val.asObject(), level); }
    }

    void serializeString(const std::string& s) {
        output_ << '"';
        for (char c : s) {
            switch (c) {
                case '"':  output_ << "\\\""; break;
                case '\\': output_ << "\\\\"; break;
                case '\b': output_ << "\\b"; break;
                case '\f': output_ << "\\f"; break;
                case '\n': output_ << "\\n"; break;
                case '\r': output_ << "\\r"; break;
                case '\t': output_ << "\\t"; break;
                default: output_ << c;
            }
        }
        output_ << '"';
    }

    void serializeArray(const JsonArray& arr, int level) {
        output_ << '[';
        bool first = true;
        for (const auto& elem : arr) {
            if (!first) output_ << ',';
            first = false;
            indent(level + 1);
            serializeValue(elem, level + 1);
        }
        indent(level);
        output_ << ']';
    }

    void serializeObject(const JsonObject& obj, int level) {
        output_ << '{';
        bool first = true;
        for (const auto& [key, val] : obj) {
            if (!first) output_ << ',';
            first = false;
            indent(level + 1);
            serializeString(key);
            output_ << (options_.pretty_print ? ": " : ":");
            serializeValue(val, level + 1);
        }
        indent(level);
        output_ << '}';
    }
};

} // namespace nexus::utility::serialization
