"""
Tests for the NexusLang stdlib network module - new socket/TLS/UDP additions.

Groups:
1. Server sockets (socket_bind, socket_listen, socket_accept, socket_set_option)
   - Tested with loopback TCP (no external network required)
2. UDP datagrams (udp_send_to, udp_receive_from)
   - Tested with loopback UDP (no external network required)
3. Unix domain sockets (socket_create family='unix', plus bind/accept)
   - Tested with a temp file socket (skipped on platforms without AF_UNIX)
4. TLS/SSL API (tls_create_context, tls_wrap_socket, tls_connect,
                tls_wrap_server_socket)
   - Context creation always runs; wrap/connect skip without NLPL_TEST_NETWORK
"""

import os
import sys
import socket as _socket
import ssl
import tempfile
import threading
import time
import pytest

_PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..")
)
if _PROJECT_ROOT not in sys.path:
    sys.path.insert(0, _PROJECT_ROOT)

from nexuslang.stdlib.network import (
    socket_create,
    socket_bind,
    socket_listen,
    socket_accept,
    socket_close,
    socket_send,
    socket_receive,
    socket_set_option,
    socket_connect,
    udp_send_to,
    udp_receive_from,
    tls_create_context,
    tls_wrap_socket,
    tls_connect,
    tls_wrap_server_socket,
)

_NETWORK = pytest.mark.skipif(
    not os.environ.get("NLPL_TEST_NETWORK"),
    reason="Set NLPL_TEST_NETWORK=1 to enable live-network TLS tests",
)

_HAS_UNIX = pytest.mark.skipif(
    not hasattr(_socket, "AF_UNIX"),
    reason="AF_UNIX not available on this platform",
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _free_port():
    """Return an available loopback port."""
    with _socket.socket(_socket.AF_INET, _socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def _free_udp_port():
    with _socket.socket(_socket.AF_INET, _socket.SOCK_DGRAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


# ===========================================================================
# socket_set_option
# ===========================================================================


class TestSocketSetOption:
    def test_reuse_address(self):
        sock = socket_create("inet", "stream")
        result = socket_set_option(sock, "reuse_address", True)
        assert "error" not in result

    def test_timeout(self):
        sock = socket_create("inet", "stream")
        result = socket_set_option(sock, "timeout", 5.0)
        assert "error" not in result
        # Verify the timeout was actually set
        assert sock["socket"].gettimeout() == 5.0
        socket_close(sock)

    def test_nodelay(self):
        sock = socket_create("inet", "stream")
        result = socket_set_option(sock, "nodelay", True)
        assert "error" not in result
        socket_close(sock)

    def test_keepalive(self):
        sock = socket_create("inet", "stream")
        result = socket_set_option(sock, "keepalive", True)
        assert "error" not in result
        socket_close(sock)

    def test_unknown_option_returns_error(self):
        sock = socket_create("inet", "stream")
        result = socket_set_option(sock, "not_a_real_option", True)
        assert "error" in result

    def test_non_blocking(self):
        sock = socket_create("inet", "stream")
        result = socket_set_option(sock, "non_blocking", True)
        assert "error" not in result
        socket_close(sock)


# ===========================================================================
# socket_bind / socket_listen / socket_accept  (loopback TCP)
# ===========================================================================


class TestServerSocket:
    def test_bind_returns_sock_info(self):
        port = _free_port()
        server = socket_create("inet", "stream")
        socket_set_option(server, "reuse_address", True)
        result = socket_bind(server, "127.0.0.1", port)
        assert "error" not in result
        assert result.get("bound") is True
        socket_close(server)

    def test_listen_marks_listening(self):
        port = _free_port()
        server = socket_create("inet", "stream")
        socket_set_option(server, "reuse_address", True)
        socket_bind(server, "127.0.0.1", port)
        result = socket_listen(server, backlog=1)
        assert "error" not in result
        assert result.get("listening") is True
        socket_close(server)

    def test_accept_receives_connection(self):
        port = _free_port()
        server = socket_create("inet", "stream")
        socket_set_option(server, "reuse_address", True)
        socket_bind(server, "127.0.0.1", port)
        socket_listen(server, backlog=1)
        socket_set_option(server, "timeout", 2.0)

        accepted_conn = {}

        def accept_thread():
            conn = socket_accept(server)
            accepted_conn.update(conn)

        t = threading.Thread(target=accept_thread, daemon=True)
        t.start()

        time.sleep(0.05)
        client = socket_create("inet", "stream")
        socket_connect(client, "127.0.0.1", port)

        t.join(timeout=3)
        assert "socket" in accepted_conn
        assert "address" in accepted_conn

        socket_close(client)
        socket_close(server)

    def test_send_receive_via_accepted_connection(self):
        port = _free_port()
        server = socket_create("inet", "stream")
        socket_set_option(server, "reuse_address", True)
        socket_bind(server, "127.0.0.1", port)
        socket_listen(server, backlog=1)

        received = {}

        def server_thread():
            socket_set_option(server, "timeout", 3.0)
            conn = socket_accept(server)
            if "error" not in conn:
                data = socket_receive(conn, 64)
                received["data"] = data
                socket_send(conn, "pong")
                socket_close(conn)

        t = threading.Thread(target=server_thread, daemon=True)
        t.start()

        time.sleep(0.05)
        client = socket_create("inet", "stream")
        socket_set_option(client, "timeout", 3.0)
        socket_connect(client, "127.0.0.1", port)
        socket_send(client, "ping")
        response = socket_receive(client, 64)
        socket_close(client)

        t.join(timeout=5)
        assert received.get("data") == "ping"
        assert response == "pong"


# ===========================================================================
# UDP send/receive
# ===========================================================================


class TestUDPSocket:
    def test_udp_send_to_returns_byte_count(self):
        port = _free_udp_port()
        receiver = socket_create("inet", "dgram")
        socket_set_option(receiver, "timeout", 2.0)
        socket_bind(receiver, "127.0.0.1", port)

        sender = socket_create("inet", "dgram")
        n = udp_send_to(sender, "hello", "127.0.0.1", port)
        assert isinstance(n, int) and n == 5

        socket_close(sender)
        socket_close(receiver)

    def test_udp_receive_from_returns_data_and_address(self):
        port = _free_udp_port()
        receiver = socket_create("inet", "dgram")
        socket_set_option(receiver, "timeout", 2.0)
        socket_bind(receiver, "127.0.0.1", port)

        sender = socket_create("inet", "dgram")
        udp_send_to(sender, "world", "127.0.0.1", port)
        result = udp_receive_from(receiver, 64)

        assert "error" not in result
        assert result["data"] == "world"
        assert isinstance(result["address"], (tuple, list))
        assert result["bytes"] == 5

        socket_close(sender)
        socket_close(receiver)

    def test_udp_roundtrip_bytes(self):
        port = _free_udp_port()
        receiver = socket_create("inet", "dgram")
        socket_set_option(receiver, "timeout", 2.0)
        socket_bind(receiver, "127.0.0.1", port)

        sender = socket_create("inet", "dgram")
        message = "NLPL UDP test"
        udp_send_to(sender, message, "127.0.0.1", port)
        result = udp_receive_from(receiver, 256)

        assert result["data"] == message

        socket_close(sender)
        socket_close(receiver)


# ===========================================================================
# Unix domain sockets
# ===========================================================================


class TestUnixDomainSocket:
    @_HAS_UNIX
    def test_unix_socket_bind_listen_accept(self, tmp_path):
        sock_path = str(tmp_path / "test.sock")

        server = socket_create("unix", "stream")
        assert "error" not in server

        socket_bind(server, sock_path)
        socket_listen(server, 1)
        socket_set_option(server, "timeout", 2.0)

        accepted = {}

        def accept_thread():
            conn = socket_accept(server)
            accepted.update(conn)

        t = threading.Thread(target=accept_thread, daemon=True)
        t.start()

        time.sleep(0.05)
        client = socket_create("unix", "stream")
        # For Unix sockets socket_connect passes (path, 0) — adjust:
        client["socket"].connect(sock_path)

        t.join(timeout=3)
        assert "socket" in accepted

        socket_close(client)
        socket_close(server)

    @_HAS_UNIX
    def test_unix_socket_create_family(self):
        sock = socket_create("unix", "stream")
        assert "error" not in sock
        assert sock["family"] == "unix"
        socket_close(sock)


# ===========================================================================
# TLS context creation (no network required)
# ===========================================================================


class TestTLSCreateContext:
    def test_default_client_context(self):
        result = tls_create_context(verify=True)
        assert "error" not in result
        assert "context" in result
        assert isinstance(result["context"], ssl.SSLContext)
        assert result["verify"] is True

    def test_no_verify_context(self):
        result = tls_create_context(verify=False)
        assert "error" not in result
        ctx = result["context"]
        assert ctx.verify_mode == ssl.CERT_NONE
        assert ctx.check_hostname is False

    def test_invalid_cafile_returns_error(self):
        result = tls_create_context(verify=True, cafile="/no/such/file.pem")
        assert "error" in result

    def test_invalid_certfile_returns_error(self):
        result = tls_create_context(verify=False, certfile="/no/such/cert.pem")
        assert "error" in result

    def test_invalid_socket_for_tls_wrap(self):
        ctx_info = tls_create_context(verify=False)
        result = tls_wrap_socket("not-a-socket", ctx_info)
        assert "error" in result

    def test_invalid_context_for_tls_wrap(self):
        sock = socket_create("inet", "stream")
        result = tls_wrap_socket(sock, "not-a-context")
        assert "error" in result
        socket_close(sock)  # safe: socket still open, not wrapped


# ===========================================================================
# TLS connect (live network — opt-in)
# ===========================================================================


class TestTLSConnect:
    @_NETWORK
    def test_tls_connect_to_google(self):
        result = tls_connect("www.google.com", 443)
        assert "error" not in result
        assert result.get("tls") is True
        assert result.get("tls_version") is not None
        socket_close(result)

    @_NETWORK
    def test_tls_connect_cipher_not_empty(self):
        result = tls_connect("www.google.com", 443)
        assert "error" not in result
        assert result.get("cipher") is not None
        socket_close(result)

    @_NETWORK
    def test_tls_connect_bad_host_returns_error(self):
        result = tls_connect("not.a.real.hostname.invalid", 443, timeout=5)
        assert "error" in result
