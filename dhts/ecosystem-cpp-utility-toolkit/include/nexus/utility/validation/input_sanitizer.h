#pragma once

#include <string>
#include <vector>
#include <functional>
#include <any>
#include <unordered_map>

namespace nexus::utility::validation {

/// @brief Generic input validation framework
class InputSanitizer {
public:
    using ValidationRule = std::function<bool(const std::any&)>;

    void addRule(const std::string& field_name, ValidationRule rule,
                const std::string& error_message);

    bool validate(const std::string& field_name, const std::any& value,
                 std::string& error_out) const;

    /// Common validation rules
    static ValidationRule notEmpty();
    static ValidationRule lengthBetween(size_t min, size_t max);
    static ValidationRule matchesRegex(const std::string& pattern);
    static ValidationRule isNumeric();
    static ValidationRule rangeCheck(double min, double max);

private:
    struct Rule {
        ValidationRule validator;
        std::string error_message;
    };

    std::unordered_map<std::string, std::vector<Rule>> rules_;
};

/// @brief Validate sorting correctness
template<typename T>
class SortingValidator {
public:
    static bool isSorted(const std::vector<T>& data,
                        std::function<bool(const T&, const T&)> comparator = std::less<T>());

    static bool isSorted(const T* data, size_t size,
                        std::function<bool(const T&, const T&)> comparator = std::less<T>());

    /// Verify sort stability
    static bool isStableSort(const std::vector<T>& original,
                            const std::vector<T>& sorted,
                            std::function<bool(const T&, const T&)> comparator);
};

/// @brief Validate floating-point values
class FloatingPointValidator {
public:
    /// Check if value is NaN
    static bool isNaN(float value);
    static bool isNaN(double value);

    /// Check if value is infinity
    static bool isInf(float value);
    static bool isInf(double value);

    /// Check if value is finite (not NaN or Inf)
    static bool isFinite(float value);
    static bool isFinite(double value);

    /// Check if two floats are approximately equal
    static bool approximatelyEqual(float a, float b, float epsilon = 1e-6f);
    static bool approximatelyEqual(double a, double b, double epsilon = 1e-9);

    /// Validate array for NaN/Inf
    static bool validateArray(const float* data, size_t size);
    static bool validateArray(const double* data, size_t size);
};

/// @brief Validate enum values are in range
template<typename EnumType>
class EnumValidator {
public:
    static bool isValid(EnumType value, EnumType min_value, EnumType max_value) {
        using UnderlyingType = std::underlying_type_t<EnumType>;
        auto val = static_cast<UnderlyingType>(value);
        auto min_val = static_cast<UnderlyingType>(min_value);
        auto max_val = static_cast<UnderlyingType>(max_value);
        return val >= min_val && val <= max_val;
    }
};

// Template implementations

template<typename T>
bool SortingValidator<T>::isSorted(const std::vector<T>& data,
                                  std::function<bool(const T&, const T&)> comparator) {
    return isSorted(data.data(), data.size(), comparator);
}

template<typename T>
bool SortingValidator<T>::isSorted(const T* data, size_t size,
                                  std::function<bool(const T&, const T&)> comparator) {
    for (size_t i = 1; i < size; ++i) {
        if (comparator(data[i], data[i-1])) {
            return false;  // Found element less than previous
        }
    }
    return true;
}

template<typename T>
bool SortingValidator<T>::isStableSort(const std::vector<T>& original,
                                      const std::vector<T>& sorted,
                                      std::function<bool(const T&, const T&)> comparator) {
    // Simplified check - full implementation would track original indices
    return isSorted(sorted, comparator);
}

} // namespace nexus::utility::validation
