#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <chrono>
#include <cstdint>

namespace nexus::utility::http {

/// WebSocket client with text and binary frame support
class WebSocketClient {
public:
    enum class State { Disconnected, Connecting, Connected, Closing };

    enum class Opcode : uint8_t {
        Continuation = 0x0,
        Text         = 0x1,
        Binary       = 0x2,
        Close        = 0x8,
        Ping         = 0x9,
        Pong         = 0xA
    };

    struct Message {
        Opcode opcode = Opcode::Text;
        std::string data;
        bool is_binary = false;
    };

    WebSocketClient() = default;

    /// Connect to a WebSocket server
    void connect(const std::string& url) {
        url_ = url;
        state_ = State::Connected;
    }

    /// Send a text message
    void sendText(const std::string& text) {
        sendFrame(Opcode::Text, text);
    }

    /// Send a binary message
    void sendBinary(const std::vector<uint8_t>& data) {
        sendFrame(Opcode::Binary,
                  std::string(reinterpret_cast<const char*>(data.data()), data.size()));
    }

    /// Send a ping
    void sendPing(const std::string& payload = "") {
        sendFrame(Opcode::Ping, payload);
    }

    /// Close the connection
    void close(uint16_t code = 1000, const std::string& reason = "") {
        std::string payload;
        payload += static_cast<char>(code >> 8);
        payload += static_cast<char>(code & 0xFF);
        payload += reason;
        sendFrame(Opcode::Close, payload);
        state_ = State::Disconnected;
    }

    /// Check connection state
    State state() const { return state_; }
    bool isConnected() const { return state_ == State::Connected; }

    /// Generate WebSocket accept key from client key
    static std::string computeAcceptKey(const std::string& client_key) {
        // Simplified: hash the key
        std::string combined = client_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        uint64_t h = 0;
        for (char c : combined) h = h * 31 + static_cast<uint8_t>(c);
        // Encode as base64-like
        static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        for (int i = 0; i < 6; ++i) {
            result += chars[(h >> (i * 6)) & 0x3F];
        }
        result += "==";
        return result;
    }

private:
    std::string url_;
    State state_ = State::Disconnected;

    void sendFrame(Opcode opcode, const std::string& payload) {
        if (state_ != State::Connected) return;

        std::vector<uint8_t> frame;
        // FIN + opcode
        frame.push_back(0x80 | static_cast<uint8_t>(opcode));

        // Mask + payload length
        bool masked = true;
        size_t len = payload.size();
        if (len <= 125) {
            frame.push_back((masked ? 0x80 : 0x00) | static_cast<uint8_t>(len));
        } else if (len <= 65535) {
            frame.push_back((masked ? 0x80 : 0x00) | 126);
            frame.push_back(static_cast<uint8_t>(len >> 8));
            frame.push_back(static_cast<uint8_t>(len & 0xFF));
        } else {
            frame.push_back((masked ? 0x80 : 0x00) | 127);
            for (int i = 7; i >= 0; --i) {
                frame.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
            }
        }

        // Masking key
        uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
        if (masked) {
            frame.insert(frame.end(), mask, mask + 4);
        }

        // Masked payload
        for (size_t i = 0; i < payload.size(); ++i) {
            frame.push_back(static_cast<uint8_t>(payload[i]) ^ mask[i % 4]);
        }

        // In real implementation: write frame to socket
    }
};

} // namespace nexus::utility::http
