#pragma once

#include <type_traits>
#include <concepts>
#include <tuple>
#include <iterator>
#include <string>
#include <vector>
#include <array>

namespace nexus::utility::reflection {

/// @brief Extended type traits beyond std::type_traits.
/// Provides concepts, type detection, and function signature introspection.

// ── Concepts ────────────────────────────────────────────────────────────────

template<typename T> concept is_aggregate = std::is_aggregate_v<T>;

template<typename T>
concept tuple_like = requires {
    typename std::tuple_size<T>::type;
} && requires(T t) { { std::get<0>(t) }; };

template<typename T>
concept is_iterable = requires(T& t) {
    { t.begin() } -> std::input_or_output_iterator;
    { t.end() } -> std::input_or_output_iterator;
};

template<typename T>
concept has_size = requires(const T& t) { { t.size() } -> std::convertible_to<std::size_t>; };

template<typename T>
concept has_empty = requires(const T& t) { { t.empty() } -> std::convertible_to<bool>; };

template<typename T>
concept container_like = is_iterable<T> && has_size<T>;

template<typename T>
concept numeric = std::integral<T> || std::floating_point<T>;

template<typename T>
concept string_like = std::convertible_to<T, std::string_view>;

// ── Function Signature ──────────────────────────────────────────────────────

template<typename T> struct function_signature;
template<typename Ret, typename... Args>
struct function_signature<Ret(Args...)> {
    using return_type = Ret;
    using argument_types = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};
template<typename Ret, typename... Args>
struct function_signature<Ret(*)(Args...)> : function_signature<Ret(Args...)> {};
template<typename Class, typename Ret, typename... Args>
struct function_signature<Ret(Class::*)(Args...)> : function_signature<Ret(Args...)> {
    using class_type = Class;
};
template<typename Class, typename Ret, typename... Args>
struct function_signature<Ret(Class::*)(Args...) const> : function_signature<Ret(Args...)> {
    using class_type = Class;
};

// ── Type Queries ────────────────────────────────────────────────────────────

template<typename T> constexpr const char* type_name() { return __PRETTY_FUNCTION__; }
template<typename T> using remove_all_t = std::remove_cv_t<std::remove_reference_t<T>>;
template<typename T, typename U>
concept same_ignoring_qualifiers = std::same_as<remove_all_t<T>, remove_all_t<U>>;

// ── Member Detection ────────────────────────────────────────────────────────

template<typename T, typename M>
concept has_member = requires(T t) { { t.*std::declval<M T::*>() }; };

template<typename T>
concept has_push_back = requires(T& c, const typename T::value_type& v) { c.push_back(v); };

template<typename T>
concept has_reserve = requires(T& c, size_t n) { c.reserve(n); };

// ── Type Lists ──────────────────────────────────────────────────────────────

template<typename... Ts> struct type_list {};
template<typename T> struct type_list_size;
template<typename... Ts> struct type_list_size<type_list<Ts...>> : std::integral_constant<size_t, sizeof...(Ts)> {};

template<size_t I, typename T> struct type_list_element;
template<typename T, typename... Ts>
struct type_list_element<0, type_list<T, Ts...>> { using type = T; };
template<size_t I, typename T, typename... Ts>
struct type_list_element<I, type_list<T, Ts...>> : type_list_element<I - 1, type_list<Ts...>> {};

// ── Utility Methods Class ───────────────────────────────────────────────────

class TypeTraits {
public:
    /// @brief Check if two types are the same at compile time.
    template<typename T, typename U>
    [[nodiscard]] static constexpr bool isSame() noexcept { return std::same_as<T, U>; }

    /// @brief Check if a type satisfies a predicate at runtime (always true if compiles).
    template<typename T>
    [[nodiscard]] static constexpr bool isDefaultConstructible() noexcept { return std::is_default_constructible_v<T>; }

    template<typename T>
    [[nodiscard]] static constexpr bool isCopyable() noexcept { return std::is_copy_constructible_v<T>; }

    template<typename T>
    [[nodiscard]] static constexpr bool isMovable() noexcept { return std::is_move_constructible_v<T>; }

    /// @brief Get the alignment requirement of a type.
    template<typename T>
    [[nodiscard]] static constexpr size_t alignment() noexcept { return alignof(T); }

    /// @brief Get the size of a type.
    template<typename T>
    [[nodiscard]] static constexpr size_t sizeOf() noexcept { return sizeof(T); }

    /// @brief Get a human-readable name for common types.
    template<typename T>
    [[nodiscard]] static std::string getName() {
        if constexpr (std::same_as<T, int>) return "int";
        else if constexpr (std::same_as<T, double>) return "double";
        else if constexpr (std::same_as<T, std::string>) return "std::string";
        else if constexpr (std::same_as<T, bool>) return "bool";
        else if constexpr (std::same_as<T, float>) return "float";
        else if constexpr (std::same_as<T, char>) return "char";
        else if constexpr (std::same_as<T, void>) return "void";
        else return "unknown";
    }

    /// @brief Verify at compile time that a type has specific traits.
    template<typename T>
    static consteval void verifyContainer() {
        static_assert(is_iterable<T>, "T must be iterable");
        static_assert(has_size<T>, "T must have size()");
    }
};

} // namespace nexus::utility::reflection
