#pragma once

#include <exception>
#include <string>
#include <vector>
#include <sstream>
#include <utility>
#include <algorithm>

namespace nexus::utility::error {

/// @brief Mixin class to add context to any exception
class ExceptionContext {
public:
    virtual ~ExceptionContext() = default;

    /// @brief Add a key-value context pair
    template<typename T>
    void addContext(const std::string& key, const T& value) {
        std::ostringstream oss;
        oss << value;
        context_.emplace_back(key, oss.str());
        onContextChanged();
    }

    /// @brief Get formatted context string
    std::string getContextString() const {
        std::ostringstream oss;
        for (const auto& [k, v] : context_) {
            oss << "\n  [Context] " << k << ": " << v;
        }
        return oss.str();
    }
    
    const std::vector<std::pair<std::string, std::string>>& getContext() const {
        return context_;
    }

protected:
    /// @brief Hook for derived classes to invalidate caches
    virtual void onContextChanged() const {}

private:
    std::vector<std::pair<std::string, std::string>> context_;
};

/// @brief Base class for exceptions that support context
class ContextException : public std::exception, public ExceptionContext {
public:
    explicit ContextException(std::string message) : message_(std::move(message)) {}

    const char* what() const noexcept override {
        try {
            if (dirty_ || full_message_.empty()) {
                full_message_ = message_ + getContextString();
                dirty_ = false;
            }
            return full_message_.c_str();
        } catch (...) {
            // fallback if allocation fails
            return message_.c_str();
        }
    }
    
    // Override addContext to mark dirty - Wait, addContext is in ExceptionContext mixin.
    // We cannot easily override a template method.
    // Instead, let's make ExceptionContext carry the state or mark dirty differently?
    // Or just always rebuild if getting what()? No, that's expensive.
    // Alternative: ExceptionContext can expose a 'version' counter or similar?
    // Or simpler: ContextException owns the buffer and we assume standard usage is: create, add context, throw.
    // If we catch and add more context, we might need invalidation.
    
    std::string getFullMessage() const {
        return message_ + getContextString();
    }

protected:
    void onContextChanged() const override {
        dirty_ = true;
    }

private:
    std::string message_;
    mutable std::string full_message_;
    mutable bool dirty_ = true;
};

} // namespace nexus::utility::error
