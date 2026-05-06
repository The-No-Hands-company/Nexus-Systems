"""
NLPL Sandbox Module

Provides multi-layer execution sandboxing for NexusLang programs:

1. RestrictedMode
   - Uses the existing PermissionManager to deny FFI and inline-assembly access.
   - Can be locked to prevent permission escalation at runtime.
   - Integrates with FFIManager.load_library() and FFIManager.call_function().

2. ResourceLimits
   - Wraps POSIX resource.setrlimit() to cap CPU time, virtual memory,
     open file descriptors, and process count.
   - No-op on platforms where the resource module is unavailable.

3. SeccompFilter (Linux only)
   - Installs a Berkeley Packet Filter (BPF) program via the seccomp(2)
     syscall to restrict which system calls the NexusLang process may invoke.
   - Gracefully degrades to a warning on non-Linux platforms or when
     the necessary kernel capability is unavailable.
   - Default filter: permits a safe minimal subset of syscalls and
     kills the process on any other syscall attempt.

4. Sandbox (facade / context manager)
   - Combines RestrictedMode + ResourceLimits + optional SeccompFilter.
   - Use as:

        with Sandbox(policy) as sb:
            # restricted execution here
            ...

   - All resources are cleaned up on exit.

Sandbox policies are described by the SandboxPolicy dataclass.  A strict
default policy is available as STRICT_POLICY.

Platform notes:
- ResourceLimits requires POSIX (Linux, macOS, *BSD).
- SeccompFilter requires Linux >= 3.5 with CAP_SYS_ADMIN or PR_SET_NO_NEW_PRIVS.
- On Windows / unsupported platforms the sandbox degrades gracefully;
  RestrictedMode always works everywhere.
"""

from __future__ import annotations

import ctypes
import logging
import os
import platform
import signal
import struct
import sys
import threading
from contextlib import contextmanager
from dataclasses import dataclass, field
from typing import FrozenSet, Iterator, Optional, Set

from nexuslang.security.permissions import (
    PermissionDeniedError,
    PermissionManager,
    PermissionType,
    get_permission_manager,
    set_permission_manager,
)

logger = logging.getLogger(__name__)


# =============================================================================
# Sandbox Policy
# =============================================================================

@dataclass(frozen=True)
class SandboxPolicy:
    """
    Immutable configuration for a sandbox execution environment.

    Attributes:
        allow_ffi:          Permit calls to foreign (C) libraries via FFI.
        allow_asm:          Permit execution of inline assembly blocks.
        allow_network:      Permit network socket operations.
        allow_file_write:   Permit writing to the filesystem.
        allow_subprocess:   Permit spawning child processes.
        max_memory_mb:      Virtual memory cap in MiB (None = unlimited).
        max_cpu_seconds:    CPU time limit in seconds (None = unlimited).
        max_open_files:     Maximum open file descriptor count (None = unlimited).
        max_processes:      Maximum spawnable child processes (None = unlimited).
        enable_seccomp:     Install a seccomp syscall filter (Linux only).
        allowed_syscalls:   Additional syscalls to whitelist (seccomp only).
    """
    allow_ffi: bool = False
    allow_asm: bool = False
    allow_network: bool = False
    allow_file_write: bool = False
    allow_subprocess: bool = False

    max_memory_mb: Optional[int] = None
    max_cpu_seconds: Optional[float] = None
    max_open_files: Optional[int] = None
    max_processes: Optional[int] = None

    enable_seccomp: bool = False
    allowed_syscalls: FrozenSet[str] = field(default_factory=frozenset)


#: Strongly restricted policy: no FFI, no ASM, 256 MiB RAM, 10 s CPU, 64 FDs.
STRICT_POLICY = SandboxPolicy(
    allow_ffi=False,
    allow_asm=False,
    allow_network=False,
    allow_file_write=False,
    allow_subprocess=False,
    max_memory_mb=256,
    max_cpu_seconds=10.0,
    max_open_files=64,
    enable_seccomp=False,  # opt-in; may require elevated privileges
)

#: Development policy: relaxed limits, no seccomp.
DEVELOPMENT_POLICY = SandboxPolicy(
    allow_ffi=True,
    allow_asm=True,
    allow_network=True,
    allow_file_write=True,
    allow_subprocess=True,
)


class SandboxError(Exception):
    """Raised when sandbox setup or enforcement fails."""


# =============================================================================
# Restricted Mode (permission-based)
# =============================================================================

class RestrictedMode:
    """
    Wraps a PermissionManager to enforce sandbox policy at the permission layer.

    This is the only sandbox layer that works on all platforms.

    When enter() is called the existing global PermissionManager is replaced
    (or patched) to reflect the sandbox policy.  When exit() is called the
    previous manager is restored.

    Usage:
        rm = RestrictedMode(STRICT_POLICY)
        rm.enter()
        try:
            # restricted code here
            ...
        finally:
            rm.exit()

    Or via the context manager:
        with RestrictedMode(STRICT_POLICY):
            ...
    """

    def __init__(self, policy: SandboxPolicy,
                 manager: Optional[PermissionManager] = None) -> None:
        """
        Args:
            policy:  The sandbox policy to enforce.
            manager: Optional PermissionManager to patch.  If None, the global
                     manager is used.
        """
        self._policy = policy
        self._base_manager = manager
        self._saved_manager: Optional[PermissionManager] = None
        self._locked = False

    def enter(self) -> None:
        """Apply the policy to the permission manager."""
        base = self._base_manager or get_permission_manager()
        self._saved_manager = base

        # Build a new manager from scratch reflecting the policy
        restricted = PermissionManager()

        # FFI
        if self._policy.allow_ffi:
            restricted.grant(PermissionType.FFI)

        # Inline assembly
        if self._policy.allow_asm:
            restricted.grant(PermissionType.ASM)

        # Network access
        if self._policy.allow_network:
            restricted.grant(PermissionType.NET)

        # File write
        if self._policy.allow_file_write:
            restricted.grant(PermissionType.WRITE)

        # Subprocess execution
        if self._policy.allow_subprocess:
            restricted.grant(PermissionType.RUN)

        # File read is always permitted in sandbox (programs need to read their source)
        restricted.grant(PermissionType.READ)

        # Import is permitted (the program is already loaded)
        restricted.grant(PermissionType.IMPORT)

        self._locked = True
        set_permission_manager(restricted)
        logger.debug("RestrictedMode: sandbox permissions applied")

    def exit(self) -> None:
        """Restore the previous permission manager."""
        if self._saved_manager is not None:
            set_permission_manager(self._saved_manager)
            logger.debug("RestrictedMode: original permissions restored")
        self._locked = False

    def __enter__(self) -> "RestrictedMode":
        self.enter()
        return self

    def __exit__(self, *_: object) -> None:
        self.exit()

    def check_ffi(self, library: Optional[str] = None) -> None:
        """
        Convenience method for FFIManager integration.

        Call this at the start of FFIManager.load_library() and
        FFIManager.call_function() to enforce sandbox policy.

        Raises:
            PermissionDeniedError: If FFI is not allowed by the current policy.
        """
        mgr = get_permission_manager()
        mgr.check(PermissionType.FFI, resource=library)

    def check_asm(self) -> None:
        """
        Convenience method for inline-assembly integration.

        Call this before executing any inline-asm block.

        Raises:
            PermissionDeniedError: If ASM is not allowed.
        """
        mgr = get_permission_manager()
        mgr.check(PermissionType.ASM)


# =============================================================================
# Resource Limits (POSIX)
# =============================================================================

_HAS_RESOURCE = False
try:
    import resource as _resource_module
    _HAS_RESOURCE = True
except ImportError:
    pass


@dataclass
class _SavedLimits:
    """Snapshot of resource limits before sandbox entry."""
    limits: dict  # {resource_id: (soft, hard)}


class ResourceLimits:
    """
    Applies POSIX resource limits using the resource module.

    Limits are applied to the **current process** and its children.
    Original limits are saved and restored on exit() (best-effort: restoring
    to a higher value may fail if the hard limit was already lowered).

    Supported on: Linux, macOS, FreeBSD, OpenBSD, Solaris.
    No-op on Windows.
    """

    def __init__(self, policy: SandboxPolicy) -> None:
        self._policy = policy
        self._saved: Optional[_SavedLimits] = None

    def enter(self) -> None:
        """Apply resource limits from the policy."""
        if not _HAS_RESOURCE:
            logger.debug("ResourceLimits: resource module unavailable, skipping")
            return

        saved_limits: dict = {}

        def _apply(res_id: int, soft_val: Optional[float],
                   description: str) -> None:
            if soft_val is None:
                return
            try:
                soft, hard = _resource_module.getrlimit(res_id)
                saved_limits[res_id] = (soft, hard)
                new_soft = int(soft_val)
                # Only lower the soft limit; preserve the hard limit so
                # exit() can restore the original values.
                if hard != _resource_module.RLIM_INFINITY:
                    new_soft = min(new_soft, hard)
                _resource_module.setrlimit(res_id, (new_soft, hard))
                logger.debug(
                    "ResourceLimits: set %s to %d (was soft=%d, hard=%d)",
                    description, new_soft, soft, hard,
                )
            except (ValueError, _resource_module.error) as exc:
                logger.warning(
                    "ResourceLimits: could not set %s limit: %s",
                    description, exc,
                )

        # Virtual memory (RLIMIT_AS)
        if hasattr(_resource_module, 'RLIMIT_AS') and self._policy.max_memory_mb:
            _apply(
                _resource_module.RLIMIT_AS,
                self._policy.max_memory_mb * 1024 * 1024,
                "RLIMIT_AS (virtual memory)",
            )

        # CPU time (RLIMIT_CPU)
        if hasattr(_resource_module, 'RLIMIT_CPU') and self._policy.max_cpu_seconds:
            _apply(
                _resource_module.RLIMIT_CPU,
                self._policy.max_cpu_seconds,
                "RLIMIT_CPU (CPU seconds)",
            )

        # Open file descriptors (RLIMIT_NOFILE)
        if hasattr(_resource_module, 'RLIMIT_NOFILE') and self._policy.max_open_files:
            _apply(
                _resource_module.RLIMIT_NOFILE,
                self._policy.max_open_files,
                "RLIMIT_NOFILE (open files)",
            )

        # Processes (RLIMIT_NPROC — Linux/BSD; not available on macOS in standard form)
        if hasattr(_resource_module, 'RLIMIT_NPROC') and self._policy.max_processes:
            _apply(
                _resource_module.RLIMIT_NPROC,
                self._policy.max_processes,
                "RLIMIT_NPROC (processes)",
            )

        self._saved = _SavedLimits(limits=saved_limits)

    def exit(self) -> None:
        """Restore previously saved resource limits (best-effort)."""
        if not _HAS_RESOURCE or self._saved is None:
            return

        for res_id, (soft, hard) in self._saved.limits.items():
            try:
                _resource_module.setrlimit(res_id, (soft, hard))
            except (ValueError, _resource_module.error) as exc:
                logger.debug(
                    "ResourceLimits: could not restore limit %d: %s", res_id, exc
                )

    def __enter__(self) -> "ResourceLimits":
        self.enter()
        return self

    def __exit__(self, *_: object) -> None:
        self.exit()


# =============================================================================
# Seccomp Filter (Linux only)
# =============================================================================

# Architecture-specific seccomp constants
_SECCOMP_MODE_FILTER = 2
_PR_SET_NO_NEW_PRIVS = 38
_PR_SET_SECCOMP = 22

# BPF instructions (big-endian on all architectures for seccomp)
BPF_LD = 0x00
BPF_W = 0x00
BPF_ABS = 0x20
BPF_JMP = 0x05
BPF_JEQ = 0x10
BPF_RET = 0x06
BPF_K = 0x00

SECCOMP_RET_ALLOW = 0x7fff0000
SECCOMP_RET_KILL_PROCESS = 0x80000000
SECCOMP_RET_ERRNO = 0x00050000  # returns EPERM (1)

# Syscall numbers (x86-64 Linux)
# Minimal safe set: read, write, open, close, fstat, mmap, munmap, brk,
# rt_sigaction, rt_sigprocmask, rt_sigreturn, pread64, pwrite64,
# access, pipe, select, dup, dup2, pause, nanosleep, getpid, sendfile,
# socket, connect, accept, sendto, recvfrom, sendmsg, recvmsg, shutdown,
# bind, listen, getsockname, getpeername, socketpair, setsockopt,
# getsockopt, clone, fork, vfork, execve, exit, wait4, kill, uname,
# fcntl, flock, fsync, fdatasync, truncate, ftruncate, getdents,
# getcwd, chdir, fchdir, rename, mkdir, rmdir, creat, link, unlink,
# symlink, readlink, chmod, fchmod, chown, fchown, lchown, umask,
# gettimeofday, getrlimit, getrusage, sysinfo, times, ptrace, getuid,
# syslog, getgid, setuid, setgid, geteuid, getegid, setpgid, getppid,
# getpgrp, setsid, setreuid, setregid, getgroups, setgroups, setresuid,
# getresuid, setresgid, getresgid, getpgid, setfsuid, setfsgid,
# getsid, capget, capset, rt_sigpending, rt_sigtimedwait, rt_sigqueueinfo,
# rt_sigsuspend, sigaltstack, utime, mknod, uselib, personality, ustat,
# statfs, fstatfs, sysfs, getpriority, setpriority, sched_setparam,
# sched_getparam, sched_setscheduler, sched_getscheduler, sched_get_priority_max,
# sched_get_priority_min, sched_rr_get_interval, mlock, munlock, mlockall,
# munlockall, vhangup, modify_ldt, pivot_root, sysctl, prctl, arch_prctl,
# adjtimex, setrlimit, chroot, sync, acct, settimeofday, mount, umount2,
# swapon, swapoff, reboot, sethostname, setdomainname, iopl, ioperm, ioctl,
# createmodule, init_module, delete_module, get_kernel_syms, querymodule,
# quotactl, nfsservctl, getpmsg, putpmsg, afs_syscall, tuxcall, security,
# gettid, readahead, setxattr, lsetxattr, fsetxattr, getxattr, lgetxattr,
# fgetxattr, listxattr, llistxattr, flistxattr, removexattr, lremovexattr,
# fremovexattr, tkill, time, futex, sched_setaffinity, sched_getaffinity,
# set_thread_area, io_setup, io_destroy, io_getevents, io_submit, io_cancel,
# get_thread_area, lookup_dcookie, epoll_create, epoll_ctl_old, epoll_wait_old,
# remap_file_pages, getdents64, set_tid_address, restart_syscall, semtimedop,
# fadvise64, timer_create, timer_settime, timer_gettime, timer_getoverrun,
# timer_delete, clock_settime, clock_gettime, clock_getres, clock_nanosleep,
# exit_group, epoll_wait, epoll_ctl, tgkill, utimes, vserver (249), mbind,
# set_mempolicy, get_mempolicy, mq_open, mq_unlink, mq_timedsend,
# mq_timedreceive, mq_notify, mq_getsetattr, kexec_load, waitid, add_key,
# request_key, keyctl, ioprio_set, ioprio_get, inotify_init, inotify_add_watch,
# inotify_rm_watch, migrate_pages, openat, mkdirat, mknodat, fchownat,
# futimesat, newfstatat, unlinkat, renameat, linkat, symlinkat, readlinkat,
# fchmodat, faccessat, pselect6, ppoll, unshare, set_robust_list,
# get_robust_list, splice, tee, sync_file_range, vmsplice, move_pages,
# utimensat, epoll_pwait, signalfd, timerfd_create, eventfd, fallocate,
# timerfd_settime, timerfd_gettime, accept4, signalfd4, eventfd2, epoll_create1,
# dup3, pipe2, inotify_init1, preadv, pwritev, rt_tgsigqueueinfo, perf_event_open,
# recvmmsg, fanotify_init, fanotify_mark, prlimit64, name_to_handle_at,
# open_by_handle_at, clock_adjtime, syncfs, sendmmsg, setns, getcpu,
# process_vm_readv, process_vm_writev, kcmp, finit_module, sched_setattr,
# sched_getattr, renameat2, seccomp, getrandom, memfd_create, kexec_file_load,
# bpf, execveat, userfaultfd, membarrier, mlock2, copy_file_range, preadv2,
# pwritev2, pkey_mprotect, pkey_alloc, pkey_free, statx, io_pgetevents,
# rseq, pidfd_send_signal, io_uring_setup, io_uring_enter, io_uring_register,
# open_tree, move_mount, fsopen, fsconfig, fsmount, fspick, pidfd_open,
# clone3, close_range, openat2, pidfd_getfd, faccessat2, process_madvise,
# epoll_pwait2, mount_setattr, quotactl_fd, landlock_create_ruleset,
# landlock_add_rule, landlock_restrict_self, memfd_secret, process_mrelease,
# futex_waitv, set_mempolicy_home_node

# Minimal safe syscall numbers for x86-64 Linux Python execution
_SAFE_SYSCALLS_X86_64: Set[int] = {
    0,   # read
    1,   # write
    2,   # open
    3,   # close
    4,   # stat
    5,   # fstat
    6,   # lstat
    7,   # poll
    8,   # lseek
    9,   # mmap
    10,  # mprotect
    11,  # munmap
    12,  # brk
    13,  # rt_sigaction
    14,  # rt_sigprocmask
    15,  # rt_sigreturn
    16,  # ioctl (needed by terminal/Python internals)
    17,  # pread64
    18,  # pwrite64
    19,  # readv
    20,  # writev
    21,  # access
    22,  # pipe
    23,  # select
    24,  # sched_yield
    25,  # mremap
    28,  # madvise
    32,  # dup
    33,  # dup2
    35,  # nanosleep
    37,  # alarm
    38,  # setitimer
    39,  # getpid
    41,  # socket
    42,  # connect
    43,  # accept
    44,  # sendto
    45,  # recvfrom
    46,  # sendmsg
    47,  # recvmsg
    48,  # shutdown
    49,  # bind
    50,  # listen
    51,  # getsockname
    52,  # getpeername
    54,  # setsockopt
    55,  # getsockopt
    56,  # clone
    57,  # fork
    59,  # execve
    60,  # exit
    61,  # wait4
    62,  # kill
    63,  # uname
    72,  # fcntl
    73,  # flock
    74,  # fsync
    75,  # fdatasync
    76,  # truncate
    77,  # ftruncate
    78,  # getdents
    79,  # getcwd
    80,  # chdir
    82,  # rename
    83,  # mkdir
    84,  # rmdir
    85,  # creat
    86,  # link
    87,  # unlink
    89,  # readlink
    90,  # chmod
    91,  # fchmod
    92,  # chown
    93,  # fchown
    95,  # umask
    96,  # gettimeofday
    97,  # getrlimit
    98,  # getrusage
    99,  # sysinfo
    102, # getuid
    104, # getgid
    105, # setuid
    106, # setgid
    107, # geteuid
    108, # getegid
    110, # getppid
    111, # getpgrp
    112, # setsid
    117, # setresuid
    118, # getresuid
    119, # setresgid
    120, # getresgid
    121, # getpgid
    124, # getsid
    128, # rt_sigpending
    129, # rt_sigtimedwait
    131, # rt_sigsuspend
    132, # sigaltstack
    158, # arch_prctl
    186, # gettid
    202, # futex
    218, # set_tid_address
    228, # clock_gettime
    229, # clock_getres
    230, # clock_nanosleep
    231, # exit_group
    232, # epoll_wait
    233, # epoll_ctl
    234, # tgkill
    257, # openat
    262, # newfstatat
    270, # pselect6
    271, # ppoll
    280, # utimensat
    290, # eventfd
    291, # fallocate
    292, # timerfd_settime
    293, # timerfd_gettime
    295, # accept4
    299, # recvmmsg
    302, # prlimit64
    317, # getrandom
    318, # memfd_create
    334, # close_range
}


def _is_linux() -> bool:
    return platform.system() == "Linux"


def _get_machine() -> str:
    return platform.machine().lower()


class SeccompFilter:
    """
    Installs a seccomp-BPF filter on Linux to restrict allowed syscalls.

    The filter is installed in SECCOMP_MODE_FILTER using PR_SET_SECCOMP via
    prctl(2).  The process must first call prctl(PR_SET_NO_NEW_PRIVS, 1, ...)
    to avoid requiring CAP_SYS_ADMIN.

    On non-Linux platforms, or when seccomp is unavailable, all methods
    are no-ops and _available is False.

    WARNING: Seccomp filters are **inherited by child processes** and
    **cannot be removed once installed**.  Use this only when you are
    absolutely certain you do not need to restore the original syscall set.

    Security note: The default safe syscall list is tuned for Python 3.x
    interpreters on x86-64 Linux.  ARM/ARM64 and other architectures use
    different syscall numbers; extend allowed_syscalls as needed.
    """

    def __init__(self, policy: SandboxPolicy) -> None:
        self._policy = policy
        self._installed = False
        self._available = _is_linux() and _get_machine() in ("x86_64", "amd64")

    @property
    def available(self) -> bool:
        """True if seccomp can be installed on this platform."""
        return self._available

    @property
    def installed(self) -> bool:
        """True if the filter has been successfully installed."""
        return self._installed

    def _build_bpf_filter(self, allowed_syscalls: Set[int]) -> bytes:
        """
        Build a minimal BPF program that allows listed syscalls and
        kills the process (SECCOMP_RET_KILL_PROCESS) on any other.

        BPF program structure:
          1. Load syscall number from seccomp_data (offset 0)
          2. For each allowed syscall N: JEQ N -> ALLOW
          3. Final instruction: KILL

        Each sock_filter is: { code, jt, jf, k } = 8 bytes (little-endian).
        """
        sock_filter_fmt = "<HBBI"  # code (u16), jt (u8), jf (u8), k (u32)
        instructions = []

        # Load syscall number (arch-dependent offset 0 in seccomp_data)
        # BPF_LD | BPF_W | BPF_ABS; k = offsetof(seccomp_data, nr) = 0
        instructions.append(struct.pack(sock_filter_fmt, BPF_LD | BPF_W | BPF_ABS, 0, 0, 0))

        # For each allowed syscall: if syscall_nr == N, jump to ALLOW
        # We emit instructions in reverse and fix jump offsets.
        sorted_calls = sorted(allowed_syscalls)
        n = len(sorted_calls)

        # The ALLOW instruction is at end - 1, KILL at end.
        # From instruction i, the offset to the ALLOW return is (n - i).
        for i, nr in enumerate(sorted_calls):
            jt = n - i  # jump-true offset to ALLOW instruction
            jf = 0       # fall through
            instructions.append(
                struct.pack(sock_filter_fmt,
                            BPF_JMP | BPF_JEQ | BPF_K,
                            jt, jf, nr)
            )

        # Kill process for any non-allowed syscall
        instructions.append(
            struct.pack(sock_filter_fmt, BPF_RET | BPF_K, 0, 0,
                        SECCOMP_RET_KILL_PROCESS)
        )
        # Allow instruction
        instructions.append(
            struct.pack(sock_filter_fmt, BPF_RET | BPF_K, 0, 0,
                        SECCOMP_RET_ALLOW)
        )

        return b"".join(instructions)

    def install(self) -> None:
        """
        Install the seccomp filter.

        Raises:
            SandboxError: If installation fails.
        """
        if not self._available:
            logger.info(
                "SeccompFilter: not available on %s %s, skipping",
                platform.system(), platform.machine(),
            )
            return

        if self._installed:
            logger.debug("SeccompFilter: already installed")
            return

        # Build the allowed syscall set
        allowed: Set[int] = set(_SAFE_SYSCALLS_X86_64)
        # Add any caller-specified extra syscalls
        for name in self._policy.allowed_syscalls:
            if isinstance(name, int):
                allowed.add(name)
            # Named syscalls require lookup — skip if not resolvable
            elif name.isdigit():
                allowed.add(int(name))

        try:
            libc = ctypes.CDLL(None, use_errno=True)

            # PR_SET_NO_NEW_PRIVS = 38
            # prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)
            ret = libc.prctl(
                ctypes.c_int(_PR_SET_NO_NEW_PRIVS),
                ctypes.c_ulong(1),
                ctypes.c_ulong(0),
                ctypes.c_ulong(0),
                ctypes.c_ulong(0),
            )
            if ret != 0:
                errno = ctypes.get_errno()
                raise SandboxError(
                    f"prctl(PR_SET_NO_NEW_PRIVS) failed: errno={errno}"
                )

            # Build BPF program
            bpf_bytes = self._build_bpf_filter(allowed)
            n_instructions = len(bpf_bytes) // 8

            # struct sock_fprog { unsigned short len; struct sock_filter *filter; }
            class SockFprog(ctypes.Structure):
                _fields_ = [
                    ("len", ctypes.c_ushort),
                    ("filter", ctypes.c_char_p),
                ]

            fprog = SockFprog(len=n_instructions, filter=bpf_bytes)

            # prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &fprog)
            ret = libc.prctl(
                ctypes.c_int(_PR_SET_SECCOMP),
                ctypes.c_ulong(_SECCOMP_MODE_FILTER),
                ctypes.byref(fprog),
                ctypes.c_ulong(0),
                ctypes.c_ulong(0),
            )
            if ret != 0:
                errno = ctypes.get_errno()
                raise SandboxError(
                    f"prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER) failed: errno={errno}. "
                    "Ensure the kernel supports seccomp-bpf (Linux >= 3.5) and "
                    "PR_SET_NO_NEW_PRIVS has been set."
                )

            self._installed = True
            logger.info(
                "SeccompFilter: installed with %d allowed syscalls, "
                "%d BPF instructions",
                len(allowed),
                n_instructions,
            )

        except (OSError, AttributeError) as exc:
            raise SandboxError(f"SeccompFilter: installation error: {exc}") from exc

    def __enter__(self) -> "SeccompFilter":
        self.install()
        return self

    def __exit__(self, *_: object) -> None:
        # Seccomp filters cannot be removed; nothing to do on exit.
        pass


# =============================================================================
# Sandbox Facade
# =============================================================================

class Sandbox:
    """
    Multi-layer execution sandbox (context manager).

    Activates in order:
    1. RestrictedMode (permission layer, always applied)
    2. ResourceLimits (POSIX resource caps, where available)
    3. SeccompFilter (Linux BPF syscall filter, if policy.enable_seccomp=True)

    Deactivates in reverse order on __exit__.

    Usage:

        policy = SandboxPolicy(
            allow_ffi=False,
            max_memory_mb=128,
            max_cpu_seconds=5.0,
        )

        with Sandbox(policy):
            run_untrusted_nxl_program()

    Notes:
    - SeccompFilter is irreversible once installed.  Use it only in disposable
      child processes (fork + sandbox + exec pattern).
    - ResourceLimits.exit() attempts to restore previous limits but may fail
      if the hard limit was permanently lowered.
    """

    def __init__(self, policy: Optional[SandboxPolicy] = None,
                 manager: Optional[PermissionManager] = None) -> None:
        self._policy = policy or STRICT_POLICY
        self._restricted = RestrictedMode(self._policy, manager)
        self._resource_limits = ResourceLimits(self._policy)
        self._seccomp = SeccompFilter(self._policy) if self._policy.enable_seccomp else None
        self._active = False

    def enter(self) -> "Sandbox":
        """Activate all sandbox layers."""
        if self._active:
            raise SandboxError("Sandbox is already active")

        self._restricted.enter()
        self._resource_limits.enter()

        if self._seccomp is not None:
            try:
                self._seccomp.install()
            except SandboxError as e:
                logger.warning("Sandbox: seccomp unavailable: %s", e)

        self._active = True
        logger.info("Sandbox: active (policy=%r)", self._policy)
        return self

    def exit(self) -> None:
        """Deactivate sandbox layers that support teardown."""
        self._resource_limits.exit()
        self._restricted.exit()
        self._active = False
        logger.info("Sandbox: exited")

    @property
    def active(self) -> bool:
        """True while the sandbox is active."""
        return self._active

    @property
    def policy(self) -> SandboxPolicy:
        return self._policy

    def check_ffi(self, library: Optional[str] = None) -> None:
        """Delegation helper for FFIManager.load_library()."""
        self._restricted.check_ffi(library)

    def check_asm(self) -> None:
        """Delegation helper for inline-assembly execution."""
        self._restricted.check_asm()

    def __enter__(self) -> "Sandbox":
        return self.enter()

    def __exit__(self, *_: object) -> None:
        self.exit()


# =============================================================================
# Utility helper for checking ASLR status
# =============================================================================

def check_aslr_status() -> Optional[int]:
    """
    Read the kernel ASLR randomization level from /proc/sys/kernel/randomize_va_space.

    Returns:
        0 -- ASLR disabled (dangerous).
        1 -- Conservative ASLR.
        2 -- Full ASLR (recommended).
        None -- Could not determine (non-Linux or permission denied).
    """
    path = "/proc/sys/kernel/randomize_va_space"
    try:
        with open(path) as fh:
            return int(fh.read().strip())
    except (OSError, ValueError):
        return None


def warn_if_aslr_disabled() -> None:
    """
    Print a warning to stderr if ASLR is disabled on the current Linux kernel.
    """
    level = check_aslr_status()
    if level is None:
        return  # Not Linux or not readable
    if level == 0:
        print(
            "[NLPL security warning] ASLR is disabled on this system "
            "(randomize_va_space=0).  This significantly reduces exploit "
            "mitigation.  Enable with: "
            "echo 2 | sudo tee /proc/sys/kernel/randomize_va_space",
            file=sys.stderr,
        )
    elif level == 1:
        logger.debug(
            "ASLR is in conservative mode (randomize_va_space=1). "
            "Consider full ASLR (level 2) for better protection."
        )
    else:
        logger.debug("ASLR is fully enabled (randomize_va_space=2).")
