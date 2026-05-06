"""
NLPL Windows Platform Library

Provides Win32, Registry, Windows Services, and COM APIs.
All functions raise PlatformError when called on non-Windows platforms.

Groups:
  - Registry access (read, write, delete, enumerate)
  - Win32 system information and UI (MessageBox, computer name, etc.)
  - Windows Services (query, start, stop, list)
  - COM object interaction (create, call method)
"""

import os
import sys
import subprocess
from typing import Any, Dict, List, Optional


# ---------------------------------------------------------------------------
# Platform guard
# ---------------------------------------------------------------------------

class PlatformError(Exception):
    """Raised when a Windows-specific API is called on a non-Windows platform."""


def _require_windows() -> None:
    if sys.platform != 'win32':
        raise PlatformError(
            f"This function requires Windows (win32); current platform is '{sys.platform}'."
        )


# ---------------------------------------------------------------------------
# Registry constants (mirrors winreg values so callers don't need winreg)
# ---------------------------------------------------------------------------

HKEY_LOCAL_MACHINE  = 0x80000002
HKEY_CURRENT_USER   = 0x80000001
HKEY_CLASSES_ROOT   = 0x80000000
HKEY_USERS          = 0x80000003
HKEY_CURRENT_CONFIG = 0x80000005

REG_SZ         = 1
REG_EXPAND_SZ  = 2
REG_BINARY     = 3
REG_DWORD      = 4
REG_MULTI_SZ   = 7
REG_QWORD      = 11

_HIVE_MAP: Dict[str, int] = {
    'HKLM': HKEY_LOCAL_MACHINE,
    'HKCU': HKEY_CURRENT_USER,
    'HKCR': HKEY_CLASSES_ROOT,
    'HKU':  HKEY_USERS,
    'HKCC': HKEY_CURRENT_CONFIG,
}


def _resolve_hkey(hkey: Any) -> int:
    """Accept an integer HKEY constant or a short string ('HKLM', 'HKCU', etc.)."""
    if isinstance(hkey, int):
        return hkey
    key = str(hkey).upper().strip('\\')
    if key in _HIVE_MAP:
        return _HIVE_MAP[key]
    raise ValueError(
        f"Unknown registry hive '{hkey}'. "
        f"Valid values: {list(_HIVE_MAP.keys())} or integer constants."
    )


# ---------------------------------------------------------------------------
# Registry API
# ---------------------------------------------------------------------------

def registry_read(hive: Any, subkey: str, value_name: str = '') -> Any:
    """Read a registry value.

    hive:       root hive — integer constant or string 'HKLM', 'HKCU', etc.
    subkey:     registry path under the hive, e.g. 'SOFTWARE\\Python\\3.12'.
    value_name: value name to read; '' or None reads the default value.

    Returns a dict {'value': <data>, 'type': <REG_* constant int>} on success,
    or {'error': <message>} on failure.
    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import winreg
    hkey_int = _resolve_hkey(hive)
    name = '' if value_name is None else str(value_name)
    try:
        with winreg.OpenKey(hkey_int, str(subkey), 0,
                            winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as key:
            data, reg_type = winreg.QueryValueEx(key, name)
            return {'value': data, 'type': reg_type}
    except FileNotFoundError:
        return {'error': f"Key or value not found: {subkey}\\{name}"}
    except PermissionError as e:
        return {'error': f"Access denied: {e}"}
    except OSError as e:
        return {'error': str(e)}


def registry_write(hive: Any, subkey: str, value_name: str,
                   data: Any, reg_type: int = REG_SZ) -> dict:
    """Write a registry value, creating the key path if necessary.

    hive, subkey: see registry_read.
    value_name:   name of the value to write.
    data:         value data (type must match reg_type).
    reg_type:     REG_SZ (1), REG_DWORD (4), REG_BINARY (3), etc.

    Returns {'success': True} or {'error': <message>}.
    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import winreg
    hkey_int = _resolve_hkey(hive)
    try:
        with winreg.CreateKeyEx(hkey_int, str(subkey), 0,
                                winreg.KEY_SET_VALUE | winreg.KEY_WOW64_64KEY) as key:
            winreg.SetValueEx(key, str(value_name) if value_name else '',
                              0, int(reg_type), data)
        return {'success': True}
    except PermissionError as e:
        return {'error': f"Access denied: {e}"}
    except OSError as e:
        return {'error': str(e)}


def registry_delete(hive: Any, subkey: str, value_name: str) -> dict:
    """Delete a registry value.

    Returns {'success': True} or {'error': <message>}.
    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import winreg
    hkey_int = _resolve_hkey(hive)
    try:
        with winreg.OpenKey(hkey_int, str(subkey), 0,
                            winreg.KEY_SET_VALUE | winreg.KEY_WOW64_64KEY) as key:
            winreg.DeleteValue(key, str(value_name))
        return {'success': True}
    except FileNotFoundError:
        return {'error': f"Value not found: {value_name}"}
    except PermissionError as e:
        return {'error': f"Access denied: {e}"}
    except OSError as e:
        return {'error': str(e)}


def registry_delete_key(hive: Any, subkey: str) -> dict:
    """Delete a registry key (must have no subkeys).

    Returns {'success': True} or {'error': <message>}.
    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import winreg
    hkey_int = _resolve_hkey(hive)
    try:
        winreg.DeleteKey(hkey_int, str(subkey))
        return {'success': True}
    except FileNotFoundError:
        return {'error': f"Key not found: {subkey}"}
    except PermissionError as e:
        return {'error': f"Access denied: {e}"}
    except OSError as e:
        return {'error': str(e)}


def registry_list_subkeys(hive: Any, subkey: str) -> list:
    """List the names of all subkeys under a registry key.

    Returns a list of subkey name strings, or a list with one error dict on failure.
    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import winreg
    hkey_int = _resolve_hkey(hive)
    try:
        with winreg.OpenKey(hkey_int, str(subkey), 0,
                            winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as key:
            i = 0
            names = []
            while True:
                try:
                    names.append(winreg.EnumKey(key, i))
                    i += 1
                except OSError:
                    break
            return names
    except FileNotFoundError:
        return [{'error': f"Key not found: {subkey}"}]
    except PermissionError as e:
        return [{'error': f"Access denied: {e}"}]
    except OSError as e:
        return [{'error': str(e)}]


def registry_list_values(hive: Any, subkey: str) -> list:
    """List all values in a registry key.

    Returns a list of dicts with keys 'name', 'value', 'type',
    or a list with one error dict on failure.
    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import winreg
    hkey_int = _resolve_hkey(hive)
    try:
        with winreg.OpenKey(hkey_int, str(subkey), 0,
                            winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as key:
            i = 0
            values = []
            while True:
                try:
                    name, data, reg_type = winreg.EnumValue(key, i)
                    values.append({'name': name, 'value': data, 'type': reg_type})
                    i += 1
                except OSError:
                    break
            return values
    except FileNotFoundError:
        return [{'error': f"Key not found: {subkey}"}]
    except PermissionError as e:
        return [{'error': f"Access denied: {e}"}]
    except OSError as e:
        return [{'error': str(e)}]


def registry_key_exists(hive: Any, subkey: str) -> bool:
    """Return True if the registry key exists, False otherwise.

    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import winreg
    hkey_int = _resolve_hkey(hive)
    try:
        with winreg.OpenKey(hkey_int, str(subkey), 0,
                            winreg.KEY_READ | winreg.KEY_WOW64_64KEY):
            return True
    except OSError:
        return False


# ---------------------------------------------------------------------------
# Win32 system information
# ---------------------------------------------------------------------------

def win32_computer_name() -> str:
    """Return the NetBIOS name of the local computer.

    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import ctypes
    buf = ctypes.create_unicode_buffer(256)
    ctypes.windll.kernel32.GetComputerNameW(buf, ctypes.byref(ctypes.c_ulong(256)))
    return buf.value


def win32_username() -> str:
    """Return the user name associated with the current thread.

    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import ctypes
    size = ctypes.c_ulong(256)
    buf  = ctypes.create_unicode_buffer(256)
    ctypes.windll.advapi32.GetUserNameW(buf, ctypes.byref(size))
    return buf.value


def win32_is_admin() -> bool:
    """Return True if the current process has administrator privileges.

    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import ctypes
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except Exception:
        return False


def win32_message_box(title: str, message: str,
                      style: int = 0) -> int:
    """Display a Win32 MessageBox dialog and return the button ID.

    style: combination of MB_* flags (0=MB_OK, 1=MB_OKCANCEL, 4=MB_YESNO).
    Return values: 1=OK, 2=Cancel, 6=Yes, 7=No.
    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import ctypes
    return ctypes.windll.user32.MessageBoxW(
        None, str(message), str(title), int(style)
    )


def win32_get_windows_dir() -> str:
    """Return the path to the Windows installation directory (e.g. C:\\Windows).

    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import ctypes
    buf = ctypes.create_unicode_buffer(260)
    ctypes.windll.kernel32.GetWindowsDirectoryW(buf, 260)
    return buf.value


def win32_get_system_dir() -> str:
    """Return the path to the Windows System32 directory.

    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import ctypes
    buf = ctypes.create_unicode_buffer(260)
    ctypes.windll.kernel32.GetSystemDirectoryW(buf, 260)
    return buf.value


def win32_get_temp_dir() -> str:
    """Return the path to the temporary file directory.

    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import ctypes
    buf = ctypes.create_unicode_buffer(260)
    ctypes.windll.kernel32.GetTempPathW(260, buf)
    return buf.value


def win32_get_env(name: str, default: Optional[str] = None) -> Optional[str]:
    """Return the value of the named environment variable.

    Returns default if the variable is not set.
    Raises PlatformError on non-Windows.
    """
    _require_windows()
    return os.environ.get(str(name), default)


def win32_get_last_error() -> int:
    """Return the last Win32 error code for the calling thread (GetLastError).

    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import ctypes
    return ctypes.windll.kernel32.GetLastError()


def win32_error_message(error_code: int) -> str:
    """Return a human-readable message for a Win32 error code.

    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import ctypes
    buf = ctypes.create_unicode_buffer(256)
    ctypes.windll.kernel32.FormatMessageW(
        0x1000, None, int(error_code), 0, buf, 256, None
    )
    return buf.value.strip()


def win32_get_version() -> dict:
    """Return the Windows version as a dict with major, minor, build keys.

    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import ctypes
    v = sys.getwindowsversion()
    return {
        'major': v.major,
        'minor': v.minor,
        'build': v.build,
        'platform': v.platform,
        'service_pack': v.service_pack,
    }


def win32_expand_env_strings(path: str) -> str:
    """Expand environment variable references in path (e.g. %APPDATA%).

    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import ctypes
    buf = ctypes.create_unicode_buffer(32767)
    ctypes.windll.kernel32.ExpandEnvironmentStringsW(str(path), buf, 32767)
    return buf.value


def win32_get_special_folder(csidl: int) -> str:
    """Return the path of a special shell folder by CSIDL constant.

    Common CSIDL values: 5=Desktop, 8=Documents, 26=AppData, 28=LocalAppData.
    Raises PlatformError on non-Windows.
    """
    _require_windows()
    import ctypes
    buf = ctypes.create_unicode_buffer(260)
    ctypes.windll.shell32.SHGetFolderPathW(None, int(csidl), None, 0, buf)
    return buf.value


# ---------------------------------------------------------------------------
# Windows Services API
# ---------------------------------------------------------------------------

def _run_sc(*args) -> dict:
    """Execute sc.exe with the given arguments. Returns output dict."""
    try:
        result = subprocess.run(
            ['sc'] + list(args),
            capture_output=True, text=True, timeout=15,
            encoding='utf-8', errors='replace'
        )
        return {
            'success': result.returncode == 0,
            'output': result.stdout.strip(),
            'error': result.stderr.strip() if result.returncode != 0 else '',
            'returncode': result.returncode,
        }
    except FileNotFoundError:
        return {'success': False, 'error': 'sc.exe not found', 'output': '',
                'returncode': -1}
    except subprocess.TimeoutExpired:
        return {'success': False, 'error': 'timeout', 'output': '', 'returncode': -1}


def winservice_query_status(service_name: str) -> dict:
    """Query the current status of a Windows service.

    Returns a dict with keys: status (str), success (bool), raw output.
    Common status values: 'RUNNING', 'STOPPED', 'PAUSED', 'START_PENDING', etc.
    Raises PlatformError on non-Windows.
    """
    _require_windows()
    r = _run_sc('query', str(service_name))
    status = ''
    for line in r.get('output', '').splitlines():
        if 'STATE' in line and ':' in line:
            parts = line.split(':')
            if len(parts) >= 2:
                status_part = parts[1].strip()
                tokens = status_part.split()
                if len(tokens) >= 2:
                    status = tokens[1]
                elif tokens:
                    status = tokens[0]
    return {
        'success': r['success'],
        'status': status,
        'output': r.get('output', ''),
        'error': r.get('error', ''),
    }


def winservice_start(service_name: str) -> dict:
    """Start a Windows service by name.

    Returns dict with 'success', 'output', 'error'.
    Raises PlatformError on non-Windows.
    """
    _require_windows()
    return _run_sc('start', str(service_name))


def winservice_stop(service_name: str) -> dict:
    """Stop a Windows service by name.

    Returns dict with 'success', 'output', 'error'.
    Raises PlatformError on non-Windows.
    """
    _require_windows()
    return _run_sc('stop', str(service_name))


def winservice_pause(service_name: str) -> dict:
    """Pause a Windows service by name.

    Returns dict with 'success', 'output', 'error'.
    Raises PlatformError on non-Windows.
    """
    _require_windows()
    return _run_sc('pause', str(service_name))


def winservice_continue(service_name: str) -> dict:
    """Continue (resume) a paused Windows service by name.

    Returns dict with 'success', 'output', 'error'.
    Raises PlatformError on non-Windows.
    """
    _require_windows()
    return _run_sc('continue', str(service_name))


def winservice_list() -> list:
    """List all installed Windows services.

    Returns a list of dicts with keys: name, display_name, state.
    Falls back to empty list with error dict if sc.exe unavailable.
    Raises PlatformError on non-Windows.
    """
    _require_windows()
    r = _run_sc('query', 'type=', 'all', 'state=', 'all')
    if not r['success'] and not r['output']:
        return [{'error': r.get('error', 'sc.exe failed')}]
    services = []
    current: Dict[str, str] = {}
    for line in r.get('output', '').splitlines():
        line = line.strip()
        if line.startswith('SERVICE_NAME:'):
            if current:
                services.append(current)
            current = {'name': line.split(':', 1)[1].strip(),
                       'display_name': '', 'state': ''}
        elif line.startswith('DISPLAY_NAME:') and current:
            current['display_name'] = line.split(':', 1)[1].strip()
        elif 'STATE' in line and ':' in line and current:
            tokens = line.split(':', 1)[1].strip().split()
            if len(tokens) >= 2:
                current['state'] = tokens[1]
            elif tokens:
                current['state'] = tokens[0]
    if current:
        services.append(current)
    return services


# ---------------------------------------------------------------------------
# COM (Component Object Model)
# ---------------------------------------------------------------------------

def com_create_object(prog_id: str) -> Any:
    """Create and return a COM object by ProgID string (e.g. 'Word.Application').

    Returns the COM dispatch object on success, or a dict with 'error' on failure.
    Requires 'pywin32' (win32com) to be installed.
    Raises PlatformError on non-Windows.
    """
    _require_windows()
    try:
        import win32com.client as _com  # type: ignore
        return _com.Dispatch(str(prog_id))
    except ImportError:
        return {'error': "pywin32 is required for COM support. Install with: pip install pywin32"}
    except Exception as e:
        return {'error': str(e)}


def com_call_method(com_obj: Any, method_name: str, *args) -> Any:
    """Call a named method on a COM dispatch object.

    com_obj:     object returned by com_create_object.
    method_name: method name to call.
    args:        positional arguments for the method.

    Returns the method's return value, or a dict with 'error' on failure.
    Raises PlatformError on non-Windows.
    """
    _require_windows()
    try:
        method = getattr(com_obj, str(method_name))
        return method(*args)
    except AttributeError:
        return {'error': f"COM object has no method '{method_name}'"}
    except Exception as e:
        return {'error': str(e)}


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

def register_platform_windows_functions(runtime) -> None:
    """Register all Windows platform functions with the NexusLang runtime."""

    # Registry constants
    for _name, _val in [
        ('HKEY_LOCAL_MACHINE', HKEY_LOCAL_MACHINE),
        ('HKEY_CURRENT_USER', HKEY_CURRENT_USER),
        ('HKEY_CLASSES_ROOT', HKEY_CLASSES_ROOT),
        ('HKEY_USERS', HKEY_USERS),
        ('HKEY_CURRENT_CONFIG', HKEY_CURRENT_CONFIG),
        ('REG_SZ', REG_SZ), ('REG_EXPAND_SZ', REG_EXPAND_SZ),
        ('REG_BINARY', REG_BINARY), ('REG_DWORD', REG_DWORD),
        ('REG_MULTI_SZ', REG_MULTI_SZ), ('REG_QWORD', REG_QWORD),
    ]:
        _n, _v = _name, _val
        runtime.register_function(_n, lambda rt, v=_v: v)

    # Registry API
    runtime.register_function("registry_read",        lambda rt, *a: registry_read(*a))
    runtime.register_function("registry_write",       lambda rt, *a: registry_write(*a))
    runtime.register_function("registry_delete",      lambda rt, *a: registry_delete(*a))
    runtime.register_function("registry_delete_key",  lambda rt, *a: registry_delete_key(*a))
    runtime.register_function("registry_list_subkeys", lambda rt, *a: registry_list_subkeys(*a))
    runtime.register_function("registry_list_values", lambda rt, *a: registry_list_values(*a))
    runtime.register_function("registry_key_exists",  lambda rt, *a: registry_key_exists(*a))

    # Win32 API
    runtime.register_function("win32_computer_name",    lambda rt: win32_computer_name())
    runtime.register_function("win32_username",         lambda rt: win32_username())
    runtime.register_function("win32_is_admin",         lambda rt: win32_is_admin())
    runtime.register_function("win32_message_box",      lambda rt, *a: win32_message_box(*a))
    runtime.register_function("win32_get_windows_dir",  lambda rt: win32_get_windows_dir())
    runtime.register_function("win32_get_system_dir",   lambda rt: win32_get_system_dir())
    runtime.register_function("win32_get_temp_dir",     lambda rt: win32_get_temp_dir())
    runtime.register_function("win32_get_env",          lambda rt, *a: win32_get_env(*a))
    runtime.register_function("win32_get_last_error",   lambda rt: win32_get_last_error())
    runtime.register_function("win32_error_message",    lambda rt, c: win32_error_message(c))
    runtime.register_function("win32_get_version",      lambda rt: win32_get_version())
    runtime.register_function("win32_expand_env_strings", lambda rt, p: win32_expand_env_strings(p))
    runtime.register_function("win32_get_special_folder", lambda rt, c: win32_get_special_folder(c))

    # Services API
    runtime.register_function("winservice_query_status", lambda rt, n: winservice_query_status(n))
    runtime.register_function("winservice_start",        lambda rt, n: winservice_start(n))
    runtime.register_function("winservice_stop",         lambda rt, n: winservice_stop(n))
    runtime.register_function("winservice_pause",        lambda rt, n: winservice_pause(n))
    runtime.register_function("winservice_continue",     lambda rt, n: winservice_continue(n))
    runtime.register_function("winservice_list",         lambda rt: winservice_list())

    # COM API
    runtime.register_function("com_create_object", lambda rt, pid: com_create_object(pid))
    runtime.register_function("com_call_method",   lambda rt, *a: com_call_method(*a))
