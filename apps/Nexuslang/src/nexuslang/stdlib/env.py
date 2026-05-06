"""
Environment variables module for the NexusLang standard library.
Provides functions for reading, writing, and querying environment variables.
"""

import os
import socket
import sys
from ..runtime.runtime import Runtime


def register_env_functions(runtime: Runtime) -> None:
    """Register environment variable functions with the runtime."""
    runtime.register_function("env_get", env_get)
    runtime.register_function("env_set", env_set)
    runtime.register_function("env_unset", env_unset)
    runtime.register_function("env_has", env_has)
    runtime.register_function("env_all", env_all)
    runtime.register_function("env_expand", env_expand)

    # Backward-compatible aliases and process/system helpers.
    runtime.register_function("get_env", get_env)
    runtime.register_function("set_env", set_env)
    runtime.register_function("unset_env", unset_env)
    runtime.register_function("has_env", has_env)
    runtime.register_function("list_env", list_env)
    runtime.register_function("get_platform", get_platform)
    runtime.register_function("get_python_version", get_python_version)
    runtime.register_function("get_executable", get_executable)
    runtime.register_function("get_path_separator", get_path_separator)
    runtime.register_function("get_line_separator", get_line_separator)
    runtime.register_function("get_user", get_user)
    runtime.register_function("get_hostname", get_hostname)
    runtime.register_function("get_cpu_count", get_cpu_count)
    runtime.register_function("get_argv", get_argv)
    runtime.register_function("exit_program", exit_program)

    runtime.register_function("getenv", get_env)
    runtime.register_function("setenv", set_env)
    runtime.register_function("platform", get_platform)
    runtime.register_function("exit", exit_program)


def env_get(name, default=None):
    """Return the value of environment variable *name*, or *default* if unset."""
    return os.environ.get(str(name), default)


def env_set(name, value):
    """Set environment variable *name* to *value*."""
    os.environ[str(name)] = str(value)
    return True


def env_unset(name):
    """Remove environment variable *name* if it exists."""
    os.environ.pop(str(name), None)
    return True


def env_has(name):
    """Return True if environment variable *name* is set."""
    return str(name) in os.environ


def env_all():
    """Return a copy of the current environment as a plain dictionary."""
    return dict(os.environ)


def env_expand(template):
    """Expand environment variable references (``$VAR`` / ``${VAR}``) in *template*."""
    return os.path.expandvars(str(template))


def get_env(name, default=None):
    """Alias for env_get."""
    return env_get(name, default)


def set_env(name, value):
    """Alias for env_set."""
    return env_set(name, value)


def unset_env(name):
    """Alias for env_unset."""
    return env_unset(name)


def has_env(name):
    """Alias for env_has."""
    return env_has(name)


def list_env():
    """Alias for env_all."""
    return env_all()


def get_platform():
    """Get platform name (linux, darwin, win32, etc.)."""
    return sys.platform


def get_python_version():
    """Get Python version."""
    return sys.version


def get_executable():
    """Get Python executable path."""
    return sys.executable


def get_path_separator():
    """Get OS path separator (: on Unix, ; on Windows)."""
    return os.pathsep


def get_line_separator():
    """Get OS line separator."""
    return os.linesep


def get_user():
    """Get current user name."""
    return os.environ.get("USER") or os.environ.get("USERNAME")


def get_hostname():
    """Get hostname."""
    return socket.gethostname()


def get_cpu_count():
    """Get number of CPU cores."""
    return os.cpu_count() or 1


def get_argv():
    """Get command line arguments."""
    return sys.argv


def exit_program(code=0):
    """Exit program with status code."""
    sys.exit(int(code))
