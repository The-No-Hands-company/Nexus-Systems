#pragma once
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <expected>
#include <ostream>
#include <sstream>

namespace nexus::utility::cpp26 {

/// Debug C++26 std::expected monadic chains (and_then, or_else, transform, transform_error)
template<typename T, typename E>
class ExpectedChainDebugger {
public:
    using Expected = std::expected<T, E>;

    struct ChainStep {
        std::string operation;
        bool hadValue;
        std::string description;
    };

    explicit ExpectedChainDebugger(Expected exp) : result_(std::move(exp)) {
        record("initial", result_.has_value(), "");
    }

    template<typename Func>
    auto andThen(Func&& func) -> ExpectedChainDebugger<
        typename std::invoke_result_t<Func, T>::value_type, E> {
        record("and_then", result_.has_value(), "");
        if (!result_.has_value()) {
            return ExpectedChainDebugger<typename std::invoke_result_t<Func, T>::value_type, E>(
                std::unexpected(result_.error()));
        }
        return ExpectedChainDebugger<typename std::invoke_result_t<Func, T>::value_type, E>(
            std::invoke(std::forward<Func>(func), std::move(*result_)));
    }

    template<typename Func>
    auto orElse(Func&& func) -> ExpectedChainDebugger<T, typename std::invoke_result_t<Func, E>::value_type> {
        record("or_else", result_.has_value(), "");
        if (result_.has_value()) {
            return ExpectedChainDebugger<T, typename std::invoke_result_t<Func, E>::value_type>(
                std::move(*result_));
        }
        return ExpectedChainDebugger<T, typename std::invoke_result_t<Func, E>::value_type>(
            std::invoke(std::forward<Func>(func), std::move(result_.error())));
    }

    template<typename Func>
    auto transform(Func&& func) -> ExpectedChainDebugger<
        std::invoke_result_t<Func, T>, E> {
        record("transform", result_.has_value(), "");
        if (!result_.has_value()) {
            return ExpectedChainDebugger<std::invoke_result_t<Func, T>, E>(
                std::unexpected(result_.error()));
        }
        return ExpectedChainDebugger<std::invoke_result_t<Func, T>, E>(
            std::invoke(std::forward<Func>(func), std::move(*result_)));
    }

    template<typename Func>
    auto transformError(Func&& func) -> ExpectedChainDebugger<T, std::invoke_result_t<Func, E>> {
        record("transform_error", result_.has_value(), "");
        if (result_.has_value()) {
            return ExpectedChainDebugger<T, std::invoke_result_t<Func, E>>(
                std::move(*result_));
        }
        return ExpectedChainDebugger<T, std::invoke_result_t<Func, E>>(
            std::unexpected(std::invoke(std::forward<Func>(func), std::move(result_.error()))));
    }

    void printChain(std::ostream& os = std::cout) const {
        os << "=== Expected Chain Debug (C++26) ===\n";
        for (const auto& step : chain_) {
            os << "  " << step.operation << ": "
               << (step.hadValue ? "value" : "error");
            if (!step.description.empty()) os << " (" << step.description << ")";
            os << "\n";
        }
        os << "Final: " << (result_.has_value() ? "value" : "error") << "\n";
        os << "====================================\n";
    }

    std::vector<ChainStep> getChain() const { return chain_; }

    const Expected& get() const { return result_; }

private:
    Expected result_;
    std::vector<ChainStep> chain_;
    std::string lastError_;

    void record(const std::string& op, bool hasValue, const std::string& desc) {
        chain_.push_back({op, hasValue, desc});
    }
};
} // namespace nexus::utility::cpp26
