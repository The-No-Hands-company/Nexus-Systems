#pragma once

#include <unordered_map>
#include <list>
#include <optional>
#include <stdexcept>
#include <shared_mutex>
#include <mutex>
#include <utility>

namespace nexus::utility::containers {

/// @brief Thread-safe Least Recently Used (LRU) cache with O(1) operations
template <typename Key, typename Value>
class LRUCache {
public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {
        if (capacity == 0) {
            throw std::invalid_argument("LRUCache capacity must be > 0");
        }
    }

    /// @brief Gets a value from the cache (promotes to most-recently-used)
    [[nodiscard]] std::optional<Value> get(const Key& key) {
        std::unique_lock lock(mutex_);
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return std::nullopt;
        }

        items_.splice(items_.begin(), items_, it->second);
        return it->second->second;
    }

    /// @brief Checks if a key exists without modifying LRU order
    [[nodiscard]] bool contains(const Key& key) const noexcept {
        std::shared_lock lock(mutex_);
        return cache_.find(key) != cache_.end();
    }

    /// @brief Puts a key-value pair (copy)
    void put(const Key& key, const Value& value) {
        std::unique_lock lock(mutex_);
        putImpl(key, value);
    }

    /// @brief Puts a key-value pair with move semantics
    void put(const Key& key, Value&& value) {
        std::unique_lock lock(mutex_);
        putImplMove(key, std::move(value));
    }

    /// @brief Puts a key-value pair (move key, move value)
    void put(Key&& key, Value&& value) {
        std::unique_lock lock(mutex_);
        putImplMove(std::move(key), std::move(value));
    }

    /// @brief Removes a key from the cache
    bool erase(const Key& key) {
        std::unique_lock lock(mutex_);
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return false;
        }
        items_.erase(it->second);
        cache_.erase(it);
        return true;
    }

    [[nodiscard]] size_t size() const noexcept {
        std::shared_lock lock(mutex_);
        return cache_.size();
    }

    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }

    [[nodiscard]] bool empty() const noexcept {
        std::shared_lock lock(mutex_);
        return cache_.empty();
    }

    void clear() noexcept {
        std::unique_lock lock(mutex_);
        cache_.clear();
        items_.clear();
    }

private:
    using ItemList = std::list<std::pair<Key, Value>>;
    using CacheMap = std::unordered_map<Key, typename ItemList::iterator>;

    size_t capacity_;
    ItemList items_;
    CacheMap cache_;
    mutable std::shared_mutex mutex_;

    void putImpl(const Key& key, const Value& value) {
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            it->second->second = value;
            items_.splice(items_.begin(), items_, it->second);
            return;
        }

        if (cache_.size() >= capacity_) {
            auto last = items_.back();
            cache_.erase(last.first);
            items_.pop_back();
        }

        items_.emplace_front(key, value);
        cache_[key] = items_.begin();
    }

    void putImplMove(Key&& key, Value&& value) {
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            it->second->second = std::move(value);
            items_.splice(items_.begin(), items_, it->second);
            return;
        }

        if (cache_.size() >= capacity_) {
            auto last = items_.back();
            cache_.erase(last.first);
            items_.pop_back();
        }

        items_.emplace_front(std::move(key), std::move(value));
        cache_[items_.front().first] = items_.begin();
    }

    void putImplMove(const Key& key, Value&& value) {
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            it->second->second = std::move(value);
            items_.splice(items_.begin(), items_, it->second);
            return;
        }

        if (cache_.size() >= capacity_) {
            auto last = items_.back();
            cache_.erase(last.first);
            items_.pop_back();
        }

        items_.emplace_front(key, std::move(value));
        cache_[key] = items_.begin();
    }
};

} // namespace nexus::utility::containers
