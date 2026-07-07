#pragma once

#include <vector>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <stdexcept>

namespace nexus::utility::encoding {

/**
 * @brief Simple binary serialization for trivially copyable types.
 *
 * NOTE: Serialized output uses host-native byte order (endianness).
 * Data is NOT portable between architectures with different endianness
 * (e.g., x86 little-endian vs ARM big-endian).
 * For cross-platform serialization, use MessagePack or similar format.
 */
class BinarySerializer {
public:
    /**
     * @brief Serializes a trivially copyable value to bytes.
     */
    template<typename T>
    requires std::is_trivially_copyable_v<T>
    static std::vector<uint8_t> serialize(const T& value) {
        std::vector<uint8_t> bytes(sizeof(T));
        std::memcpy(bytes.data(), &value, sizeof(T));
        return bytes;
    }

    /**
     * @brief Deserializes bytes to a trivially copyable value.
     */
    template<typename T>
    requires std::is_trivially_copyable_v<T>
    static T deserialize(const std::vector<uint8_t>& bytes) {
        if (bytes.size() != sizeof(T)) {
            throw std::invalid_argument("Byte size mismatch for deserialization");
        }
        
        T value;
        std::memcpy(&value, bytes.data(), sizeof(T));
        return value;
    }

    /**
     * @brief Deserializes bytes from raw pointer.
     */
    template<typename T>
    requires std::is_trivially_copyable_v<T>
    static T deserialize(const uint8_t* data, size_t size) {
        if (size != sizeof(T)) {
            throw std::invalid_argument("Byte size mismatch for deserialization");
        }
        
        T value;
        std::memcpy(&value, data, sizeof(T));
        return value;
    }
};

} // namespace nexus::utility::encoding
