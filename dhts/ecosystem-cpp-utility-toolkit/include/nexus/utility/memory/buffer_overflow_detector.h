#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <memory>

namespace nexus::utility::memory {

/**
 * @brief Buffer overflow detection with canaries and bounds checking.
 */
class BufferOverflowDetector {
public:
    static constexpr uint32_t CANARY_VALUE = 0xDEADBEEF;
    static constexpr size_t CANARY_SIZE = sizeof(uint32_t);

    /**
     * @brief Guarded buffer with canaries.
     */
    template<typename T>
    class GuardedBuffer {
    public:
        explicit GuardedBuffer(size_t size) : size_(size) {
            // Allocate with front and back canaries
            total_size_ = CANARY_SIZE + (size * sizeof(T)) + CANARY_SIZE;
            raw_buffer_ = std::make_unique<uint8_t[]>(total_size_);
            
            // Set canaries
            setCanary(raw_buffer_.get(), CANARY_VALUE);
            setCanary(raw_buffer_.get() + CANARY_SIZE + (size * sizeof(T)), CANARY_VALUE);
            
            // Get data pointer
            data_ = reinterpret_cast<T*>(raw_buffer_.get() + CANARY_SIZE);
        }

        ~GuardedBuffer() {
            checkCanaries();
        }

        T& operator[](size_t index) {
            if (index >= size_) {
                throw std::out_of_range("Buffer overflow detected: index out of bounds");
            }
            return data_[index];
        }

        const T& operator[](size_t index) const {
            if (index >= size_) {
                throw std::out_of_range("Buffer overflow detected: index out of bounds");
            }
            return data_[index];
        }

        T* data() { return data_; }
        const T* data() const { return data_; }
        size_t size() const { return size_; }

        /**
         * @brief Checks if canaries are intact.
         */
        bool checkCanaries() const {
            uint32_t front_canary = getCanary(raw_buffer_.get());
            uint32_t back_canary = getCanary(raw_buffer_.get() + CANARY_SIZE + (size_ * sizeof(T)));
            
            if (front_canary != CANARY_VALUE) {
                throw std::runtime_error("Buffer underflow detected: front canary corrupted");
            }
            if (back_canary != CANARY_VALUE) {
                throw std::runtime_error("Buffer overflow detected: back canary corrupted");
            }
            return true;
        }

    private:
        static void setCanary(uint8_t* addr, uint32_t value) {
            std::memcpy(addr, &value, sizeof(value));
        }

        static uint32_t getCanary(const uint8_t* addr) {
            uint32_t value;
            std::memcpy(&value, addr, sizeof(value));
            return value;
        }

        size_t size_;
        size_t total_size_;
        std::unique_ptr<uint8_t[]> raw_buffer_;
        T* data_;
    };

    /**
     * @brief Bounds-checked array access.
     */
    template<typename T>
    static T& checkedAccess(T* array, size_t index, size_t size) {
        if (index >= size) {
            throw std::out_of_range("Array access out of bounds");
        }
        return array[index];
    }

    /**
     * @brief Bounds-checked memory copy.
     */
    static void checkedMemcpy(void* dest, size_t dest_size, 
                              const void* src, size_t src_size) {
        if (src_size > dest_size) {
            throw std::runtime_error("Buffer overflow in memcpy: source larger than destination");
        }
        std::memcpy(dest, src, src_size);
    }

    /**
     * @brief Checks if a pointer is within bounds.
     */
    template<typename T>
    static bool isWithinBounds(const T* ptr, const T* base, size_t size) {
        return ptr >= base && ptr < (base + size);
    }
};

} // namespace nexus::utility::memory
