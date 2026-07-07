#pragma once

#include <string>
#include <optional>
#include <vector>
#include <functional>
#include <map>
#include <stdexcept>
#include <any>
#include <typeindex>

namespace nexus::utility::serialization {

/// Generic schema validation for structured data
class SchemaValidator2 {
public:
    enum class Type { String, Integer, Float, Boolean, Array, Object, Any, Null };

    struct Property {
        std::string name;
        Type type = Type::Any;
        bool required = false;
        std::any default_value;
        std::function<bool(const std::any&)> validator;
    };

    struct Schema {
        std::string title;
        Type root_type = Type::Object;
        std::vector<Property> properties;
        bool allow_additional_properties = true;
        std::optional<size_t> min_properties;
        std::optional<size_t> max_properties;
    };

    explicit SchemaValidator2(Schema schema) : schema_(std::move(schema)) {}

    /// Validate a generic property bag against the schema
    struct ValidationResult {
        bool valid = true;
        std::vector<std::string> errors;
    };

    ValidationResult validate(const std::map<std::string, std::any>& data) const {
        ValidationResult result;

        // Check min/max properties
        if (schema_.min_properties && data.size() < *schema_.min_properties) {
            result.valid = false;
            result.errors.push_back(
                "Too few properties: " + std::to_string(data.size()) +
                " < " + std::to_string(*schema_.min_properties));
        }
        if (schema_.max_properties && data.size() > *schema_.max_properties) {
            result.valid = false;
            result.errors.push_back(
                "Too many properties: " + std::to_string(data.size()) +
                " > " + std::to_string(*schema_.max_properties));
        }

        // Build property map for O(1) lookup
        std::map<std::string, Property> prop_map;
        for (const auto& p : schema_.properties) {
            prop_map[p.name] = p;
        }

        // Validate provided properties
        for (const auto& [key, value] : data) {
            auto it = prop_map.find(key);
            if (it == prop_map.end()) {
                if (!schema_.allow_additional_properties) {
                    result.valid = false;
                    result.errors.push_back("Unknown property: " + key);
                }
                continue;
            }

            const auto& prop = it->second;
            if (!validateType(value, prop.type)) {
                result.valid = false;
                result.errors.push_back(
                    "Property '" + key + "': type mismatch (expected " +
                    typeName(prop.type) + ")");
            }

            if (prop.validator && !prop.validator(value)) {
                result.valid = false;
                result.errors.push_back(
                    "Property '" + key + "': custom validation failed");
            }
        }

        // Check required properties
        for (const auto& [name, prop] : prop_map) {
            if (prop.required && data.find(name) == data.end()) {
                result.valid = false;
                result.errors.push_back("Missing required property: " + name);
            }
        }

        return result;
    }

    const Schema& schema() const { return schema_; }

private:
    Schema schema_;

    static bool validateType(const std::any& value, Type expected) {
        if (expected == Type::Any || expected == Type::Null) return true;

        const auto& ti = value.type();
        if (ti == typeid(std::string) || ti == typeid(const char*)) {
            return expected == Type::String;
        }
        if (ti == typeid(int) || ti == typeid(long) || ti == typeid(long long) ||
            ti == typeid(unsigned) || ti == typeid(unsigned long) || ti == typeid(unsigned long long)) {
            return expected == Type::Integer;
        }
        if (ti == typeid(float) || ti == typeid(double)) {
            return expected == Type::Float || expected == Type::Integer;
        }
        if (ti == typeid(bool)) {
            return expected == Type::Boolean;
        }
        if (ti == typeid(std::vector<std::any>)) {
            return expected == Type::Array;
        }
        if (ti == typeid(std::map<std::string, std::any>)) {
            return expected == Type::Object;
        }
        return false;
    }

    static std::string typeName(Type t) {
        switch (t) {
            case Type::String:  return "string";
            case Type::Integer: return "integer";
            case Type::Float:   return "float";
            case Type::Boolean: return "boolean";
            case Type::Array:   return "array";
            case Type::Object:  return "object";
            case Type::Any:     return "any";
            case Type::Null:    return "null";
        }
        return "unknown";
    }
};

} // namespace nexus::utility::serialization
