#pragma once

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <cstring>
#include <stdexcept>
#include <optional>
#include <chrono>
#include <cerrno>

namespace nexus::utility::network {

/**
 * @brief RAII TCP/UDP socket wrapper.
 */
class SocketWrapper {
public:
    enum class Type { TCP, UDP };

    /**
     * @brief Creates a socket.
     */
    explicit SocketWrapper(Type type = Type::TCP) {
        int sock_type = (type == Type::TCP) ? SOCK_STREAM : SOCK_DGRAM;
        fd_ = socket(AF_INET, sock_type, 0);
        if (fd_ < 0) {
            throw std::runtime_error("Failed to create socket");
        }
    }

    ~SocketWrapper() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    // Non-copyable, movable
    SocketWrapper(const SocketWrapper&) = delete;
    SocketWrapper& operator=(const SocketWrapper&) = delete;

    SocketWrapper(SocketWrapper&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    SocketWrapper& operator=(SocketWrapper&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) close(fd_);
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    /**
     * @brief Connects to a remote host (thread-safe via getaddrinfo).
     */
    bool connect(const std::string& host, int port) {
        struct addrinfo hints {};
        struct addrinfo* result = nullptr;

        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        std::string port_str = std::to_string(port);
        int ret = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result);
        if (ret != 0 || !result) {
            return false;
        }

        bool success = ::connect(fd_, result->ai_addr, result->ai_addrlen) >= 0;
        freeaddrinfo(result);
        return success;
    }

    /**
     * @brief Binds to a local address.
     */
    bool bind(int port) {
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        // Set SO_REUSEADDR
        int opt = 1;
        setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        return ::bind(fd_, (struct sockaddr*)&addr, sizeof(addr)) >= 0;
    }

    /**
     * @brief Listens for connections.
     */
    bool listen(int backlog = 10) {
        return ::listen(fd_, backlog) >= 0;
    }

    /**
     * @brief Accepts a connection.
     */
    std::optional<SocketWrapper> accept() {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        int client_fd = ::accept(fd_, (struct sockaddr*)&addr, &addr_len);

        if (client_fd < 0) {
            return std::nullopt;
        }

        return SocketWrapper(client_fd);
    }

    /**
     * @brief Sends data. Partial sends possible - use sendAll for guaranteed delivery.
     */
    ssize_t send(const std::string& data) {
        return ::send(fd_, data.c_str(), data.length(), MSG_NOSIGNAL);
    }

    /**
     * @brief Sends all data, retrying on partial writes. Returns true on success.
     */
    bool sendAll(const std::string& data) {
        size_t total = 0;
        while (total < data.size()) {
            ssize_t sent = ::send(fd_, data.c_str() + total, data.size() - total, MSG_NOSIGNAL);
            if (sent < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            total += static_cast<size_t>(sent);
        }
        return true;
    }

    /**
     * @brief Receives data. Returns:
     *   std::nullopt on error,
     *   empty string on clean EOF,
     *   non-empty string on successful read.
     */
    std::optional<std::string> receive(size_t max_size = 4096) {
        std::string buffer(max_size, '\0');
        ssize_t bytes = ::recv(fd_, &buffer[0], max_size, 0);

        if (bytes < 0) {
            return std::nullopt;  // Error
        }
        if (bytes == 0) {
            return std::string{};  // EOF
        }
        buffer.resize(static_cast<size_t>(bytes));
        return buffer;
    }

    /**
     * @brief Sets send/receive timeout in milliseconds.
     */
    bool setTimeout(std::chrono::milliseconds timeout) {
        struct timeval tv;
        tv.tv_sec = static_cast<time_t>(timeout.count() / 1000);
        tv.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);

        bool ok = setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) >= 0;
        ok = setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) >= 0 && ok;
        return ok;
    }
    bool setNonBlocking(bool enabled) {
        int flags = fcntl(fd_, F_GETFL, 0);
        if (flags < 0) return false;

        if (enabled) {
            flags |= O_NONBLOCK;
        } else {
            flags &= ~O_NONBLOCK;
        }

        return fcntl(fd_, F_SETFL, flags) >= 0;
    }

    /**
     * @brief Gets the file descriptor.
     */
    int fd() const { return fd_; }

    /**
     * @brief Checks if socket is valid.
     */
    bool isValid() const { return fd_ >= 0; }

private:
    int fd_ = -1;

    /// @brief Adopt an existing file descriptor (used by accept())
    explicit SocketWrapper(int existing_fd) : fd_(existing_fd) {}
};

} // namespace nexus::utility::network
