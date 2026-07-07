#pragma once

#include <functional>
#include <stdexcept>

namespace nexus::utility::database {

/**
 * @brief RAII transaction helper.
 */
template<typename ConnectionType>
class TransactionHelper {
public:
    explicit TransactionHelper(ConnectionType& conn) : conn_(conn), committed_(false) {
        conn_.execute("BEGIN TRANSACTION");
    }

    ~TransactionHelper() {
        if (!committed_) {
            try {
                rollback();
            } catch (...) {
                // Suppress exceptions in destructor
            }
        }
    }

    void commit() {
        if (!committed_) {
            conn_.execute("COMMIT");
            committed_ = true;
        }
    }

    void rollback() {
        if (!committed_) {
            conn_.execute("ROLLBACK");
            committed_ = true;
        }
    }

private:
    ConnectionType& conn_;
    bool committed_;
};

/**
 * @brief Transaction utilities.
 */
class TransactionUtils {
public:
    /**
     * @brief Executes function within transaction.
     */
    template<typename ConnectionType, typename Func>
    static bool executeInTransaction(ConnectionType& conn, Func&& func) {
        TransactionHelper<ConnectionType> transaction(conn);
        
        try {
            func();
            transaction.commit();
            return true;
        } catch (...) {
            transaction.rollback();
            throw;
        }
    }

    /**
     * @brief Retry logic for transactions.
     */
    template<typename ConnectionType, typename Func>
    static bool retryTransaction(ConnectionType& conn, Func&& func, int maxRetries = 3) {
        for (int attempt = 0; attempt < maxRetries; ++attempt) {
            try {
                return executeInTransaction(conn, std::forward<Func>(func));
            } catch (const std::exception& e) {
                if (attempt == maxRetries - 1) {
                    throw;
                }
                // Could add exponential backoff here
            }
        }
        return false;
    }
};

} // namespace nexus::utility::database
