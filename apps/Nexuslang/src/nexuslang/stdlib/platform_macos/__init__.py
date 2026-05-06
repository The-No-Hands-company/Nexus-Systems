"""
NLPL macOS Platform Library

Provides Foundation, CoreGraphics, Keychain, Defaults, and notification APIs
for macOS environments. All functions raise PlatformError on non-macOS platforms.

Groups:
  - Foundation / OS info (bundle path, directories, version, hostname, etc.)
  - CoreGraphics (screen metrics via system_profiler)
  - Keychain (get, set, delete via the 'security' CLI)
  - NSUserDefaults (read, write, delete, list via the 'defaults' CLI)
  - Notifications (post via osascript/AppleScript)
  - System utilities (pbcopy/pbpaste, open URL, say TTS, screencapture)
"""

import os
import sys
import json
import shutil
import subprocess
import tempfile
from typing import Any, Dict, List, Optional


# ---------------------------------------------------------------------------
# Platform guard
# ---------------------------------------------------------------------------

class PlatformError(Exception):
    """Raised when a macOS-specific API is called on a non-macOS platform."""


def _require_macos() -> None:
    if sys.platform != 'darwin':
        raise PlatformError(
            f"This function requires macOS (darwin); current platform is '{sys.platform}'."
        )


# ---------------------------------------------------------------------------
# Subprocess helper
# ---------------------------------------------------------------------------

def _run(args: List[str], input_text: Optional[str] = None,
         timeout: int = 15) -> dict:
    """Run a subprocess and return {'success', 'output', 'error', 'returncode'}."""
    try:
        result = subprocess.run(
            args, capture_output=True, text=True, timeout=timeout,
            input=input_text
        )
        return {
            'success': result.returncode == 0,
            'output': result.stdout.strip(),
            'error': result.stderr.strip(),
            'returncode': result.returncode,
        }
    except FileNotFoundError:
        return {'success': False, 'output': '',
                'error': f"Command not found: {args[0]}", 'returncode': -1}
    except subprocess.TimeoutExpired:
        return {'success': False, 'output': '',
                'error': 'timeout', 'returncode': -1}


# ---------------------------------------------------------------------------
# Foundation / OS info
# ---------------------------------------------------------------------------

def macos_version() -> dict:
    """Return the macOS version as a dict with keys: string, major, minor, patch.

    Raises PlatformError on non-macOS.
    """
    _require_macos()
    import platform
    v = platform.mac_ver()[0]   # e.g. '14.3.1'
    parts = v.split('.')
    return {
        'string': v,
        'major': int(parts[0]) if len(parts) > 0 else 0,
        'minor': int(parts[1]) if len(parts) > 1 else 0,
        'patch': int(parts[2]) if len(parts) > 2 else 0,
    }


def macos_hostname() -> str:
    """Return the local hostname of the Mac.

    Raises PlatformError on non-macOS.
    """
    _require_macos()
    import socket
    return socket.gethostname()


def macos_username() -> str:
    """Return the short login name of the current user.

    Raises PlatformError on non-macOS.
    """
    _require_macos()
    return os.environ.get('USER', os.environ.get('LOGNAME', ''))


def macos_home_dir() -> str:
    """Return the current user's home directory path (e.g. /Users/alice).

    Raises PlatformError on non-macOS.
    """
    _require_macos()
    return os.path.expanduser('~')


def macos_temp_dir() -> str:
    """Return the temporary directory for the current user.

    On macOS this is typically /var/folders/... (a per-user secure temp dir).
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    return tempfile.gettempdir()


def macos_app_support_dir(app_name: Optional[str] = None) -> str:
    """Return the Application Support directory path.

    If app_name is provided, returns the app-specific subdirectory path
    (the directory is NOT created automatically).
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    base = os.path.join(os.path.expanduser('~'), 'Library', 'Application Support')
    if app_name:
        return os.path.join(base, str(app_name))
    return base


def macos_caches_dir(app_name: Optional[str] = None) -> str:
    """Return the Caches directory path (~/Library/Caches[/app_name]).

    Raises PlatformError on non-macOS.
    """
    _require_macos()
    base = os.path.join(os.path.expanduser('~'), 'Library', 'Caches')
    if app_name:
        return os.path.join(base, str(app_name))
    return base


def macos_bundle_path() -> str:
    """Return the path to the .app bundle of the running process, if any.

    Returns the executable path if the process is not running inside a bundle.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    exe = os.path.abspath(sys.executable)
    # Walk up to find the .app bundle root
    parts = exe.split(os.sep)
    for i in range(len(parts) - 1, 0, -1):
        if parts[i].endswith('.app'):
            return os.sep.join(parts[:i + 1])
    return exe


def macos_documents_dir() -> str:
    """Return the path to the user's Documents folder.

    Raises PlatformError on non-macOS.
    """
    _require_macos()
    return os.path.join(os.path.expanduser('~'), 'Documents')


def macos_desktop_dir() -> str:
    """Return the path to the user's Desktop folder.

    Raises PlatformError on non-macOS.
    """
    _require_macos()
    return os.path.join(os.path.expanduser('~'), 'Desktop')


def macos_downloads_dir() -> str:
    """Return the path to the user's Downloads folder.

    Raises PlatformError on non-macOS.
    """
    _require_macos()
    return os.path.join(os.path.expanduser('~'), 'Downloads')


# ---------------------------------------------------------------------------
# CoreGraphics: display / screen information
# ---------------------------------------------------------------------------

def macos_screen_size() -> dict:
    """Return the resolution of the primary display.

    Returns dict {'width': int, 'height': int} on success, or {'error': ...}.
    Uses 'system_profiler SPDisplaysDataType -json'.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    r = _run(['system_profiler', 'SPDisplaysDataType', '-json'])
    if not r['success']:
        # Fallback: try screenresolution via osascript
        r2 = _run(['osascript', '-e',
                   'tell application "Finder" to get bounds of window of desktop'])
        if r2['success']:
            parts = r2['output'].split(',')
            if len(parts) == 4:
                try:
                    return {'width': int(parts[2].strip()),
                            'height': int(parts[3].strip())}
                except ValueError:
                    pass
        return {'error': r.get('error', 'system_profiler failed')}
    try:
        data = json.loads(r['output'])
        displays = data.get('SPDisplaysDataType', [])
        for d in displays:
            res = d.get('spdisplays_resolution', '')
            if res:
                parts = res.replace(' ', '').split('x')
                if len(parts) == 2:
                    return {'width': int(parts[0]), 'height': int(parts[1])}
    except (json.JSONDecodeError, KeyError, ValueError):
        pass
    return {'error': 'Could not parse display resolution from system_profiler output'}


def macos_display_count() -> int:
    """Return the number of connected displays.

    Returns -1 if the count cannot be determined.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    r = _run(['system_profiler', 'SPDisplaysDataType', '-json'])
    if not r['success']:
        return -1
    try:
        data = json.loads(r['output'])
        return len(data.get('SPDisplaysDataType', []))
    except (json.JSONDecodeError, KeyError):
        return -1


def macos_display_info() -> list:
    """Return a list of dicts describing each connected display.

    Each dict contains: name, resolution, display_type, pixels_per_inch, vendor.
    Returns [{'error': message}] if the information cannot be retrieved.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    r = _run(['system_profiler', 'SPDisplaysDataType', '-json'])
    if not r['success']:
        return [{'error': r.get('error', 'system_profiler failed')}]
    try:
        data = json.loads(r['output'])
        displays = data.get('SPDisplaysDataType', [])
        result = []
        for d in displays:
            result.append({
                'name': d.get('_name', ''),
                'resolution': d.get('spdisplays_resolution', ''),
                'display_type': d.get('spdisplays_display_type', ''),
                'pixels_per_inch': d.get('spdisplays_pixels_per_inch', ''),
                'vendor': d.get('spdisplays_vendor', ''),
            })
        return result
    except (json.JSONDecodeError, KeyError):
        return [{'error': 'Could not parse display info'}]


# ---------------------------------------------------------------------------
# Keychain (via 'security' CLI)
# ---------------------------------------------------------------------------

def macos_keychain_get(service: str, account: str) -> Optional[str]:
    """Retrieve a generic password from the macOS Keychain.

    Returns the password string on success, None if not found,
    or {'error': message} on other failures.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    r = _run([
        'security', 'find-generic-password',
        '-s', str(service),
        '-a', str(account),
        '-w'
    ])
    if r['returncode'] == 44:    # errSecItemNotFound
        return None
    if not r['success']:
        return {'error': r.get('error') or r.get('output', 'keychain read failed')}
    return r['output']


def macos_keychain_set(service: str, account: str, password: str,
                       label: Optional[str] = None) -> dict:
    """Store a generic password in the macOS Keychain.

    Creates or updates the entry for (service, account).
    label: human-readable label shown in Keychain Access (defaults to service).
    Returns {'success': True} or {'error': message}.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    # Delete existing entry first to allow update (security add errors on duplicate)
    _run(['security', 'delete-generic-password', '-s', str(service), '-a', str(account)])
    args = [
        'security', 'add-generic-password',
        '-s', str(service),
        '-a', str(account),
        '-w', str(password),
        '-l', str(label) if label else str(service),
        '-U'   # update if already exists
    ]
    r = _run(args)
    if r['success']:
        return {'success': True}
    return {'error': r.get('error') or r.get('output', 'keychain write failed')}


def macos_keychain_delete(service: str, account: str) -> dict:
    """Delete a generic password from the macOS Keychain.

    Returns {'success': True} if deleted (or not found), {'error': ...} otherwise.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    r = _run([
        'security', 'delete-generic-password',
        '-s', str(service),
        '-a', str(account)
    ])
    if r['success'] or r['returncode'] == 44:   # 44 = not found, treat as success
        return {'success': True}
    return {'error': r.get('error') or r.get('output', 'keychain delete failed')}


def macos_keychain_find_internet(server: str, account: str,
                                 protocol: Optional[str] = None) -> Optional[str]:
    """Retrieve an internet password from the macOS Keychain.

    Returns the password string, None if not found, or {'error': ...} on failure.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    args = ['security', 'find-internet-password', '-s', str(server),
            '-a', str(account), '-w']
    if protocol:
        args += ['-r', str(protocol)]
    r = _run(args)
    if r['returncode'] == 44:
        return None
    if not r['success']:
        return {'error': r.get('error') or r.get('output', 'keychain read failed')}
    return r['output']


# ---------------------------------------------------------------------------
# NSUserDefaults (via 'defaults' CLI)
# ---------------------------------------------------------------------------

def macos_defaults_read(domain: str, key: Optional[str] = None) -> Any:
    """Read a value from NSUserDefaults for domain.

    If key is omitted, returns all defaults for the domain as a dict.
    Returns None if the key does not exist, or {'error': ...} on failure.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    args = ['defaults', 'read', str(domain)]
    if key:
        args.append(str(key))
    r = _run(args)
    if not r['success']:
        # Check if it's simply 'not found' (exit 1 + empty/specific error)
        err = r.get('error', '').lower()
        if 'does not exist' in err or 'no such pair' in err:
            return None
        return {'error': r.get('error', 'defaults read failed')}
    output = r['output']
    # Try to parse as JSON-like plist text
    # 'defaults read' returns plist format; attempt basic type conversions
    if output in ('1', 'YES', 'true'):
        return True
    if output in ('0', 'NO', 'false'):
        return False
    try:
        return int(output)
    except ValueError:
        pass
    try:
        return float(output)
    except ValueError:
        pass
    return output


def macos_defaults_write(domain: str, key: str, value: Any,
                         value_type: Optional[str] = None) -> dict:
    """Write a value to NSUserDefaults for domain.

    value_type: explicit type tag ('string', 'bool', 'int', 'float', 'array', 'dict').
    If omitted, type is inferred from the Python value type.
    Returns {'success': True} or {'error': message}.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    if value_type is None:
        if isinstance(value, bool):
            value_type = 'bool'
        elif isinstance(value, int):
            value_type = 'int'
        elif isinstance(value, float):
            value_type = 'float'
        else:
            value_type = 'string'
    type_flag = {
        'string': '-string', 'str': '-string',
        'bool': '-bool', 'boolean': '-bool',
        'int': '-integer', 'integer': '-integer',
        'float': '-float',
        'array': '-array', 'dict': '-dict',
    }.get(str(value_type).lower(), '-string')
    val_str = str(value).lower() if isinstance(value, bool) else str(value)
    args = ['defaults', 'write', str(domain), str(key), type_flag, val_str]
    r = _run(args)
    if r['success']:
        return {'success': True}
    return {'error': r.get('error', 'defaults write failed')}


def macos_defaults_delete(domain: str, key: Optional[str] = None) -> dict:
    """Delete a key (or all defaults) for domain.

    If key is omitted, all defaults for the domain are deleted.
    Returns {'success': True} or {'error': message}.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    args = ['defaults', 'delete', str(domain)]
    if key:
        args.append(str(key))
    r = _run(args)
    if r['success']:
        return {'success': True}
    return {'error': r.get('error', 'defaults delete failed')}


def macos_defaults_list_domains() -> list:
    """Return a list of all known NSUserDefaults domains.

    Returns a list of domain strings, or [{'error': ...}] on failure.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    r = _run(['defaults', 'domains'])
    if not r['success']:
        return [{'error': r.get('error', 'defaults domains failed')}]
    # Output is comma-separated on one line
    domains = [d.strip() for d in r['output'].split(',') if d.strip()]
    return domains


def macos_defaults_find(key: str) -> list:
    """Search all defaults databases for key.

    Returns raw output as a list of lines, or [{'error': ...}] on failure.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    r = _run(['defaults', 'find', str(key)])
    if not r['success'] and not r['output']:
        return [{'error': r.get('error', 'defaults find failed')}]
    return r['output'].splitlines()


# ---------------------------------------------------------------------------
# Notifications (via osascript)
# ---------------------------------------------------------------------------

def macos_post_notification(title: str, message: str,
                             subtitle: Optional[str] = None,
                             sound_name: Optional[str] = None) -> dict:
    """Post a macOS user notification via AppleScript/osascript.

    title:      notification title (shown in bold).
    message:    notification body text.
    subtitle:   optional subtitle line.
    sound_name: optional sound name (e.g. 'Ping', 'Basso', 'Submarine').

    Returns {'success': True} or {'error': message}.
    Raises PlatformError on non-macOS.
    """
    _require_macos()

    def _esc(s: str) -> str:
        return s.replace('\\', '\\\\').replace('"', '\\"')

    script = f'display notification "{_esc(str(message))}" with title "{_esc(str(title))}"'
    if subtitle:
        script += f' subtitle "{_esc(str(subtitle))}"'
    if sound_name:
        script += f' sound name "{_esc(str(sound_name))}"'
    r = _run(['osascript', '-e', script])
    if r['success']:
        return {'success': True}
    return {'error': r.get('error', 'osascript notification failed')}


# ---------------------------------------------------------------------------
# Clipboard (pbcopy / pbpaste)
# ---------------------------------------------------------------------------

def macos_clipboard_get() -> str:
    """Return the current text content of the macOS clipboard (pasteboard).

    Returns a string, or {'error': message} on failure.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    r = _run(['pbpaste'])
    if not r['success'] and r['returncode'] != 0:
        return {'error': r.get('error', 'pbpaste failed')}
    # pbpaste exits 0 but may write nothing for non-text content
    return r['output']


def macos_clipboard_set(text: str) -> dict:
    """Write text to the macOS clipboard (pasteboard).

    Returns {'success': True} or {'error': message}.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    r = _run(['pbcopy'], input_text=str(text))
    if r['success']:
        return {'success': True}
    return {'error': r.get('error', 'pbcopy failed')}


# ---------------------------------------------------------------------------
# Miscellaneous system utilities
# ---------------------------------------------------------------------------

def macos_open_url(url: str) -> dict:
    """Open a URL or file path using the default application (via 'open').

    Returns {'success': True} or {'error': message}.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    r = _run(['open', str(url)])
    if r['success']:
        return {'success': True}
    return {'error': r.get('error', 'open command failed')}


def macos_say(text: str, voice: Optional[str] = None,
              rate: Optional[int] = None) -> dict:
    """Speak text using macOS text-to-speech ('say' command).

    voice: voice name (e.g. 'Alex', 'Karen', 'Samantha').
    rate:  words per minute (default is system default, typically 175-200).
    Returns {'success': True} or {'error': message}.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    args = ['say', str(text)]
    if voice:
        args += ['-v', str(voice)]
    if rate is not None:
        args += ['-r', str(int(rate))]
    r = _run(args)
    if r['success']:
        return {'success': True}
    return {'error': r.get('error', 'say command failed')}


def macos_screencapture(output_path: str, interactive: bool = False,
                        display_id: Optional[int] = None) -> dict:
    """Capture the screen to a file using 'screencapture'.

    output_path: path for the saved screenshot (PNG, PDF, TIFF, etc.).
    interactive: if True, let the user select a region (-i flag).
    display_id:  capture a specific display index (-D flag).
    Returns {'success': True, 'path': output_path} or {'error': message}.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    args = ['screencapture']
    if interactive:
        args.append('-i')
    if display_id is not None:
        args += ['-D', str(int(display_id))]
    args.append(str(output_path))
    r = _run(args)
    if r['success']:
        return {'success': True, 'path': str(output_path)}
    return {'error': r.get('error', 'screencapture failed')}


def macos_system_info() -> dict:
    """Return basic macOS hardware and software information.

    Uses 'system_profiler SPHardwareDataType SPSoftwareDataType -json'.
    Returns a dict with keys: os_version, build_version, model, cpu, memory, serial.
    Returns {'error': message} on failure.
    Raises PlatformError on non-macOS.
    """
    _require_macos()
    r = _run(['system_profiler', 'SPHardwareDataType', 'SPSoftwareDataType', '-json'])
    if not r['success']:
        return {'error': r.get('error', 'system_profiler failed')}
    try:
        data = json.loads(r['output'])
        hw = data.get('SPHardwareDataType', [{}])[0]
        sw = data.get('SPSoftwareDataType', [{}])[0]
        return {
            'os_version': sw.get('os_version', ''),
            'build_version': sw.get('kernel_version', ''),
            'model': hw.get('machine_name', ''),
            'cpu': hw.get('cpu_type', ''),
            'memory': hw.get('physical_memory', ''),
            'serial': hw.get('serial_number', ''),
            'uptime': sw.get('uptime', ''),
        }
    except (json.JSONDecodeError, KeyError, IndexError):
        return {'error': 'Could not parse system_profiler JSON output'}


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

def register_platform_macos_functions(runtime) -> None:
    """Register all macOS platform functions with the NexusLang runtime."""

    # Foundation / OS info
    runtime.register_function("macos_version",        lambda rt: macos_version())
    runtime.register_function("macos_hostname",       lambda rt: macos_hostname())
    runtime.register_function("macos_username",       lambda rt: macos_username())
    runtime.register_function("macos_home_dir",       lambda rt: macos_home_dir())
    runtime.register_function("macos_temp_dir",       lambda rt: macos_temp_dir())
    runtime.register_function("macos_app_support_dir", lambda rt, *a: macos_app_support_dir(*a))
    runtime.register_function("macos_caches_dir",     lambda rt, *a: macos_caches_dir(*a))
    runtime.register_function("macos_bundle_path",    lambda rt: macos_bundle_path())
    runtime.register_function("macos_documents_dir",  lambda rt: macos_documents_dir())
    runtime.register_function("macos_desktop_dir",    lambda rt: macos_desktop_dir())
    runtime.register_function("macos_downloads_dir",  lambda rt: macos_downloads_dir())
    runtime.register_function("macos_system_info",    lambda rt: macos_system_info())

    # CoreGraphics / display
    runtime.register_function("macos_screen_size",    lambda rt: macos_screen_size())
    runtime.register_function("macos_display_count",  lambda rt: macos_display_count())
    runtime.register_function("macos_display_info",   lambda rt: macos_display_info())

    # Keychain
    runtime.register_function("macos_keychain_get",          lambda rt, *a: macos_keychain_get(*a))
    runtime.register_function("macos_keychain_set",          lambda rt, *a: macos_keychain_set(*a))
    runtime.register_function("macos_keychain_delete",       lambda rt, *a: macos_keychain_delete(*a))
    runtime.register_function("macos_keychain_find_internet", lambda rt, *a: macos_keychain_find_internet(*a))

    # NSUserDefaults
    runtime.register_function("macos_defaults_read",         lambda rt, *a: macos_defaults_read(*a))
    runtime.register_function("macos_defaults_write",        lambda rt, *a: macos_defaults_write(*a))
    runtime.register_function("macos_defaults_delete",       lambda rt, *a: macos_defaults_delete(*a))
    runtime.register_function("macos_defaults_list_domains", lambda rt: macos_defaults_list_domains())
    runtime.register_function("macos_defaults_find",         lambda rt, k: macos_defaults_find(k))

    # Notifications
    runtime.register_function("macos_post_notification", lambda rt, *a: macos_post_notification(*a))

    # Clipboard
    runtime.register_function("macos_clipboard_get", lambda rt: macos_clipboard_get())
    runtime.register_function("macos_clipboard_set", lambda rt, t: macos_clipboard_set(t))

    # System utilities
    runtime.register_function("macos_open_url",      lambda rt, u: macos_open_url(u))
    runtime.register_function("macos_say",            lambda rt, *a: macos_say(*a))
    runtime.register_function("macos_screencapture",  lambda rt, *a: macos_screencapture(*a))
