#pragma once

#include <string>
#include <typeindex>
#include <typeinfo>
#include <type_traits>
#include <optional>
#include <memory>
#include <source_location>
#include <stdexcept>

// Platform-specific for demangling
#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#include <cstdlib>
#endif

namespace nexus::utility::types {

/**
 * @brief Utilities for runtime type checking, name demangling, and validation.
 */
class TypeValidator {
public:
    /**
     * @brief Get the demangled human-readable name of a type T.
     */
    template<typename T>
    static std::string getTypeName() {
        return demangle(typeid(T).name());
    }

    /**
     * @brief Get the demangled name of the runtime type of an object.
     */
    template<typename T>
    static std::string getRuntimeTypeName(const T& obj) {
        return demangle(typeid(obj).name());
    }

    /**
     * @brief Check if a pointer's dynamic type matches the specified type T exactly.
     */
    template<typename T, typename U>
    static bool isExactType(const U* ptr) {
        if (!ptr) return false;
        return typeid(*ptr) == typeid(T);
    }

    /**
     * @brief Check if a pointer can be dynamic_cast to T.
     */
    template<typename T, typename U>
    static bool isInstanceOf(const U* ptr) {
        return dynamic_cast<const T*>(ptr) != nullptr;
    }

    /**
     * @brief Validate that an object is of expected type, throwing if not.
     */
    template<typename ExpectedType, typename U>
    static void validateType(const U* ptr, std::source_location loc = std::source_location::current()) {
        if (!ptr) {
            throw std::runtime_error("TypeValidator: Null pointer check failed");
        }
        if (!isInstanceOf<ExpectedType>(ptr)) {
            std::string actual = getRuntimeTypeName(*ptr);
            std::string expected = getTypeName<ExpectedType>();
            throw std::runtime_error("TypeValidator: Type mismatch. Expected " + expected + ", got " + actual);
        }
    }

private:
    static std::string demangle(const char* name) {
#if defined(__GNUC__) || defined(__clang__)
        int status = 0;
        std::unique_ptr<char, void(*)(void*)> res {
            abi::__cxa_demangle(name, nullptr, nullptr, &status),
            std::free
        };
        return (status == 0) ? res.get() : name;
#else
        return name; // MSVC names are usually readable or we can use another API
#endif
    }
};

} // namespace nexus::utility::types
