#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <stdexcept>

// Note: Requires SQLite3 library
// #include <sqlite3.h>

namespace nexus::utility::database {

/**
 * @brief RAII wrapper for SQLite database.
 * Note: This is a design template - actual implementation requires linking sqlite3.
 */
class SQLiteWrapper {
public:
    using RowCallback = std::function<void(const std::vector<std::string>&)>;

    /**
     * @brief Opens database.
     */
    explicit SQLiteWrapper(const std::string& filename) : filename_(filename), db_(nullptr) {
        // Implementation would use: sqlite3_open(filename.c_str(), &db_);
    }

    ~SQLiteWrapper() {
        close();
    }

    // Non-copyable
    SQLiteWrapper(const SQLiteWrapper&) = delete;
    SQLiteWrapper& operator=(const SQLiteWrapper&) = delete;

    /**
     * @brief Executes SQL query.
     */
    bool execute(const std::string& sql) {
        // Implementation would use sqlite3_exec
        return true;
    }

    /**
     * @brief Executes query with callback for each row.
     */
    bool query(const std::string& sql, RowCallback callback) {
        // Implementation would iterate through results
        return true;
    }

    /**
     * @brief Prepares statement (for parameterized queries).
     */
    class Statement {
    public:
        Statement() = default;
        
        void bind(int index, int value) {
            // sqlite3_bind_int
        }
        
        void bind(int index, double value) {
            // sqlite3_bind_double
        }
        
        void bind(int index, const std::string& value) {
            // sqlite3_bind_text
        }
        
        bool step() {
            // sqlite3_step
            return false;
        }
        
        void reset() {
            // sqlite3_reset
        }
    };

    Statement prepare(const std::string& sql) {
        // Implementation would use sqlite3_prepare_v2
        return Statement();
    }

    /**
     * @brief Gets last insert row ID.
     */
    int64_t getLastInsertId() const {
        // sqlite3_last_insert_rowid
        return 0;
    }

    /**
     * @brief Gets number of rows affected.
     */
    int getChangesCount() const {
        // sqlite3_changes
        return 0;
    }

    /**
     * @brief Checks if database is open.
     */
    bool isOpen() const {
        return db_ != nullptr;
    }

    /**
     * @brief Gets last error message.
     */
    std::string getLastError() const {
        // sqlite3_errmsg
        return "";
    }

private:
    void close() {
        if (db_) {
            // sqlite3_close(db_);
            db_ = nullptr;
        }
    }

    std::string filename_;
    void* db_; // sqlite3* in actual implementation
};

} // namespace nexus::utility::database
