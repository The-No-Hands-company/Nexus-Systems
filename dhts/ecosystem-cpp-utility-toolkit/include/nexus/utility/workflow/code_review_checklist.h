#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>

namespace nexus::utility::workflow {

/**
 * @brief Track code review checklist items and completion state.
 */
class CodeReviewChecklist {
public:
    enum class Category { Correctness, Style, Security, Performance, Testing, Documentation };

    struct Item {
        std::string description;
        Category category = Category::Correctness;
        bool required = true;
        bool checked = false;
    };

    static CodeReviewChecklist& instance() {
        static CodeReviewChecklist inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    std::size_t addItem(const std::string& description, Category category,
                        bool required = true) {
        std::lock_guard<std::mutex> lk(mutex_);
        items_.push_back({description, category, required, false});
        return items_.size() - 1;
    }

    void check(std::size_t index, bool value = true) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (index < items_.size()) items_[index].checked = value;
    }

    /// True when all required items are checked.
    bool isComplete() const {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& it : items_)
            if (it.required && !it.checked) return false;
        return true;
    }

    double completionPercentage() const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (items_.empty()) return 100.0;
        std::size_t done = 0;
        for (const auto& it : items_) if (it.checked) ++done;
        return 100.0 * done / items_.size();
    }

    std::vector<Item> pendingRequired() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Item> out;
        for (const auto& it : items_) if (it.required && !it.checked) out.push_back(it);
        return out;
    }

    std::vector<Item> items() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return items_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        items_.clear();
    }

private:
    CodeReviewChecklist() = default;
    ~CodeReviewChecklist() = default;
    CodeReviewChecklist(const CodeReviewChecklist&) = delete;
    CodeReviewChecklist& operator=(const CodeReviewChecklist&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<Item> items_;
};

} // namespace nexus::utility::workflow
