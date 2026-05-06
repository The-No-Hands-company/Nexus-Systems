"""
Network module for the NexusLang standard library.
This module provides networking functions and utilities.
"""

import socket
import urllib.request
import urllib.parse
import urllib.error
import json
import ssl
import http.client
from typing import Dict, Any, Optional, Union
from ...runtime.runtime import Runtime

def register_network_functions(runtime: Runtime) -> None:
    """Register network functions with the runtime."""
    # HTTP operations
    runtime.register_function("http_get", http_get)
    runtime.register_function("http_post", http_post)
    runtime.register_function("http_put", http_put)
    runtime.register_function("http_delete", http_delete)
    runtime.register_function("http_head", http_head)
    runtime.register_function("http_request", http_request)
    
    # URL operations
    runtime.register_function("url_encode", url_encode)
    runtime.register_function("url_decode", url_decode)
    runtime.register_function("url_parse", url_parse)
    runtime.register_function("url_join", url_join)
    
    # Socket operations
    runtime.register_function("socket_create", socket_create)
    runtime.register_function("socket_connect", socket_connect)
    runtime.register_function("socket_send", socket_send)
    runtime.register_function("socket_receive", socket_receive)
    runtime.register_function("socket_close", socket_close)
    
    # Server socket operations
    runtime.register_function("socket_bind", socket_bind)
    runtime.register_function("socket_listen", socket_listen)
    runtime.register_function("socket_accept", socket_accept)
    runtime.register_function("socket_set_option", socket_set_option)

    # UDP datagram operations
    runtime.register_function("udp_send_to", udp_send_to)
    runtime.register_function("udp_receive_from", udp_receive_from)

    # TLS/SSL operations
    runtime.register_function("tls_create_context", tls_create_context)
    runtime.register_function("tls_wrap_socket", tls_wrap_socket)
    runtime.register_function("tls_connect", tls_connect)
    runtime.register_function("tls_wrap_server_socket", tls_wrap_server_socket)

    # DNS operations
    runtime.register_function("dns_lookup", dns_lookup)
    runtime.register_function("dns_reverse_lookup", dns_reverse_lookup)

    # JSON operations
    runtime.register_function("json_encode", json_encode)
    runtime.register_function("json_decode", json_decode)

    # Raw socket operations
    runtime.register_function("raw_socket_create", raw_socket_create)
    runtime.register_function("raw_socket_create_icmp", raw_socket_create_icmp)
    runtime.register_function("raw_socket_create_ethernet", raw_socket_create_ethernet)
    runtime.register_function("raw_socket_bind", raw_socket_bind)
    runtime.register_function("raw_socket_set_ip_hdrincl", raw_socket_set_ip_hdrincl)
    runtime.register_function("raw_socket_set_timeout", raw_socket_set_timeout)
    runtime.register_function("raw_socket_send", raw_socket_send)
    runtime.register_function("raw_socket_recv", raw_socket_recv)
    runtime.register_function("raw_socket_close", raw_socket_close)
    runtime.register_function("raw_socket_sniff", raw_socket_sniff)

# HTTP operations
def http_get(url, headers=None, timeout=30):
    """Send an HTTP GET request to the specified URL."""
    return http_request("GET", url, None, headers, timeout)

def http_post(url, data=None, headers=None, timeout=30):
    """Send an HTTP POST request to the specified URL."""
    return http_request("POST", url, data, headers, timeout)

def http_put(url, data=None, headers=None, timeout=30):
    """Send an HTTP PUT request to the specified URL."""
    return http_request("PUT", url, data, headers, timeout)

def http_delete(url, headers=None, timeout=30):
    """Send an HTTP DELETE request to the specified URL."""
    return http_request("DELETE", url, None, headers, timeout)

def http_head(url, headers=None, timeout=30):
    """Send an HTTP HEAD request to the specified URL."""
    return http_request("HEAD", url, None, headers, timeout)

def http_request(method, url, data=None, headers=None, timeout=30):
    """Send an HTTP request to the specified URL."""
    try:
        # Prepare the request
        headers = headers or {}
        
        # Convert data to bytes if it's a string or dictionary
        if isinstance(data, dict):
            data = json.dumps(data).encode('utf-8')
            if 'Content-Type' not in headers:
                headers['Content-Type'] = 'application/json'
        elif isinstance(data, str):
            data = data.encode('utf-8')
        
        # Create the request
        req = urllib.request.Request(
            url,
            data=data,
            headers=headers,
            method=method
        )
        
        # Send the request
        context = ssl.create_default_context()
        with urllib.request.urlopen(req, timeout=timeout, context=context) as response:
            status_code = response.status
            response_headers = dict(response.getheaders())
            content = response.read().decode('utf-8')
            
            # Try to parse JSON response
            try:
                content = json.loads(content)
            except json.JSONDecodeError:
                pass
            
            return {
                'status_code': status_code,
                'headers': response_headers,
                'content': content
            }
    except urllib.error.HTTPError as e:
        return {
            'status_code': e.code,
            'headers': dict(e.headers),
            'error': str(e),
            'content': e.read().decode('utf-8')
        }
    except Exception as e:
        return {
            'status_code': 0,
            'error': str(e),
            'content': None
        }

# URL operations
def url_encode(data):
    """Encode data for use in a URL.

    Accepts a string (percent-encodes it) or a dict (encodes as query string).
    """
    if isinstance(data, str):
        return urllib.parse.quote_plus(data)
    if isinstance(data, dict):
        return urllib.parse.urlencode(data)
    raise TypeError("url_encode() expects a string or dictionary")

def url_decode(query_string):
    """Decode a URL-encoded string or query string.

    If the input contains '=' it is treated as a query string and decoded into
    a dictionary.  Otherwise it is treated as a percent-encoded string and the
    decoded plain string is returned.
    """
    if not isinstance(query_string, str):
        raise TypeError("Expected a string")
    # Plain percent-encoded string (no key=value pairs)
    if '=' not in query_string:
        return urllib.parse.unquote_plus(query_string)
    result = dict(urllib.parse.parse_qsl(query_string))
    if result:
        return result
    # Fallback: plain string decode
    return urllib.parse.unquote_plus(query_string)

def url_parse(url):
    """Parse a URL into its components."""
    if not isinstance(url, str):
        raise TypeError("Expected a string")
    
    parsed = urllib.parse.urlparse(url)
    return {
        'scheme': parsed.scheme,
        'netloc': parsed.netloc,
        'path': parsed.path,
        'params': parsed.params,
        'query': parsed.query,
        'fragment': parsed.fragment,
        'username': parsed.username,
        'password': parsed.password,
        'hostname': parsed.hostname,
        'port': parsed.port
    }

def url_join(base, url):
    """Join a base URL and a possibly relative URL to form an absolute URL."""
    if not isinstance(base, str) or not isinstance(url, str):
        raise TypeError("Expected strings")
    
    return urllib.parse.urljoin(base, url)

# Socket operations
def socket_create(family='inet', type='stream'):
    """Create a new socket."""
    family_map = {
        'inet': socket.AF_INET,
        'inet6': socket.AF_INET6,
        'unix': socket.AF_UNIX
    }
    
    type_map = {
        'stream': socket.SOCK_STREAM,
        'dgram': socket.SOCK_DGRAM
    }
    
    if family not in family_map:
        raise ValueError(f"Invalid socket family: {family}")
    
    if type not in type_map:
        raise ValueError(f"Invalid socket type: {type}")
    
    sock = socket.socket(family_map[family], type_map[type])
    return {
        'socket': sock,
        'family': family,
        'type': type,
        'closed': False
    }

def socket_connect(sock_info, host, port):
    """Connect a socket to a remote host."""
    if not isinstance(sock_info, dict) or 'socket' not in sock_info:
        raise TypeError("Expected a socket object")
    
    sock = sock_info['socket']
    
    if sock_info.get('closed', True):
        raise ValueError("Socket is closed")
    
    try:
        sock.connect((host, port))
        return sock_info
    except Exception as e:
        return {
            'error': str(e)
        }

def socket_send(sock_info, data):
    """Send data through a socket."""
    if not isinstance(sock_info, dict) or 'socket' not in sock_info:
        raise TypeError("Expected a socket object")
    
    sock = sock_info['socket']
    
    if sock_info.get('closed', True):
        raise ValueError("Socket is closed")
    
    if isinstance(data, str):
        data = data.encode('utf-8')
    
    try:
        bytes_sent = sock.send(data)
        return bytes_sent
    except Exception as e:
        return {
            'error': str(e)
        }

def socket_receive(sock_info, buffer_size=4096):
    """Receive data from a socket."""
    if not isinstance(sock_info, dict) or 'socket' not in sock_info:
        raise TypeError("Expected a socket object")
    
    sock = sock_info['socket']
    
    if sock_info.get('closed', True):
        raise ValueError("Socket is closed")
    
    try:
        data = sock.recv(buffer_size)
        return data.decode('utf-8')
    except Exception as e:
        return {
            'error': str(e)
        }

def socket_close(sock_info):
    """Close a socket."""
    if not isinstance(sock_info, dict) or 'socket' not in sock_info:
        raise TypeError("Expected a socket object")
    
    sock = sock_info['socket']
    
    if sock_info.get('closed', True):
        return True
    
    try:
        sock.close()
        sock_info['closed'] = True
        return True
    except Exception as e:
        return {
            'error': str(e)
        }


def socket_bind(sock_info, host_or_path, port=None):
    """Bind a socket to an address.

    For TCP/UDP sockets pass *host_or_path* as a host string and *port* as an
    integer.  For Unix-domain sockets pass the socket-file path as
    *host_or_path* and omit *port*.
    """
    if not isinstance(sock_info, dict) or 'socket' not in sock_info:
        raise TypeError("Expected a socket object")
    sock = sock_info['socket']
    try:
        if sock_info.get('family') == 'unix' or port is None:
            sock.bind(host_or_path)
        else:
            sock.bind((host_or_path, int(port)))
        sock_info['bound'] = True
        return sock_info
    except Exception as e:
        return {'error': str(e)}


def socket_listen(sock_info, backlog=5):
    """Put the socket into listening mode.

    *backlog* is the maximum number of queued connections (default 5).
    """
    if not isinstance(sock_info, dict) or 'socket' not in sock_info:
        raise TypeError("Expected a socket object")
    sock = sock_info['socket']
    try:
        sock.listen(int(backlog))
        sock_info['listening'] = True
        return sock_info
    except Exception as e:
        return {'error': str(e)}


def socket_accept(sock_info):
    """Accept an incoming connection on a listening socket.

    Returns a new socket-info dict for the accepted connection together with
    the remote address as ``{'socket': ..., 'address': (host, port), ...}``.
    """
    if not isinstance(sock_info, dict) or 'socket' not in sock_info:
        raise TypeError("Expected a socket object")
    sock = sock_info['socket']
    try:
        conn, addr = sock.accept()
        return {
            'socket': conn,
            'family': sock_info.get('family', 'inet'),
            'type': sock_info.get('type', 'stream'),
            'closed': False,
            'address': addr,
        }
    except Exception as e:
        return {'error': str(e)}


def socket_set_option(sock_info, option, value):
    """Set a socket-level option using ``setsockopt``.

    Supported *option* strings:

    - ``'reuse_address'`` / ``'reuseaddr'`` — ``SO_REUSEADDR`` (value: bool/int)
    - ``'reuse_port'`` / ``'reuseport'``   — ``SO_REUSEPORT`` (value: bool/int)
    - ``'timeout'``                         — sets blocking timeout in seconds
    - ``'non_blocking'``                    — makes socket non-blocking (value: bool)
    - ``'keepalive'``                       — ``SO_KEEPALIVE`` (value: bool/int)
    - ``'nodelay'``                         — ``TCP_NODELAY`` (value: bool/int)
    - ``'broadcast'``                       — ``SO_BROADCAST`` (value: bool/int)
    """
    if not isinstance(sock_info, dict) or 'socket' not in sock_info:
        raise TypeError("Expected a socket object")
    sock = sock_info['socket']
    opt = option.lower().replace('-', '_')
    try:
        if opt in ('reuse_address', 'reuseaddr'):
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, int(bool(value)))
        elif opt in ('reuse_port', 'reuseport'):
            if hasattr(socket, 'SO_REUSEPORT'):
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, int(bool(value)))
        elif opt == 'timeout':
            sock.settimeout(float(value) if value else None)
        elif opt == 'non_blocking':
            sock.setblocking(not bool(value))
        elif opt == 'keepalive':
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, int(bool(value)))
        elif opt == 'nodelay':
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, int(bool(value)))
        elif opt == 'broadcast':
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, int(bool(value)))
        else:
            raise ValueError(f"Unknown socket option: '{option}'")
        return sock_info
    except Exception as e:
        return {'error': str(e)}


# ---------------------------------------------------------------------------
# UDP datagram operations
# ---------------------------------------------------------------------------

def udp_send_to(sock_info, data, host, port):
    """Send a datagram to (host, port) using the given UDP socket.

    Returns the number of bytes sent.
    """
    if not isinstance(sock_info, dict) or 'socket' not in sock_info:
        raise TypeError("Expected a socket object")
    if sock_info.get('type') not in ('dgram', None) and sock_info.get('type') != 'dgram':
        pass  # allow caller to pass any socket; sendto works on SOCK_DGRAM
    sock = sock_info['socket']
    if isinstance(data, str):
        data = data.encode('utf-8')
    try:
        return sock.sendto(data, (host, int(port)))
    except Exception as e:
        return {'error': str(e)}


def udp_receive_from(sock_info, buffer_size=4096):
    """Receive a datagram from the socket.

    Returns ``{'data': str, 'address': (host, port)}``.
    """
    if not isinstance(sock_info, dict) or 'socket' not in sock_info:
        raise TypeError("Expected a socket object")
    sock = sock_info['socket']
    try:
        raw, addr = sock.recvfrom(int(buffer_size))
        return {
            'data': raw.decode('utf-8', errors='replace'),
            'address': addr,
            'bytes': len(raw),
        }
    except Exception as e:
        return {'error': str(e)}


# ---------------------------------------------------------------------------
# TLS/SSL operations
# ---------------------------------------------------------------------------

def tls_create_context(verify=True, cafile=None, certfile=None, keyfile=None,
                       protocol=None, check_hostname=True):
    """Create an SSL context for use with TLS connections.

    Args:
        verify:          Verify peer certificate (default True).
        cafile:          Path to CA certificate bundle (PEM).  Uses system
                         default CAs when None.
        certfile:        Path to client/server certificate (PEM).
        keyfile:         Path to private key (PEM).  Defaults to *certfile*.
        protocol:        Ignored — always uses TLS 1.2+ (SSLContext default).
        check_hostname:  Enforce hostname verification (default True, only
                         meaningful when *verify* is True).

    Returns a dict holding the ``SSLContext`` under the key ``'context'``.
    """
    try:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT if verify else ssl.PROTOCOL_TLS_SERVER)

        if verify:
            ctx.verify_mode = ssl.CERT_REQUIRED
            ctx.check_hostname = bool(check_hostname)
            if cafile:
                ctx.load_verify_locations(cafile=cafile)
            else:
                ctx.load_default_certs()
        else:
            ctx.verify_mode = ssl.CERT_NONE
            ctx.check_hostname = False

        if certfile:
            ctx.load_cert_chain(certfile=certfile,
                                keyfile=keyfile or certfile)

        return {
            'context': ctx,
            'verify': verify,
            'cafile': cafile,
            'certfile': certfile,
        }
    except Exception as e:
        return {'error': str(e)}


def tls_wrap_socket(sock_info, context_info, server_hostname=None):
    """Wrap an existing connected socket with TLS (client side).

    Args:
        sock_info:        A socket dict as returned by ``socket_create``.
        context_info:     An SSL-context dict from ``tls_create_context``.
        server_hostname:  Hostname for SNI / certificate verification.

    Returns an updated socket dict whose ``'socket'`` key holds the wrapped
    ``SSLSocket`` so all other socket helpers continue to work.
    """
    if not isinstance(sock_info, dict) or 'socket' not in sock_info:
        return {'error': "Expected a socket dict from socket_create()"}
    if not isinstance(context_info, dict) or 'context' not in context_info:
        return {'error': "Expected an SSL context dict from tls_create_context()"}
    ctx: ssl.SSLContext = context_info['context']
    sock = sock_info['socket']
    try:
        wrapped = ctx.wrap_socket(sock, server_hostname=server_hostname)
        result = dict(sock_info)
        result['socket'] = wrapped
        result['tls'] = True
        result['server_hostname'] = server_hostname
        result['cipher'] = wrapped.cipher()
        result['tls_version'] = wrapped.version()
        return result
    except Exception as e:
        return {'error': str(e)}


def tls_connect(host, port, cafile=None, certfile=None, keyfile=None, verify=True,
                timeout=30):
    """Open a TLS-secured TCP connection to *host*:*port*.

    Convenience wrapper that creates a socket, connects, and wraps with TLS in
    one call.

    Returns a socket dict (same shape as ``socket_create`` + TLS metadata) on
    success, or ``{'error': str}`` on failure.
    """
    try:
        ctx_info = tls_create_context(verify=verify, cafile=cafile,
                                      certfile=certfile, keyfile=keyfile,
                                      check_hostname=verify)
        if 'error' in ctx_info:
            return ctx_info

        raw_sock = socket.create_connection((host, int(port)), timeout=timeout)
        sock_info = {
            'socket': raw_sock,
            'family': 'inet',
            'type': 'stream',
            'closed': False,
        }
        wrapped = tls_wrap_socket(sock_info, ctx_info, server_hostname=host)
        return wrapped
    except Exception as e:
        return {'error': str(e)}


def tls_wrap_server_socket(sock_info, certfile, keyfile, cafile=None,
                           require_client_cert=False):
    """Wrap a bound/listening socket with TLS for use as a TLS server.

    Args:
        sock_info:            Server socket dict (already bound + listening).
        certfile:             Path to the server certificate (PEM).
        keyfile:              Path to the server private key (PEM).
        cafile:               Path to CA bundle for client-cert verification.
        require_client_cert:  Require and verify client certificates when True.

    Returns an updated socket dict.  Call ``socket_accept`` on the returned
    dict to get TLS-enabled client connections.
    """
    if not isinstance(sock_info, dict) or 'socket' not in sock_info:
        raise TypeError("Expected a socket object")
    try:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(certfile=certfile, keyfile=keyfile)

        if require_client_cert and cafile:
            ctx.verify_mode = ssl.CERT_REQUIRED
            ctx.load_verify_locations(cafile=cafile)
        elif cafile:
            ctx.load_verify_locations(cafile=cafile)

        # Wrap the raw server socket so accept() yields TLS connections
        wrapped_server = ctx.wrap_socket(sock_info['socket'], server_side=True)
        result = dict(sock_info)
        result['socket'] = wrapped_server
        result['tls'] = True
        result['certfile'] = certfile
        return result
    except Exception as e:
        return {'error': str(e)}


# DNS operations
def dns_lookup(hostname):
    """Look up the IP address for a hostname."""
    try:
        return socket.gethostbyname(hostname)
    except socket.gaierror as e:
        return {
            'error': str(e)
        }

def dns_reverse_lookup(ip_address):
    """Look up the hostname for an IP address."""
    try:
        return socket.gethostbyaddr(ip_address)[0]
    except socket.herror as e:
        return {
            'error': str(e)
        }

# JSON operations
def json_encode(obj, indent=None):
    """Encode an object as a JSON string."""
    try:
        return json.dumps(obj, indent=indent)
    except (TypeError, ValueError) as e:
        return {
            'error': str(e)
        }

def json_decode(json_str):
    """Decode a JSON string into an object."""
    if not isinstance(json_str, str):
        raise TypeError("Expected a string")
    
    try:
        return json.loads(json_str)
    except json.JSONDecodeError as e:
        return {
            'error': str(e)
        }


# ---------------------------------------------------------------------------
# Raw socket operations
# ---------------------------------------------------------------------------

_RAW_PROTOCOL_MAP: Dict[str, int] = {
    'icmp':  socket.IPPROTO_ICMP,
    'tcp':   socket.IPPROTO_TCP,
    'udp':   socket.IPPROTO_UDP,
    'ip':    socket.IPPROTO_IP,
    'igmp':  getattr(socket, 'IPPROTO_IGMP', 2),
    'ospf':  getattr(socket, 'IPPROTO_OSPF', 89),
    'sctp':  getattr(socket, 'IPPROTO_SCTP', 132),
    'all':   socket.IPPROTO_RAW,
    'raw':   socket.IPPROTO_RAW,
}


def raw_socket_create(protocol: Any = 'icmp') -> dict:
    """Create a raw IP socket for the given protocol.

    protocol: one of 'icmp', 'tcp', 'udp', 'ip', 'igmp', 'ospf', 'sctp',
              'all', 'raw', or an integer protocol number.

    Returns a dict with keys: 'socket', 'type', 'protocol', 'protocol_num', 'af'.
    Raises PermissionError if the process lacks CAP_NET_RAW.
    Raises ValueError for unknown protocol names.
    """
    if isinstance(protocol, int):
        proto_num = protocol
        proto_name = str(protocol)
    else:
        proto_lower = str(protocol).lower()
        if proto_lower not in _RAW_PROTOCOL_MAP:
            raise ValueError(
                f"Unknown protocol '{protocol}'. "
                f"Valid options: {sorted(_RAW_PROTOCOL_MAP.keys())}"
            )
        proto_num = _RAW_PROTOCOL_MAP[proto_lower]
        proto_name = proto_lower
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_RAW, proto_num)
    except PermissionError:
        raise PermissionError(
            f"Creating a raw socket requires CAP_NET_RAW (root or sudo). "
            f"Protocol: {proto_name}"
        )
    return {
        'socket': sock,
        'type': 'raw',
        'protocol': proto_name,
        'protocol_num': proto_num,
        'af': 'inet',
    }


def raw_socket_create_icmp() -> dict:
    """Convenience: create an ICMP raw socket (AF_INET/SOCK_RAW/IPPROTO_ICMP)."""
    return raw_socket_create('icmp')


def raw_socket_create_ethernet() -> dict:
    """Create a Layer-2 raw socket (AF_PACKET/SOCK_RAW) on Linux.

    Captures all Ethernet frames when bound to an interface.
    Requires CAP_NET_RAW. Raises OSError on non-Linux platforms.
    """
    if not hasattr(socket, 'AF_PACKET'):
        raise OSError(
            "AF_PACKET is not available on this platform (Linux only). "
            "Use raw_socket_create() for IP-level raw sockets on other platforms."
        )
    try:
        ETH_P_ALL = socket.htons(0x0003)
        sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, ETH_P_ALL)
    except PermissionError:
        raise PermissionError(
            "Creating a packet socket requires CAP_NET_RAW (root or sudo)."
        )
    return {
        'socket': sock,
        'type': 'raw_ethernet',
        'protocol': 'ethernet',
        'protocol_num': 0x0003,
        'af': 'packet',
    }


def raw_socket_bind(sock_dict: dict, addr: str = '',
                    interface: str = '') -> dict:
    """Bind a raw socket to an IP address or network interface.

    For AF_INET sockets: pass addr as an IP string (e.g. '0.0.0.0').
    For AF_PACKET sockets: pass interface as an interface name (e.g. 'eth0').
    Returns an updated copy of sock_dict with 'bound' and 'bound_addr' keys,
    or 'error' on failure.
    """
    sock = sock_dict['socket']
    try:
        if sock_dict.get('af') == 'packet':
            sock.bind((str(interface), 0))
            bound_identifier = interface
        else:
            bind_addr = str(addr) if addr else ''
            sock.bind((bind_addr, 0))
            bound_identifier = addr or ''
        return dict(sock_dict, bound=True, bound_addr=bound_identifier)
    except OSError as e:
        return dict(sock_dict, error=str(e))


def raw_socket_set_ip_hdrincl(sock_dict: dict, enabled: bool = True) -> None:
    """Set IP_HDRINCL on the raw socket.

    When enabled=True the caller is responsible for constructing the full
    IP header in each packet passed to raw_socket_send().
    """
    sock = sock_dict['socket']
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_HDRINCL, int(bool(enabled)))


def raw_socket_set_timeout(sock_dict: dict, timeout: Optional[float]) -> None:
    """Set a receive timeout (seconds) on a raw socket. Pass None to disable."""
    sock = sock_dict['socket']
    sock.settimeout(float(timeout) if timeout is not None else None)


def raw_socket_send(sock_dict: dict, data: Any, addr: str = '',
                    port: int = 0) -> int:
    """Send raw data via the socket.

    data: str (encoded as latin-1 to preserve byte values), bytes, bytearray,
          or list of ints.
    addr: destination IP for AF_INET sockets (ignored for AF_PACKET).
    port: used only when addr is provided (most raw sockets ignore it).
    Returns the number of bytes transmitted.
    """
    sock = sock_dict['socket']
    if isinstance(data, str):
        payload = data.encode('latin-1')
    elif isinstance(data, (bytes, bytearray)):
        payload = bytes(data)
    elif isinstance(data, list):
        payload = bytes(data)
    else:
        raise TypeError(
            f"data must be bytes, bytearray, list[int], or str; "
            f"got {type(data).__name__}"
        )
    if sock_dict.get('af') == 'packet':
        return sock.send(payload)
    dest = (str(addr), int(port)) if addr else ('0.0.0.0', 0)
    return sock.sendto(payload, dest)


def raw_socket_recv(sock_dict: dict, buffer_size: int = 65535) -> dict:
    """Receive one raw packet from the socket.

    Returns a dict with keys:
    'data' (raw bytes), 'hex' (hex string), 'from_addr', 'from_port', 'length'.
    On timeout returns a dict with 'error': 'timeout'.
    On other OS errors returns a dict with 'error': <message>.
    """
    sock = sock_dict['socket']
    try:
        raw_data, addr = sock.recvfrom(int(buffer_size))
        from_ip = addr[0] if addr else ''
        from_port = addr[1] if len(addr) > 1 else 0
        return {
            'data': raw_data,
            'hex': raw_data.hex(),
            'from_addr': from_ip,
            'from_port': from_port,
            'length': len(raw_data),
        }
    except socket.timeout:
        return {'error': 'timeout', 'data': b'', 'length': 0}
    except OSError as e:
        return {'error': str(e), 'data': b'', 'length': 0}


def raw_socket_close(sock_dict: dict) -> None:
    """Close a raw socket. Safe to call on already-closed or None sockets."""
    if sock_dict is None:
        return
    sock = sock_dict.get('socket')
    if sock is not None:
        try:
            sock.close()
        except Exception:
            pass


def raw_socket_sniff(
    interface: str = '',
    count: int = 10,
    filter_proto: Optional[str] = None,
    buffer_size: int = 65535,
    timeout: float = 5.0,
) -> list:
    """Capture raw network packets from an interface (Linux only).

    Creates a temporary AF_PACKET socket, captures up to `count` packets,
    and returns them as a list of dicts.

    interface: network interface name (e.g. 'eth0', 'lo'). Empty = any.
    count: maximum number of packets to capture.
    filter_proto: optional protocol name to filter: 'icmp', 'tcp', 'udp',
                  'igmp', 'ospf', 'sctp'. None = capture all.
    buffer_size: per-packet receive buffer in bytes.
    timeout: per-recv timeout; stops when first timeout occurs.

    Each returned dict has keys: 'data', 'hex', 'length', 'interface',
    'ip_protocol', 'src_ip', 'dst_ip'.

    Requires CAP_NET_RAW (root or sudo). Linux only.
    """
    if not hasattr(socket, 'AF_PACKET'):
        raise OSError("raw_socket_sniff requires AF_PACKET (Linux only).")

    _FILTER_PROTOS: Dict[str, int] = {
        'icmp': 1, 'igmp': 2, 'tcp': 6, 'udp': 17,
        'ospf': 89, 'sctp': 132,
    }
    proto_filter_num: Optional[int] = None
    if filter_proto:
        key = str(filter_proto).lower()
        proto_filter_num = _FILTER_PROTOS.get(key)

    try:
        ETH_P_ALL = socket.htons(0x0003)
        sniff_sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, ETH_P_ALL)
        if interface:
            sniff_sock.bind((str(interface), 0))
        sniff_sock.settimeout(float(timeout))
    except PermissionError:
        raise PermissionError(
            "raw_socket_sniff requires CAP_NET_RAW (root or sudo)."
        )

    captured = []
    try:
        for _ in range(int(count)):
            try:
                raw, addr = sniff_sock.recvfrom(int(buffer_size))
                from_iface = addr[0] if addr else ''
                if len(raw) >= 20:
                    ip_proto = raw[9]
                    src_ip = '.'.join(str(b) for b in raw[12:16])
                    dst_ip = '.'.join(str(b) for b in raw[16:20])
                else:
                    ip_proto = 0
                    src_ip = dst_ip = ''
                if proto_filter_num is not None and ip_proto != proto_filter_num:
                    continue
                captured.append({
                    'data': raw,
                    'hex': raw.hex(),
                    'length': len(raw),
                    'interface': from_iface,
                    'ip_protocol': ip_proto,
                    'src_ip': src_ip,
                    'dst_ip': dst_ip,
                })
            except socket.timeout:
                break
    finally:
        sniff_sock.close()

    return captured
