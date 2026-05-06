"""
Tests for NexusLang stdlib raw socket operations.

Groups:
1. Protocol map and constants
2. raw_socket_create (PermissionError when not root, ValueError for bad protocol)
3. raw_socket_create_icmp (convenience, same results as create('icmp'))
4. raw_socket_create_ethernet (Linux-only, skip on other platforms)
5. raw_socket_close (safe on None, valid dict, already-closed socket)
6. raw_socket_send (data type coercions, TypeError for bad types)
7. raw_socket_recv (mock-based: timeout, OSError, success path)
8. raw_socket_set_ip_hdrincl / raw_socket_set_timeout (mock-based)
9. raw_socket_bind (mock-based, checks dict update)
10. raw_socket_sniff (OSError when no AF_PACKET; PermissionError path)
11. sock_dict structure tests
"""

import os
import sys
import socket as _socket
from unittest.mock import MagicMock, patch, call

import pytest

_PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..")
)
if _PROJECT_ROOT not in sys.path:
    sys.path.insert(0, _PROJECT_ROOT)

from nexuslang.stdlib.network import (
    raw_socket_create,
    raw_socket_create_icmp,
    raw_socket_create_ethernet,
    raw_socket_bind,
    raw_socket_set_ip_hdrincl,
    raw_socket_set_timeout,
    raw_socket_send,
    raw_socket_recv,
    raw_socket_close,
    raw_socket_sniff,
    _RAW_PROTOCOL_MAP,
)

_IS_ROOT = os.geteuid() == 0 if hasattr(os, 'geteuid') else False
_requires_root = pytest.mark.skipif(not _IS_ROOT, reason="requires CAP_NET_RAW (root)")
_no_root = pytest.mark.skipif(_IS_ROOT, reason="test verifies PermissionError when not root")
_linux_only = pytest.mark.skipif(
    not hasattr(_socket, 'AF_PACKET'),
    reason="AF_PACKET not available on this platform (Linux only)"
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_raw_dict(af='inet', proto='icmp', proto_num=1):
    """Return a fake sock_dict backed by a real MagicMock (no privileges needed)."""
    mock_sock = MagicMock()
    mock_sock.recvfrom.return_value = (b'\x00' * 20, ('1.2.3.4', 0))
    return {
        'socket': mock_sock,
        'type': 'raw',
        'protocol': proto,
        'protocol_num': proto_num,
        'af': af,
    }


# ---------------------------------------------------------------------------
# 1. Protocol map
# ---------------------------------------------------------------------------

class TestProtocolMap:
    def test_icmp_is_present(self):
        assert 'icmp' in _RAW_PROTOCOL_MAP
        assert _RAW_PROTOCOL_MAP['icmp'] == _socket.IPPROTO_ICMP

    def test_tcp_is_present(self):
        assert 'tcp' in _RAW_PROTOCOL_MAP
        assert _RAW_PROTOCOL_MAP['tcp'] == _socket.IPPROTO_TCP

    def test_udp_is_present(self):
        assert 'udp' in _RAW_PROTOCOL_MAP
        assert _RAW_PROTOCOL_MAP['udp'] == _socket.IPPROTO_UDP

    def test_ip_is_present(self):
        assert 'ip' in _RAW_PROTOCOL_MAP
        assert _RAW_PROTOCOL_MAP['ip'] == _socket.IPPROTO_IP

    def test_all_values_are_integers(self):
        for name, num in _RAW_PROTOCOL_MAP.items():
            assert isinstance(num, int), f"protocol '{name}' value must be int"

    def test_at_least_six_protocols(self):
        assert len(_RAW_PROTOCOL_MAP) >= 6

    def test_all_raw_synonym(self):
        assert 'all' in _RAW_PROTOCOL_MAP
        assert 'raw' in _RAW_PROTOCOL_MAP

    def test_case_sensitivity_keys_are_lowercase(self):
        for key in _RAW_PROTOCOL_MAP:
            assert key == key.lower(), f"map key '{key}' should be lowercase"


# ---------------------------------------------------------------------------
# 2. raw_socket_create — error paths (no root required)
# ---------------------------------------------------------------------------

class TestRawSocketCreateErrors:
    def test_unknown_protocol_raises_value_error(self):
        with pytest.raises(ValueError, match="Unknown protocol"):
            raw_socket_create('bogus_proto_xyz')

    def test_unknown_protocol_message_lists_valid_options(self):
        with pytest.raises(ValueError) as exc_info:
            raw_socket_create('___invalid___')
        assert 'icmp' in str(exc_info.value).lower() or 'valid' in str(exc_info.value).lower()

    @_no_root
    def test_permission_error_without_root(self):
        with pytest.raises(PermissionError):
            raw_socket_create('icmp')

    @_no_root
    def test_permission_error_message_mentions_cap_net_raw(self):
        with pytest.raises(PermissionError) as exc_info:
            raw_socket_create('icmp')
        assert 'CAP_NET_RAW' in str(exc_info.value) or 'root' in str(exc_info.value).lower()

    def test_integer_protocol_raises_permission_without_root(self):
        if _IS_ROOT:
            pytest.skip("root — PermissionError not observable")
        with pytest.raises(PermissionError):
            raw_socket_create(1)   # IPPROTO_ICMP


# ---------------------------------------------------------------------------
# 3. raw_socket_create — success paths (root only)
# ---------------------------------------------------------------------------

class TestRawSocketCreateSuccess:
    @_requires_root
    def test_create_icmp_returns_dict(self):
        d = raw_socket_create('icmp')
        assert d['type'] == 'raw'
        assert d['protocol'] == 'icmp'
        assert d['af'] == 'inet'
        assert 'socket' in d
        raw_socket_close(d)

    @_requires_root
    def test_create_tcp_raw(self):
        d = raw_socket_create('tcp')
        assert d['protocol'] == 'tcp'
        raw_socket_close(d)

    @_requires_root
    def test_create_integer_protocol(self):
        d = raw_socket_create(_socket.IPPROTO_UDP)
        assert isinstance(d['protocol'], str)
        raw_socket_close(d)

    @_requires_root
    def test_create_protocol_uppercase_accepted(self):
        d = raw_socket_create('ICMP')
        assert d['protocol'] == 'icmp'
        raw_socket_close(d)

    @_requires_root
    def test_dict_has_all_required_keys(self):
        d = raw_socket_create('icmp')
        for key in ('socket', 'type', 'protocol', 'protocol_num', 'af'):
            assert key in d, f"missing key: {key}"
        raw_socket_close(d)


# ---------------------------------------------------------------------------
# 4. raw_socket_create_icmp
# ---------------------------------------------------------------------------

class TestRawSocketCreateIcmp:
    @_no_root
    def test_raises_permission_error_without_root(self):
        with pytest.raises(PermissionError):
            raw_socket_create_icmp()

    @_requires_root
    def test_equivalent_to_create_icmp(self):
        d = raw_socket_create_icmp()
        assert d['protocol'] == 'icmp'
        assert d['af'] == 'inet'
        raw_socket_close(d)


# ---------------------------------------------------------------------------
# 5. raw_socket_create_ethernet
# ---------------------------------------------------------------------------

class TestRawSocketCreateEthernet:
    def test_raises_os_error_without_af_packet(self):
        if hasattr(_socket, 'AF_PACKET'):
            pytest.skip("AF_PACKET present; can't test non-Linux path")
        with pytest.raises(OSError, match="AF_PACKET"):
            raw_socket_create_ethernet()

    @_linux_only
    @_no_root
    def test_raises_permission_error_without_root(self):
        with pytest.raises(PermissionError):
            raw_socket_create_ethernet()

    @_linux_only
    @_requires_root
    def test_creates_packet_socket(self):
        d = raw_socket_create_ethernet()
        assert d['type'] == 'raw_ethernet'
        assert d['af'] == 'packet'
        assert d['protocol'] == 'ethernet'
        raw_socket_close(d)


# ---------------------------------------------------------------------------
# 6. raw_socket_close
# ---------------------------------------------------------------------------

class TestRawSocketClose:
    def test_close_none_is_safe(self):
        raw_socket_close(None)  # must not raise

    def test_close_dict_with_none_socket(self):
        raw_socket_close({'socket': None})  # must not raise

    def test_close_already_closed_mock(self):
        mock = MagicMock()
        mock.close.side_effect = OSError("already closed")
        d = {'socket': mock}
        raw_socket_close(d)  # must not raise; swallows error

    def test_close_calls_socket_close(self):
        mock = MagicMock()
        d = {'socket': mock}
        raw_socket_close(d)
        mock.close.assert_called_once()

    def test_close_empty_dict(self):
        raw_socket_close({})  # 'socket' key absent — safe

    def test_close_twice_is_safe(self):
        mock = MagicMock()
        d = {'socket': mock}
        raw_socket_close(d)
        raw_socket_close(d)
        assert mock.close.call_count == 2


# ---------------------------------------------------------------------------
# 7. raw_socket_send
# ---------------------------------------------------------------------------

class TestRawSocketSend:
    def test_send_str_encodes_latin1(self):
        d = _make_raw_dict()
        d['socket'].sendto.return_value = 5
        n = raw_socket_send(d, 'hello', '1.2.3.4')
        d['socket'].sendto.assert_called_once_with(b'hello', ('1.2.3.4', 0))
        assert n == 5

    def test_send_bytes_unchanged(self):
        d = _make_raw_dict()
        d['socket'].sendto.return_value = 4
        raw_socket_send(d, b'\xde\xad\xbe\xef', '1.2.3.4')
        args, _ = d['socket'].sendto.call_args
        assert args[0] == b'\xde\xad\xbe\xef'

    def test_send_bytearray(self):
        d = _make_raw_dict()
        d['socket'].sendto.return_value = 3
        raw_socket_send(d, bytearray(b'\x01\x02\x03'), '10.0.0.1')
        args, _ = d['socket'].sendto.call_args
        assert args[0] == b'\x01\x02\x03'

    def test_send_list_of_ints(self):
        d = _make_raw_dict()
        d['socket'].sendto.return_value = 3
        raw_socket_send(d, [0x45, 0x00, 0x14], '10.0.0.1')
        args, _ = d['socket'].sendto.call_args
        assert args[0] == bytes([0x45, 0x00, 0x14])

    def test_send_invalid_type_raises_type_error(self):
        d = _make_raw_dict()
        with pytest.raises(TypeError, match="bytes|str|list"):
            raw_socket_send(d, 12345, '1.2.3.4')

    def test_send_no_addr_uses_default_dest(self):
        d = _make_raw_dict()
        d['socket'].sendto.return_value = 1
        raw_socket_send(d, b'\x00')
        args, _ = d['socket'].sendto.call_args
        assert args[1] == ('0.0.0.0', 0)

    def test_send_af_packet_uses_send_not_sendto(self):
        d = _make_raw_dict(af='packet')
        d['socket'].send.return_value = 6
        n = raw_socket_send(d, b'\x01\x02\x03\x04\x05\x06')
        d['socket'].send.assert_called_once()
        assert n == 6


# ---------------------------------------------------------------------------
# 8. raw_socket_recv
# ---------------------------------------------------------------------------

class TestRawSocketRecv:
    def test_recv_success_path(self):
        payload = b'\x45\x00\x14\x00' + b'\x00' * 16
        d = _make_raw_dict()
        d['socket'].recvfrom.return_value = (payload, ('192.168.1.1', 0))
        result = raw_socket_recv(d, 65535)
        assert result['data'] == payload
        assert result['from_addr'] == '192.168.1.1'
        assert result['length'] == len(payload)
        assert result['hex'] == payload.hex()

    def test_recv_timeout_returns_error_dict(self):
        d = _make_raw_dict()
        d['socket'].recvfrom.side_effect = _socket.timeout("timed out")
        result = raw_socket_recv(d)
        assert result['error'] == 'timeout'
        assert result['data'] == b''
        assert result['length'] == 0

    def test_recv_os_error_returns_error_dict(self):
        d = _make_raw_dict()
        d['socket'].recvfrom.side_effect = OSError("network unreachable")
        result = raw_socket_recv(d)
        assert 'error' in result
        assert 'unreachable' in result['error']

    def test_recv_passes_buffer_size(self):
        d = _make_raw_dict()
        d['socket'].recvfrom.return_value = (b'x', ('1.1.1.1', 0))
        raw_socket_recv(d, 1234)
        d['socket'].recvfrom.assert_called_with(1234)

    def test_recv_result_contains_hex(self):
        d = _make_raw_dict()
        d['socket'].recvfrom.return_value = (b'\xff\x00', ('10.0.0.1', 0))
        result = raw_socket_recv(d)
        assert result['hex'] == 'ff00'


# ---------------------------------------------------------------------------
# 9. raw_socket_set_ip_hdrincl and raw_socket_set_timeout
# ---------------------------------------------------------------------------

class TestRawSocketOptions:
    def test_set_ip_hdrincl_true(self):
        d = _make_raw_dict()
        raw_socket_set_ip_hdrincl(d, True)
        d['socket'].setsockopt.assert_called_once_with(
            _socket.IPPROTO_IP, _socket.IP_HDRINCL, 1
        )

    def test_set_ip_hdrincl_false(self):
        d = _make_raw_dict()
        raw_socket_set_ip_hdrincl(d, False)
        d['socket'].setsockopt.assert_called_once_with(
            _socket.IPPROTO_IP, _socket.IP_HDRINCL, 0
        )

    def test_set_ip_hdrincl_default_is_true(self):
        d = _make_raw_dict()
        raw_socket_set_ip_hdrincl(d)
        d['socket'].setsockopt.assert_called_once_with(
            _socket.IPPROTO_IP, _socket.IP_HDRINCL, 1
        )

    def test_set_timeout_with_float(self):
        d = _make_raw_dict()
        raw_socket_set_timeout(d, 2.5)
        d['socket'].settimeout.assert_called_once_with(2.5)

    def test_set_timeout_none_disables_timeout(self):
        d = _make_raw_dict()
        raw_socket_set_timeout(d, None)
        d['socket'].settimeout.assert_called_once_with(None)

    def test_set_timeout_zero(self):
        d = _make_raw_dict()
        raw_socket_set_timeout(d, 0.0)
        d['socket'].settimeout.assert_called_once_with(0.0)


# ---------------------------------------------------------------------------
# 10. raw_socket_bind
# ---------------------------------------------------------------------------

class TestRawSocketBind:
    def test_bind_inet_socket_calls_bind(self):
        d = _make_raw_dict(af='inet')
        result = raw_socket_bind(d, addr='0.0.0.0')
        d['socket'].bind.assert_called_once_with(('0.0.0.0', 0))
        assert result.get('bound') is True
        assert result.get('bound_addr') == '0.0.0.0'

    def test_bind_packet_socket_calls_bind_with_interface(self):
        d = _make_raw_dict(af='packet')
        result = raw_socket_bind(d, interface='eth0')
        d['socket'].bind.assert_called_once_with(('eth0', 0))
        assert result.get('bound_addr') == 'eth0'

    def test_bind_os_error_returns_error_in_dict(self):
        d = _make_raw_dict(af='inet')
        d['socket'].bind.side_effect = OSError("address already in use")
        result = raw_socket_bind(d, addr='0.0.0.0')
        assert 'error' in result

    def test_bind_returns_copy_not_mutate_original(self):
        d = _make_raw_dict(af='inet')
        result = raw_socket_bind(d, addr='0.0.0.0')
        assert 'bound' not in d  # original not mutated
        assert result.get('bound') is True


# ---------------------------------------------------------------------------
# 11. raw_socket_sniff
# ---------------------------------------------------------------------------

class TestRawSocketSniff:
    def test_raises_os_error_without_af_packet(self):
        if hasattr(_socket, 'AF_PACKET'):
            pytest.skip("AF_PACKET present; can't test non-Linux path")
        with pytest.raises(OSError, match="AF_PACKET"):
            raw_socket_sniff()

    @_linux_only
    @_no_root
    def test_raises_permission_error_without_root(self):
        with pytest.raises(PermissionError, match="CAP_NET_RAW"):
            raw_socket_sniff()

    @_linux_only
    def test_sniff_with_mock_socket_returns_list(self):
        """Test the sniffing logic without actual privileges by mocking socket."""
        # Build a fake 20-byte IPv4 header: protocol=6 (TCP), src=10.0.0.1, dst=10.0.0.2
        src = bytes([10, 0, 0, 1])
        dst = bytes([10, 0, 0, 2])
        header = bytearray(20)
        header[9] = 6   # TCP
        header[12:16] = src
        header[16:20] = dst

        with patch('nexuslang.stdlib.network.socket') as mock_socket_module:
            mock_socket_module.htons.return_value = 0x0300
            mock_socket_module.AF_PACKET = _socket.AF_PACKET
            mock_socket_module.SOCK_RAW = _socket.SOCK_RAW
            mock_sock = MagicMock()
            mock_sock.recvfrom.side_effect = [
                (bytes(header), ('lo', 0x0300, 0, 0, b'')),
                _socket.timeout(),
            ]
            mock_socket_module.socket.return_value = mock_sock
            mock_socket_module.timeout = _socket.timeout

            result = raw_socket_sniff(count=5, timeout=0.1)

        assert isinstance(result, list)


# ---------------------------------------------------------------------------
# 12. Sock dict structure
# ---------------------------------------------------------------------------

class TestSockDictStructure:
    @_requires_root
    def test_create_all_keys_present(self):
        d = raw_socket_create('icmp')
        required = {'socket', 'type', 'protocol', 'protocol_num', 'af'}
        assert required.issubset(d.keys())
        raw_socket_close(d)

    def test_mock_dict_close_does_not_mutate(self):
        d = _make_raw_dict()
        original_keys = set(d.keys())
        raw_socket_close(d)
        assert set(d.keys()) == original_keys

    def test_raw_type_always_in_inet_dict(self):
        d = _make_raw_dict(af='inet')
        assert d['type'] == 'raw'

    def test_raw_ethernet_type_in_packet_dict(self):
        d = _make_raw_dict(af='packet')
        d['type'] = 'raw_ethernet'
        assert d['type'] == 'raw_ethernet'
