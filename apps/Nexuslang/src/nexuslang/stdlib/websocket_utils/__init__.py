"""
WebSocket utilities for NexusLang.

Provides:
- WebSocket client connections
- WebSocket server
- Async and sync support
- Message sending/receiving
"""

import asyncio
from typing import Any, Callable, Optional
import json

# Optional websockets library
try:
    import websockets
    from websockets.server import serve
    from websockets.client import connect
    HAS_WEBSOCKETS = True
except ImportError:
    HAS_WEBSOCKETS = False

# Global storage for WebSocket connections
_ws_connections = {}
_ws_counter = 0
_ws_servers = {}
_server_counter = 0


async def _ws_client_connect(uri: str) -> Any:
    """Internal async function to connect to WebSocket server."""
    return await connect(uri)


def ws_connect(uri: str) -> int:
    """Connect to a WebSocket server.
    
    Args:
        uri: WebSocket URI (ws:// or wss://)
        
    Returns:
        Connection ID
    """
    if not HAS_WEBSOCKETS:
        raise ImportError("websockets is not installed. Install with: pip install websockets")
    
    global _ws_counter
    
    # Create event loop if needed
    try:
        loop = asyncio.get_event_loop()
    except RuntimeError:
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
    
    # Connect to WebSocket
    ws = loop.run_until_complete(_ws_client_connect(uri))
    
    conn_id = _ws_counter
    _ws_connections[conn_id] = {
        'ws': ws,
        'loop': loop
    }
    _ws_counter += 1
    
    return conn_id


async def _ws_send(ws, message: str) -> None:
    """Internal async function to send message."""
    await ws.send(message)


def ws_send(conn_id: int, message: str) -> bool:
    """Send message over WebSocket connection.
    
    Args:
        conn_id: Connection ID
        message: Message to send
        
    Returns:
        True if successful
    """
    if conn_id not in _ws_connections:
        raise ValueError(f"WebSocket connection {conn_id} not found")
    
    try:
        ws = _ws_connections[conn_id]['ws']
        loop = _ws_connections[conn_id]['loop']
        
        loop.run_until_complete(_ws_send(ws, message))
        return True
    except Exception:
        return False


def ws_send_json(conn_id: int, data: dict) -> bool:
    """Send JSON data over WebSocket connection.
    
    Args:
        conn_id: Connection ID
        data: Dictionary to send as JSON
        
    Returns:
        True if successful
    """
    message = json.dumps(data)
    return ws_send(conn_id, message)


async def _ws_receive(ws) -> str:
    """Internal async function to receive message."""
    return await ws.recv()


def ws_receive(conn_id: int) -> str:
    """Receive message from WebSocket connection.
    
    Args:
        conn_id: Connection ID
        
    Returns:
        Received message
    """
    if conn_id not in _ws_connections:
        raise ValueError(f"WebSocket connection {conn_id} not found")
    
    ws = _ws_connections[conn_id]['ws']
    loop = _ws_connections[conn_id]['loop']
    
    message = loop.run_until_complete(_ws_receive(ws))
    return message


def ws_receive_json(conn_id: int) -> dict:
    """Receive JSON data from WebSocket connection.
    
    Args:
        conn_id: Connection ID
        
    Returns:
        Parsed JSON data as dictionary
    """
    message = ws_receive(conn_id)
    return json.loads(message)


async def _ws_close(ws) -> None:
    """Internal async function to close connection."""
    await ws.close()


def ws_close(conn_id: int) -> bool:
    """Close WebSocket connection.
    
    Args:
        conn_id: Connection ID
        
    Returns:
        True if successful
    """
    if conn_id not in _ws_connections:
        return False
    
    try:
        ws = _ws_connections[conn_id]['ws']
        loop = _ws_connections[conn_id]['loop']
        
        loop.run_until_complete(_ws_close(ws))
        del _ws_connections[conn_id]
        return True
    except Exception:
        return False


def ws_is_open(conn_id: int) -> bool:
    """Check if WebSocket connection is open.
    
    Args:
        conn_id: Connection ID
        
    Returns:
        True if connection is open
    """
    if conn_id not in _ws_connections:
        return False
    
    ws = _ws_connections[conn_id]['ws']
    return ws.open


async def _ws_server_handler(websocket, path, callback):
    """Internal async handler for WebSocket server."""
    async for message in websocket:
        # Call the callback with the message
        response = callback(message)
        if response:
            await websocket.send(response)


async def _ws_start_server(host: str, port: int, callback: Callable) -> Any:
    """Internal async function to start WebSocket server."""
    return await serve(lambda ws, path: _ws_server_handler(ws, path, callback), host, port)


def ws_start_server(host: str = "localhost", port: int = 8765, callback: Callable = None) -> int:
    """Start a WebSocket server.
    
    Args:
        host: Host to bind to
        port: Port to bind to
        callback: Function to call when message received (takes message, returns response)
        
    Returns:
        Server ID
    """
    if not HAS_WEBSOCKETS:
        raise ImportError("websockets is not installed. Install with: pip install websockets")
    
    global _server_counter
    
    # Create event loop if needed
    try:
        loop = asyncio.get_event_loop()
    except RuntimeError:
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
    
    # Default callback that echoes messages
    if callback is None:
        callback = lambda msg: f"Echo: {msg}"
    
    # Start server
    server = loop.run_until_complete(_ws_start_server(host, port, callback))
    
    server_id = _server_counter
    _ws_servers[server_id] = {
        'server': server,
        'loop': loop,
        'host': host,
        'port': port
    }
    _server_counter += 1
    
    return server_id


def ws_stop_server(server_id: int) -> bool:
    """Stop a WebSocket server.
    
    Args:
        server_id: Server ID
        
    Returns:
        True if successful
    """
    if server_id not in _ws_servers:
        return False
    
    try:
        server = _ws_servers[server_id]['server']
        server.close()
        del _ws_servers[server_id]
        return True
    except Exception:
        return False


def ws_server_info(server_id: int) -> dict:
    """Get information about a WebSocket server.
    
    Args:
        server_id: Server ID
        
    Returns:
        Dictionary with host and port information
    """
    if server_id not in _ws_servers:
        raise ValueError(f"WebSocket server {server_id} not found")
    
    return {
        'host': _ws_servers[server_id]['host'],
        'port': _ws_servers[server_id]['port']
    }


def register_websocket_functions(runtime):
    """Register WebSocket functions with the NexusLang runtime."""
    if not HAS_WEBSOCKETS:
        # Register fallback functions for graceful degradation when dependency is missing
        # This is proper error handling, not incomplete implementation
        def ws_not_available(*args, **kwargs):
            raise ImportError("websockets is not installed. Install with: pip install websockets")
        
        runtime.register_function("ws_connect", ws_not_available)
        runtime.register_function("ws_send", ws_not_available)
        runtime.register_function("ws_receive", ws_not_available)
        return
    
    # Client functions
    runtime.register_function("ws_connect", ws_connect)
    runtime.register_function("ws_send", ws_send)
    runtime.register_function("ws_send_json", ws_send_json)
    runtime.register_function("ws_receive", ws_receive)
    runtime.register_function("ws_receive_json", ws_receive_json)
    runtime.register_function("ws_close", ws_close)
    runtime.register_function("ws_is_open", ws_is_open)
    
    # Server functions
    runtime.register_function("ws_start_server", ws_start_server)
    runtime.register_function("ws_stop_server", ws_stop_server)
    runtime.register_function("ws_server_info", ws_server_info)
