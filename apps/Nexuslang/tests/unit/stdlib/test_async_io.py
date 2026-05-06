"""
Tests for NexusLang stdlib async I/O additions:
  - Async TCP sockets (connect, listen, accept, send, recv, recv_exactly, close)
  - Async UDP sockets (open, send_to, recv_from, close)
  - Async subprocess (subprocess, subprocess_output)
  - Async DNS (dns_resolve, dns_reverse)
  - Async readlines (readlines, readline_chunks)

All tests use loopback (127.0.0.1) or localhost so no external network is needed.
"""

import os
import sys
import socket as _socket
import tempfile
import threading
import time

import pytest

_PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..")
)
if _PROJECT_ROOT not in sys.path:
    sys.path.insert(0, _PROJECT_ROOT)

from nexuslang.stdlib.asyncio_utils.async_runtime import (
    NLPLTask,
    task_result,
    task_is_done,
    async_tcp_connect,
    async_tcp_listen,
    async_tcp_accept,
    async_tcp_send,
    async_tcp_recv,
    async_tcp_recv_exactly,
    async_tcp_close,
    async_udp_open,
    async_udp_send_to,
    async_udp_recv_from,
    async_udp_close,
    async_subprocess,
    async_subprocess_output,
    async_dns_resolve,
    async_dns_reverse,
    async_readlines,
    async_readline_chunks,
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _free_port() -> int:
    """Ask the OS for a free ephemeral port."""
    with _socket.socket(_socket.AF_INET, _socket.SOCK_STREAM) as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


# ---------------------------------------------------------------------------
# TCP: basic API contracts
# ---------------------------------------------------------------------------

class TestAsyncTcpApi:
    def test_async_tcp_listen_returns_task(self):
        port = _free_port()
        task = async_tcp_listen('127.0.0.1', port)
        assert isinstance(task, NLPLTask)
        result = task_result(task, timeout=5)
        assert 'socket' in result
        assert result['port'] == port
        result['socket'].close()

    def test_async_tcp_listen_os_assigns_port_when_zero(self):
        task = async_tcp_listen('127.0.0.1', 0)
        result = task_result(task, timeout=5)
        assert isinstance(result['port'], int)
        assert result['port'] > 0
        result['socket'].close()

    def test_async_tcp_connect_returns_task(self):
        port = _free_port()
        srv_sock = _socket.socket(_socket.AF_INET, _socket.SOCK_STREAM)
        srv_sock.setsockopt(_socket.SOL_SOCKET, _socket.SO_REUSEADDR, 1)
        srv_sock.bind(('127.0.0.1', port))
        srv_sock.listen(1)
        try:
            task = async_tcp_connect('127.0.0.1', port)
            assert isinstance(task, NLPLTask)
            conn_dict = task_result(task, timeout=5)
            assert 'socket' in conn_dict
            assert conn_dict['host'] == '127.0.0.1'
            assert conn_dict['port'] == port
            conn_dict['socket'].close()
        finally:
            srv_sock.close()

    def test_async_tcp_connect_timeout_on_refused(self):
        port = _free_port()
        task = async_tcp_connect('127.0.0.1', port, timeout=1.0)
        with pytest.raises(ConnectionRefusedError):
            task_result(task, timeout=3)

    def test_async_tcp_accept_returns_client_info(self):
        port = _free_port()
        srv_task = async_tcp_listen('127.0.0.1', port)
        server = task_result(srv_task, timeout=5)

        def _client():
            c = _socket.socket(_socket.AF_INET, _socket.SOCK_STREAM)
            c.connect(('127.0.0.1', port))
            time.sleep(0.2)
            c.close()

        t = threading.Thread(target=_client)
        t.start()

        accept_task = async_tcp_accept(server)
        accepted = task_result(accept_task, timeout=5)
        assert 'socket' in accepted
        assert isinstance(accepted['host'], str)
        assert isinstance(accepted['port'], int)
        accepted['socket'].close()
        server['socket'].close()
        t.join(timeout=2)

    def test_async_tcp_close_is_idempotent(self):
        port = _free_port()
        srv = _socket.socket(_socket.AF_INET, _socket.SOCK_STREAM)
        srv.setsockopt(_socket.SOL_SOCKET, _socket.SO_REUSEADDR, 1)
        srv.bind(('127.0.0.1', port))
        srv.listen(1)
        task = async_tcp_connect('127.0.0.1', port)
        conn = task_result(task, timeout=5)
        async_tcp_close(conn)
        async_tcp_close(conn)  # second call must not raise
        srv.close()


class TestAsyncTcpSendRecv:
    """Round-trip send/recv over a loopback TCP connection."""

    def _make_pair(self):
        port = _free_port()
        srv_task = async_tcp_listen('127.0.0.1', port)
        server = task_result(srv_task, timeout=5)
        connect_task = async_tcp_connect('127.0.0.1', port)
        accept_task = async_tcp_accept(server)
        client = task_result(connect_task, timeout=5)
        accepted = task_result(accept_task, timeout=5)
        server['socket'].close()
        return client, accepted

    def test_send_and_recv_str(self):
        client, server = self._make_pair()
        try:
            send_task = async_tcp_send(client, "hello world")
            n = task_result(send_task, timeout=5)
            assert n == len("hello world".encode('utf-8'))

            recv_task = async_tcp_recv(server, 64)
            result = task_result(recv_task, timeout=5)
            assert result['data'] == 'hello world'
            assert result['length'] == 11
            assert not result['eof']
        finally:
            async_tcp_close(client)
            async_tcp_close(server)

    def test_send_and_recv_bytes(self):
        client, server = self._make_pair()
        try:
            payload = b'\x00\x01\x02\x03'
            send_task = async_tcp_send(client, payload)
            task_result(send_task, timeout=5)

            recv_task = async_tcp_recv(server, 64)
            result = task_result(recv_task, timeout=5)
            assert result['bytes'] == payload
            assert result['length'] == 4
        finally:
            async_tcp_close(client)
            async_tcp_close(server)

    def test_recv_exactly(self):
        client, server = self._make_pair()
        try:
            message = b'ABCDEFGHIJ'
            send_task = async_tcp_send(client, message)
            task_result(send_task, timeout=5)

            recv_task = async_tcp_recv_exactly(server, 10)
            result = task_result(recv_task, timeout=5)
            assert result['bytes'] == message
            assert result['length'] == 10
        finally:
            async_tcp_close(client)
            async_tcp_close(server)

    def test_recv_shows_eof_on_closed_connection(self):
        client, server = self._make_pair()
        async_tcp_close(client)
        recv_task = async_tcp_recv(server, 64)
        result = task_result(recv_task, timeout=5)
        assert result['eof']
        assert result['length'] == 0
        async_tcp_close(server)

    def test_send_non_str_converts_to_str(self):
        client, server = self._make_pair()
        try:
            send_task = async_tcp_send(client, 42)
            n = task_result(send_task, timeout=5)
            assert n == len(b'42')
        finally:
            async_tcp_close(client)
            async_tcp_close(server)


# ---------------------------------------------------------------------------
# UDP
# ---------------------------------------------------------------------------

class TestAsyncUdp:
    def test_udp_open_returns_task(self):
        task = async_udp_open()
        assert isinstance(task, NLPLTask)
        result = task_result(task, timeout=5)
        assert 'socket' in result
        assert isinstance(result['port'], int)
        assert result['port'] > 0
        async_udp_close(result)

    def test_udp_open_with_explicit_bind(self):
        port = _free_port()
        task = async_udp_open('127.0.0.1', port)
        result = task_result(task, timeout=5)
        assert result['port'] == port
        async_udp_close(result)

    def test_udp_send_recv_loopback(self):
        recv_task = async_udp_open('127.0.0.1')
        receiver = task_result(recv_task, timeout=5)
        port = receiver['port']

        send_task = async_udp_open()
        sender = task_result(send_task, timeout=5)

        try:
            st = async_udp_send_to(sender, "ping", '127.0.0.1', port)
            n = task_result(st, timeout=5)
            assert n == 4

            rt = async_udp_recv_from(receiver, 64)
            data = task_result(rt, timeout=5)
            assert data['data'] == 'ping'
            assert data['from_host'] == '127.0.0.1'
            assert data['length'] == 4
        finally:
            async_udp_close(sender)
            async_udp_close(receiver)

    def test_udp_send_bytes(self):
        recv_task = async_udp_open('127.0.0.1')
        receiver = task_result(recv_task, timeout=5)
        port = receiver['port']

        send_task = async_udp_open()
        sender = task_result(send_task, timeout=5)
        try:
            st = async_udp_send_to(sender, b'\xde\xad\xbe\xef', '127.0.0.1', port)
            n = task_result(st, timeout=5)
            assert n == 4

            rt = async_udp_recv_from(receiver, 64)
            data = task_result(rt, timeout=5)
            assert data['bytes'] == b'\xde\xad\xbe\xef'
        finally:
            async_udp_close(sender)
            async_udp_close(receiver)

    def test_udp_close_is_idempotent(self):
        task = async_udp_open()
        conn = task_result(task, timeout=5)
        async_udp_close(conn)
        async_udp_close(conn)


# ---------------------------------------------------------------------------
# Subprocess
# ---------------------------------------------------------------------------

class TestAsyncSubprocess:
    def test_echo_command_succeeds(self):
        task = async_subprocess('echo', ['hello'])
        assert isinstance(task, NLPLTask)
        result = task_result(task, timeout=10)
        assert result['success']
        assert 'hello' in result['stdout']
        assert result['returncode'] == 0

    def test_nonzero_exit_code(self):
        task = async_subprocess('false')
        result = task_result(task, timeout=10)
        assert not result['success']
        assert result['returncode'] != 0

    def test_stdin_data_is_passed(self):
        task = async_subprocess('cat', stdin_data='from stdin')
        result = task_result(task, timeout=10)
        assert result['success']
        assert 'from stdin' in result['stdout']

    def test_missing_command_returns_error_dict(self):
        task = async_subprocess('this_command_does_not_exist_nxl_test')
        result = task_result(task, timeout=10)
        assert 'error' in result
        assert not result['success']

    def test_subprocess_output_returns_string(self):
        task = async_subprocess_output('echo', ['world'])
        assert isinstance(task, NLPLTask)
        result = task_result(task, timeout=10)
        assert isinstance(result, str)
        assert 'world' in result

    def test_subprocess_output_missing_command(self):
        task = async_subprocess_output('this_command_does_not_exist_nxl_test')
        result = task_result(task, timeout=10)
        assert isinstance(result, dict)
        assert 'error' in result

    def test_subprocess_with_args_list(self):
        task = async_subprocess('printf', ['%s %s', 'foo', 'bar'])
        result = task_result(task, timeout=10)
        assert result['success']
        assert 'foo' in result['stdout']

    def test_subprocess_captures_stderr(self):
        task = async_subprocess('sh', ['-c', 'echo err 1>&2'])
        result = task_result(task, timeout=10)
        assert 'err' in result['stderr']


# ---------------------------------------------------------------------------
# DNS
# ---------------------------------------------------------------------------

class TestAsyncDns:
    def test_resolve_localhost_returns_dict(self):
        task = async_dns_resolve('localhost')
        assert isinstance(task, NLPLTask)
        result = task_result(task, timeout=10)
        assert 'addresses' in result
        assert isinstance(result['addresses'], list)
        assert len(result['addresses']) > 0
        assert 'primary' in result
        assert result['hostname'] == 'localhost'

    def test_resolve_invalid_hostname_returns_error(self):
        task = async_dns_resolve('this.hostname.absolutely.does.not.exist.invalid')
        result = task_result(task, timeout=10)
        assert 'error' in result

    def test_resolve_returns_ip_strings(self):
        task = async_dns_resolve('127.0.0.1')
        result = task_result(task, timeout=10)
        # getaddrinfo on a literal IP also works
        if 'error' not in result:
            for addr in result['addresses']:
                assert isinstance(addr, str)
                parts = addr.split('.')
                if len(parts) == 4:
                    assert all(p.isdigit() for p in parts)

    def test_reverse_lookup_loopback(self):
        task = async_dns_reverse('127.0.0.1')
        assert isinstance(task, NLPLTask)
        result = task_result(task, timeout=10)
        # Either success ('hostname' key) or an error dict
        assert 'ip' in result

    def test_reverse_lookup_invalid_ip_returns_error(self):
        task = async_dns_reverse('999.999.999.999')
        result = task_result(task, timeout=10)
        assert 'error' in result


# ---------------------------------------------------------------------------
# Async readlines
# ---------------------------------------------------------------------------

class TestAsyncReadlines:
    def test_readlines_returns_task(self, tmp_path):
        f = tmp_path / "lines.txt"
        f.write_text("line1\nline2\nline3\n")
        task = async_readlines(str(f))
        assert isinstance(task, NLPLTask)
        lines = task_result(task, timeout=5)
        assert isinstance(lines, list)
        assert len(lines) == 3
        assert lines[0] == 'line1\n'
        assert lines[2] == 'line3\n'

    def test_readlines_empty_file(self, tmp_path):
        f = tmp_path / "empty.txt"
        f.write_text("")
        lines = task_result(async_readlines(str(f)), timeout=5)
        assert lines == []

    def test_readlines_preserves_newlines(self, tmp_path):
        f = tmp_path / "mixed.txt"
        f.write_text("a\nb\nc")
        lines = task_result(async_readlines(str(f)), timeout=5)
        assert lines[0] == 'a\n'
        assert lines[1] == 'b\n'
        assert lines[2] == 'c'

    def test_readline_chunks_returns_task(self, tmp_path):
        f = tmp_path / "big.txt"
        f.write_text('\n'.join(str(i) for i in range(200)) + '\n')
        task = async_readline_chunks(str(f), chunk_size=50)
        assert isinstance(task, NLPLTask)
        lines = task_result(task, timeout=5)
        assert isinstance(lines, list)
        assert len(lines) == 50

    def test_readline_chunks_default_100(self, tmp_path):
        f = tmp_path / "long.txt"
        f.write_text('\n'.join(['x'] * 150) + '\n')
        lines = task_result(async_readline_chunks(str(f)), timeout=5)
        assert len(lines) == 100

    def test_readline_chunks_fewer_than_chunk_size(self, tmp_path):
        f = tmp_path / "short.txt"
        f.write_text("only five\nlines here\nthree\nfour\nfive\n")
        lines = task_result(async_readline_chunks(str(f), chunk_size=100), timeout=5)
        assert len(lines) == 5

    def test_readlines_encoding(self, tmp_path):
        f = tmp_path / "utf8.txt"
        f.write_bytes("hello\nworld\n".encode('utf-8'))
        lines = task_result(async_readlines(str(f), encoding='utf-8'), timeout=5)
        assert lines[0] == 'hello\n'


# ---------------------------------------------------------------------------
# Task lifecycle correctness
# ---------------------------------------------------------------------------

class TestTaskLifecycle:
    def test_task_is_done_after_result(self):
        port = _free_port()
        task = async_tcp_listen('127.0.0.1', port)
        result = task_result(task, timeout=5)
        assert task_is_done(task)
        result['socket'].close()

    def test_udp_task_is_done_after_result(self):
        task = async_udp_open()
        conn = task_result(task, timeout=5)
        assert task_is_done(task)
        async_udp_close(conn)

    def test_subprocess_task_is_done_after_result(self):
        task = async_subprocess('true')
        task_result(task, timeout=5)
        assert task_is_done(task)

    def test_dns_task_is_done_after_result(self):
        task = async_dns_resolve('localhost')
        task_result(task, timeout=10)
        assert task_is_done(task)

    def test_readlines_task_is_done_after_result(self, tmp_path):
        f = tmp_path / "t.txt"
        f.write_text("a\nb\n")
        task = async_readlines(str(f))
        task_result(task, timeout=5)
        assert task_is_done(task)
