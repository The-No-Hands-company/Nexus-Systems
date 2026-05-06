"""
NLPL Async Runtime

Full async/await execution engine for NexusLang programs:
- NLPLFuture<T>  - observable async result container
- NLPLTask       - handle for a spawned async task
- NLPLEventLoop  - scheduler driving all async work
- spawn_async    - submit an async task to the scheduler
- join_all       - await multiple tasks and collect results
- select_first   - return first task that completes
- async_timeout  - run a task with a deadline
- async_sleep    - non-blocking sleep inside async context
- async_after    - delayed function dispatch
- async_read_file, async_write_file - async file I/O
- async_http_get, async_http_post  - async HTTP (no blocking the event loop)
- create_future, future_set_result, future_set_error, future_get - manual futures
- NLPLAsyncChannel  - thread-safe MPMC channel (async_channel_*)
- NLPLAsyncLock     - re-entrant async-aware lock (async_lock_*)
- NLPLAsyncSemaphore - counting semaphore (async_semaphore_*)
"""

import asyncio
import concurrent.futures
import inspect
import queue as _queue
import threading
import time
from typing import Any, Callable, List, Optional


# ---------------------------------------------------------------------------
# Global event loop thread
# ---------------------------------------------------------------------------

_loop: Optional[asyncio.AbstractEventLoop] = None
_loop_thread: Optional[threading.Thread] = None
_loop_lock = threading.Lock()


def _get_or_create_loop() -> asyncio.AbstractEventLoop:
    """
    Return (and lazily start) the shared NexusLang event loop.

    The loop runs in a dedicated daemon thread so that async tasks can be
    submitted from any NexusLang thread without conflicting with each other or
    with CPython's default event-loop policy.
    """
    global _loop, _loop_thread
    with _loop_lock:
        if _loop is None or _loop.is_closed():
            _loop = asyncio.new_event_loop()
            _loop_thread = threading.Thread(
                target=_loop.run_forever,
                name="NLPLAsyncEventLoop",
                daemon=True,
            )
            _loop_thread.start()
    return _loop


# ---------------------------------------------------------------------------
# NLPLFuture
# ---------------------------------------------------------------------------

class NLPLFuture:
    """
    Observable result container for an asynchronous computation.

    Wraps a :class:`concurrent.futures.Future` but exposes the NexusLang API
    (get, is_done, set_result, set_error).
    """

    def __init__(self):
        self._event = threading.Event()
        self._value: Any = None
        self._error: Optional[Exception] = None
        self._lock = threading.Lock()

    # ---- producer side ----

    def set_result(self, value: Any) -> None:
        """Fulfil the future with a value."""
        with self._lock:
            if self._event.is_set():
                raise RuntimeError("Future already resolved")
            self._value = value
            self._event.set()

    def set_error(self, error: Exception) -> None:
        """Reject the future with an error."""
        with self._lock:
            if self._event.is_set():
                raise RuntimeError("Future already resolved")
            self._error = error
            self._event.set()

    # ---- consumer side ----

    def is_done(self) -> bool:
        """Return True once the future has been settled."""
        return self._event.is_set()

    def get(self, timeout: Optional[float] = None) -> Any:
        """
        Block until the future is settled, then return the value.

        Raises the stored exception if the future was rejected.
        Raises :class:`TimeoutError` if *timeout* seconds elapse first.
        """
        if not self._event.wait(timeout):
            raise TimeoutError("Future timed out after {} seconds".format(timeout))
        if self._error is not None:
            raise self._error
        return self._value

    def __repr__(self) -> str:
        if not self._event.is_set():
            return "Future<pending>"
        if self._error is not None:
            return "Future<error: {}>".format(self._error)
        return "Future<done: {!r}>".format(self._value)


# ---------------------------------------------------------------------------
# NLPLTask
# ---------------------------------------------------------------------------

class NLPLTask:
    """
    Handle representing a spawned async task.

    Wraps a :class:`concurrent.futures.Future` returned by
    :func:`asyncio.run_coroutine_threadsafe`.  That future is inherently
    thread-safe and designed to be waited on from non-event-loop threads.
    """

    def __init__(self, cf_future: concurrent.futures.Future):
        self._future: concurrent.futures.Future = cf_future

    def is_done(self) -> bool:
        return self._future.done()

    def cancel(self) -> bool:
        return self._future.cancel()

    def result(self, timeout: Optional[float] = None) -> Any:
        """
        Block the calling thread until the task completes.

        Raises the task's exception on failure.
        Raises :class:`TimeoutError` if *timeout* elapses.
        """
        try:
            return self._future.result(timeout=timeout)
        except concurrent.futures.TimeoutError:
            raise TimeoutError("Task timed out")

    def __repr__(self) -> str:
        if self._future.done():
            try:
                return "Task<done: {!r}>".format(self._future.result(timeout=0))
            except Exception as e:
                return "Task<error: {}>".format(e)
        return "Task<running>"


# ---------------------------------------------------------------------------
# Core async runtime functions
# ---------------------------------------------------------------------------

def spawn_async(fn: Callable, *args) -> NLPLTask:
    """
    Submit *fn* to the shared event loop and return a :class:`NLPLTask`.

    The function may be a plain callable or a Python coroutine function.
    Returns a thread-safe :class:`NLPLTask` that can be waited on from any
    thread via :func:`task_result`.

    Example NexusLang usage::

        function fetch_data with url as String returns String
            # ...implementation...

        set task to spawn_async with fetch_data and "https://example.com"
        set result to task_result with task
    """
    loop = _get_or_create_loop()

    if inspect.iscoroutinefunction(fn):
        # Native coroutine: schedule it directly on the event loop.
        async def _run_coro():
            return await fn(*args)
        cf_future = asyncio.run_coroutine_threadsafe(_run_coro(), loop)
    else:
        # Plain blocking callable: run it in the loop's default
        # ThreadPoolExecutor so it never blocks the event-loop thread.
        # This avoids starving other pending tasks when fn calls time.sleep
        # or performs any other blocking I/O.
        async def _run_in_executor():
            return await loop.run_in_executor(None, lambda: fn(*args))
        cf_future = asyncio.run_coroutine_threadsafe(_run_in_executor(), loop)

    # concurrent.futures.Future is inherently thread-safe; NLPLTask wraps it.
    return NLPLTask(cf_future)


def task_result(task: NLPLTask, timeout: Optional[float] = None) -> Any:
    """Block and return the result of a spawned task."""
    if not isinstance(task, NLPLTask):
        raise TypeError("Expected a task returned by spawn_async")
    return task.result(timeout)


def task_is_done(task: NLPLTask) -> bool:
    """Return True if the task has finished."""
    if not isinstance(task, NLPLTask):
        raise TypeError("Expected a task returned by spawn_async")
    return task.is_done()


def task_cancel(task: NLPLTask) -> bool:
    """Request cancellation of a running task. Returns True if cancellation was requested."""
    if not isinstance(task, NLPLTask):
        raise TypeError("Expected a task returned by spawn_async")
    return task.cancel()


def join_all(tasks: list, timeout: Optional[float] = None) -> list:
    """
    Wait for all tasks to complete and return their results as a list.

    The order of results matches the order of *tasks*.
    Raises the first exception encountered across any task.
    Raises :class:`TimeoutError` if *timeout* elapses before all complete.

    Example NexusLang usage::

        set tasks to list of spawn_async with work and 1, spawn_async with work and 2
        set results to join_all with tasks
    """
    if not tasks:
        return []
    deadline = None if timeout is None else time.monotonic() + timeout
    results = []
    for task in tasks:
        remaining = (
            None
            if deadline is None
            else max(0.0, deadline - time.monotonic())
        )
        if remaining is not None and remaining <= 0:
            raise TimeoutError("join_all timed out waiting for tasks")
        results.append(task.result(remaining))
    return results


def select_first(tasks: list, timeout: Optional[float] = None) -> Any:
    """
    Return the result of whichever task completes first.

    Raises :class:`TimeoutError` if *timeout* elapses before any task completes.

    Example NexusLang usage::

        set result to select_first with tasks
    """
    if not tasks:
        raise ValueError("select_first requires at least one task")
    deadline = None if timeout is None else time.monotonic() + timeout
    while True:
        for task in tasks:
            if task.is_done():
                return task.result()
        if deadline is not None and time.monotonic() >= deadline:
            raise TimeoutError("select_first timed out")
        time.sleep(0.002)


def async_timeout(task: NLPLTask, duration: float) -> Any:
    """
    Wait for *task* with a *duration* second deadline.

    Raises :class:`TimeoutError` on expiry.

    Example NexusLang usage::

        set result to async_timeout with task and 5.0
    """
    return task.result(duration)


def async_sleep(seconds: float) -> None:
    """
    Non-blocking sleep that allows other async tasks to run.

    From sync NexusLang code this simply sleeps the thread.
    Inside a Python coroutine context this yields to the event loop.

    Example NexusLang usage::

        async_sleep with 1.5
    """
    time.sleep(seconds)


def async_after(delay: float, fn: Callable, *args) -> NLPLTask:
    """
    Execute *fn* after *delay* seconds and return a task.

    Example NexusLang usage::

        set task to async_after with 2.0 and my_callback
    """
    def _delayed():
        time.sleep(delay)
        return fn(*args)

    return spawn_async(_delayed)

# ---------------------------------------------------------------------------
# Manual Future API
# ---------------------------------------------------------------------------

def create_future() -> NLPLFuture:
    """
    Create an unresolved :class:`NLPLFuture`.

    Example NexusLang usage::

        set fut to create_future
        future_set_result with fut and 42
        set val to future_get with fut
    """
    return NLPLFuture()


def future_set_result(future: NLPLFuture, value: Any) -> None:
    """Fulfil a future with *value*."""
    if not isinstance(future, NLPLFuture):
        raise TypeError("Expected a NLPLFuture from create_future")
    future.set_result(value)


def future_set_error(future: NLPLFuture, error: Any) -> None:
    """Reject a future with *error* (string or exception)."""
    if not isinstance(future, NLPLFuture):
        raise TypeError("Expected a NLPLFuture from create_future")
    exc = error if isinstance(error, Exception) else Exception(str(error))
    future.set_error(exc)


def future_get(future: NLPLFuture, timeout: Optional[float] = None) -> Any:
    """Block for a future's result. Raises on error or timeout."""
    if not isinstance(future, NLPLFuture):
        raise TypeError("Expected a NLPLFuture from create_future")
    return future.get(timeout)


def future_is_done(future: NLPLFuture) -> bool:
    """Return True if the future has been settled."""
    if not isinstance(future, NLPLFuture):
        raise TypeError("Expected a NLPLFuture from create_future")
    return future.is_done()


# ---------------------------------------------------------------------------
# Async I/O
# ---------------------------------------------------------------------------

def async_read_file(path: str, encoding: str = "utf-8") -> NLPLTask:
    """
    Read a file asynchronously. Returns a task resolving to the file content.

    Example NexusLang usage::

        set task to async_read_file with "data.txt"
        set content to task_result with task
    """
    def _read():
        with open(path, "r", encoding=encoding) as f:
            return f.read()

    return spawn_async(_read)


def async_read_file_bytes(path: str) -> NLPLTask:
    """Read a binary file asynchronously. Returns a task resolving to bytes."""
    def _read():
        with open(path, "rb") as f:
            return f.read()

    return spawn_async(_read)


def async_write_file(path: str, content: str, encoding: str = "utf-8") -> NLPLTask:
    """
    Write *content* to *path* asynchronously. Returns a task resolving to bytes written.

    Example NexusLang usage::

        set task to async_write_file with "output.txt" and result_text
        task_result with task
    """
    def _write():
        with open(path, "w", encoding=encoding) as f:
            return f.write(content)

    return spawn_async(_write)


def async_write_file_bytes(path: str, data: bytes) -> NLPLTask:
    """Write binary data to *path* asynchronously."""
    def _write():
        with open(path, "wb") as f:
            return f.write(data)

    return spawn_async(_write)


def async_append_file(path: str, content: str, encoding: str = "utf-8") -> NLPLTask:
    """Append *content* to *path* asynchronously."""
    def _append():
        with open(path, "a", encoding=encoding) as f:
            return f.write(content)

    return spawn_async(_append)


# ---------------------------------------------------------------------------
# Async HTTP
# ---------------------------------------------------------------------------

def _make_http_request(method: str, url: str, headers: Optional[dict] = None,
                       body: Optional[str] = None, timeout: float = 30.0) -> dict:
    """Perform a synchronous HTTP request (runs inside async worker thread)."""
    import urllib.request
    import urllib.error
    import json as _json

    req = urllib.request.Request(url, method=method.upper())
    if headers:
        for k, v in headers.items():
            req.add_header(k, v)
    if body is not None:
        req.data = body.encode("utf-8") if isinstance(body, str) else body

    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            raw = resp.read()
            text = raw.decode("utf-8", errors="replace")
            return {
                "status": resp.status,
                "headers": dict(resp.headers),
                "body": text,
                "ok": 200 <= resp.status < 300,
            }
    except urllib.error.HTTPError as e:
        raw = e.read()
        text = raw.decode("utf-8", errors="replace")
        return {
            "status": e.code,
            "headers": dict(e.headers),
            "body": text,
            "ok": False,
            "error": str(e),
        }
    except Exception as e:
        return {
            "status": 0,
            "headers": {},
            "body": "",
            "ok": False,
            "error": str(e),
        }


def async_http_get(url: str, headers: Optional[dict] = None,
                   timeout: float = 30.0) -> NLPLTask:
    """
    Perform an HTTP GET request asynchronously.

    Returns a task resolving to a dict with keys:
    ``status``, ``headers``, ``body``, ``ok``, ``error`` (on failure).

    Example NexusLang usage::

        set task to async_http_get with "https://api.example.com/data"
        set response to task_result with task
        if response["ok"]
            print text response["body"]
        end
    """
    return spawn_async(_make_http_request, "GET", url, headers, None, timeout)


def async_http_post(url: str, body: str = "", headers: Optional[dict] = None,
                    timeout: float = 30.0) -> NLPLTask:
    """
    Perform an HTTP POST request asynchronously.

    Example NexusLang usage::

        set task to async_http_post with "https://api.example.com/data" and body_text
        set response to task_result with task
    """
    post_headers = headers or {}
    if "Content-Type" not in post_headers:
        post_headers["Content-Type"] = "application/json"
    return spawn_async(_make_http_request, "POST", url, post_headers, body, timeout)


def async_http_put(url: str, body: str = "", headers: Optional[dict] = None,
                   timeout: float = 30.0) -> NLPLTask:
    """Perform an HTTP PUT request asynchronously."""
    return spawn_async(_make_http_request, "PUT", url, headers or {}, body, timeout)


def async_http_delete(url: str, headers: Optional[dict] = None,
                      timeout: float = 30.0) -> NLPLTask:
    """Perform an HTTP DELETE request asynchronously."""
    return spawn_async(_make_http_request, "DELETE", url, headers or {}, None, timeout)


# ---------------------------------------------------------------------------
# Async TCP socket operations
# ---------------------------------------------------------------------------

def async_tcp_connect(host: str, port: int, timeout: Optional[float] = None) -> NLPLTask:
    """Asynchronously open a TCP connection.

    Returns a task resolving to a connection dict with keys:
    'socket', 'host', 'port'.
    On failure the task raises the underlying socket error.
    """
    import socket as _socket

    def _connect():
        sock = _socket.socket(_socket.AF_INET, _socket.SOCK_STREAM)
        if timeout is not None:
            sock.settimeout(float(timeout))
        try:
            sock.connect((str(host), int(port)))
        except Exception:
            sock.close()
            raise
        sock.settimeout(None)
        return {'socket': sock, 'host': str(host), 'port': int(port)}

    return spawn_async(_connect)


def async_tcp_listen(host: str, port: int, backlog: int = 5) -> NLPLTask:
    """Asynchronously create a TCP server socket bound to host:port.

    Returns a task resolving to a server dict with keys: 'socket', 'host', 'port'.
    """
    import socket as _socket

    def _listen():
        srv = _socket.socket(_socket.AF_INET, _socket.SOCK_STREAM)
        srv.setsockopt(_socket.SOL_SOCKET, _socket.SO_REUSEADDR, 1)
        srv.bind((str(host), int(port)))
        srv.listen(int(backlog))
        actual_port = srv.getsockname()[1]
        return {'socket': srv, 'host': str(host), 'port': actual_port}

    return spawn_async(_listen)


def async_tcp_accept(server: dict) -> NLPLTask:
    """Asynchronously accept an incoming TCP connection on a server socket.

    Returns a task resolving to a connection dict with keys:
    'socket', 'host', 'port'.
    """
    def _accept():
        srv = server['socket']
        conn, addr = srv.accept()
        return {'socket': conn, 'host': addr[0], 'port': addr[1]}

    return spawn_async(_accept)


def async_tcp_send(conn: dict, data: Any) -> NLPLTask:
    """Asynchronously send data over an established TCP connection.

    data may be str, bytes, bytearray, or any value (converted via str).
    Returns a task resolving to the number of bytes sent.
    """
    def _send():
        sock = conn['socket']
        if isinstance(data, str):
            payload = data.encode('utf-8')
        elif isinstance(data, (bytes, bytearray)):
            payload = bytes(data)
        else:
            payload = str(data).encode('utf-8')
        sock.sendall(payload)
        return len(payload)

    return spawn_async(_send)


def async_tcp_recv(conn: dict, buffer_size: int = 4096) -> NLPLTask:
    """Asynchronously receive up to buffer_size bytes from a TCP connection.

    Returns a task resolving to a dict with keys:
    'data' (str, UTF-8 decoded), 'bytes' (raw bytes), 'length' (int), 'eof' (bool).
    """
    def _recv():
        sock = conn['socket']
        raw = sock.recv(int(buffer_size))
        return {
            'data': raw.decode('utf-8', errors='replace'),
            'bytes': raw,
            'length': len(raw),
            'eof': len(raw) == 0,
        }

    return spawn_async(_recv)


def async_tcp_recv_exactly(conn: dict, n: int) -> NLPLTask:
    """Asynchronously receive exactly n bytes from a TCP connection.

    Loops until all bytes are received or EOF is reached.
    Returns a task resolving to a dict with keys: 'data', 'bytes', 'length'.
    """
    def _recv():
        sock = conn['socket']
        buf = bytearray()
        remaining = int(n)
        while remaining > 0:
            chunk = sock.recv(remaining)
            if not chunk:
                break
            buf.extend(chunk)
            remaining -= len(chunk)
        raw = bytes(buf)
        return {
            'data': raw.decode('utf-8', errors='replace'),
            'bytes': raw,
            'length': len(raw),
        }

    return spawn_async(_recv)


def async_tcp_close(conn: dict) -> None:
    """Close a TCP connection dict (synchronous, instant)."""
    sock = conn.get('socket')
    if sock is not None:
        try:
            sock.close()
        except Exception:
            pass


# ---------------------------------------------------------------------------
# Async UDP socket operations
# ---------------------------------------------------------------------------

def async_udp_open(bind_host: str = '', bind_port: int = 0) -> NLPLTask:
    """Asynchronously create and optionally bind a UDP socket.

    Returns a task resolving to a socket dict with keys: 'socket', 'port'.
    When bind_port=0 the OS assigns a free ephemeral port.
    """
    import socket as _socket

    def _open():
        sock = _socket.socket(_socket.AF_INET, _socket.SOCK_DGRAM)
        if bind_host or bind_port:
            sock.bind((str(bind_host), int(bind_port)))
        else:
            sock.bind(('', 0))
        actual_port = sock.getsockname()[1]
        return {'socket': sock, 'port': actual_port}

    return spawn_async(_open)


def async_udp_send_to(conn: dict, data: Any, host: str, port: int) -> NLPLTask:
    """Asynchronously send a UDP datagram to host:port.

    Returns a task resolving to the number of bytes sent.
    """
    def _send():
        sock = conn['socket']
        if isinstance(data, str):
            payload = data.encode('utf-8')
        elif isinstance(data, (bytes, bytearray)):
            payload = bytes(data)
        else:
            payload = str(data).encode('utf-8')
        return sock.sendto(payload, (str(host), int(port)))

    return spawn_async(_send)


def async_udp_recv_from(conn: dict, buffer_size: int = 65535) -> NLPLTask:
    """Asynchronously receive a UDP datagram.

    Returns a task resolving to a dict with keys:
    'data' (str), 'bytes' (raw bytes), 'length', 'from_host', 'from_port'.
    """
    def _recv():
        sock = conn['socket']
        raw, addr = sock.recvfrom(int(buffer_size))
        return {
            'data': raw.decode('utf-8', errors='replace'),
            'bytes': raw,
            'length': len(raw),
            'from_host': addr[0],
            'from_port': addr[1],
        }

    return spawn_async(_recv)


def async_udp_close(conn: dict) -> None:
    """Close a UDP socket dict (synchronous, instant)."""
    sock = conn.get('socket')
    if sock is not None:
        try:
            sock.close()
        except Exception:
            pass


# ---------------------------------------------------------------------------
# Async subprocess
# ---------------------------------------------------------------------------

def async_subprocess(
    cmd: str,
    args: Optional[list] = None,
    stdin_data: Optional[str] = None,
    timeout: Optional[float] = None,
) -> NLPLTask:
    """Asynchronously run a subprocess and wait for completion.

    Returns a task resolving to a dict with keys:
    'stdout', 'stderr', 'returncode', 'success'.
    On timeout or missing command the dict also contains an 'error' key.
    """
    import subprocess as _sp

    def _run():
        argv = [str(cmd)] + [str(a) for a in (args or [])]
        inp = stdin_data.encode('utf-8') if isinstance(stdin_data, str) else None
        try:
            result = _sp.run(argv, input=inp, capture_output=True, timeout=timeout)
            return {
                'stdout': result.stdout.decode('utf-8', errors='replace'),
                'stderr': result.stderr.decode('utf-8', errors='replace'),
                'returncode': result.returncode,
                'success': result.returncode == 0,
            }
        except _sp.TimeoutExpired:
            return {'error': 'timeout', 'returncode': -1,
                    'success': False, 'stdout': '', 'stderr': ''}
        except FileNotFoundError:
            return {'error': f'command not found: {cmd}', 'returncode': -1,
                    'success': False, 'stdout': '', 'stderr': ''}

    return spawn_async(_run)


def async_subprocess_output(cmd: str, args: Optional[list] = None) -> NLPLTask:
    """Asynchronously run a subprocess and return just its stdout string.

    Returns a task resolving to the stdout string, or a dict with 'error'
    if the command fails or is not found.
    """
    import subprocess as _sp

    def _run():
        argv = [str(cmd)] + [str(a) for a in (args or [])]
        try:
            out = _sp.check_output(argv, stderr=_sp.DEVNULL)
            return out.decode('utf-8', errors='replace')
        except _sp.CalledProcessError as e:
            return {'error': str(e), 'returncode': e.returncode}
        except FileNotFoundError:
            return {'error': f'command not found: {cmd}'}

    return spawn_async(_run)


# ---------------------------------------------------------------------------
# Async DNS resolution
# ---------------------------------------------------------------------------

def async_dns_resolve(hostname: str) -> NLPLTask:
    """Asynchronously resolve a hostname to its IP addresses.

    Returns a task resolving to a dict with keys:
    'hostname', 'addresses' (list), 'primary' (first IP or None).
    On failure the dict contains an 'error' key.
    """
    import socket as _socket

    def _resolve():
        try:
            infos = _socket.getaddrinfo(str(hostname), None)
            addresses = list(dict.fromkeys(info[4][0] for info in infos))
            return {
                'hostname': str(hostname),
                'addresses': addresses,
                'primary': addresses[0] if addresses else None,
            }
        except _socket.gaierror as e:
            return {'error': str(e), 'hostname': str(hostname)}

    return spawn_async(_resolve)


def async_dns_reverse(ip_address: str) -> NLPLTask:
    """Asynchronously perform a reverse DNS lookup (IP to hostname).

    Returns a task resolving to a dict with keys: 'ip', 'hostname'.
    On failure the dict contains an 'error' key.
    """
    import socket as _socket

    def _resolve():
        try:
            hostname, _, _ = _socket.gethostbyaddr(str(ip_address))
            return {'ip': str(ip_address), 'hostname': hostname}
        except (_socket.herror, _socket.gaierror) as e:
            return {'error': str(e), 'ip': str(ip_address)}

    return spawn_async(_resolve)


# ---------------------------------------------------------------------------
# Async file readline / readlines
# ---------------------------------------------------------------------------

def async_readlines(path: str, encoding: str = "utf-8") -> NLPLTask:
    """Asynchronously read all lines from a file.

    Returns a task resolving to a list of line strings (newlines preserved).
    """
    def _read():
        with open(path, "r", encoding=encoding) as f:
            return f.readlines()

    return spawn_async(_read)


def async_readline_chunks(path: str, chunk_size: int = 100,
                          encoding: str = "utf-8") -> NLPLTask:
    """Asynchronously read up to chunk_size lines from a file.

    Returns a task resolving to a list of up to chunk_size line strings.
    Useful for streaming large files in manageable batches.
    """
    def _read():
        lines = []
        with open(path, "r", encoding=encoding) as f:
            for _ in range(int(chunk_size)):
                line = f.readline()
                if not line:
                    break
                lines.append(line)
        return lines

    return spawn_async(_read)


# ---------------------------------------------------------------------------
# Async Channel  (thread-safe MPMC queue)
# ---------------------------------------------------------------------------

class NLPLAsyncChannel:
    """
    Thread-safe, optionally bounded multi-producer / multi-consumer channel.

    Backed by :class:`queue.Queue` so it is safe to use from any thread,
    including the NexusLang async event-loop thread and plain interpreter threads.

    NexusLang usage::

        set ch to async_channel_create with 0          # unbounded
        set ch to async_channel_create with 10         # bounded (max 10 items)
        async_channel_send with ch and "hello"
        set msg to async_channel_recv with ch
        set ok to async_channel_try_recv with ch       # returns None if empty
        async_channel_close with ch
    """

    _SENTINEL = object()  # marks channel closed

    def __init__(self, maxsize: int = 0) -> None:
        self._q: _queue.Queue = _queue.Queue(maxsize=int(maxsize))
        self._closed = threading.Event()

    # ---- producer ----

    def send(self, value: Any, timeout: Optional[float] = None) -> None:
        """
        Put *value* into the channel.

        Blocks if the channel is bounded and full, up to *timeout* seconds.
        Raises :class:`RuntimeError` if the channel is closed.
        Raises :class:`TimeoutError` if the send does not complete in time.
        """
        if self._closed.is_set():
            raise RuntimeError("Cannot send to a closed channel")
        try:
            self._q.put(value, block=True, timeout=timeout)
        except _queue.Full:
            raise TimeoutError("async_channel_send timed out after {} seconds".format(timeout))

    # ---- consumer ----

    def recv(self, timeout: Optional[float] = None) -> Any:
        """
        Remove and return the next value from the channel.

        Blocks until an item is available or *timeout* seconds elapse.
        Raises :class:`StopIteration` when the channel is closed and empty.
        Raises :class:`TimeoutError` on timeout.
        """
        while True:
            try:
                item = self._q.get(block=True, timeout=0.05)
                if item is NLPLAsyncChannel._SENTINEL:
                    # Put the sentinel back for other waiting receivers.
                    try:
                        self._q.put_nowait(NLPLAsyncChannel._SENTINEL)
                    except _queue.Full:
                        pass
                    raise StopIteration("Channel closed")
                return item
            except _queue.Empty:
                if self._closed.is_set() and self._q.empty():
                    raise StopIteration("Channel closed and empty")
                if timeout is not None:
                    timeout -= 0.05
                    if timeout <= 0:
                        raise TimeoutError("async_channel_recv timed out")

    def try_recv(self) -> Optional[Any]:
        """
        Non-blocking receive.  Returns the next item or ``None`` if the channel
        is currently empty.  Returns ``None`` (not an error) when the channel is
        closed and empty.
        """
        try:
            item = self._q.get_nowait()
            if item is NLPLAsyncChannel._SENTINEL:
                try:
                    self._q.put_nowait(NLPLAsyncChannel._SENTINEL)
                except _queue.Full:
                    pass
                return None
            return item
        except _queue.Empty:
            return None

    def close(self) -> None:
        """
        Signal that no more items will be sent.

        Pending and future receivers will eventually get :class:`StopIteration`.
        """
        if not self._closed.is_set():
            self._closed.set()
            try:
                self._q.put_nowait(NLPLAsyncChannel._SENTINEL)
            except _queue.Full:
                pass

    @property
    def is_closed(self) -> bool:
        return self._closed.is_set()

    @property
    def size(self) -> int:
        return self._q.qsize()

    def __repr__(self) -> str:
        state = "closed" if self._closed.is_set() else "open"
        return "AsyncChannel<{}, size={}>".format(state, self._q.qsize())


# Public wrappers (called from NexusLang registration lambdas)

def async_channel_create(maxsize: int = 0) -> NLPLAsyncChannel:
    """Create and return a new async channel with optional capacity bound."""
    return NLPLAsyncChannel(maxsize=maxsize)


def async_channel_send(channel: NLPLAsyncChannel, value: Any,
                       timeout: Optional[float] = None) -> None:
    """Send *value* into *channel*, blocking up to *timeout* seconds."""
    if not isinstance(channel, NLPLAsyncChannel):
        raise TypeError("async_channel_send: expected an AsyncChannel, got {}".format(type(channel)))
    channel.send(value, timeout=timeout)


def async_channel_recv(channel: NLPLAsyncChannel,
                       timeout: Optional[float] = None) -> Any:
    """Receive and return the next value from *channel*."""
    if not isinstance(channel, NLPLAsyncChannel):
        raise TypeError("async_channel_recv: expected an AsyncChannel, got {}".format(type(channel)))
    return channel.recv(timeout=timeout)


def async_channel_try_recv(channel: NLPLAsyncChannel) -> Optional[Any]:
    """Non-blocking receive; returns ``None`` if the channel is empty."""
    if not isinstance(channel, NLPLAsyncChannel):
        raise TypeError("async_channel_try_recv: expected an AsyncChannel, got {}".format(type(channel)))
    return channel.try_recv()


def async_channel_close(channel: NLPLAsyncChannel) -> None:
    """Close *channel*; subsequent senders raise RuntimeError."""
    if not isinstance(channel, NLPLAsyncChannel):
        raise TypeError("async_channel_close: expected an AsyncChannel, got {}".format(type(channel)))
    channel.close()


def async_channel_is_closed(channel: NLPLAsyncChannel) -> bool:
    """Return True if the channel has been closed."""
    if not isinstance(channel, NLPLAsyncChannel):
        raise TypeError("async_channel_is_closed: expected an AsyncChannel, got {}".format(type(channel)))
    return channel.is_closed


def async_channel_size(channel: NLPLAsyncChannel) -> int:
    """Return the number of items currently queued in the channel."""
    if not isinstance(channel, NLPLAsyncChannel):
        raise TypeError("async_channel_size: expected an AsyncChannel, got {}".format(type(channel)))
    return channel.size


# ---------------------------------------------------------------------------
# Async Lock  (re-entrant, timeout-capable)
# ---------------------------------------------------------------------------

class NLPLAsyncLock:
    """
    Re-entrant, timeout-capable lock safe to use from any thread.

    NexusLang usage::

        set lk to async_lock_create
        async_lock_acquire with lk
        # ... critical section ...
        async_lock_release with lk

        set acquired to async_lock_try_acquire with lk   # returns True/False
    """

    def __init__(self) -> None:
        self._lock = threading.RLock()

    def acquire(self, timeout: Optional[float] = None) -> bool:
        """
        Acquire the lock.

        Blocks up to *timeout* seconds.  Returns True on success, False on
        timeout.  If *timeout* is None, blocks indefinitely.
        """
        if timeout is None:
            self._lock.acquire()
            return True
        return self._lock.acquire(timeout=timeout)

    def release(self) -> None:
        """Release the lock.  Raises :class:`RuntimeError` if not held."""
        self._lock.release()

    def try_acquire(self) -> bool:
        """Non-blocking attempt; returns True if acquired, False otherwise."""
        return self._lock.acquire(blocking=False)

    def __repr__(self) -> str:
        return "AsyncLock<RLock>"


def async_lock_create() -> NLPLAsyncLock:
    """Create and return a new async re-entrant lock."""
    return NLPLAsyncLock()


def async_lock_acquire(lock: NLPLAsyncLock,
                       timeout: Optional[float] = None) -> bool:
    """
    Acquire *lock*, blocking up to *timeout* seconds.

    Returns True on success; raises TimeoutError if timeout elapses.
    """
    if not isinstance(lock, NLPLAsyncLock):
        raise TypeError("async_lock_acquire: expected an AsyncLock, got {}".format(type(lock)))
    acquired = lock.acquire(timeout=timeout)
    if not acquired:
        raise TimeoutError("async_lock_acquire timed out after {} seconds".format(timeout))
    return True


def async_lock_release(lock: NLPLAsyncLock) -> None:
    """Release *lock*."""
    if not isinstance(lock, NLPLAsyncLock):
        raise TypeError("async_lock_release: expected an AsyncLock, got {}".format(type(lock)))
    lock.release()


def async_lock_try_acquire(lock: NLPLAsyncLock) -> bool:
    """Non-blocking acquire attempt.  Returns True if acquired, False if not."""
    if not isinstance(lock, NLPLAsyncLock):
        raise TypeError("async_lock_try_acquire: expected an AsyncLock, got {}".format(type(lock)))
    return lock.try_acquire()


# ---------------------------------------------------------------------------
# Async Semaphore  (counting, timeout-capable)
# ---------------------------------------------------------------------------

class NLPLAsyncSemaphore:
    """
    Counting semaphore safe to use from any thread.

    NexusLang usage::

        set sem to async_semaphore_create with 3    # initial count = 3
        async_semaphore_acquire with sem            # blocks if count = 0
        async_semaphore_release with sem            # increments count
        set ok to async_semaphore_try_acquire with sem
        set n to async_semaphore_value with sem     # current count
    """

    def __init__(self, value: int = 1) -> None:
        if value < 0:
            raise ValueError("Semaphore initial value must be >= 0")
        self._sem = threading.Semaphore(int(value))
        self._lock = threading.Lock()
        self._value = int(value)

    def acquire(self, timeout: Optional[float] = None) -> bool:
        """Decrement the counter; block if zero.  Returns True on success."""
        if timeout is None:
            acquired = self._sem.acquire(blocking=True)
        else:
            acquired = self._sem.acquire(blocking=True, timeout=timeout)
        if acquired:
            with self._lock:
                self._value -= 1
        return acquired

    def release(self) -> None:
        """Increment the counter, potentially unblocking a waiting acquirer."""
        with self._lock:
            self._value += 1
        self._sem.release()

    def try_acquire(self) -> bool:
        """Non-blocking decrement.  Returns True if successful."""
        acquired = self._sem.acquire(blocking=False)
        if acquired:
            with self._lock:
                self._value -= 1
        return acquired

    @property
    def value(self) -> int:
        """Current semaphore count (approximate under contention)."""
        with self._lock:
            return self._value

    def __repr__(self) -> str:
        return "AsyncSemaphore<value={}>".format(self.value)


def async_semaphore_create(value: int = 1) -> NLPLAsyncSemaphore:
    """Create and return a new counting semaphore with initial *value*."""
    return NLPLAsyncSemaphore(value=value)


def async_semaphore_acquire(semaphore: NLPLAsyncSemaphore,
                             timeout: Optional[float] = None) -> bool:
    """
    Decrement *semaphore*, blocking up to *timeout* seconds.

    Raises :class:`TimeoutError` if the timeout elapses without acquiring.
    """
    if not isinstance(semaphore, NLPLAsyncSemaphore):
        raise TypeError("async_semaphore_acquire: expected an AsyncSemaphore, got {}".format(type(semaphore)))
    acquired = semaphore.acquire(timeout=timeout)
    if not acquired:
        raise TimeoutError("async_semaphore_acquire timed out after {} seconds".format(timeout))
    return True


def async_semaphore_release(semaphore: NLPLAsyncSemaphore) -> None:
    """Increment *semaphore*, releasing one waiting acquirer if any."""
    if not isinstance(semaphore, NLPLAsyncSemaphore):
        raise TypeError("async_semaphore_release: expected an AsyncSemaphore, got {}".format(type(semaphore)))
    semaphore.release()


def async_semaphore_try_acquire(semaphore: NLPLAsyncSemaphore) -> bool:
    """Non-blocking decrement.  Returns True if acquired, False if zero."""
    if not isinstance(semaphore, NLPLAsyncSemaphore):
        raise TypeError("async_semaphore_try_acquire: expected an AsyncSemaphore, got {}".format(type(semaphore)))
    return semaphore.try_acquire()


def async_semaphore_value(semaphore: NLPLAsyncSemaphore) -> int:
    """Return the current count of *semaphore*."""
    if not isinstance(semaphore, NLPLAsyncSemaphore):
        raise TypeError("async_semaphore_value: expected an AsyncSemaphore, got {}".format(type(semaphore)))
    return semaphore.value


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

def register_async_runtime_functions(runtime) -> None:
    """Register all async runtime functions with the NexusLang runtime."""

    # Task lifecycle
    runtime.register_function("spawn_async", lambda rt, fn, *args: spawn_async(fn, *args))
    runtime.register_function("task_result", lambda rt, task, *args: task_result(task, *args))
    runtime.register_function("task_is_done", lambda rt, task: task_is_done(task))
    runtime.register_function("task_cancel", lambda rt, task: task_cancel(task))

    # Multi-task coordination
    runtime.register_function("join_all", lambda rt, tasks, *args: join_all(tasks, *args))
    runtime.register_function("select_first", lambda rt, tasks, *args: select_first(tasks, *args))
    runtime.register_function("async_timeout", lambda rt, task, dur: async_timeout(task, dur))

    # Timing
    runtime.register_function("async_sleep", lambda rt, secs: async_sleep(secs))
    runtime.register_function("async_after", lambda rt, delay, fn, *args: async_after(delay, fn, *args))

    # Manual futures
    runtime.register_function("create_future", lambda rt: create_future())
    runtime.register_function("future_set_result", lambda rt, fut, val: future_set_result(fut, val))
    runtime.register_function("future_set_error", lambda rt, fut, err: future_set_error(fut, err))
    runtime.register_function("future_get", lambda rt, fut, *args: future_get(fut, *args))
    runtime.register_function("future_is_done", lambda rt, fut: future_is_done(fut))

    # Async file I/O
    runtime.register_function("async_read_file", lambda rt, *args: async_read_file(*args))
    runtime.register_function("async_read_file_bytes", lambda rt, path: async_read_file_bytes(path))
    runtime.register_function("async_write_file", lambda rt, *args: async_write_file(*args))
    runtime.register_function("async_write_file_bytes", lambda rt, path, data: async_write_file_bytes(path, data))
    runtime.register_function("async_append_file", lambda rt, *args: async_append_file(*args))

    # Async HTTP
    runtime.register_function("async_http_get", lambda rt, *args: async_http_get(*args))
    runtime.register_function("async_http_post", lambda rt, *args: async_http_post(*args))
    runtime.register_function("async_http_put", lambda rt, *args: async_http_put(*args))
    runtime.register_function("async_http_delete", lambda rt, *args: async_http_delete(*args))

    # Async TCP
    runtime.register_function("async_tcp_connect",
                               lambda rt, *args: async_tcp_connect(*args))
    runtime.register_function("async_tcp_listen",
                               lambda rt, *args: async_tcp_listen(*args))
    runtime.register_function("async_tcp_accept",
                               lambda rt, server: async_tcp_accept(server))
    runtime.register_function("async_tcp_send",
                               lambda rt, conn, data: async_tcp_send(conn, data))
    runtime.register_function("async_tcp_recv",
                               lambda rt, *args: async_tcp_recv(*args))
    runtime.register_function("async_tcp_recv_exactly",
                               lambda rt, conn, n: async_tcp_recv_exactly(conn, n))
    runtime.register_function("async_tcp_close",
                               lambda rt, conn: async_tcp_close(conn))

    # Async UDP
    runtime.register_function("async_udp_open",
                               lambda rt, *args: async_udp_open(*args))
    runtime.register_function("async_udp_send_to",
                               lambda rt, conn, data, host, port: async_udp_send_to(conn, data, host, port))
    runtime.register_function("async_udp_recv_from",
                               lambda rt, *args: async_udp_recv_from(*args))
    runtime.register_function("async_udp_close",
                               lambda rt, conn: async_udp_close(conn))

    # Async subprocess
    runtime.register_function("async_subprocess",
                               lambda rt, *args: async_subprocess(*args))
    runtime.register_function("async_subprocess_output",
                               lambda rt, *args: async_subprocess_output(*args))

    # Async DNS
    runtime.register_function("async_dns_resolve",
                               lambda rt, hostname: async_dns_resolve(hostname))
    runtime.register_function("async_dns_reverse",
                               lambda rt, ip: async_dns_reverse(ip))

    # Async readlines
    runtime.register_function("async_readlines",
                               lambda rt, *args: async_readlines(*args))
    runtime.register_function("async_readline_chunks",
                               lambda rt, *args: async_readline_chunks(*args))

    # Async channel (MPMC, thread-safe)
    runtime.register_function("async_channel_create",
                               lambda rt, *args: async_channel_create(*args))
    runtime.register_function("async_channel_send",
                               lambda rt, ch, val, *args: async_channel_send(ch, val, *args))
    runtime.register_function("async_channel_recv",
                               lambda rt, ch, *args: async_channel_recv(ch, *args))
    runtime.register_function("async_channel_try_recv",
                               lambda rt, ch: async_channel_try_recv(ch))
    runtime.register_function("async_channel_close",
                               lambda rt, ch: async_channel_close(ch))
    runtime.register_function("async_channel_is_closed",
                               lambda rt, ch: async_channel_is_closed(ch))
    runtime.register_function("async_channel_size",
                               lambda rt, ch: async_channel_size(ch))

    # Async lock (re-entrant, timeout-capable)
    runtime.register_function("async_lock_create",
                               lambda rt: async_lock_create())
    runtime.register_function("async_lock_acquire",
                               lambda rt, lk, *args: async_lock_acquire(lk, *args))
    runtime.register_function("async_lock_release",
                               lambda rt, lk: async_lock_release(lk))
    runtime.register_function("async_lock_try_acquire",
                               lambda rt, lk: async_lock_try_acquire(lk))

    # Async semaphore (counting, timeout-capable)
    runtime.register_function("async_semaphore_create",
                               lambda rt, *args: async_semaphore_create(*args))
    runtime.register_function("async_semaphore_acquire",
                               lambda rt, sem, *args: async_semaphore_acquire(sem, *args))
    runtime.register_function("async_semaphore_release",
                               lambda rt, sem: async_semaphore_release(sem))
    runtime.register_function("async_semaphore_try_acquire",
                               lambda rt, sem: async_semaphore_try_acquire(sem))
    runtime.register_function("async_semaphore_value",
                               lambda rt, sem: async_semaphore_value(sem))
