#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <functional>
#include <chrono>

namespace nexus::utility::database {

/**
 * @brief Generic connection pool for database connections.
 */
template<typename ConnectionType>
class ConnectionPool {
public:
    using ConnectionFactory = std::function<std::unique_ptr<ConnectionType>()>;

    /**
     * @brief RAII connection handle.
     */
    class ConnectionHandle {
    public:
        ConnectionHandle(ConnectionPool* pool, std::unique_ptr<ConnectionType> conn)
            : pool_(pool), connection_(std::move(conn)) {}

        ~ConnectionHandle() {
            if (connection_ && pool_) {
                pool_->returnConnection(std::move(connection_));
            }
        }

        // Non-copyable, movable
        ConnectionHandle(const ConnectionHandle&) = delete;
        ConnectionHandle& operator=(const ConnectionHandle&) = delete;
        ConnectionHandle(ConnectionHandle&&) = default;
        ConnectionHandle& operator=(ConnectionHandle&&) = default;

        ConnectionType* get() { return connection_.get(); }
        ConnectionType* operator->() { return connection_.get(); }

    private:
        ConnectionPool* pool_;
        std::unique_ptr<ConnectionType> connection_;
    };

    explicit ConnectionPool(ConnectionFactory factory, size_t poolSize = 10)
        : factory_(factory), maxSize_(poolSize) {
        // Pre-create connections
        for (size_t i = 0; i < poolSize; ++i) {
            connections_.push(factory_());
        }
    }

    /**
     * @brief Acquires connection from pool.
     */
    ConnectionHandle acquire(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
        std::unique_lock lock(mutex_);
        
        if (!cv_.wait_for(lock, timeout, [this] { return !connections_.empty(); })) {
            throw std::runtime_error("Connection pool timeout");
        }
        
        auto conn = std::move(connections_.front());
        connections_.pop();
        activeCount_++;
        
        return ConnectionHandle(this, std::move(conn));
    }

    /**
     * @brief Gets pool statistics.
     */
    struct Stats {
        size_t available;
        size_t active;
        size_t total;
    };

    Stats getStats() const {
        std::lock_guard lock(mutex_);
        return {connections_.size(), activeCount_, maxSize_};
    }

private:
    void returnConnection(std::unique_ptr<ConnectionType> conn) {
        std::lock_guard lock(mutex_);
        connections_.push(std::move(conn));
        activeCount_--;
        cv_.notify_one();
    }

    ConnectionFactory factory_;
    size_t maxSize_;
    size_t activeCount_ = 0;
    std::queue<std::unique_ptr<ConnectionType>> connections_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

} // namespace nexus::utility::database
