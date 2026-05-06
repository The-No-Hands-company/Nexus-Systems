"""
Subprocess execution for NexusLang.
"""

import subprocess
import shlex
from typing import Optional, List, Dict
from ...runtime.runtime import Runtime


class ProcessResult:
    """Result of subprocess execution."""
    def __init__(self, returncode: int, stdout: str, stderr: str):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr
        self.success = returncode == 0


def run_command(command: str, shell: bool = True, timeout: Optional[int] = None,
                cwd: Optional[str] = None, env: Optional[Dict] = None) -> ProcessResult:
    """
    Run shell command and return result.
    """
    try:
        result = subprocess.run(
            command,
            shell=shell,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=cwd,
            env=env
        )
        return ProcessResult(result.returncode, result.stdout, result.stderr)
    except subprocess.TimeoutExpired:
        return ProcessResult(-1, "", f"Command timed out after {timeout}s")
    except Exception as e:
        return ProcessResult(-1, "", str(e))


def run_command_list(args: List[str], timeout: Optional[int] = None,
                     cwd: Optional[str] = None, env: Optional[Dict] = None) -> ProcessResult:
    """
    Run command from argument list (safer than shell=True).
    """
    try:
        result = subprocess.run(
            args,
            shell=False,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=cwd,
            env=env
        )
        return ProcessResult(result.returncode, result.stdout, result.stderr)
    except subprocess.TimeoutExpired:
        return ProcessResult(-1, "", f"Command timed out after {timeout}s")
    except Exception as e:
        return ProcessResult(-1, "", str(e))


def get_output(command: str, shell: bool = True) -> str:
    """Run command and return stdout."""
    result = run_command(command, shell=shell)
    return result.stdout


def get_status(command: str, shell: bool = True) -> int:
    """Run command and return exit code."""
    result = run_command(command, shell=shell)
    return result.returncode


def check_command(command: str, shell: bool = True) -> bool:
    """Check if command succeeds (returns True if exit code is 0)."""
    result = run_command(command, shell=shell)
    return result.success


def which(program: str) -> Optional[str]:
    """Find program in PATH (like Unix 'which' command)."""
    import shutil
    return shutil.which(program)


def split_command(command: str) -> List[str]:
    """Split shell command into argument list (respects quotes)."""
    return shlex.split(command)


def join_command(args: List[str]) -> str:
    """Join argument list into shell command (adds quotes as needed)."""
    return shlex.join(args)


def get_result_stdout(result: ProcessResult) -> str:
    """Get stdout from ProcessResult."""
    return result.stdout


def get_result_stderr(result: ProcessResult) -> str:
    """Get stderr from ProcessResult."""
    return result.stderr


def get_result_returncode(result: ProcessResult) -> int:
    """Get return code from ProcessResult."""
    return result.returncode


def is_result_success(result: ProcessResult) -> bool:
    """Check if ProcessResult indicates success."""
    return result.success


def register_subprocess_functions(runtime: Runtime) -> None:
    """Register subprocess functions with the runtime."""
    
    # Command execution
    runtime.register_function("run_command", run_command)
    runtime.register_function("run_command_list", run_command_list)
    runtime.register_function("get_output", get_output)
    runtime.register_function("get_status", get_status)
    runtime.register_function("check_command", check_command)
    
    # Command utilities
    runtime.register_function("which", which)
    runtime.register_function("split_command", split_command)
    runtime.register_function("join_command", join_command)
    
    # Result accessors
    runtime.register_function("get_result_stdout", get_result_stdout)
    runtime.register_function("get_result_stderr", get_result_stderr)
    runtime.register_function("get_result_returncode", get_result_returncode)
    runtime.register_function("is_result_success", is_result_success)
    
    # Aliases
    runtime.register_function("shell", run_command)
    runtime.register_function("exec", run_command_list)
