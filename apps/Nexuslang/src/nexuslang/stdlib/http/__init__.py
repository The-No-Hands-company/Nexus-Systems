"""
HTTP Client and Server for NexusLang.

Features:
- HTTP Client: requests-like functionality for HTTP operations
- HTTP Server: routing, middleware, sessions, authentication
- URL utilities: encoding, parsing, joining
- Response handling: text, JSON, status codes
"""

import urllib.request
import urllib.parse
import urllib.error
import json as json_lib
import socket
import threading
import re
import secrets
import time
from typing import Optional, Dict, Any, Callable, List, Tuple, Pattern
from collections import defaultdict
from http.server import BaseHTTPRequestHandler, HTTPServer
from socketserver import ThreadingMixIn
from ...runtime.runtime import Runtime


class HTTPResponse:
    """HTTP response wrapper."""
    def __init__(self, status_code: int, headers: dict, body: bytes, url: str):
        self.status_code = status_code
        self.headers = headers
        self.body = body
        self.url = url
        self._text = None
        self._json = None
    
    @property
    def text(self) -> str:
        """Get response body as text."""
        if self._text is None:
            self._text = self.body.decode('utf-8', errors='replace')
        return self._text
    
    @property
    def json(self) -> Any:
        """Parse response body as JSON."""
        if self._json is None:
            self._json = json_lib.loads(self.text)
        return self._json
    
    @property
    def ok(self) -> bool:
        """Check if request was successful (status 200-299)."""
        return 200 <= self.status_code < 300


def http_request(method: str, url: str, headers: Optional[Dict] = None, 
                 data: Optional[Any] = None, timeout: int = 30) -> HTTPResponse:
    """Make HTTP request."""
    # Prepare headers
    req_headers = headers or {}
    
    # Prepare data
    req_data = None
    if data is not None:
        if isinstance(data, dict):
            req_data = json_lib.dumps(data).encode('utf-8')
            req_headers['Content-Type'] = 'application/json'
        elif isinstance(data, str):
            req_data = data.encode('utf-8')
        elif isinstance(data, bytes):
            req_data = data
    
    # Create request
    request = urllib.request.Request(url, data=req_data, headers=req_headers, method=method)
    
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            status_code = response.status
            headers = dict(response.headers)
            body = response.read()
            return HTTPResponse(status_code, headers, body, url)
    except urllib.error.HTTPError as e:
        # Return error response
        return HTTPResponse(e.code, dict(e.headers), e.read(), url)
    except urllib.error.URLError as e:
        raise RuntimeError(f"HTTP request failed: {e.reason}")


def http_get(url: str, headers: Optional[Dict] = None, timeout: int = 30) -> HTTPResponse:
    """Make HTTP GET request."""
    return http_request("GET", url, headers=headers, timeout=timeout)


def http_post(url: str, data: Optional[Any] = None, headers: Optional[Dict] = None, 
              timeout: int = 30) -> HTTPResponse:
    """Make HTTP POST request."""
    return http_request("POST", url, headers=headers, data=data, timeout=timeout)


def http_put(url: str, data: Optional[Any] = None, headers: Optional[Dict] = None,
             timeout: int = 30) -> HTTPResponse:
    """Make HTTP PUT request."""
    return http_request("PUT", url, headers=headers, data=data, timeout=timeout)


def http_delete(url: str, headers: Optional[Dict] = None, timeout: int = 30) -> HTTPResponse:
    """Make HTTP DELETE request."""
    return http_request("DELETE", url, headers=headers, timeout=timeout)


def http_patch(url: str, data: Optional[Any] = None, headers: Optional[Dict] = None,
               timeout: int = 30) -> HTTPResponse:
    """Make HTTP PATCH request."""
    return http_request("PATCH", url, headers=headers, data=data, timeout=timeout)


def http_head(url: str, headers: Optional[Dict] = None, timeout: int = 30) -> HTTPResponse:
    """Make HTTP HEAD request."""
    return http_request("HEAD", url, headers=headers, timeout=timeout)


def url_encode(params) -> str:
    """URL encode parameters or a plain string."""
    if isinstance(params, str):
        return urllib.parse.quote_plus(params)
    return urllib.parse.urlencode(params)


def url_decode(query_string: str):
    """Decode URL query string or plain encoded string."""
    if '=' not in query_string:
        return urllib.parse.unquote_plus(query_string)
    result = dict(urllib.parse.parse_qsl(query_string))
    return result if result else urllib.parse.unquote_plus(query_string)


def url_parse(url: str) -> dict:
    """Parse URL into components."""
    parsed = urllib.parse.urlparse(url)
    return {
        'scheme': parsed.scheme,
        'netloc': parsed.netloc,
        'path': parsed.path,
        'params': parsed.params,
        'query': parsed.query,
        'fragment': parsed.fragment,
        'hostname': parsed.hostname,
        'port': parsed.port,
        'username': parsed.username,
        'password': parsed.password,
    }


def url_join(base: str, path: str) -> str:
    """Join base URL with path."""
    return urllib.parse.urljoin(base, path)


def download_file(url: str, filepath: str, timeout: int = 30) -> bool:
    """Download file from URL."""
    try:
        urllib.request.urlretrieve(url, filepath)
        return True
    except Exception as e:
        print(f"Download failed: {e}")
        return False


def get_response_text(response: HTTPResponse) -> str:
    """Get response body as text."""
    return response.text


def get_response_json(response: HTTPResponse) -> Any:
    """Parse response body as JSON."""
    return response.json


def get_response_status(response: HTTPResponse) -> int:
    """Get response status code."""
    return response.status_code


def get_response_headers(response: HTTPResponse) -> dict:
    """Get response headers."""
    return response.headers


def is_response_ok(response: HTTPResponse) -> bool:
    """Check if response is successful."""
    return response.ok


# ============================================================================
# HTTP SERVER
# ============================================================================

class Request:
    """HTTP request object for server."""
    def __init__(self, method: str, path: str, headers: dict, body: bytes, 
                 query_params: dict, path_params: dict, cookies: dict):
        self.method = method
        self.path = path
        self.headers = headers
        self.body = body
        self.query_params = query_params
        self.path_params = path_params
        self.cookies = cookies
        self._json = None
        self._text = None
        self._form = None
    
    @property
    def json(self) -> Any:
        """Parse request body as JSON."""
        if self._json is None and self.body:
            try:
                self._json = json_lib.loads(self.body.decode('utf-8'))
            except (json_lib.JSONDecodeError, UnicodeDecodeError):
                self._json = None
        return self._json
    
    @property
    def text(self) -> str:
        """Get request body as text."""
        if self._text is None and self.body:
            self._text = self.body.decode('utf-8', errors='replace')
        return self._text
    
    @property
    def form(self) -> dict:
        """Parse form data from request body."""
        if self._form is None and self.body:
            try:
                self._form = dict(urllib.parse.parse_qsl(self.body.decode('utf-8')))
            except UnicodeDecodeError:
                self._form = {}
        return self._form if self._form else {}
    
    def get_header(self, name: str, default: str = '') -> str:
        """Get header value (case-insensitive)."""
        name_lower = name.lower()
        for key, value in self.headers.items():
            if key.lower() == name_lower:
                return value
        return default


class Response:
    """HTTP response object for server."""
    def __init__(self, body: Any = '', status: int = 200, headers: Optional[Dict] = None):
        self.status = status
        self.headers = headers or {}
        self._body = body
        self._cookies = []
    
    @property
    def body(self) -> bytes:
        """Get response body as bytes."""
        if isinstance(self._body, bytes):
            return self._body
        elif isinstance(self._body, str):
            return self._body.encode('utf-8')
        elif isinstance(self._body, dict) or isinstance(self._body, list):
            # Auto-convert to JSON
            if 'Content-Type' not in self.headers:
                self.headers['Content-Type'] = 'application/json'
            return json_lib.dumps(self._body).encode('utf-8')
        else:
            return str(self._body).encode('utf-8')
    
    def set_header(self, name: str, value: str):
        """Set response header."""
        self.headers[name] = value
    
    def set_cookie(self, name: str, value: str, max_age: Optional[int] = None, 
                   path: str = '/', http_only: bool = True, secure: bool = False):
        """Set response cookie."""
        cookie = f"{name}={value}; Path={path}"
        if max_age is not None:
            cookie += f"; Max-Age={max_age}"
        if http_only:
            cookie += "; HttpOnly"
        if secure:
            cookie += "; Secure"
        self._cookies.append(cookie)
    
    def get_cookies(self) -> List[str]:
        """Get all cookies to set."""
        return self._cookies


class Route:
    """URL route with pattern matching."""
    def __init__(self, method: str, pattern: str, handler: Callable):
        self.method = method.upper()
        self.pattern = pattern
        self.handler = handler
        self.regex, self.param_names = self._compile_pattern(pattern)
    
    def _compile_pattern(self, pattern: str) -> Tuple[Pattern, List[str]]:
        """Compile route pattern to regex."""
        param_names = []
        regex_pattern = pattern
        
        # Replace <param> with regex capture groups
        for match in re.finditer(r'<([^>]+)>', pattern):
            param_name = match.group(1)
            param_names.append(param_name)
            # Replace <param> with named capture group
            regex_pattern = regex_pattern.replace(f'<{param_name}>', r'(?P<' + param_name + r'>[^/]+)')
        
        # Anchor pattern
        regex_pattern = '^' + regex_pattern + '$'
        return re.compile(regex_pattern), param_names
    
    def match(self, path: str) -> Optional[Dict[str, str]]:
        """Match path against route pattern. Returns path parameters if match."""
        match = self.regex.match(path)
        if match:
            return match.groupdict()
        return None


class Session:
    """Server-side session storage."""
    def __init__(self, session_id: str):
        self.session_id = session_id
        self.data = {}
        self.created_at = time.time()
        self.last_accessed = time.time()
    
    def get(self, key: str, default: Any = None) -> Any:
        """Get session value."""
        self.last_accessed = time.time()
        return self.data.get(key, default)
    
    def set(self, key: str, value: Any):
        """Set session value."""
        self.last_accessed = time.time()
        self.data[key] = value
    
    def delete(self, key: str):
        """Delete session key."""
        self.last_accessed = time.time()
        if key in self.data:
            del self.data[key]
    
    def clear(self):
        """Clear all session data."""
        self.last_accessed = time.time()
        self.data.clear()
    
    def is_expired(self, max_age: int) -> bool:
        """Check if session is expired."""
        return (time.time() - self.last_accessed) > max_age


class SessionManager:
    """Manages server sessions."""
    def __init__(self, max_age: int = 3600):
        self.sessions: Dict[str, Session] = {}
        self.max_age = max_age
        self._lock = threading.Lock()
    
    def create_session(self) -> str:
        """Create new session and return session ID."""
        session_id = secrets.token_urlsafe(32)
        with self._lock:
            self.sessions[session_id] = Session(session_id)
        return session_id
    
    def get_session(self, session_id: str) -> Optional[Session]:
        """Get session by ID."""
        with self._lock:
            session = self.sessions.get(session_id)
            if session and session.is_expired(self.max_age):
                del self.sessions[session_id]
                return None
            return session
    
    def delete_session(self, session_id: str):
        """Delete session."""
        with self._lock:
            if session_id in self.sessions:
                del self.sessions[session_id]
    
    def cleanup_expired(self):
        """Remove expired sessions."""
        with self._lock:
            expired = [sid for sid, session in self.sessions.items() 
                      if session.is_expired(self.max_age)]
            for sid in expired:
                del self.sessions[sid]


class HTTPServerApp:
    """HTTP Server application with routing and middleware."""
    def __init__(self, host: str = '127.0.0.1', port: int = 8000):
        self.host = host
        self.port = port
        self.routes: List[Route] = []
        self.middlewares: List[Callable] = []
        self.error_handlers: Dict[int, Callable] = {}
        self.session_manager = SessionManager()
        self._server = None
        self._server_thread = None
    
    def route(self, method: str, pattern: str):
        """Decorator to register route handler."""
        def decorator(handler: Callable):
            self.add_route(method, pattern, handler)
            return handler
        return decorator
    
    def add_route(self, method: str, pattern: str, handler: Callable):
        """Add route to application."""
        route = Route(method, pattern, handler)
        self.routes.append(route)
    
    def get(self, pattern: str):
        """Decorator for GET routes."""
        return self.route('GET', pattern)
    
    def post(self, pattern: str):
        """Decorator for POST routes."""
        return self.route('POST', pattern)
    
    def put(self, pattern: str):
        """Decorator for PUT routes."""
        return self.route('PUT', pattern)
    
    def delete(self, pattern: str):
        """Decorator for DELETE routes."""
        return self.route('DELETE', pattern)
    
    def patch(self, pattern: str):
        """Decorator for PATCH routes."""
        return self.route('PATCH', pattern)
    
    def use(self, middleware: Callable):
        """Add middleware to application."""
        self.middlewares.append(middleware)
    
    def error_handler(self, status_code: int):
        """Decorator to register error handler."""
        def decorator(handler: Callable):
            self.error_handlers[status_code] = handler
            return handler
        return decorator
    
    def find_route(self, method: str, path: str) -> Optional[Tuple[Route, Dict[str, str]]]:
        """Find matching route and extract path parameters."""
        for route in self.routes:
            if route.method == method:
                params = route.match(path)
                if params is not None:
                    return route, params
        return None
    
    def parse_cookies(self, cookie_header: str) -> Dict[str, str]:
        """Parse cookie header into dictionary."""
        cookies = {}
        if cookie_header:
            for cookie in cookie_header.split(';'):
                cookie = cookie.strip()
                if '=' in cookie:
                    name, value = cookie.split('=', 1)
                    cookies[name] = value
        return cookies
    
    def handle_request(self, method: str, path: str, headers: dict, body: bytes) -> Response:
        """Handle HTTP request."""
        # Parse URL and query parameters
        parsed = urllib.parse.urlparse(path)
        path_only = parsed.path
        query_params = dict(urllib.parse.parse_qsl(parsed.query))
        
        # Parse cookies
        cookie_header = headers.get('Cookie', '')
        cookies = self.parse_cookies(cookie_header)
        
        # Find matching route
        route_match = self.find_route(method, path_only)
        
        if route_match is None:
            # 404 Not Found
            if 404 in self.error_handlers:
                return self.error_handlers[404](Request(method, path_only, headers, body, 
                                                       query_params, {}, cookies))
            return Response(f"404 Not Found: {path_only}", status=404)
        
        route, path_params = route_match
        
        # Create request object
        request = Request(method, path_only, headers, body, query_params, path_params, cookies)
        
        # Get or create session
        session_id = cookies.get('session_id')
        if session_id:
            session = self.session_manager.get_session(session_id)
            if session is None:
                session_id = self.session_manager.create_session()
                session = self.session_manager.get_session(session_id)
        else:
            session_id = self.session_manager.create_session()
            session = self.session_manager.get_session(session_id)
        
        request.session = session
        
        try:
            # Apply middlewares (before)
            for middleware in self.middlewares:
                result = middleware(request)
                if isinstance(result, Response):
                    return result  # Middleware can short-circuit
            
            # Call route handler
            response = route.handler(request)
            
            # Ensure response is Response object
            if not isinstance(response, Response):
                if isinstance(response, dict) or isinstance(response, list):
                    response = Response(response, status=200)
                elif isinstance(response, str):
                    response = Response(response, status=200)
                else:
                    response = Response(str(response), status=200)
            
            # Set session cookie if needed
            if 'session_id' not in cookies or cookies['session_id'] != session_id:
                response.set_cookie('session_id', session_id, max_age=3600)
            
            return response
            
        except Exception as e:
            # 500 Internal Server Error
            if 500 in self.error_handlers:
                return self.error_handlers[500](request, e)
            return Response(f"500 Internal Server Error: {str(e)}", status=500)
    
    def start(self, blocking: bool = True):
        """Start HTTP server."""
        app = self
        
        class RequestHandler(BaseHTTPRequestHandler):
            def do_GET(self):
                self.handle_request('GET')
            
            def do_POST(self):
                self.handle_request('POST')
            
            def do_PUT(self):
                self.handle_request('PUT')
            
            def do_DELETE(self):
                self.handle_request('DELETE')
            
            def do_PATCH(self):
                self.handle_request('PATCH')
            
            def handle_request(self, method: str):
                # Read request body
                content_length = int(self.headers.get('Content-Length', 0))
                body = self.rfile.read(content_length) if content_length > 0 else b''
                
                # Get response from app
                response = app.handle_request(method, self.path, dict(self.headers), body)
                
                # Send response
                self.send_response(response.status)
                for name, value in response.headers.items():
                    self.send_header(name, value)
                for cookie in response.get_cookies():
                    self.send_header('Set-Cookie', cookie)
                self.end_headers()
                self.wfile.write(response.body)
            
            def log_message(self, format: str, *args):
                # Override to control logging
                print(f"{self.client_address[0]} - {format % args}")
        
        class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
            daemon_threads = True
        
        self._server = ThreadedHTTPServer((self.host, self.port), RequestHandler)
        print(f"Starting HTTP server on {self.host}:{self.port}")
        
        if blocking:
            self._server.serve_forever()
        else:
            self._server_thread = threading.Thread(target=self._server.serve_forever)
            self._server_thread.daemon = True
            self._server_thread.start()
    
    def stop(self):
        """Stop HTTP server."""
        if self._server:
            self._server.shutdown()
            self._server.server_close()
            if self._server_thread:
                self._server_thread.join(timeout=5)
            print("HTTP server stopped")


# ============================================================================
# MIDDLEWARE FUNCTIONS
# ============================================================================

def cors_middleware(allowed_origins: str = '*', allowed_methods: str = 'GET, POST, PUT, DELETE, PATCH',
                   allowed_headers: str = 'Content-Type, Authorization'):
    """Create CORS middleware."""
    def middleware(request: Request) -> Optional[Response]:
        # CORS is typically handled in response headers, not request
        # This is a placeholder - actual CORS would modify response
        return None
    return middleware


def logging_middleware(request: Request) -> Optional[Response]:
    """Log incoming requests."""
    print(f"{request.method} {request.path} - {dict(request.query_params)}")
    return None


def auth_middleware(auth_check: Callable[[Request], bool]):
    """Create authentication middleware."""
    def middleware(request: Request) -> Optional[Response]:
        if not auth_check(request):
            return Response({'error': 'Unauthorized'}, status=401)
        return None
    return middleware


# ============================================================================
# AUTHENTICATION UTILITIES
# ============================================================================

def basic_auth_decode(auth_header: str) -> Optional[Tuple[str, str]]:
    """Decode HTTP Basic Auth header."""
    if not auth_header or not auth_header.startswith('Basic '):
        return None
    try:
        import base64
        encoded = auth_header[6:]  # Remove 'Basic '
        decoded = base64.b64decode(encoded).decode('utf-8')
        if ':' in decoded:
            username, password = decoded.split(':', 1)
            return username, password
    except Exception:
        pass
    return None


def jwt_create(payload: dict, secret: str, algorithm: str = 'HS256') -> str:
    """Create JWT token (simplified - requires pyjwt in production)."""
    import base64
    import hmac
    import hashlib
    
    # Header
    header = {'typ': 'JWT', 'alg': algorithm}
    header_b64 = base64.urlsafe_b64encode(json_lib.dumps(header).encode()).decode().rstrip('=')
    
    # Payload
    payload_b64 = base64.urlsafe_b64encode(json_lib.dumps(payload).encode()).decode().rstrip('=')
    
    # Signature
    message = f"{header_b64}.{payload_b64}"
    signature = hmac.new(secret.encode(), message.encode(), hashlib.sha256).digest()
    signature_b64 = base64.urlsafe_b64encode(signature).decode().rstrip('=')
    
    return f"{message}.{signature_b64}"


def jwt_verify(token: str, secret: str) -> Optional[dict]:
    """Verify and decode JWT token (simplified)."""
    import base64
    import hmac
    import hashlib
    
    try:
        parts = token.split('.')
        if len(parts) != 3:
            return None
        
        header_b64, payload_b64, signature_b64 = parts
        
        # Verify signature
        message = f"{header_b64}.{payload_b64}"
        expected_signature = hmac.new(secret.encode(), message.encode(), hashlib.sha256).digest()
        expected_signature_b64 = base64.urlsafe_b64encode(expected_signature).decode().rstrip('=')
        
        if signature_b64 != expected_signature_b64:
            return None
        
        # Decode payload
        # Add padding if needed
        payload_b64 += '=' * (4 - len(payload_b64) % 4)
        payload_json = base64.urlsafe_b64decode(payload_b64).decode()
        return json_lib.loads(payload_json)
    except Exception:
        return None


def is_response_ok(response: HTTPResponse) -> bool:
    """Check if response is successful."""
    return response.ok


def register_http_functions(runtime: Runtime) -> None:
    """Register HTTP functions with the runtime."""
    
    # ===== HTTP CLIENT =====
    
    # HTTP methods
    runtime.register_function("http_get", http_get)
    runtime.register_function("http_post", http_post)
    runtime.register_function("http_put", http_put)
    runtime.register_function("http_delete", http_delete)
    runtime.register_function("http_patch", http_patch)
    runtime.register_function("http_head", http_head)
    runtime.register_function("http_request", http_request)
    
    # URL utilities
    runtime.register_function("url_encode", url_encode)
    runtime.register_function("url_decode", url_decode)
    runtime.register_function("url_parse", url_parse)
    runtime.register_function("url_join", url_join)
    
    # Download
    runtime.register_function("download_file", download_file)
    
    # Response utilities
    runtime.register_function("get_response_text", get_response_text)
    runtime.register_function("get_response_json", get_response_json)
    runtime.register_function("get_response_status", get_response_status)
    runtime.register_function("get_response_headers", get_response_headers)
    runtime.register_function("is_response_ok", is_response_ok)
    
    # Aliases
    runtime.register_function("GET", http_get)
    runtime.register_function("POST", http_post)
    runtime.register_function("PUT", http_put)
    runtime.register_function("DELETE", http_delete)
    
    # ===== HTTP SERVER =====
    
    # Server classes (exposed as constructors)
    runtime.register_function("create_http_server", lambda host='127.0.0.1', port=8000: HTTPServerApp(host, port))
    runtime.register_function("create_response", lambda body='', status=200, headers=None: Response(body, status, headers))
    
    # Middleware
    runtime.register_function("cors_middleware", cors_middleware)
    runtime.register_function("logging_middleware", logging_middleware)
    runtime.register_function("auth_middleware", auth_middleware)
    
    # Authentication
    runtime.register_function("basic_auth_decode", basic_auth_decode)
    runtime.register_function("jwt_create", jwt_create)
    runtime.register_function("jwt_verify", jwt_verify)
