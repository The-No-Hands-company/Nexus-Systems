#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <concepts>
#include <stdexcept>

namespace nexus::utility::serialization {

/// Binary serialization with versioning and endianness control
class BinarySerializer {
public:
    enum class Endianness { Little, Big, Native };

    explicit BinarySerializer(Endianness endian = Endianness::Little)
        : endianness_(endian) {}

    // ---- Writing ----
    void clear() { buffer_.clear(); pos_ = 0; }
    const std::vector<uint8_t>& data() const { return buffer_; }
    std::vector<uint8_t> release() { auto d = std::move(buffer_); clear(); return d; }
    size_t size() const { return buffer_.size(); }

    void write(const void* data, size_t size) {
        buffer_.insert(buffer_.end(),
                       static_cast<const uint8_t*>(data),
                       static_cast<const uint8_t*>(data) + size);
    }

    template<typename T> requires std::is_arithmetic_v<T>
    void writeValue(T value) {
        if (endianness_ != Endianness::Native && sizeof(T) > 1) {
            value = swapEndian(value);
        }
        write(&value, sizeof(T));
    }

    void writeString(const std::string& s) {
        writeValue(static_cast<uint32_t>(s.size()));
        write(s.data(), s.size());
    }

    void writeBytes(const std::vector<uint8_t>& bytes) {
        writeValue(static_cast<uint32_t>(bytes.size()));
        write(bytes.data(), bytes.size());
    }

    template<typename T>
    void writeVector(const std::vector<T>& vec) {
        writeValue(static_cast<uint32_t>(vec.size()));
        for (const auto& item : vec) {
            if constexpr (std::is_arithmetic_v<T>) {
                writeValue(item);
            } else {
                item.serialize(*this);
            }
        }
    }

    // ---- Reading ----
    void load(const std::vector<uint8_t>& data) {
        buffer_ = data;
        pos_ = 0;
    }

    void read(void* data, size_t size) {
        if (pos_ + size > buffer_.size()) {
            throw std::runtime_error("BinarySerializer: read past end of buffer");
        }
        std::memcpy(data, buffer_.data() + pos_, size);
        pos_ += size;
    }

    template<typename T> requires std::is_arithmetic_v<T>
    T readValue() {
        T value;
        read(&value, sizeof(T));
        if (endianness_ != Endianness::Native && sizeof(T) > 1) {
            value = swapEndian(value);
        }
        return value;
    }

    std::string readString() {
        auto len = readValue<uint32_t>();
        if (pos_ + len > buffer_.size()) {
            throw std::runtime_error("BinarySerializer: string length exceeds buffer");
        }
        std::string result(reinterpret_cast<const char*>(buffer_.data() + pos_), len);
        pos_ += len;
        return result;
    }

    std::vector<uint8_t> readBytes() {
        auto len = readValue<uint32_t>();
        if (pos_ + len > buffer_.size()) {
            throw std::runtime_error("BinarySerializer: bytes length exceeds buffer");
        }
        std::vector<uint8_t> result(buffer_.begin() + pos_, buffer_.begin() + pos_ + len);
        pos_ += len;
        return result;
    }

    template<typename T>
    std::vector<T> readVector() {
        auto count = readValue<uint32_t>();
        std::vector<T> result;
        result.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            if constexpr (std::is_arithmetic_v<T>) {
                result.push_back(readValue<T>());
            } else {
                T item;
                item.deserialize(*this);
                result.push_back(std::move(item));
            }
        }
        return result;
    }

    size_t remaining() const { return buffer_.size() - pos_; }

private:
    std::vector<uint8_t> buffer_;
    size_t pos_ = 0;
    Endianness endianness_;

    template<typename T>
    static T swapEndian(T value) {
        auto* bytes = reinterpret_cast<uint8_t*>(&value);
        for (size_t i = 0; i < sizeof(T) / 2; ++i) {
            std::swap(bytes[i], bytes[sizeof(T) - 1 - i]);
        }
        return value;
    }
};

/// Schema-based binary format version validator
class SchemaValidator {
public:
    struct Field {
        std::string name;
        size_t offset;
        size_t size;
        std::string type_name;
    };

    struct Schema {
        uint32_t version = 1;
        std::string name;
        std::vector<Field> fields;

        size_t totalSize() const {
            if (fields.empty()) return 0;
            const auto& last = fields.back();
            return last.offset + last.size;
        }
    };

    explicit SchemaValidator(Schema schema) : schema_(std::move(schema)) {}

    bool validate(const std::vector<uint8_t>& data) const {
        return data.size() >= schema_.totalSize();
    }

    const Schema& schema() const { return schema_; }
    uint32_t version() const { return schema_.version; }

    /// Write schema header into a binary buffer
    void writeHeader(BinarySerializer& ser) const {
        ser.writeValue(schema_.version);
        ser.writeString(schema_.name);
        ser.writeValue(static_cast<uint32_t>(schema_.fields.size()));
        for (const auto& field : schema_.fields) {
            ser.writeString(field.name);
            ser.writeValue(static_cast<uint32_t>(field.offset));
            ser.writeValue(static_cast<uint32_t>(field.size));
            ser.writeString(field.type_name);
        }
    }

    /// Read and validate schema header
    static Schema readHeader(BinarySerializer& ser) {
        Schema s;
        s.version = ser.readValue<uint32_t>();
        s.name = ser.readString();
        auto field_count = ser.readValue<uint32_t>();
        for (uint32_t i = 0; i < field_count; ++i) {
            Field f;
            f.name = ser.readString();
            f.offset = ser.readValue<uint32_t>();
            f.size = ser.readValue<uint32_t>();
            f.type_name = ser.readString();
            s.fields.push_back(std::move(f));
        }
        return s;
    }

private:
    Schema schema_;
};

} // namespace nexus::utility::serialization
