#pragma once

#include <vector>
#include <stdexcept>
#include <optional>
#include <cstddef>

namespace nexus::utility::containers {

/// @brief Fixed-size circular buffer with overwrite semantics. Not thread-safe.
template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : buffer_(capacity > 0 ? capacity : 1)
        , capacity_(capacity > 0 ? capacity : 1)
        , head_(0), tail_(0), size_(0) {
        static_assert(std::is_nothrow_move_constructible_v<T> ||
                      std::is_copy_constructible_v<T>,
                      "T must be copyable or moveable");
    }

    /// @brief Pushes an item. Overwrites oldest if full.
    void push(const T& value) {
        buffer_[tail_] = value;
        advance();
    }

    void push(T&& value) {
        buffer_[tail_] = std::move(value);
        advance();
    }

    /// @brief Pops the oldest item.
    [[nodiscard]] std::optional<T> pop() noexcept {
        if (empty()) {
            return std::nullopt;
        }

        T value = std::move_if_noexcept(buffer_[head_]);
        head_ = (head_ + 1) % capacity_;
        --size_;
        return value;
    }

    /// @brief Peek at the oldest element without removing.
    [[nodiscard]] const T& front() const {
        if (empty()) throw std::out_of_range("RingBuffer::front: buffer is empty");
        return buffer_[head_];
    }

    /// @brief Peek at the newest element without removing.
    [[nodiscard]] const T& back() const {
        if (empty()) throw std::out_of_range("RingBuffer::back: buffer is empty");
        size_t idx = tail_ == 0 ? capacity_ - 1 : tail_ - 1;
        return buffer_[idx];
    }

    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] bool isFull() const noexcept { return size_ == capacity_; }
    [[nodiscard]] size_t size() const noexcept { return size_; }
    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }

    /// @brief Access element by index (0 = oldest). Bounds-checked.
    [[nodiscard]] const T& operator[](size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("RingBuffer index out of range");
        }
        return buffer_[(head_ + index) % capacity_];
    }

    [[nodiscard]] T& operator[](size_t index) {
        if (index >= size_) {
            throw std::out_of_range("RingBuffer index out of range");
        }
        return buffer_[(head_ + index) % capacity_];
    }

    void clear() noexcept {
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }

private:
    std::vector<T> buffer_;
    size_t capacity_;
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t size_ = 0;

    void advance() {
        tail_ = (tail_ + 1) % capacity_;
        if (size_ == capacity_) {
            head_ = (head_ + 1) % capacity_;
        } else {
            ++size_;
        }
    }
};

} // namespace nexus::utility::containers
