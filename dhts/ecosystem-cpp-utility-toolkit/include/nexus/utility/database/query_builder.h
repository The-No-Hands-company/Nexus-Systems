#pragma once

#include <string>
#include <vector>
#include <sstream>

namespace nexus::utility::database {

/**
 * @brief SQL query builder for safe query construction.
 */
class QueryBuilder {
public:
    /**
     * @brief SELECT query builder.
     */
    class SelectBuilder {
    public:
        SelectBuilder& select(const std::vector<std::string>& columns) {
            columns_ = columns;
            return *this;
        }
        
        SelectBuilder& from(const std::string& table) {
            table_ = table;
            return *this;
        }
        
        SelectBuilder& where(const std::string& condition) {
            where_ = condition;
            return *this;
        }
        
        SelectBuilder& orderBy(const std::string& column, bool ascending = true) {
            orderBy_ = column + (ascending ? " ASC" : " DESC");
            return *this;
        }
        
        SelectBuilder& limit(int count) {
            limit_ = count;
            return *this;
        }
        
        std::string build() const {
            std::ostringstream sql;
            sql << "SELECT ";
            
            if (columns_.empty()) {
                sql << "*";
            } else {
                for (size_t i = 0; i < columns_.size(); ++i) {
                    if (i > 0) sql << ", ";
                    sql << columns_[i];
                }
            }
            
            sql << " FROM " << table_;
            
            if (!where_.empty()) {
                sql << " WHERE " << where_;
            }
            
            if (!orderBy_.empty()) {
                sql << " ORDER BY " << orderBy_;
            }
            
            if (limit_ > 0) {
                sql << " LIMIT " << limit_;
            }
            
            return sql.str();
        }
        
    private:
        std::vector<std::string> columns_;
        std::string table_;
        std::string where_;
        std::string orderBy_;
        int limit_ = 0;
    };

    /**
     * @brief INSERT query builder.
     */
    class InsertBuilder {
    public:
        InsertBuilder& into(const std::string& table) {
            table_ = table;
            return *this;
        }
        
        InsertBuilder& columns(const std::vector<std::string>& cols) {
            columns_ = cols;
            return *this;
        }
        
        InsertBuilder& values(const std::vector<std::string>& vals) {
            values_ = vals;
            return *this;
        }
        
        std::string build() const {
            std::ostringstream sql;
            sql << "INSERT INTO " << table_;
            
            if (!columns_.empty()) {
                sql << " (";
                for (size_t i = 0; i < columns_.size(); ++i) {
                    if (i > 0) sql << ", ";
                    sql << columns_[i];
                }
                sql << ")";
            }
            
            sql << " VALUES (";
            for (size_t i = 0; i < values_.size(); ++i) {
                if (i > 0) sql << ", ";
                sql << values_[i];
            }
            sql << ")";
            
            return sql.str();
        }
        
    private:
        std::string table_;
        std::vector<std::string> columns_;
        std::vector<std::string> values_;
    };

    /**
     * @brief UPDATE query builder.
     */
    class UpdateBuilder {
    public:
        UpdateBuilder& table(const std::string& tbl) {
            table_ = tbl;
            return *this;
        }
        
        UpdateBuilder& set(const std::string& column, const std::string& value) {
            sets_.push_back(column + " = " + value);
            return *this;
        }
        
        UpdateBuilder& where(const std::string& condition) {
            where_ = condition;
            return *this;
        }
        
        std::string build() const {
            std::ostringstream sql;
            sql << "UPDATE " << table_ << " SET ";
            
            for (size_t i = 0; i < sets_.size(); ++i) {
                if (i > 0) sql << ", ";
                sql << sets_[i];
            }
            
            if (!where_.empty()) {
                sql << " WHERE " << where_;
            }
            
            return sql.str();
        }
        
    private:
        std::string table_;
        std::vector<std::string> sets_;
        std::string where_;
    };

    /**
     * @brief DELETE query builder.
     */
    class DeleteBuilder {
    public:
        DeleteBuilder& from(const std::string& table) {
            table_ = table;
            return *this;
        }
        
        DeleteBuilder& where(const std::string& condition) {
            where_ = condition;
            return *this;
        }
        
        std::string build() const {
            std::ostringstream sql;
            sql << "DELETE FROM " << table_;
            
            if (!where_.empty()) {
                sql << " WHERE " << where_;
            }
            
            return sql.str();
        }
        
    private:
        std::string table_;
        std::string where_;
    };

    static SelectBuilder select() { return SelectBuilder(); }
    static InsertBuilder insert() { return InsertBuilder(); }
    static UpdateBuilder update() { return UpdateBuilder(); }
    static DeleteBuilder deleteFrom() { return DeleteBuilder(); }
};

} // namespace nexus::utility::database
