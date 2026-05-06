"""
NLPL Standard Library - SQLite Database Module
SQLite database operations
"""

import sqlite3
from typing import Any, Dict, List, Optional, Tuple


def register_sqlite_functions(runtime: Any) -> None:
    """Register SQLite database functions with the runtime."""
    
    # Connection
    runtime.register_function("db_connect", db_connect)
    runtime.register_function("db_close", db_close)
    
    # Execution
    runtime.register_function("db_execute", db_execute)
    runtime.register_function("db_execute_many", db_execute_many)
    runtime.register_function("db_query", db_query)
    runtime.register_function("db_query_one", db_query_one)
    
    # Fetching
    runtime.register_function("db_fetch_all", db_fetch_all)
    runtime.register_function("db_fetch_one", db_fetch_one)
    
    # Transactions
    runtime.register_function("db_commit", db_commit)
    runtime.register_function("db_rollback", db_rollback)
    runtime.register_function("db_begin", db_begin)
    
    # Utilities
    runtime.register_function("db_last_insert_id", db_last_insert_id)
    runtime.register_function("db_row_count", db_row_count)
    runtime.register_function("db_table_exists", db_table_exists)
    runtime.register_function("db_list_tables", db_list_tables)


# Global connection storage
_connections: Dict[str, sqlite3.Connection] = {}
_connection_counter = 0


# =======================
# Connection Management
# =======================

def db_connect(database: str, check_same_thread: bool = False) -> str:
    """
    Connect to SQLite database.
    Returns connection ID (string) for use with other db functions.
    Use ":memory:" for in-memory database.
    """
    global _connection_counter
    try:
        conn = sqlite3.connect(database, check_same_thread=check_same_thread)
        conn.row_factory = sqlite3.Row  # Enable dict-like access
        
        conn_id = f"conn_{_connection_counter}"
        _connections[conn_id] = conn
        _connection_counter += 1
        
        return conn_id
    except sqlite3.Error as e:
        raise RuntimeError(f"Database connection error: {e}")


def db_close(conn_id: str) -> bool:
    """Close database connection."""
    try:
        if conn_id in _connections:
            _connections[conn_id].close()
            del _connections[conn_id]
            return True
        return False
    except sqlite3.Error as e:
        raise RuntimeError(f"Error closing database: {e}")


def _get_connection(conn_id: str) -> sqlite3.Connection:
    """Get connection by ID (internal helper)."""
    if conn_id not in _connections:
        raise RuntimeError(f"Invalid connection ID: {conn_id}")
    return _connections[conn_id]


# =======================
# Execution
# =======================

def db_execute(conn_id: str, sql: str, params: Optional[List[Any]] = None) -> bool:
    """
    Execute SQL statement (INSERT, UPDATE, DELETE, CREATE, etc.).
    Returns True on success.
    """
    try:
        conn = _get_connection(conn_id)
        cursor = conn.cursor()
        
        if params:
            cursor.execute(sql, params)
        else:
            cursor.execute(sql)
        
        conn.commit()
        cursor.close()
        return True
    except sqlite3.Error as e:
        raise RuntimeError(f"Database execution error: {e}")


def db_execute_many(conn_id: str, sql: str, params_list: List[List[Any]]) -> bool:
    """
    Execute SQL statement with multiple parameter sets (bulk insert).
    Returns True on success.
    """
    try:
        conn = _get_connection(conn_id)
        cursor = conn.cursor()
        cursor.executemany(sql, params_list)
        conn.commit()
        cursor.close()
        return True
    except sqlite3.Error as e:
        raise RuntimeError(f"Database execution error: {e}")


def db_query(conn_id: str, sql: str, params: Optional[List[Any]] = None) -> List[Dict[str, Any]]:
    """
    Execute SELECT query and return all rows as list of dicts.
    Each dict has column_name: value pairs.
    """
    try:
        conn = _get_connection(conn_id)
        cursor = conn.cursor()
        
        if params:
            cursor.execute(sql, params)
        else:
            cursor.execute(sql)
        
        rows = cursor.fetchall()
        cursor.close()
        
        # Convert sqlite3.Row to dict
        return [dict(row) for row in rows]
    except sqlite3.Error as e:
        raise RuntimeError(f"Database query error: {e}")


def db_query_one(conn_id: str, sql: str, params: Optional[List[Any]] = None) -> Optional[Dict[str, Any]]:
    """
    Execute SELECT query and return first row as dict.
    Returns None if no rows found.
    """
    try:
        conn = _get_connection(conn_id)
        cursor = conn.cursor()
        
        if params:
            cursor.execute(sql, params)
        else:
            cursor.execute(sql)
        
        row = cursor.fetchone()
        cursor.close()
        
        return dict(row) if row else None
    except sqlite3.Error as e:
        raise RuntimeError(f"Database query error: {e}")


# =======================
# Fetching
# =======================

def db_fetch_all(conn_id: str, sql: str, params: Optional[List[Any]] = None) -> List[Dict[str, Any]]:
    """Alias for db_query."""
    return db_query(conn_id, sql, params)


def db_fetch_one(conn_id: str, sql: str, params: Optional[List[Any]] = None) -> Optional[Dict[str, Any]]:
    """Alias for db_query_one."""
    return db_query_one(conn_id, sql, params)


# =======================
# Transactions
# =======================

def db_commit(conn_id: str) -> bool:
    """Commit current transaction."""
    try:
        conn = _get_connection(conn_id)
        conn.commit()
        return True
    except sqlite3.Error as e:
        raise RuntimeError(f"Commit error: {e}")


def db_rollback(conn_id: str) -> bool:
    """Rollback current transaction."""
    try:
        conn = _get_connection(conn_id)
        conn.rollback()
        return True
    except sqlite3.Error as e:
        raise RuntimeError(f"Rollback error: {e}")


def db_begin(conn_id: str) -> bool:
    """Begin explicit transaction."""
    try:
        conn = _get_connection(conn_id)
        conn.execute("BEGIN")
        return True
    except sqlite3.Error as e:
        raise RuntimeError(f"Begin transaction error: {e}")


# =======================
# Utilities
# =======================

def db_last_insert_id(conn_id: str) -> int:
    """Get last inserted row ID (for autoincrement columns)."""
    try:
        conn = _get_connection(conn_id)
        return conn.execute("SELECT last_insert_rowid()").fetchone()[0]
    except sqlite3.Error as e:
        raise RuntimeError(f"Error getting last insert ID: {e}")


def db_row_count(conn_id: str) -> int:
    """Get number of rows affected by last statement."""
    try:
        conn = _get_connection(conn_id)
        return conn.total_changes
    except sqlite3.Error as e:
        raise RuntimeError(f"Error getting row count: {e}")


def db_table_exists(conn_id: str, table_name: str) -> bool:
    """Check if table exists in database."""
    try:
        conn = _get_connection(conn_id)
        cursor = conn.cursor()
        cursor.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name=?",
            [table_name]
        )
        result = cursor.fetchone()
        cursor.close()
        return result is not None
    except sqlite3.Error as e:
        raise RuntimeError(f"Error checking table existence: {e}")


def db_list_tables(conn_id: str) -> List[str]:
    """List all tables in database."""
    try:
        conn = _get_connection(conn_id)
        cursor = conn.cursor()
        cursor.execute(
            "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name"
        )
        tables = [row[0] for row in cursor.fetchall()]
        cursor.close()
        return tables
    except sqlite3.Error as e:
        raise RuntimeError(f"Error listing tables: {e}")
