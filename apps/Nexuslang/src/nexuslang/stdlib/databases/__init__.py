"""
Database connector utilities for NexusLang.

Provides connectors for:
- PostgreSQL (psycopg2)
- MySQL (mysql-connector-python)
- MongoDB (pymongo)

Features:
- Connection pooling
- Query execution
- Transactions
- Parameterized queries
- Result iteration
"""

from typing import Any, Dict, List, Optional
import os

# Optional database libraries
try:
    import psycopg2
    from psycopg2 import pool as pg_pool
    HAS_POSTGRES = True
except ImportError:
    HAS_POSTGRES = False

try:
    import mysql.connector
    from mysql.connector import pooling
    HAS_MYSQL = True
except ImportError:
    HAS_MYSQL = False

try:
    import pymongo
    from pymongo import MongoClient
    HAS_MONGODB = True
except ImportError:
    HAS_MONGODB = False

# Global connection storage
_postgres_pools = {}
_postgres_counter = 0
_mysql_pools = {}
_mysql_counter = 0
_mongo_clients = {}
_mongo_counter = 0


# ==========================================
# PostgreSQL Functions
# ==========================================

def pg_connect(host: str = "localhost", port: int = 5432, database: str = "postgres",
               user: str = "postgres", password: str = "", pool_size: int = 5) -> int:
    """Connect to PostgreSQL database with connection pooling.
    
    Args:
        host: Database host
        port: Database port
        database: Database name
        user: Username
        password: Password
        pool_size: Connection pool size
        
    Returns:
        Pool ID
    """
    if not HAS_POSTGRES:
        raise ImportError("psycopg2 is not installed. Install with: pip install psycopg2-binary")
    
    global _postgres_counter
    
    # Create connection pool
    conn_pool = pg_pool.SimpleConnectionPool(
        1, pool_size,
        host=host,
        port=port,
        database=database,
        user=user,
        password=password
    )
    
    pool_id = _postgres_counter
    _postgres_pools[pool_id] = conn_pool
    _postgres_counter += 1
    
    return pool_id


def pg_execute(pool_id: int, query: str, params: tuple = None) -> int:
    """Execute a PostgreSQL query (INSERT, UPDATE, DELETE).
    
    Args:
        pool_id: Pool ID
        query: SQL query
        params: Query parameters (for parameterized queries)
        
    Returns:
        Number of affected rows
    """
    if pool_id not in _postgres_pools:
        raise ValueError(f"PostgreSQL pool {pool_id} not found")
    
    pool = _postgres_pools[pool_id]
    conn = pool.getconn()
    
    try:
        cursor = conn.cursor()
        cursor.execute(query, params or ())
        conn.commit()
        affected_rows = cursor.rowcount
        cursor.close()
        return affected_rows
    finally:
        pool.putconn(conn)


def pg_query(pool_id: int, query: str, params: tuple = None) -> List[tuple]:
    """Execute a PostgreSQL SELECT query.
    
    Args:
        pool_id: Pool ID
        query: SQL query
        params: Query parameters
        
    Returns:
        List of result rows
    """
    if pool_id not in _postgres_pools:
        raise ValueError(f"PostgreSQL pool {pool_id} not found")
    
    pool = _postgres_pools[pool_id]
    conn = pool.getconn()
    
    try:
        cursor = conn.cursor()
        cursor.execute(query, params or ())
        results = cursor.fetchall()
        cursor.close()
        return results
    finally:
        pool.putconn(conn)


def pg_query_one(pool_id: int, query: str, params: tuple = None) -> Optional[tuple]:
    """Execute a PostgreSQL SELECT query and return one row.
    
    Args:
        pool_id: Pool ID
        query: SQL query
        params: Query parameters
        
    Returns:
        Single result row or None
    """
    if pool_id not in _postgres_pools:
        raise ValueError(f"PostgreSQL pool {pool_id} not found")
    
    pool = _postgres_pools[pool_id]
    conn = pool.getconn()
    
    try:
        cursor = conn.cursor()
        cursor.execute(query, params or ())
        result = cursor.fetchone()
        cursor.close()
        return result
    finally:
        pool.putconn(conn)


def pg_close(pool_id: int) -> bool:
    """Close PostgreSQL connection pool.
    
    Args:
        pool_id: Pool ID
        
    Returns:
        True if successful
    """
    if pool_id not in _postgres_pools:
        return False
    
    pool = _postgres_pools[pool_id]
    pool.closeall()
    del _postgres_pools[pool_id]
    return True


# ==========================================
# MySQL Functions
# ==========================================

def mysql_connect(host: str = "localhost", port: int = 3306, database: str = "mysql",
                  user: str = "root", password: str = "", pool_size: int = 5) -> int:
    """Connect to MySQL database with connection pooling.
    
    Args:
        host: Database host
        port: Database port
        database: Database name
        user: Username
        password: Password
        pool_size: Connection pool size
        
    Returns:
        Pool ID
    """
    if not HAS_MYSQL:
        raise ImportError("mysql-connector-python is not installed. Install with: pip install mysql-connector-python")
    
    global _mysql_counter
    
    # Create connection pool
    conn_pool = pooling.MySQLConnectionPool(
        pool_name=f"nxl_pool_{_mysql_counter}",
        pool_size=pool_size,
        host=host,
        port=port,
        database=database,
        user=user,
        password=password
    )
    
    pool_id = _mysql_counter
    _mysql_pools[pool_id] = conn_pool
    _mysql_counter += 1
    
    return pool_id


def mysql_execute(pool_id: int, query: str, params: tuple = None) -> int:
    """Execute a MySQL query (INSERT, UPDATE, DELETE).
    
    Args:
        pool_id: Pool ID
        query: SQL query
        params: Query parameters
        
    Returns:
        Number of affected rows
    """
    if pool_id not in _mysql_pools:
        raise ValueError(f"MySQL pool {pool_id} not found")
    
    pool = _mysql_pools[pool_id]
    conn = pool.get_connection()
    
    try:
        cursor = conn.cursor()
        cursor.execute(query, params or ())
        conn.commit()
        affected_rows = cursor.rowcount
        cursor.close()
        return affected_rows
    finally:
        conn.close()


def mysql_query(pool_id: int, query: str, params: tuple = None) -> List[tuple]:
    """Execute a MySQL SELECT query.
    
    Args:
        pool_id: Pool ID
        query: SQL query
        params: Query parameters
        
    Returns:
        List of result rows
    """
    if pool_id not in _mysql_pools:
        raise ValueError(f"MySQL pool {pool_id} not found")
    
    pool = _mysql_pools[pool_id]
    conn = pool.get_connection()
    
    try:
        cursor = conn.cursor()
        cursor.execute(query, params or ())
        results = cursor.fetchall()
        cursor.close()
        return results
    finally:
        conn.close()


def mysql_query_one(pool_id: int, query: str, params: tuple = None) -> Optional[tuple]:
    """Execute a MySQL SELECT query and return one row.
    
    Args:
        pool_id: Pool ID
        query: SQL query
        params: Query parameters
        
    Returns:
        Single result row or None
    """
    if pool_id not in _mysql_pools:
        raise ValueError(f"MySQL pool {pool_id} not found")
    
    pool = _mysql_pools[pool_id]
    conn = pool.get_connection()
    
    try:
        cursor = conn.cursor()
        cursor.execute(query, params or ())
        result = cursor.fetchone()
        cursor.close()
        return result
    finally:
        conn.close()


def mysql_close(pool_id: int) -> bool:
    """Close MySQL connection pool.
    
    Args:
        pool_id: Pool ID
        
    Returns:
        True if successful
    """
    if pool_id not in _mysql_pools:
        return False
    
    del _mysql_pools[pool_id]
    return True


# ==========================================
# MongoDB Functions
# ==========================================

def mongo_connect(uri: str = "mongodb://localhost:27017/", database: str = "test") -> int:
    """Connect to MongoDB.
    
    Args:
        uri: MongoDB connection URI
        database: Database name
        
    Returns:
        Client ID
    """
    if not HAS_MONGODB:
        raise ImportError("pymongo is not installed. Install with: pip install pymongo")
    
    global _mongo_counter
    
    client = MongoClient(uri)
    db = client[database]
    
    client_id = _mongo_counter
    _mongo_clients[client_id] = {
        'client': client,
        'database': db,
        'db_name': database
    }
    _mongo_counter += 1
    
    return client_id


def mongo_insert_one(client_id: int, collection: str, document: dict) -> str:
    """Insert one document into MongoDB collection.
    
    Args:
        client_id: Client ID
        collection: Collection name
        document: Document to insert
        
    Returns:
        Inserted document ID
    """
    if client_id not in _mongo_clients:
        raise ValueError(f"MongoDB client {client_id} not found")
    
    db = _mongo_clients[client_id]['database']
    result = db[collection].insert_one(document)
    return str(result.inserted_id)


def mongo_insert_many(client_id: int, collection: str, documents: List[dict]) -> List[str]:
    """Insert multiple documents into MongoDB collection.
    
    Args:
        client_id: Client ID
        collection: Collection name
        documents: List of documents to insert
        
    Returns:
        List of inserted document IDs
    """
    if client_id not in _mongo_clients:
        raise ValueError(f"MongoDB client {client_id} not found")
    
    db = _mongo_clients[client_id]['database']
    result = db[collection].insert_many(documents)
    return [str(id) for id in result.inserted_ids]


def mongo_find(client_id: int, collection: str, query: dict = None, limit: int = 0) -> List[dict]:
    """Find documents in MongoDB collection.
    
    Args:
        client_id: Client ID
        collection: Collection name
        query: Query filter (None = all documents)
        limit: Maximum number of documents to return (0 = no limit)
        
    Returns:
        List of matching documents
    """
    if client_id not in _mongo_clients:
        raise ValueError(f"MongoDB client {client_id} not found")
    
    db = _mongo_clients[client_id]['database']
    cursor = db[collection].find(query or {})
    
    if limit > 0:
        cursor = cursor.limit(limit)
    
    return list(cursor)


def mongo_find_one(client_id: int, collection: str, query: dict = None) -> Optional[dict]:
    """Find one document in MongoDB collection.
    
    Args:
        client_id: Client ID
        collection: Collection name
        query: Query filter
        
    Returns:
        Matching document or None
    """
    if client_id not in _mongo_clients:
        raise ValueError(f"MongoDB client {client_id} not found")
    
    db = _mongo_clients[client_id]['database']
    return db[collection].find_one(query or {})


def mongo_update_one(client_id: int, collection: str, query: dict, update: dict) -> int:
    """Update one document in MongoDB collection.
    
    Args:
        client_id: Client ID
        collection: Collection name
        query: Query filter
        update: Update operations
        
    Returns:
        Number of modified documents
    """
    if client_id not in _mongo_clients:
        raise ValueError(f"MongoDB client {client_id} not found")
    
    db = _mongo_clients[client_id]['database']
    result = db[collection].update_one(query, update)
    return result.modified_count


def mongo_delete_one(client_id: int, collection: str, query: dict) -> int:
    """Delete one document from MongoDB collection.
    
    Args:
        client_id: Client ID
        collection: Collection name
        query: Query filter
        
    Returns:
        Number of deleted documents
    """
    if client_id not in _mongo_clients:
        raise ValueError(f"MongoDB client {client_id} not found")
    
    db = _mongo_clients[client_id]['database']
    result = db[collection].delete_one(query)
    return result.deleted_count


def mongo_count(client_id: int, collection: str, query: dict = None) -> int:
    """Count documents in MongoDB collection.
    
    Args:
        client_id: Client ID
        collection: Collection name
        query: Query filter (None = all documents)
        
    Returns:
        Number of matching documents
    """
    if client_id not in _mongo_clients:
        raise ValueError(f"MongoDB client {client_id} not found")
    
    db = _mongo_clients[client_id]['database']
    return db[collection].count_documents(query or {})


def mongo_close(client_id: int) -> bool:
    """Close MongoDB client connection.
    
    Args:
        client_id: Client ID
        
    Returns:
        True if successful
    """
    if client_id not in _mongo_clients:
        return False
    
    client = _mongo_clients[client_id]['client']
    client.close()
    del _mongo_clients[client_id]
    return True


def register_database_functions(runtime):
    """Register database functions with the NexusLang runtime."""
    # PostgreSQL
    if HAS_POSTGRES:
        runtime.register_function("pg_connect", pg_connect)
        runtime.register_function("pg_execute", pg_execute)
        runtime.register_function("pg_query", pg_query)
        runtime.register_function("pg_query_one", pg_query_one)
        runtime.register_function("pg_close", pg_close)
    
    # MySQL
    if HAS_MYSQL:
        runtime.register_function("mysql_connect", mysql_connect)
        runtime.register_function("mysql_execute", mysql_execute)
        runtime.register_function("mysql_query", mysql_query)
        runtime.register_function("mysql_query_one", mysql_query_one)
        runtime.register_function("mysql_close", mysql_close)
    
    # MongoDB
    if HAS_MONGODB:
        runtime.register_function("mongo_connect", mongo_connect)
        runtime.register_function("mongo_insert_one", mongo_insert_one)
        runtime.register_function("mongo_insert_many", mongo_insert_many)
        runtime.register_function("mongo_find", mongo_find)
        runtime.register_function("mongo_find_one", mongo_find_one)
        runtime.register_function("mongo_update_one", mongo_update_one)
        runtime.register_function("mongo_delete_one", mongo_delete_one)
        runtime.register_function("mongo_count", mongo_count)
        runtime.register_function("mongo_close", mongo_close)
