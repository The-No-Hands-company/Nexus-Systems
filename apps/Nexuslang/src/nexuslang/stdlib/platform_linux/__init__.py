"""
NLPL Linux/Unix Platform Library

Provides POSIX, epoll, inotify, and systemd APIs for Linux/Unix environments.
Raises PlatformError when imported and executed on non-Linux platforms.

Groups:
  - POSIX extras (process IDs, signals, fork/wait, uname)
  - epoll I/O event notification (level- and edge-triggered)
  - inotify filesystem event monitoring
  - systemd integration (sd_notify, journal, unit management)
"""

import os
import sys
import struct
import socket
import select
import ctypes
import subprocess
import platform as _platform
from typing import Any, Dict, List, Optional


# ---------------------------------------------------------------------------
# Platform guard
# ---------------------------------------------------------------------------

class PlatformError(Exception):
    """Raised when a Linux-specific API is called on a non-Linux platform."""


def _require_linux() -> None:
    if sys.platform not in ('linux', 'linux2'):
        raise PlatformError(
            f"This function requires Linux; current platform is '{sys.platform}'."
        )


# ---------------------------------------------------------------------------
# POSIX extras
# ---------------------------------------------------------------------------

def posix_getpid() -> int:
    """Return the process ID of the current process."""
    _require_linux()
    return os.getpid()


def posix_getppid() -> int:
    """Return the parent process ID of the current process."""
    _require_linux()
    return os.getppid()


def posix_getuid() -> int:
    """Return the real user ID of the current process."""
    _require_linux()
    return os.getuid()


def posix_getgid() -> int:
    """Return the real group ID of the current process."""
    _require_linux()
    return os.getgid()


def posix_geteuid() -> int:
    """Return the effective user ID of the current process."""
    _require_linux()
    return os.geteuid()


def posix_getegid() -> int:
    """Return the effective group ID of the current process."""
    _require_linux()
    return os.getegid()


def posix_getcwd() -> str:
    """Return the current working directory as an absolute path string."""
    _require_linux()
    return os.getcwd()


def posix_chdir(path: str) -> None:
    """Change the current working directory to path.

    Returns None on success, or a dict with 'error' on failure.
    """
    _require_linux()
    try:
        os.chdir(str(path))
    except OSError as e:
        return {'error': str(e), 'errno': e.errno}


def posix_umask(mask: int) -> int:
    """Set the file mode creation mask to mask and return the old mask.

    The mask must be a valid POSIX file mode mask integer (e.g. 0o022).
    """
    _require_linux()
    return os.umask(int(mask))


def posix_uname() -> dict:
    """Return OS and hardware information (wraps os.uname / uname -a).

    Returns a dict with keys: sysname, nodename, release, version, machine.
    """
    _require_linux()
    u = os.uname()
    return {
        'sysname': u.sysname,
        'nodename': u.nodename,
        'release': u.release,
        'version': u.version,
        'machine': u.machine,
    }


def posix_hostname() -> str:
    """Return the hostname of the current machine."""
    _require_linux()
    return socket.gethostname()


def posix_username() -> str:
    """Return the login name of the current user."""
    _require_linux()
    import pwd
    return pwd.getpwuid(os.geteuid()).pw_name


def posix_groups() -> List[int]:
    """Return a list of supplementary group IDs for the current process."""
    _require_linux()
    return list(os.getgroups())


def posix_kill(pid: int, sig: int) -> None:
    """Send signal sig to process pid.

    Returns None on success, or a dict with 'error' and 'errno' on failure.
    Common signal numbers: SIGTERM=15, SIGKILL=9, SIGHUP=1, SIGINT=2.
    """
    _require_linux()
    import signal as _sig
    try:
        os.kill(int(pid), int(sig))
    except ProcessLookupError as e:
        return {'error': str(e), 'errno': e.errno}
    except PermissionError as e:
        return {'error': str(e), 'errno': e.errno}
    except OSError as e:
        return {'error': str(e), 'errno': e.errno}


def posix_fork() -> int:
    """Fork the current process.

    Returns the child PID to the parent, and 0 to the child.
    Use with care — the child should call os._exit() rather than return.
    """
    _require_linux()
    return os.fork()


def posix_waitpid(pid: int, options: int = 0) -> dict:
    """Wait for child process pid to change state.

    options = 0 (block) or os.WNOHANG (non-blocking).
    Returns a dict with keys: pid, status, exit_code, exited, signaled.
    On failure returns a dict with 'error'.
    """
    _require_linux()
    try:
        got_pid, status = os.waitpid(int(pid), int(options))
        return {
            'pid': got_pid,
            'status': status,
            'exit_code': os.waitstatus_to_exitcode(status) if got_pid else None,
            'exited': os.WIFEXITED(status),
            'signaled': os.WIFSIGNALED(status),
        }
    except ChildProcessError as e:
        return {'error': str(e), 'errno': e.errno, 'pid': int(pid)}
    except OSError as e:
        return {'error': str(e), 'errno': e.errno, 'pid': int(pid)}


def posix_getenv(name: str, default: Optional[str] = None) -> Optional[str]:
    """Return the value of the environment variable name.

    Returns default if the variable is not set.
    """
    _require_linux()
    return os.environ.get(str(name), default)


def posix_setenv(name: str, value: str) -> None:
    """Set environment variable name to value."""
    _require_linux()
    os.environ[str(name)] = str(value)


def posix_unsetenv(name: str) -> None:
    """Unset (delete) environment variable name."""
    _require_linux()
    os.environ.pop(str(name), None)


def posix_environ() -> dict:
    """Return a copy of the current environment as a plain dict."""
    _require_linux()
    return dict(os.environ)


# ---------------------------------------------------------------------------
# epoll helpers
# ---------------------------------------------------------------------------

_EPOLL_EVENT_MAP: Dict[str, int] = {
    'in':       select.EPOLLIN,
    'out':      select.EPOLLOUT,
    'both':     select.EPOLLIN | select.EPOLLOUT,
    'err':      select.EPOLLERR,
    'hup':      select.EPOLLHUP,
    'rdhup':    getattr(select, 'EPOLLRDHUP', 0x2000),
    'et':       select.EPOLLET,
    'oneshot':  select.EPOLLONESHOT,
    'read':     select.EPOLLIN,
    'write':    select.EPOLLOUT,
}


def _parse_epoll_events(events: Any) -> int:
    if isinstance(events, int):
        return events
    flags = 0
    for token in str(events).lower().replace(',', ' ').split():
        if token in _EPOLL_EVENT_MAP:
            flags |= _EPOLL_EVENT_MAP[token]
        elif token.isdigit():
            flags |= int(token)
        else:
            raise ValueError(
                f"Unknown epoll event flag '{token}'. "
                f"Valid options: {sorted(_EPOLL_EVENT_MAP.keys())}"
            )
    return flags or select.EPOLLIN


# ---------------------------------------------------------------------------
# epoll API
# ---------------------------------------------------------------------------

def epoll_create() -> dict:
    """Create a new epoll object.

    Returns a dict with keys: 'epoll' (select.epoll instance), 'fd' (fileno).
    On non-Linux platforms raises PlatformError.

    Example NexusLang usage::

        set ep to epoll_create with
        epoll_add with ep and server_fd and "in"
        set events to epoll_wait with ep
    """
    _require_linux()
    ep = select.epoll()
    return {'epoll': ep, 'fd': ep.fileno()}


def epoll_add(ep_dict: dict, fd: int, events: Any = 'in') -> None:
    """Register file descriptor fd with the epoll object.

    events: 'in', 'out', 'both', 'et', 'oneshot', or an integer flag,
            or multiple flags separated by spaces or commas.
    Returns None on success, or a dict with 'error' on failure.
    """
    _require_linux()
    ep = ep_dict['epoll']
    flags = _parse_epoll_events(events)
    try:
        ep.register(int(fd), flags)
    except (FileExistsError, OSError) as e:
        return {'error': str(e), 'errno': getattr(e, 'errno', None)}


def epoll_modify(ep_dict: dict, fd: int, events: Any) -> None:
    """Change the event mask for file descriptor fd on the epoll object.

    Returns None on success, or a dict with 'error' on failure.
    """
    _require_linux()
    ep = ep_dict['epoll']
    flags = _parse_epoll_events(events)
    try:
        ep.modify(int(fd), flags)
    except OSError as e:
        return {'error': str(e), 'errno': e.errno}


def epoll_remove(ep_dict: dict, fd: int) -> None:
    """Unregister file descriptor fd from the epoll object.

    Returns None on success, or a dict with 'error' on failure.
    """
    _require_linux()
    ep = ep_dict['epoll']
    try:
        ep.unregister(int(fd))
    except OSError as e:
        return {'error': str(e), 'errno': e.errno}


def epoll_wait(ep_dict: dict, maxevents: int = 10,
               timeout: float = -1) -> List[dict]:
    """Wait for events on the epoll object.

    maxevents: maximum number of events to return per call.
    timeout: seconds to wait (-1 = block indefinitely, 0 = return immediately).

    Returns a list of event dicts, each with keys:
    'fd' (int), 'event_mask' (int), 'readable' (bool), 'writable' (bool),
    'error' (bool), 'hangup' (bool).
    """
    _require_linux()
    ep = ep_dict['epoll']
    raw = ep.poll(timeout=float(timeout), maxevents=int(maxevents))
    result = []
    for fd, event in raw:
        result.append({
            'fd': fd,
            'event_mask': event,
            'readable': bool(event & select.EPOLLIN),
            'writable': bool(event & select.EPOLLOUT),
            'error': bool(event & select.EPOLLERR),
            'hangup': bool(event & select.EPOLLHUP),
        })
    return result


def epoll_close(ep_dict: dict) -> None:
    """Close the epoll file descriptor. Safe to call on an already-closed object."""
    if ep_dict is None:
        return
    ep = ep_dict.get('epoll')
    if ep is not None:
        try:
            ep.close()
        except Exception:
            pass


# ---------------------------------------------------------------------------
# inotify constants
# ---------------------------------------------------------------------------

IN_ACCESS        = 0x00000001   # File was accessed
IN_MODIFY        = 0x00000002   # File was modified
IN_ATTRIB        = 0x00000004   # Metadata changed
IN_CLOSE_WRITE   = 0x00000008   # Writable file closed
IN_CLOSE_NOWRITE = 0x00000010   # Non-writable file closed
IN_OPEN          = 0x00000020   # File was opened
IN_MOVED_FROM    = 0x00000040   # File moved from watched dir
IN_MOVED_TO      = 0x00000080   # File moved into watched dir
IN_CREATE        = 0x00000100   # File/dir created in watched dir
IN_DELETE        = 0x00000200   # File/dir deleted in watched dir
IN_DELETE_SELF   = 0x00000400   # Watched dir/file deleted
IN_MOVE_SELF     = 0x00000800   # Watched dir/file moved
IN_CLOSE         = IN_CLOSE_WRITE | IN_CLOSE_NOWRITE
IN_MOVE          = IN_MOVED_FROM | IN_MOVED_TO
IN_ALL_EVENTS    = 0x00000FFF
IN_UNMOUNT       = 0x00002000   # Filesystem containing watched object unmounted
IN_Q_OVERFLOW    = 0x00004000   # Event queue overflowed
IN_IGNORED       = 0x00008000   # Watch removed
IN_ONLYDIR       = 0x01000000   # Watch directory only
IN_DONT_FOLLOW   = 0x02000000   # Don't follow symlinks
IN_EXCL_UNLINK   = 0x04000000   # Exclude unlinked files
IN_MASK_ADD      = 0x20000000   # Add to existing mask
IN_ISDIR         = 0x40000000   # Subject of event is a directory
IN_ONESHOT       = 0x80000000   # Monitor only once

_INOTIFY_EVENT_NAMES: Dict[int, str] = {
    IN_ACCESS:        'IN_ACCESS',
    IN_MODIFY:        'IN_MODIFY',
    IN_ATTRIB:        'IN_ATTRIB',
    IN_CLOSE_WRITE:   'IN_CLOSE_WRITE',
    IN_CLOSE_NOWRITE: 'IN_CLOSE_NOWRITE',
    IN_OPEN:          'IN_OPEN',
    IN_MOVED_FROM:    'IN_MOVED_FROM',
    IN_MOVED_TO:      'IN_MOVED_TO',
    IN_CREATE:        'IN_CREATE',
    IN_DELETE:        'IN_DELETE',
    IN_DELETE_SELF:   'IN_DELETE_SELF',
    IN_MOVE_SELF:     'IN_MOVE_SELF',
    IN_UNMOUNT:       'IN_UNMOUNT',
    IN_Q_OVERFLOW:    'IN_Q_OVERFLOW',
    IN_IGNORED:       'IN_IGNORED',
    IN_ISDIR:         'IN_ISDIR',
}

_INOTIFY_EVENT_STRING_MAP: Dict[str, int] = {
    'access':        IN_ACCESS,
    'modify':        IN_MODIFY,
    'attrib':        IN_ATTRIB,
    'close_write':   IN_CLOSE_WRITE,
    'close_nowrite': IN_CLOSE_NOWRITE,
    'open':          IN_OPEN,
    'moved_from':    IN_MOVED_FROM,
    'moved_to':      IN_MOVED_TO,
    'create':        IN_CREATE,
    'delete':        IN_DELETE,
    'delete_self':   IN_DELETE_SELF,
    'move_self':     IN_MOVE_SELF,
    'close':         IN_CLOSE,
    'move':          IN_MOVE,
    'all':           IN_ALL_EVENTS,
}

# inotify_event struct layout: wd(i4) mask(u4) cookie(u4) len(u4)
_INOTIFY_EVENT_STRUCT = struct.Struct('iIII')
_INOTIFY_EVENT_SIZE   = _INOTIFY_EVENT_STRUCT.size   # 16


def _decode_inotify_mask(mask: int) -> List[str]:
    names = []
    for flag, name in _INOTIFY_EVENT_NAMES.items():
        if mask & flag:
            names.append(name)
    return names


def _parse_inotify_events(events: Any) -> int:
    if isinstance(events, int):
        return events
    flags = 0
    for token in str(events).lower().replace(',', ' ').split():
        if token in _INOTIFY_EVENT_STRING_MAP:
            flags |= _INOTIFY_EVENT_STRING_MAP[token]
        elif token.isdigit():
            flags |= int(token)
        else:
            raise ValueError(
                f"Unknown inotify event '{token}'. "
                f"Valid options: {sorted(_INOTIFY_EVENT_STRING_MAP.keys())}"
            )
    return flags or IN_ALL_EVENTS


# ---------------------------------------------------------------------------
# inotify API (Linux syscalls via ctypes libc)
# ---------------------------------------------------------------------------

_libc: Optional[Any] = None


def _get_libc():
    global _libc
    if _libc is None:
        _libc = ctypes.CDLL('libc.so.6', use_errno=True)
    return _libc


def inotify_create(nonblock: bool = False) -> int:
    """Create an inotify instance and return its file descriptor.

    nonblock: if True, set O_NONBLOCK flag on the inotify fd.
    Raises PlatformError on non-Linux; raises OSError if creation fails.
    """
    _require_linux()
    libc = _get_libc()
    flags = 0x00080000 if nonblock else 0  # O_NONBLOCK
    fd = libc.inotify_init1(flags)
    if fd < 0:
        errno = ctypes.get_errno()
        raise OSError(errno, os.strerror(errno), 'inotify_init1')
    return fd


def inotify_add_watch(fd: int, path: str, events: Any = 'all') -> int:
    """Add a watch for path to the inotify instance fd.

    events: string name(s) ('create', 'modify', 'delete', 'move', 'all', etc.),
            comma/space separated; or an integer mask.
    Returns a watch descriptor (wd >= 0).
    Raises OSError on failure (e.g. path not found, permission denied).
    """
    _require_linux()
    libc = _get_libc()
    mask = _parse_inotify_events(events)
    path_bytes = str(path).encode(errors='replace')
    wd = libc.inotify_add_watch(int(fd), path_bytes, ctypes.c_uint32(mask))
    if wd < 0:
        errno = ctypes.get_errno()
        raise OSError(errno, os.strerror(errno), str(path))
    return wd


def inotify_remove_watch(fd: int, wd: int) -> None:
    """Remove the watch descriptor wd from the inotify instance fd.

    Returns None on success, or a dict with 'error' on failure.
    """
    _require_linux()
    libc = _get_libc()
    ret = libc.inotify_rm_watch(int(fd), ctypes.c_int(wd))
    if ret < 0:
        errno = ctypes.get_errno()
        return {'error': os.strerror(errno), 'errno': errno}


def inotify_read_events(fd: int, buffer_size: int = 4096,
                        timeout: Optional[float] = None) -> List[dict]:
    """Read available inotify events from fd.

    Blocks until at least one event is available (or timeout expires).
    timeout: seconds to wait (None = block indefinitely, 0 = non-blocking).

    Returns a list of event dicts with keys:
    'wd' (watch descriptor), 'mask' (int), 'cookie' (int),
    'name' (str or ''), 'is_dir' (bool), 'event_names' (list[str]).
    On timeout returns an empty list.
    On error returns [{'error': message}].
    """
    _require_linux()
    if timeout is not None:
        ready = select.select([fd], [], [], float(timeout))[0]
        if not ready:
            return []
    try:
        raw = os.read(int(fd), int(buffer_size))
    except BlockingIOError:
        return []
    except OSError as e:
        return [{'error': str(e), 'errno': e.errno}]

    events = []
    offset = 0
    while offset + _INOTIFY_EVENT_SIZE <= len(raw):
        wd, mask, cookie, length = _INOTIFY_EVENT_STRUCT.unpack_from(raw, offset)
        offset += _INOTIFY_EVENT_SIZE
        name = ''
        if length > 0 and offset + length <= len(raw):
            name_raw = raw[offset: offset + length]
            name = name_raw.rstrip(b'\x00').decode(errors='replace')
            offset += length
        events.append({
            'wd': wd,
            'mask': mask,
            'cookie': cookie,
            'name': name,
            'is_dir': bool(mask & IN_ISDIR),
            'event_names': _decode_inotify_mask(mask),
        })
    return events


def inotify_close(fd: int) -> None:
    """Close an inotify file descriptor. Safe to call on an already-closed fd."""
    try:
        os.close(int(fd))
    except OSError:
        pass


# ---------------------------------------------------------------------------
# systemd integration
# ---------------------------------------------------------------------------

def systemd_notify(message: str) -> bool:
    """Send a notification message to systemd via the $NOTIFY_SOCKET.

    Common messages:
      'READY=1'             — service is ready
      'STOPPING=1'          — service is about to stop
      'WATCHDOG=1'          — watchdog keepalive
      'STATUS=description'  — human-readable status

    Returns True if the notification was sent, False if $NOTIFY_SOCKET is
    not set (service not started by systemd or notification socket disabled).
    Raises PlatformError on non-Linux.
    """
    _require_linux()
    notify_socket = os.environ.get('NOTIFY_SOCKET')
    if not notify_socket:
        return False
    msg = str(message).encode()
    abstract = notify_socket.startswith('@')
    addr = '\0' + notify_socket[1:] if abstract else notify_socket
    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        try:
            sock.sendto(msg, addr)
        finally:
            sock.close()
        return True
    except OSError:
        return False


def systemd_notify_ready() -> bool:
    """Notify systemd that the service has finished initializing (READY=1)."""
    return systemd_notify('READY=1')


def systemd_notify_stopping() -> bool:
    """Notify systemd that the service is about to stop (STOPPING=1)."""
    return systemd_notify('STOPPING=1')


def systemd_notify_watchdog() -> bool:
    """Send a watchdog keepalive to systemd (WATCHDOG=1)."""
    return systemd_notify('WATCHDOG=1')


def systemd_notify_status(status: str) -> bool:
    """Send a human-readable status string to systemd (STATUS=...)."""
    return systemd_notify(f'STATUS={status}')


def systemd_journal_log(message: str, priority: int = 6) -> None:
    """Write a message to the systemd journal via syslog.

    priority: syslog priority level 0-7
               (0=EMERG, 3=ERR, 4=WARNING, 5=NOTICE, 6=INFO, 7=DEBUG).
    Requires 'syslog' (available on Linux/macOS).
    """
    _require_linux()
    import syslog
    syslog.openlog(facility=syslog.LOG_DAEMON)
    syslog.syslog(int(priority), str(message))
    syslog.closelog()


def _run_systemctl(*args) -> dict:
    """Run systemctl with the given arguments. Returns output dict."""
    try:
        result = subprocess.run(
            ['systemctl'] + list(args),
            capture_output=True, text=True, timeout=15
        )
        return {
            'success': result.returncode == 0,
            'output': result.stdout.strip(),
            'error': result.stderr.strip() if result.returncode != 0 else '',
            'returncode': result.returncode,
        }
    except FileNotFoundError:
        return {'success': False, 'error': 'systemctl not found', 'output': '',
                'returncode': -1}
    except subprocess.TimeoutExpired:
        return {'success': False, 'error': 'timeout', 'output': '', 'returncode': -1}


def systemd_unit_status(unit: str) -> dict:
    """Query the status of a systemd unit.

    Returns a dict with keys: active_state, sub_state, load_state, description,
    and optionally 'error' if systemctl is unavailable.
    """
    _require_linux()
    r = _run_systemctl('show', '--no-pager', '--property',
                        'ActiveState,SubState,LoadState,Description',
                        str(unit))
    if not r['success'] and not r['output']:
        return r
    props: Dict[str, str] = {}
    for line in r['output'].splitlines():
        if '=' in line:
            k, _, v = line.partition('=')
            props[k.strip()] = v.strip()
    return {
        'active_state': props.get('ActiveState', ''),
        'sub_state': props.get('SubState', ''),
        'load_state': props.get('LoadState', ''),
        'description': props.get('Description', ''),
    }


def systemd_is_active(unit: str) -> bool:
    """Return True if the systemd unit is currently active (running).

    Returns False if inactive, failed, or systemctl not available.
    """
    _require_linux()
    r = _run_systemctl('is-active', '--quiet', str(unit))
    return r['returncode'] == 0


def systemd_start_unit(unit: str) -> dict:
    """Start a systemd unit. Returns dict with 'success', 'output', 'error'."""
    _require_linux()
    return _run_systemctl('start', str(unit))


def systemd_stop_unit(unit: str) -> dict:
    """Stop a systemd unit. Returns dict with 'success', 'output', 'error'."""
    _require_linux()
    return _run_systemctl('stop', str(unit))


def systemd_restart_unit(unit: str) -> dict:
    """Restart a systemd unit. Returns dict with 'success', 'output', 'error'."""
    _require_linux()
    return _run_systemctl('restart', str(unit))


def systemd_reload_unit(unit: str) -> dict:
    """Reload a systemd unit's configuration.

    Returns dict with 'success', 'output', 'error'.
    """
    _require_linux()
    return _run_systemctl('reload', str(unit))


def systemd_enable_unit(unit: str) -> dict:
    """Enable a systemd unit to start at boot.

    Returns dict with 'success', 'output', 'error'.
    """
    _require_linux()
    return _run_systemctl('enable', str(unit))


def systemd_disable_unit(unit: str) -> dict:
    """Disable a systemd unit from starting at boot.

    Returns dict with 'success', 'output', 'error'.
    """
    _require_linux()
    return _run_systemctl('disable', str(unit))


def systemd_list_units(
    unit_type: Optional[str] = None,
    state: Optional[str] = None,
) -> list:
    """List loaded systemd units.

    unit_type: filter by type ('service', 'socket', 'timer', 'target', etc.).
    state: filter by active state ('active', 'inactive', 'failed', etc.).

    Returns a list of dicts with keys: unit, load, active, sub, description.
    Returns a dict with 'error' if systemctl is unavailable.
    """
    _require_linux()
    args = ['list-units', '--no-pager', '--no-legend', '--plain', '--all']
    if unit_type:
        args += ['--type', str(unit_type)]
    if state:
        args += ['--state', str(state)]
    r = _run_systemctl(*args)
    if not r['success'] and not r['output']:
        return [{'error': r.get('error', 'systemctl failed')}]
    units = []
    for line in r['output'].splitlines():
        parts = line.split(None, 4)
        if len(parts) >= 4:
            units.append({
                'unit': parts[0],
                'load': parts[1],
                'active': parts[2],
                'sub': parts[3],
                'description': parts[4] if len(parts) > 4 else '',
            })
    return units


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

def register_platform_linux_functions(runtime) -> None:
    """Register all Linux platform functions with the NexusLang runtime."""

    # POSIX extras
    runtime.register_function("posix_getpid",  lambda rt: posix_getpid())
    runtime.register_function("posix_getppid", lambda rt: posix_getppid())
    runtime.register_function("posix_getuid",  lambda rt: posix_getuid())
    runtime.register_function("posix_getgid",  lambda rt: posix_getgid())
    runtime.register_function("posix_geteuid", lambda rt: posix_geteuid())
    runtime.register_function("posix_getegid", lambda rt: posix_getegid())
    runtime.register_function("posix_getcwd",  lambda rt: posix_getcwd())
    runtime.register_function("posix_chdir",   lambda rt, p: posix_chdir(p))
    runtime.register_function("posix_umask",   lambda rt, m: posix_umask(m))
    runtime.register_function("posix_uname",   lambda rt: posix_uname())
    runtime.register_function("posix_hostname", lambda rt: posix_hostname())
    runtime.register_function("posix_username", lambda rt: posix_username())
    runtime.register_function("posix_groups",  lambda rt: posix_groups())
    runtime.register_function("posix_kill",    lambda rt, pid, sig: posix_kill(pid, sig))
    runtime.register_function("posix_fork",    lambda rt: posix_fork())
    runtime.register_function("posix_waitpid", lambda rt, *a: posix_waitpid(*a))
    runtime.register_function("posix_getenv",  lambda rt, *a: posix_getenv(*a))
    runtime.register_function("posix_setenv",  lambda rt, n, v: posix_setenv(n, v))
    runtime.register_function("posix_unsetenv", lambda rt, n: posix_unsetenv(n))
    runtime.register_function("posix_environ", lambda rt: posix_environ())

    # epoll
    runtime.register_function("epoll_create",  lambda rt: epoll_create())
    runtime.register_function("epoll_add",     lambda rt, *a: epoll_add(*a))
    runtime.register_function("epoll_modify",  lambda rt, *a: epoll_modify(*a))
    runtime.register_function("epoll_remove",  lambda rt, ep, fd: epoll_remove(ep, fd))
    runtime.register_function("epoll_wait",    lambda rt, *a: epoll_wait(*a))
    runtime.register_function("epoll_close",   lambda rt, ep: epoll_close(ep))

    # inotify
    runtime.register_function("inotify_create",       lambda rt, *a: inotify_create(*a))
    runtime.register_function("inotify_add_watch",    lambda rt, *a: inotify_add_watch(*a))
    runtime.register_function("inotify_remove_watch", lambda rt, fd, wd: inotify_remove_watch(fd, wd))
    runtime.register_function("inotify_read_events",  lambda rt, *a: inotify_read_events(*a))
    runtime.register_function("inotify_close",        lambda rt, fd: inotify_close(fd))
    # inotify constants
    for _name, _val in [
        ("IN_ACCESS", IN_ACCESS), ("IN_MODIFY", IN_MODIFY),
        ("IN_ATTRIB", IN_ATTRIB), ("IN_CLOSE_WRITE", IN_CLOSE_WRITE),
        ("IN_CLOSE_NOWRITE", IN_CLOSE_NOWRITE), ("IN_OPEN", IN_OPEN),
        ("IN_MOVED_FROM", IN_MOVED_FROM), ("IN_MOVED_TO", IN_MOVED_TO),
        ("IN_CREATE", IN_CREATE), ("IN_DELETE", IN_DELETE),
        ("IN_DELETE_SELF", IN_DELETE_SELF), ("IN_MOVE_SELF", IN_MOVE_SELF),
        ("IN_CLOSE", IN_CLOSE), ("IN_MOVE", IN_MOVE),
        ("IN_ALL_EVENTS", IN_ALL_EVENTS), ("IN_ISDIR", IN_ISDIR),
        ("IN_ONESHOT", IN_ONESHOT), ("IN_ONLYDIR", IN_ONLYDIR),
    ]:
        _name_local, _val_local = _name, _val
        runtime.register_function(
            _name_local, lambda rt, v=_val_local: v
        )

    # systemd
    runtime.register_function("systemd_notify",         lambda rt, msg: systemd_notify(msg))
    runtime.register_function("systemd_notify_ready",   lambda rt: systemd_notify_ready())
    runtime.register_function("systemd_notify_stopping", lambda rt: systemd_notify_stopping())
    runtime.register_function("systemd_notify_watchdog", lambda rt: systemd_notify_watchdog())
    runtime.register_function("systemd_notify_status",  lambda rt, s: systemd_notify_status(s))
    runtime.register_function("systemd_journal_log",    lambda rt, *a: systemd_journal_log(*a))
    runtime.register_function("systemd_unit_status",    lambda rt, u: systemd_unit_status(u))
    runtime.register_function("systemd_is_active",      lambda rt, u: systemd_is_active(u))
    runtime.register_function("systemd_start_unit",     lambda rt, u: systemd_start_unit(u))
    runtime.register_function("systemd_stop_unit",      lambda rt, u: systemd_stop_unit(u))
    runtime.register_function("systemd_restart_unit",   lambda rt, u: systemd_restart_unit(u))
    runtime.register_function("systemd_reload_unit",    lambda rt, u: systemd_reload_unit(u))
    runtime.register_function("systemd_enable_unit",    lambda rt, u: systemd_enable_unit(u))
    runtime.register_function("systemd_disable_unit",   lambda rt, u: systemd_disable_unit(u))
    runtime.register_function("systemd_list_units",     lambda rt, *a: systemd_list_units(*a))
