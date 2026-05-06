"""
System module for the NexusLang standard library.
This module provides system-related functions and utilities.
"""

import os
import sys
import platform
import time
import datetime
import subprocess
from ...runtime.runtime import Runtime

def register_system_functions(runtime: Runtime) -> None:
    """Register system functions with the runtime."""
    # System information
    runtime.register_function("get_os_name", get_os_name)
    runtime.register_function("get_os_version", get_os_version)
    runtime.register_function("get_platform", get_platform)
    runtime.register_function("get_python_version", get_python_version)
    runtime.register_function("get_hostname", get_hostname)
    runtime.register_function("get_username", get_username)
    
    # Environment variables
    runtime.register_function("get_env", get_env)
    runtime.register_function("set_env", set_env)
    runtime.register_function("list_env", list_env)
    
    # Process management
    runtime.register_function("execute_command", execute_command)
    runtime.register_function("get_process_id", get_process_id)
    runtime.register_function("exit_program", exit_program)
    
    # Time and date
    runtime.register_function("get_current_time", get_current_time)
    runtime.register_function("get_current_date", get_current_date)
    runtime.register_function("format_time", format_time)
    runtime.register_function("format_date", format_date)
    runtime.register_function("sleep", sleep)
    runtime.register_function("timestamp", timestamp)

# System information functions
def get_os_name():
    """Get the name of the operating system."""
    return platform.system()

def get_os_version():
    """Get the version of the operating system."""
    return platform.version()

def get_platform():
    """Get the platform information."""
    return platform.platform()

def get_python_version():
    """Get the Python version."""
    return platform.python_version()

def get_hostname():
    """Get the hostname of the machine."""
    return platform.node()

def get_username():
    """Get the username of the current user."""
    import getpass
    return getpass.getuser()

# Environment variable functions
def get_env(name, default=None):
    """Get the value of an environment variable."""
    return os.environ.get(name, default)

def set_env(name, value):
    """Set the value of an environment variable."""
    os.environ[name] = str(value)
    return True

def list_env():
    """List all environment variables."""
    return dict(os.environ)

# Process management functions
def execute_command(command, shell=True, capture_output=True):
    """Execute a shell command and return the output."""
    try:
        result = subprocess.run(
            command,
            shell=shell,
            capture_output=capture_output,
            text=True,
            check=False
        )
        
        if capture_output:
            return {
                "stdout": result.stdout,
                "stderr": result.stderr,
                "returncode": result.returncode
            }
        return {"returncode": result.returncode}
    except Exception as e:
        return {"error": str(e), "returncode": -1}

def get_process_id():
    """Get the process ID of the current process."""
    return os.getpid()

def exit_program(code=0):
    """Exit the program with the given exit code."""
    sys.exit(code)

# Time and date functions
def get_current_time():
    """Get the current time as a string in HH:MM:SS format."""
    return time.strftime("%H:%M:%S")

def get_current_date():
    """Get the current date as a string in YYYY-MM-DD format."""
    return time.strftime("%Y-%m-%d")

def format_time(timestamp=None, format_string="%H:%M:%S"):
    """Format a timestamp as a time string."""
    if timestamp is None:
        timestamp = time.time()
    return time.strftime(format_string, time.localtime(timestamp))

def format_date(timestamp=None, format_string="%Y-%m-%d"):
    """Format a timestamp as a date string."""
    if timestamp is None:
        timestamp = time.time()
    return time.strftime(format_string, time.localtime(timestamp))

def sleep(seconds):
    """Sleep for the specified number of seconds."""
    time.sleep(seconds)
    return None

def timestamp():
    """Get the current timestamp (seconds since epoch)."""
    return time.time() 