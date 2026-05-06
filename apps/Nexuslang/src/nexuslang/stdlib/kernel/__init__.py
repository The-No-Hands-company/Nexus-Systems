"""
OS Kernel Primitives standard library for NexusLang.

Exposes process management, virtual memory control, raw syscall invocation,
and CPU scheduler interaction.  All APIs degrade gracefully on platforms that
do not support the requested functionality (raises NxlRuntimeError or returns
a sentinel value rather than crashing).

This module is intentionally low-level and intended for use in system
programming, OS component development, and embedded applications where direct
kernel interaction is required.
"""
from __future__ import annotations

import os
import sys
import signal
import struct
import ctypes
import ctypes.util
import subprocess
import platform
from typing import Any, Dict, List, Optional, Tuple

_IS_LINUX = platform.system() == "Linux"
_IS_WINDOWS = platform.system() == "Windows"
_IS_MACOS = platform.system() == "Darwin"
_IS_POSIX = os.name == "posix"

# ---------------------------------------------------------------------------
# Errors
# ---------------------------------------------------------------------------

class KernelError(Exception):
    """Raised when a kernel primitive fails."""


def _require_posix(op: str) -> None:
    if not _IS_POSIX:
        raise KernelError(f"kernel.{op}: operation requires POSIX (Linux/macOS)")


def _require_linux(op: str) -> None:
    if not _IS_LINUX:
        raise KernelError(f"kernel.{op}: operation requires Linux")


# ---------------------------------------------------------------------------
# Process management
# ---------------------------------------------------------------------------

class NLPLProcess:
    """Represents a child process spawned by NexusLang."""

    def __init__(self, popen: subprocess.Popen) -> None:
        self._proc = popen

    @property
    def pid(self) -> int:
        return self._proc.pid

    def wait(self, timeout: Optional[float] = None) -> int:
        return self._proc.wait(timeout=timeout)

    def kill(self) -> None:
        self._proc.kill()

    def terminate(self) -> None:
        self._proc.terminate()

    def poll(self) -> Optional[int]:
        return self._proc.poll()

    def returncode(self) -> Optional[int]:
        return self._proc.returncode

    def stdout_read(self) -> str:
        if self._proc.stdout is None:
            return ""
        return self._proc.stdout.read().decode(errors="replace")

    def stdin_write(self, data: str) -> None:
        if self._proc.stdin is None:
            raise KernelError("process stdin is not a pipe")
        self._proc.stdin.write(data.encode())
        self._proc.stdin.flush()


def create_process(_runtime: Any, command: str,
                   args: Optional[list] = None,
                   env: Optional[dict] = None,
                   capture_stdout: bool = False,
                   capture_stderr: bool = False,
                   stdin_pipe: bool = False,
                   working_dir: Optional[str] = None) -> NLPLProcess:
    """Spawn a child process.  Returns an NLPLProcess handle."""
    cmd = [command] + (list(args) if args else [])
    env_map = dict(env) if env else None
    stdout = subprocess.PIPE if capture_stdout else None
    stderr = subprocess.PIPE if capture_stderr else None
    stdin = subprocess.PIPE if stdin_pipe else None
    cwd = str(working_dir) if working_dir else None
    proc = subprocess.Popen(cmd, env=env_map, stdout=stdout,
                            stderr=stderr, stdin=stdin, cwd=cwd)
    return NLPLProcess(proc)


def wait_process(_runtime: Any, process: NLPLProcess,
                 timeout_ms: Optional[int] = None) -> int:
    """Wait for process to exit; returns exit code."""
    timeout = (timeout_ms / 1000.0) if timeout_ms is not None else None
    return process.wait(timeout=timeout)


def kill_process(_runtime: Any, process: NLPLProcess) -> None:
    """Send SIGKILL to process."""
    process.kill()


def terminate_process(_runtime: Any, process: NLPLProcess) -> None:
    """Send SIGTERM to process."""
    process.terminate()


def process_pid(_runtime: Any, process: NLPLProcess) -> int:
    return process.pid


def process_exit_code(_runtime: Any, process: NLPLProcess) -> int:
    code = process.returncode()
    return code if code is not None else -1


def process_is_running(_runtime: Any, process: NLPLProcess) -> bool:
    return process.poll() is None


def process_stdout(_runtime: Any, process: NLPLProcess) -> str:
    return process.stdout_read()


def process_stdin_write(_runtime: Any, process: NLPLProcess, data: str) -> None:
    process.stdin_write(str(data))


def get_process_id(_runtime: Any) -> int:
    """Return the current process ID."""
    return os.getpid()


def get_parent_process_id(_runtime: Any) -> int:
    """Return the parent process ID."""
    if _IS_POSIX:
        return os.getppid()
    raise KernelError("kernel.get_parent_process_id: requires POSIX")


def create_pipe(_runtime: Any) -> Tuple[int, int]:
    """Create an OS-level pipe.  Returns (read_fd, write_fd)."""
    _require_posix("create_pipe")
    r, w = os.pipe()
    return (r, w)


def pipe_read(_runtime: Any, fd: int, size: int = 4096) -> bytes:
    """Read up to size bytes from a file descriptor."""
    return os.read(int(fd), int(size))


def pipe_write(_runtime: Any, fd: int, data: Any) -> int:
    """Write data to a file descriptor.  Returns bytes written."""
    if isinstance(data, str):
        data = data.encode()
    return os.write(int(fd), bytes(data))


def close_fd(_runtime: Any, fd: int) -> None:
    """Close a file descriptor."""
    os.close(int(fd))


def send_signal(_runtime: Any, pid: int, sig: int) -> None:
    """Send signal sig to process pid."""
    _require_posix("send_signal")
    os.kill(int(pid), int(sig))


# Signal constants exposed to NexusLang
SIGNAL_SIGINT = int(signal.SIGINT)
SIGNAL_SIGTERM = int(signal.SIGTERM)
SIGNAL_SIGKILL = int(signal.SIGKILL) if hasattr(signal, "SIGKILL") else 9
SIGNAL_SIGSTOP = int(signal.SIGSTOP) if hasattr(signal, "SIGSTOP") else 19
SIGNAL_SIGCONT = int(signal.SIGCONT) if hasattr(signal, "SIGCONT") else 18
SIGNAL_SIGUSR1 = int(signal.SIGUSR1) if hasattr(signal, "SIGUSR1") else 10
SIGNAL_SIGUSR2 = int(signal.SIGUSR2) if hasattr(signal, "SIGUSR2") else 12
SIGNAL_SIGCHLD = int(signal.SIGCHLD) if hasattr(signal, "SIGCHLD") else 17


# ---------------------------------------------------------------------------
# Virtual memory
# ---------------------------------------------------------------------------

# mmap protection flag constants (POSIX values)
PROT_NONE  = 0x0
PROT_READ  = 0x1
PROT_WRITE = 0x2
PROT_EXEC  = 0x4

# mmap type constants
MAP_PRIVATE   = 0x2
MAP_ANONYMOUS = 0x20 if _IS_LINUX else 0x1000

_PAGE_SIZE: Optional[int] = None


def get_page_size(_runtime: Any) -> int:
    """Return the OS virtual memory page size in bytes."""
    global _PAGE_SIZE
    if _PAGE_SIZE is None:
        if _IS_POSIX:
            _PAGE_SIZE = os.sysconf("SC_PAGESIZE")
        else:
            _PAGE_SIZE = 4096  # Windows default
    return _PAGE_SIZE


class VMemRegion:
    """Represents an mmap-backed virtual memory region."""

    def __init__(self, address: int, size: int, prot: int) -> None:
        self.address = address
        self.size = size
        self.prot = prot
        self._buf: Optional[ctypes.Array] = None
        self._from_mmap = False

    def __repr__(self) -> str:
        return f"VMemRegion(addr=0x{self.address:016x}, size={self.size}, prot={self.prot})"


def vmem_allocate(_runtime: Any, size: int, prot: int = PROT_READ | PROT_WRITE,
                  flags: int = MAP_PRIVATE | MAP_ANONYMOUS) -> VMemRegion:
    """Allocate a virtual memory region of at least size bytes.

    On POSIX systems this uses mmap(2) with MAP_ANONYMOUS so the region is
    backed by the OS zero page.  On other platforms falls back to ctypes heap
    allocation (no mmap).
    """
    sz = int(size)
    pr = int(prot)
    if _IS_POSIX:
        libc_name = ctypes.util.find_library("c")
        if libc_name:
            libc = ctypes.CDLL(libc_name, use_errno=True)
            libc.mmap.restype = ctypes.c_void_p
            libc.mmap.argtypes = [
                ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int,
                ctypes.c_int, ctypes.c_int, ctypes.c_long,
            ]
            addr = libc.mmap(None, sz, pr, int(flags), -1, 0)
            if addr is None or addr == ctypes.c_void_p(-1).value:
                err = ctypes.get_errno()
                raise KernelError(f"vmem_allocate: mmap failed errno={err}")
            region = VMemRegion(int(addr), sz, pr)
            region._from_mmap = True
            return region

    # Fallback: ctypes heap buffer
    buf = (ctypes.c_uint8 * sz)()
    region = VMemRegion(ctypes.addressof(buf), sz, pr)
    region._buf = buf
    return region


def vmem_free(_runtime: Any, region: VMemRegion) -> None:
    """Unmap / free a VMemRegion."""
    if region._from_mmap and _IS_POSIX:
        libc_name = ctypes.util.find_library("c")
        if libc_name:
            libc = ctypes.CDLL(libc_name, use_errno=True)
            libc.munmap.restype = ctypes.c_int
            libc.munmap.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
            libc.munmap(ctypes.c_void_p(region.address), region.size)
    region._buf = None


def vmem_protect(_runtime: Any, region: VMemRegion, new_prot: int) -> None:
    """Change protection flags on a virtual memory region (mprotect)."""
    _require_posix("vmem_protect")
    libc_name = ctypes.util.find_library("c")
    if not libc_name:
        raise KernelError("vmem_protect: libc not found")
    libc = ctypes.CDLL(libc_name, use_errno=True)
    libc.mprotect.restype = ctypes.c_int
    libc.mprotect.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int]
    ret = libc.mprotect(ctypes.c_void_p(region.address), region.size, int(new_prot))
    if ret != 0:
        err = ctypes.get_errno()
        raise KernelError(f"vmem_protect: mprotect failed errno={err}")
    region.prot = int(new_prot)


def vmem_write(_runtime: Any, region: VMemRegion,
               offset: int, data: Any) -> int:
    """Write bytes into a virtual memory region at offset.  Returns bytes written."""
    if isinstance(data, str):
        data = data.encode()
    data = bytes(data)
    off = int(offset)
    if off + len(data) > region.size:
        raise KernelError(f"vmem_write: out of bounds (offset={off}, len={len(data)}, size={region.size})")
    buf = (ctypes.c_uint8 * len(data))(*data)
    ctypes.memmove(region.address + off, buf, len(data))
    return len(data)


def vmem_read(_runtime: Any, region: VMemRegion,
              offset: int, size: int) -> bytes:
    """Read size bytes from a virtual memory region at offset."""
    off = int(offset)
    sz = int(size)
    if off + sz > region.size:
        raise KernelError(f"vmem_read: out of bounds (offset={off}, size={sz}, region={region.size})")
    buf = (ctypes.c_uint8 * sz)()
    ctypes.memmove(buf, region.address + off, sz)
    return bytes(buf)


def vmem_address(_runtime: Any, region: VMemRegion) -> int:
    """Return the base address of a virtual memory region."""
    return region.address


def vmem_size(_runtime: Any, region: VMemRegion) -> int:
    """Return the size in bytes of a virtual memory region."""
    return region.size


# ---------------------------------------------------------------------------
# Raw syscall invocation  (Linux x86-64 only)
# ---------------------------------------------------------------------------

# Selected Linux x86-64 syscall numbers
LINUX_SYSCALLS: Dict[str, int] = {
    "read":          0,
    "write":         1,
    "open":          2,
    "close":         3,
    "stat":          4,
    "fstat":         5,
    "mmap":          9,
    "mprotect":      10,
    "munmap":        11,
    "brk":           12,
    "sigaction":     13,
    "ioctl":         16,
    "pipe":          22,
    "select":        23,
    "sched_yield":   24,
    "mremap":        25,
    "socket":        41,
    "connect":       42,
    "accept":        43,
    "sendto":        44,
    "recvfrom":      45,
    "bind":          49,
    "listen":        50,
    "fork":          57,
    "vfork":         58,
    "execve":        59,
    "exit":          60,
    "wait4":         61,
    "kill":          62,
    "getpid":        39,
    "getppid":       110,
    "getuid":        102,
    "getgid":        104,
    "geteuid":       107,
    "getegid":       108,
    "clone":         56,
    "futex":         202,
    "set_tid_address": 218,
    "exit_group":    231,
    "epoll_create":  213,
    "epoll_ctl":     233,
    "epoll_wait":    232,
    "nanosleep":     35,
    "clock_gettime": 228,
    "clock_nanosleep": 230,
}


def syscall(_runtime: Any, number: int, *args: int) -> int:
    """Invoke a raw Linux system call (x86-64 only).

    number can be an integer syscall number or a string name (looked up in
    LINUX_SYSCALLS).  Arguments are passed as integers.  Returns the syscall
    return value as a signed integer.
    """
    _require_linux("syscall")
    if isinstance(number, str):
        name = str(number).lower()
        if name not in LINUX_SYSCALLS:
            raise KernelError(f"syscall: unknown syscall name '{name}'")
        num = LINUX_SYSCALLS[name]
    else:
        num = int(number)

    # Build ctypes prototype for the syscall
    # syscall(2) calling convention: syscall(number, arg0, ..., arg5)
    int_args = [int(a) for a in args[:6]]
    # Pad to 6 arguments
    while len(int_args) < 6:
        int_args.append(0)

    libc_name = ctypes.util.find_library("c")
    if not libc_name:
        raise KernelError("syscall: libc not found")
    libc = ctypes.CDLL(libc_name, use_errno=True)
    libc.syscall.restype = ctypes.c_long
    libc.syscall.argtypes = [ctypes.c_long] + [ctypes.c_long] * 6
    ret = libc.syscall(ctypes.c_long(num), *[ctypes.c_long(a) for a in int_args])
    return int(ret)


def syscall_number(_runtime: Any, name: str) -> int:
    """Return the Linux x86-64 syscall number for the named call, or -1."""
    return LINUX_SYSCALLS.get(str(name).lower(), -1)


def get_uid(_runtime: Any) -> int:
    """Return the real user ID of the current process."""
    _require_posix("get_uid")
    return os.getuid()


def get_gid(_runtime: Any) -> int:
    """Return the real group ID."""
    _require_posix("get_gid")
    return os.getgid()


def get_euid(_runtime: Any) -> int:
    _require_posix("get_euid")
    return os.geteuid()


def get_egid(_runtime: Any) -> int:
    _require_posix("get_egid")
    return os.getegid()


# ---------------------------------------------------------------------------
# Scheduler interface
# ---------------------------------------------------------------------------

# POSIX scheduling policy constants (Linux values)
SCHED_OTHER   = 0
SCHED_FIFO    = 1
SCHED_RR      = 2
SCHED_BATCH   = 3
SCHED_IDLE    = 5
SCHED_DEADLINE = 6


def get_scheduling_policy(_runtime: Any, pid: int = 0) -> int:
    """Return the scheduling policy of pid (0 = current process).

    Returns one of SCHED_OTHER, SCHED_FIFO, SCHED_RR, SCHED_BATCH, SCHED_IDLE.
    """
    _require_linux("get_scheduling_policy")
    libc_name = ctypes.util.find_library("c")
    if not libc_name:
        raise KernelError("get_scheduling_policy: libc not found")
    libc = ctypes.CDLL(libc_name, use_errno=True)
    libc.sched_getscheduler.restype = ctypes.c_int
    libc.sched_getscheduler.argtypes = [ctypes.c_int]
    return libc.sched_getscheduler(int(pid))


class _SchedParam(ctypes.Structure):
    _fields_ = [("sched_priority", ctypes.c_int)]


def set_scheduling_policy(_runtime: Any, pid: int, policy: int,
                          priority: int = 0) -> None:
    """Set the scheduling policy and priority for pid (0 = current process).

    For SCHED_FIFO and SCHED_RR, priority must be in [1, 99].
    For SCHED_OTHER, SCHED_BATCH, and SCHED_IDLE, priority must be 0.
    """
    _require_linux("set_scheduling_policy")
    libc_name = ctypes.util.find_library("c")
    if not libc_name:
        raise KernelError("set_scheduling_policy: libc not found")
    libc = ctypes.CDLL(libc_name, use_errno=True)
    libc.sched_setscheduler.restype = ctypes.c_int
    libc.sched_setscheduler.argtypes = [
        ctypes.c_int, ctypes.c_int, ctypes.POINTER(_SchedParam)
    ]
    param = _SchedParam(sched_priority=int(priority))
    ret = libc.sched_setscheduler(int(pid), int(policy), ctypes.byref(param))
    if ret != 0:
        err = ctypes.get_errno()
        raise KernelError(f"set_scheduling_policy: sched_setscheduler failed errno={err}")


def sched_yield(_runtime: Any) -> None:
    """Yield the CPU to another runnable thread."""
    _require_posix("sched_yield")
    os.sched_yield() if hasattr(os, "sched_yield") else None


def get_cpu_affinity(_runtime: Any, pid: int = 0) -> int:
    """Return CPU affinity bitmask of pid (0 = current process, Linux only)."""
    _require_linux("get_cpu_affinity")
    cpus: set = os.sched_getaffinity(int(pid))
    mask = 0
    for cpu in cpus:
        mask |= (1 << cpu)
    return mask


def set_cpu_affinity(_runtime: Any, pid: int, mask: int) -> None:
    """Set CPU affinity of pid to the CPUs indicated by bitmask (Linux only)."""
    _require_linux("set_cpu_affinity")
    cpus = {i for i in range(64) if mask & (1 << i)}
    os.sched_setaffinity(int(pid), cpus)


def get_scheduler_priority_min(_runtime: Any, policy: int) -> int:
    """Return minimum priority for policy (Linux only)."""
    _require_linux("get_scheduler_priority_min")
    libc_name = ctypes.util.find_library("c")
    if not libc_name:
        return 0
    libc = ctypes.CDLL(libc_name, use_errno=True)
    libc.sched_get_priority_min.restype = ctypes.c_int
    libc.sched_get_priority_min.argtypes = [ctypes.c_int]
    return libc.sched_get_priority_min(int(policy))


def get_scheduler_priority_max(_runtime: Any, policy: int) -> int:
    """Return maximum priority for policy (Linux only)."""
    _require_linux("get_scheduler_priority_max")
    libc_name = ctypes.util.find_library("c")
    if not libc_name:
        return 99
    libc = ctypes.CDLL(libc_name, use_errno=True)
    libc.sched_get_priority_max.restype = ctypes.c_int
    libc.sched_get_priority_max.argtypes = [ctypes.c_int]
    return libc.sched_get_priority_max(int(policy))


# ---------------------------------------------------------------------------
# Kernel info / /proc interface (Linux)
# ---------------------------------------------------------------------------

def kernel_version(_runtime: Any) -> str:
    """Return the kernel version string."""
    r = platform.release()
    return r if r else "unknown"


def get_system_uptime(_runtime: Any) -> float:
    """Return system uptime in seconds (Linux only, via /proc/uptime)."""
    _require_linux("get_system_uptime")
    with open("/proc/uptime") as f:
        line = f.read().split()
        return float(line[0])


def get_memory_info(_runtime: Any) -> dict:
    """Return a dict of key memory statistics from /proc/meminfo (Linux only)."""
    _require_linux("get_memory_info")
    info: dict = {}
    with open("/proc/meminfo") as f:
        for line in f:
            parts = line.split()
            if len(parts) >= 2:
                key = parts[0].rstrip(":")
                try:
                    val = int(parts[1])
                    info[key] = val * 1024 if len(parts) == 3 and parts[2] == "kB" else val
                except ValueError:
                    info[key] = parts[1]
    return info


def get_cpu_info(_runtime: Any) -> dict:
    """Return basic CPU info from /proc/cpuinfo (Linux only)."""
    _require_linux("get_cpu_info")
    info: dict = {"processors": 0}
    with open("/proc/cpuinfo") as f:
        for line in f:
            if line.startswith("processor"):
                info["processors"] = int(line.split(":")[1].strip()) + 1
            elif line.startswith("model name") and "model_name" not in info:
                info["model_name"] = line.split(":")[1].strip()
            elif line.startswith("cpu MHz") and "mhz" not in info:
                try:
                    info["mhz"] = float(line.split(":")[1].strip())
                except ValueError:
                    pass
    return info


def read_proc_file(_runtime: Any, path: str) -> str:
    """Read a file under /proc or /sys and return its contents as a string."""
    _require_linux("read_proc_file")
    p = str(path)
    if not p.startswith("/proc") and not p.startswith("/sys"):
        raise KernelError(f"read_proc_file: path must start with /proc or /sys, got {p!r}")
    with open(p) as f:
        return f.read()


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

def register_kernel_functions(runtime: Any) -> None:
    """Register all kernel primitive functions with the NexusLang runtime."""

    # --- Process management ---
    runtime.register_function("create_process",
        lambda *a: create_process(runtime, a[0],
                                  a[1] if len(a) > 1 else None,
                                  a[2] if len(a) > 2 else None,
                                  bool(a[3]) if len(a) > 3 else False,
                                  bool(a[4]) if len(a) > 4 else False,
                                  bool(a[5]) if len(a) > 5 else False,
                                  a[6] if len(a) > 6 else None))
    runtime.register_function("wait_process",
        lambda *a: wait_process(runtime, a[0], a[1] if len(a) > 1 else None))
    runtime.register_function("kill_process",
        lambda *a: kill_process(runtime, a[0]))
    runtime.register_function("terminate_process",
        lambda *a: terminate_process(runtime, a[0]))
    runtime.register_function("process_pid",
        lambda *a: process_pid(runtime, a[0]))
    runtime.register_function("process_exit_code",
        lambda *a: process_exit_code(runtime, a[0]))
    runtime.register_function("process_is_running",
        lambda *a: process_is_running(runtime, a[0]))
    runtime.register_function("process_stdout",
        lambda *a: process_stdout(runtime, a[0]))
    runtime.register_function("process_stdin_write",
        lambda *a: process_stdin_write(runtime, a[0], a[1]))
    runtime.register_function("get_process_id",
        lambda *_a: get_process_id(runtime))
    runtime.register_function("get_parent_process_id",
        lambda *_a: get_parent_process_id(runtime))
    runtime.register_function("create_pipe",
        lambda *_a: list(create_pipe(runtime)))
    runtime.register_function("pipe_read",
        lambda *a: pipe_read(runtime, a[0], a[1] if len(a) > 1 else 4096))
    runtime.register_function("pipe_write",
        lambda *a: pipe_write(runtime, a[0], a[1]))
    runtime.register_function("close_fd",
        lambda *a: close_fd(runtime, a[0]))
    runtime.register_function("send_signal",
        lambda *a: send_signal(runtime, a[0], a[1]))

    # Signal constants
    runtime.register_function("SIGINT",  lambda *_a: SIGNAL_SIGINT)
    runtime.register_function("SIGTERM", lambda *_a: SIGNAL_SIGTERM)
    runtime.register_function("SIGKILL", lambda *_a: SIGNAL_SIGKILL)
    runtime.register_function("SIGSTOP", lambda *_a: SIGNAL_SIGSTOP)
    runtime.register_function("SIGCONT", lambda *_a: SIGNAL_SIGCONT)
    runtime.register_function("SIGUSR1", lambda *_a: SIGNAL_SIGUSR1)
    runtime.register_function("SIGUSR2", lambda *_a: SIGNAL_SIGUSR2)
    runtime.register_function("SIGCHLD", lambda *_a: SIGNAL_SIGCHLD)

    # --- Virtual memory ---
    runtime.register_function("vmem_allocate",
        lambda *a: vmem_allocate(runtime, a[0],
                                  a[1] if len(a) > 1 else PROT_READ | PROT_WRITE,
                                  a[2] if len(a) > 2 else MAP_PRIVATE | MAP_ANONYMOUS))
    runtime.register_function("vmem_free",
        lambda *a: vmem_free(runtime, a[0]))
    runtime.register_function("vmem_protect",
        lambda *a: vmem_protect(runtime, a[0], a[1]))
    runtime.register_function("vmem_write",
        lambda *a: vmem_write(runtime, a[0], a[1], a[2]))
    runtime.register_function("vmem_read",
        lambda *a: vmem_read(runtime, a[0], a[1], a[2]))
    runtime.register_function("vmem_address",
        lambda *a: vmem_address(runtime, a[0]))
    runtime.register_function("vmem_size",
        lambda *a: vmem_size(runtime, a[0]))
    runtime.register_function("get_page_size",
        lambda *_a: get_page_size(runtime))

    # Protection flag constants
    runtime.register_function("PROT_NONE",  lambda *_a: PROT_NONE)
    runtime.register_function("PROT_READ",  lambda *_a: PROT_READ)
    runtime.register_function("PROT_WRITE", lambda *_a: PROT_WRITE)
    runtime.register_function("PROT_EXEC",  lambda *_a: PROT_EXEC)

    # --- Raw syscall ---
    runtime.register_function("syscall",
        lambda *a: syscall(runtime, a[0], *a[1:]))
    runtime.register_function("syscall_number",
        lambda *a: syscall_number(runtime, a[0]))
    runtime.register_function("get_uid",  lambda *_a: get_uid(runtime))
    runtime.register_function("get_gid",  lambda *_a: get_gid(runtime))
    runtime.register_function("get_euid", lambda *_a: get_euid(runtime))
    runtime.register_function("get_egid", lambda *_a: get_egid(runtime))

    # --- Scheduler ---
    runtime.register_function("get_scheduling_policy",
        lambda *a: get_scheduling_policy(runtime, a[0] if a else 0))
    runtime.register_function("set_scheduling_policy",
        lambda *a: set_scheduling_policy(runtime, a[0], a[1],
                                          a[2] if len(a) > 2 else 0))
    runtime.register_function("sched_yield",
        lambda *_a: sched_yield(runtime))
    runtime.register_function("get_cpu_affinity",
        lambda *a: get_cpu_affinity(runtime, a[0] if a else 0))
    runtime.register_function("set_cpu_affinity",
        lambda *a: set_cpu_affinity(runtime, a[0], a[1]))
    runtime.register_function("get_scheduler_priority_min",
        lambda *a: get_scheduler_priority_min(runtime, a[0]))
    runtime.register_function("get_scheduler_priority_max",
        lambda *a: get_scheduler_priority_max(runtime, a[0]))

    # Scheduler policy constants
    runtime.register_function("SCHED_OTHER",    lambda *_a: SCHED_OTHER)
    runtime.register_function("SCHED_FIFO",     lambda *_a: SCHED_FIFO)
    runtime.register_function("SCHED_RR",       lambda *_a: SCHED_RR)
    runtime.register_function("SCHED_BATCH",    lambda *_a: SCHED_BATCH)
    runtime.register_function("SCHED_IDLE",     lambda *_a: SCHED_IDLE)
    runtime.register_function("SCHED_DEADLINE", lambda *_a: SCHED_DEADLINE)

    # --- Kernel info ---
    runtime.register_function("kernel_version",
        lambda *_a: kernel_version(runtime))
    runtime.register_function("get_system_uptime",
        lambda *_a: get_system_uptime(runtime))
    runtime.register_function("get_memory_info",
        lambda *_a: get_memory_info(runtime))
    runtime.register_function("get_cpu_info",
        lambda *_a: get_cpu_info(runtime))
    runtime.register_function("read_proc_file",
        lambda *a: read_proc_file(runtime, a[0]))
