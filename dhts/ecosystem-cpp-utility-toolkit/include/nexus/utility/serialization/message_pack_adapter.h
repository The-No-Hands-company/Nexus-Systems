#pragma once
#include <cstring>

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <map>
#include <variant>
#include <sstream>

namespace nexus::utility::serialization {

/// MessagePack-like binary encoding with type preservation
class MessagePackAdapter {
public:
    enum class Format : uint8_t {
        Nil           = 0xC0,
        False         = 0xC2,
        True          = 0xC3,
        Int8          = 0xD0,
        Int16         = 0xD1,
        Int32         = 0xD2,
        Int64         = 0xD3,
        Uint8         = 0xCC,
        Uint16        = 0xCD,
        Uint32        = 0xCE,
        Uint64        = 0xCF,
        Float32       = 0xCA,
        Float64       = 0xCB,
        Str8          = 0xD9,
        Str16         = 0xDA,
        Str32         = 0xDB,
        Array16       = 0xDC,
        Array32       = 0xDD,
        Map16         = 0xDE,
        Map32         = 0xDF,
        Bin8          = 0xC4,
        Bin16         = 0xC5,
        Bin32         = 0xC6,
        FixStr        = 0b10100000,  // mask
        FixArray      = 0b10010000,
        FixMap        = 0b10000000,
        PositiveFixInt = 0b01111111,
        NegativeFixInt = 0b11100000,
    };

    // ---- Encoding ----
    void packNil(std::vector<uint8_t>& out) {
        out.push_back(static_cast<uint8_t>(Format::Nil));
    }

    void packBool(std::vector<uint8_t>& out, bool v) {
        out.push_back(v ? static_cast<uint8_t>(Format::True) : static_cast<uint8_t>(Format::False));
    }

    void packInt(std::vector<uint8_t>& out, int64_t v) {
        if (v >= 0) packUint(out, static_cast<uint64_t>(v));
        else if (v >= -32) {
            out.push_back(static_cast<uint8_t>(v));
        } else if (v >= INT8_MIN) {
            out.push_back(static_cast<uint8_t>(Format::Int8));
            out.push_back(static_cast<uint8_t>(v));
        } else if (v >= INT16_MIN) {
            out.push_back(static_cast<uint8_t>(Format::Int16));
            appendBigEndian(out, static_cast<int16_t>(v));
        } else if (v >= INT32_MIN) {
            out.push_back(static_cast<uint8_t>(Format::Int32));
            appendBigEndian(out, static_cast<int32_t>(v));
        } else {
            out.push_back(static_cast<uint8_t>(Format::Int64));
            appendBigEndian(out, v);
        }
    }

    void packUint(std::vector<uint8_t>& out, uint64_t v) {
        if (v <= 127) {
            out.push_back(static_cast<uint8_t>(v));
        } else if (v <= UINT8_MAX) {
            out.push_back(static_cast<uint8_t>(Format::Uint8));
            out.push_back(static_cast<uint8_t>(v));
        } else if (v <= UINT16_MAX) {
            out.push_back(static_cast<uint8_t>(Format::Uint16));
            appendBigEndian(out, static_cast<uint16_t>(v));
        } else if (v <= UINT32_MAX) {
            out.push_back(static_cast<uint8_t>(Format::Uint32));
            appendBigEndian(out, static_cast<uint32_t>(v));
        } else {
            out.push_back(static_cast<uint8_t>(Format::Uint64));
            appendBigEndian(out, v);
        }
    }

    void packFloat(std::vector<uint8_t>& out, double v) {
        out.push_back(static_cast<uint8_t>(Format::Float64));
        appendBigEndian(out, v);
    }

    void packString(std::vector<uint8_t>& out, const std::string& s) {
        if (s.size() <= 31) {
            out.push_back(static_cast<uint8_t>(Format::FixStr) | static_cast<uint8_t>(s.size()));
        } else if (s.size() <= UINT8_MAX) {
            out.push_back(static_cast<uint8_t>(Format::Str8));
            out.push_back(static_cast<uint8_t>(s.size()));
        } else if (s.size() <= UINT16_MAX) {
            out.push_back(static_cast<uint8_t>(Format::Str16));
            appendBigEndian(out, static_cast<uint16_t>(s.size()));
        } else {
            out.push_back(static_cast<uint8_t>(Format::Str32));
            appendBigEndian(out, static_cast<uint32_t>(s.size()));
        }
        out.insert(out.end(), s.begin(), s.end());
    }

    void packBinary(std::vector<uint8_t>& out, const std::vector<uint8_t>& data) {
        if (data.size() <= UINT8_MAX) {
            out.push_back(static_cast<uint8_t>(Format::Bin8));
            out.push_back(static_cast<uint8_t>(data.size()));
        } else if (data.size() <= UINT16_MAX) {
            out.push_back(static_cast<uint8_t>(Format::Bin16));
            appendBigEndian(out, static_cast<uint16_t>(data.size()));
        } else {
            out.push_back(static_cast<uint8_t>(Format::Bin32));
            appendBigEndian(out, static_cast<uint32_t>(data.size()));
        }
        out.insert(out.end(), data.begin(), data.end());
    }

    void packArrayHeader(std::vector<uint8_t>& out, size_t count) {
        if (count <= 15) {
            out.push_back(static_cast<uint8_t>(Format::FixArray) | static_cast<uint8_t>(count));
        } else if (count <= UINT16_MAX) {
            out.push_back(static_cast<uint8_t>(Format::Array16));
            appendBigEndian(out, static_cast<uint16_t>(count));
        } else {
            out.push_back(static_cast<uint8_t>(Format::Array32));
            appendBigEndian(out, static_cast<uint32_t>(count));
        }
    }

    void packMapHeader(std::vector<uint8_t>& out, size_t count) {
        if (count <= 15) {
            out.push_back(static_cast<uint8_t>(Format::FixMap) | static_cast<uint8_t>(count));
        } else if (count <= UINT16_MAX) {
            out.push_back(static_cast<uint8_t>(Format::Map16));
            appendBigEndian(out, static_cast<uint16_t>(count));
        } else {
            out.push_back(static_cast<uint8_t>(Format::Map32));
            appendBigEndian(out, static_cast<uint32_t>(count));
        }
    }

    // ---- Decoding ----
    struct DecodedValue {
        enum class Type { Nil, Bool, Int, Uint, Float, String, Binary, Array, Map } type;
        bool bool_val = false;
        int64_t int_val = 0;
        uint64_t uint_val = 0;
        double float_val = 0.0;
        std::string string_val;
        std::vector<uint8_t> binary_val;
        size_t array_count = 0;
        size_t map_count = 0;
    };

    DecodedValue unpackOne(const std::vector<uint8_t>& data, size_t& pos) {
        if (pos >= data.size()) throw std::runtime_error("MessagePack: unexpected end of data");
        uint8_t byte = data[pos++];

        // Nil
        if (byte == 0xC0) return {DecodedValue::Type::Nil};

        // Bool
        if (byte == 0xC2) return {DecodedValue::Type::Bool, false};
        if (byte == 0xC3) return {DecodedValue::Type::Bool, true};

        // Positive fixint
        if ((byte & 0x80) == 0) {
            DecodedValue v;
            v.type = DecodedValue::Type::Uint;
            v.uint_val = byte;
            return v;
        }

        // Negative fixint
        if ((byte & 0xE0) == 0xE0) {
            DecodedValue v;
            v.type = DecodedValue::Type::Int;
            v.int_val = static_cast<int8_t>(byte);
            return v;
        }

        // FixStr
        if ((byte & 0xE0) == 0xA0) {
            size_t len = byte & 0x1F;
            DecodedValue v;
            v.type = DecodedValue::Type::String;
            v.string_val.assign(reinterpret_cast<const char*>(data.data() + pos), len);
            pos += len;
            return v;
        }

        // FixArray
        if ((byte & 0xF0) == 0x90) {
            return {DecodedValue::Type::Array, false, 0, 0, 0.0, "", {}, static_cast<size_t>(byte & 0x0F)};
        }

        // FixMap
        if ((byte & 0xF0) == 0x80) {
            return {DecodedValue::Type::Map, false, 0, 0, 0.0, "", {}, 0, static_cast<size_t>(byte & 0x0F)};
        }

        switch (static_cast<Format>(byte)) {
            case Format::Int8:  return {DecodedValue::Type::Int,  false, static_cast<int64_t>(static_cast<int8_t>(data[pos++]))};
            case Format::Int16: { auto v = readBigEndian<int16_t>(data, pos); return {DecodedValue::Type::Int, false, v}; }
            case Format::Int32: { auto v = readBigEndian<int32_t>(data, pos); return {DecodedValue::Type::Int, false, v}; }
            case Format::Int64: { auto v = readBigEndian<int64_t>(data, pos); return {DecodedValue::Type::Int, false, v}; }
            case Format::Uint8:  return {DecodedValue::Type::Uint, false, 0, static_cast<uint64_t>(data[pos++])};
            case Format::Uint16: { auto v = readBigEndian<uint16_t>(data, pos); return {DecodedValue::Type::Uint, false, 0, v}; }
            case Format::Uint32: { auto v = readBigEndian<uint32_t>(data, pos); return {DecodedValue::Type::Uint, false, 0, v}; }
            case Format::Uint64: { auto v = readBigEndian<uint64_t>(data, pos); return {DecodedValue::Type::Uint, false, 0, v}; }
            case Format::Float32: { uint32_t raw = readBigEndian<uint32_t>(data, pos); float v; std::memcpy(&v, &raw, sizeof(v)); return {DecodedValue::Type::Float, false, 0, 0, static_cast<double>(v)}; }
            case Format::Float64: { uint64_t raw = readBigEndian<uint64_t>(data, pos); double v; std::memcpy(&v, &raw, sizeof(v)); return {DecodedValue::Type::Float, false, 0, 0, v}; }
            case Format::Str8:  { size_t len = data[pos++]; return readString(data, pos, len); }
            case Format::Str16: { auto len = readBigEndian<uint16_t>(data, pos); return readString(data, pos, len); }
            case Format::Str32: { auto len = readBigEndian<uint32_t>(data, pos); return readString(data, pos, len); }
            case Format::Bin8:  { size_t len = data[pos++]; return readBinary(data, pos, len); }
            case Format::Bin16: { auto len = readBigEndian<uint16_t>(data, pos); return readBinary(data, pos, len); }
            case Format::Bin32: { auto len = readBigEndian<uint32_t>(data, pos); return readBinary(data, pos, len); }
            case Format::Array16: return {DecodedValue::Type::Array, false, 0, 0, 0.0, "", {}, readBigEndian<uint16_t>(data, pos)};
            case Format::Array32: return {DecodedValue::Type::Array, false, 0, 0, 0.0, "", {}, readBigEndian<uint32_t>(data, pos)};
            case Format::Map16:   return {DecodedValue::Type::Map, false, 0, 0, 0.0, "", {}, 0, readBigEndian<uint16_t>(data, pos)};
            case Format::Map32:   return {DecodedValue::Type::Map, false, 0, 0, 0.0, "", {}, 0, readBigEndian<uint32_t>(data, pos)};
            default: throw std::runtime_error("MessagePack: unknown format byte 0x" + toHex(byte));
        }
    }

private:
    template<typename T>
    static void appendBigEndian(std::vector<uint8_t>& out, T val) {
        if constexpr (std::is_floating_point_v<T>) {
            // Floats can't use bit shifts - convert via integer representation
            using UInt = std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>;
            UInt raw;
            std::memcpy(&raw, &val, sizeof(T));
            for (size_t i = sizeof(T); i > 0; --i) {
                out.push_back(static_cast<uint8_t>(raw >> ((i - 1) * 8)));
            }
        } else {
            for (size_t i = sizeof(T); i > 0; --i) {
                out.push_back(static_cast<uint8_t>(val >> ((i - 1) * 8)));
            }
        }
    }

    template<typename T>
    static T readBigEndian(const std::vector<uint8_t>& data, size_t& pos) {
        T val = 0;
        for (size_t i = 0; i < sizeof(T); ++i) {
            val = (val << 8) | data[pos++];
        }
        return val;
    }

    static DecodedValue readString(const std::vector<uint8_t>& data, size_t& pos, size_t len) {
        DecodedValue v;
        v.type = DecodedValue::Type::String;
        v.string_val.assign(reinterpret_cast<const char*>(data.data() + pos), len);
        pos += len;
        return v;
    }

    static DecodedValue readBinary(const std::vector<uint8_t>& data, size_t& pos, size_t len) {
        DecodedValue v;
        v.type = DecodedValue::Type::Binary;
        v.binary_val.assign(data.begin() + pos, data.begin() + pos + len);
        pos += len;
        return v;
    }

    static std::string toHex(uint8_t byte) {
        const char* hex = "0123456789ABCDEF";
        return {hex[byte >> 4], hex[byte & 0x0F]};
    }
};

} // namespace nexus::utility::serialization
