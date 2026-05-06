"""Tests for the OS kernel primitives standard library module."""
import pytest
import sys
import os
import platform

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

_IS_LINUX = platform.system() == "Linux"
_IS_POSIX = os.name == "posix"

from nexuslang.stdlib.kernel import (
    get_process_id,
    get_page_size,
    kernel_version,
    syscall_number,
    LINUX_SYSCALLS,
    NLPLProcess,
    VMemRegion,
    PROT_READ, PROT_WRITE, PROT_EXEC, PROT_NONE,
    SCHED_OTHER, SCHED_FIFO, SCHED_RR,
    register_kernel_functions,
    create_process,
    wait_process,
    vmem_allocate,
    vmem_read,
    vmem_write,
    vmem_free,
    get_uid,
    KernelError,
)


class MockRuntime:
    def __init__(self):
        self._funcs = {}

    def register_function(self, name, fn):
        self._funcs[name] = fn


# ---------------------------------------------------------------------------
# Process management
# ---------------------------------------------------------------------------

class TestProcessManagement:
    def test_get_process_id(self):
        rt = MockRuntime()
        pid = get_process_id(rt)
        assert pid == os.getpid()

    def test_get_parent_process_id(self):
        if not _IS_POSIX:
            pytest.skip("POSIX only")
        from nexuslang.stdlib.kernel import get_parent_process_id
        ppid = get_parent_process_id(None)
        assert ppid == os.getppid()

    def test_create_process_simple(self):
        rt = MockRuntime()
        proc = create_process(rt, sys.executable, ['-c', 'import sys; sys.exit(0)'])
        assert isinstance(proc, NLPLProcess)
        code = wait_process(rt, proc)
        assert code == 0

    def test_create_process_with_stdout(self):
        rt = MockRuntime()
        proc = create_process(rt, sys.executable,
                               ['-c', 'print("hello")'],
                               capture_stdout=True)
        wait_process(rt, proc)
        out = proc.stdout_read()
        assert 'hello' in out

    def test_process_exit_code(self):
        from nexuslang.stdlib.kernel import process_exit_code
        rt = MockRuntime()
        proc = create_process(rt, sys.executable, ['-c', 'import sys; sys.exit(42)'])
        wait_process(rt, proc)
        code = process_exit_code(rt, proc)
        assert code == 42

    def test_process_pid(self):
        from nexuslang.stdlib.kernel import process_pid
        rt = MockRuntime()
        proc = create_process(rt, sys.executable, ['-c', 'import time; time.sleep(0.5)'])
        pid = process_pid(rt, proc)
        assert isinstance(pid, int)
        assert pid > 0
        wait_process(rt, proc)


# ---------------------------------------------------------------------------
# Pipe
# ---------------------------------------------------------------------------

class TestPipes:
    @pytest.mark.skipif(not _IS_POSIX, reason="POSIX only")
    def test_create_and_use_pipe(self):
        from nexuslang.stdlib.kernel import create_pipe, pipe_read, pipe_write, close_fd
        rt = MockRuntime()
        r, w = create_pipe(rt)
        pipe_write(rt, w, b'hello')
        close_fd(rt, w)
        data = pipe_read(rt, r, 5)
        close_fd(rt, r)
        assert data == b'hello'


# ---------------------------------------------------------------------------
# Page size
# ---------------------------------------------------------------------------

class TestPageSize:
    def test_page_size(self):
        rt = MockRuntime()
        size = get_page_size(rt)
        assert isinstance(size, int)
        assert size > 0
        # Must be a power of 2
        assert (size & (size - 1)) == 0


# ---------------------------------------------------------------------------
# Virtual memory
# ---------------------------------------------------------------------------

class TestVirtualMemory:
    def test_allocate_and_free(self):
        rt = MockRuntime()
        region = vmem_allocate(rt, 4096)
        assert isinstance(region, VMemRegion)
        assert region.size >= 4096
        vmem_free(rt, region)

    def test_write_and_read(self):
        rt = MockRuntime()
        region = vmem_allocate(rt, 4096)
        data = b'NLPL bare metal test'
        vmem_write(rt, region, 0, data)
        result = vmem_read(rt, region, 0, len(data))
        assert result == data
        vmem_free(rt, region)

    def test_write_at_offset(self):
        rt = MockRuntime()
        region = vmem_allocate(rt, 4096)
        vmem_write(rt, region, 512, b'offset data')
        result = vmem_read(rt, region, 512, 11)
        assert result == b'offset data'
        vmem_free(rt, region)

    def test_out_of_bounds_write_raises(self):
        rt = MockRuntime()
        region = vmem_allocate(rt, 64)
        with pytest.raises(KernelError):
            vmem_write(rt, region, 60, b'this is 10 bytes')
        vmem_free(rt, region)

    def test_out_of_bounds_read_raises(self):
        rt = MockRuntime()
        region = vmem_allocate(rt, 64)
        with pytest.raises(KernelError):
            vmem_read(rt, region, 60, 100)
        vmem_free(rt, region)

    def test_protection_constants(self):
        assert PROT_NONE == 0
        assert PROT_READ == 0x1
        assert PROT_WRITE == 0x2
        assert PROT_EXEC == 0x4


# ---------------------------------------------------------------------------
# Syscall numbers
# ---------------------------------------------------------------------------

class TestSyscallNumbers:
    def test_known_syscall(self):
        rt = MockRuntime()
        assert syscall_number(rt, 'read') == 0
        assert syscall_number(rt, 'write') == 1
        assert syscall_number(rt, 'open') == 2
        assert syscall_number(rt, 'getpid') == 39

    def test_unknown_syscall(self):
        rt = MockRuntime()
        assert syscall_number(rt, 'definitely_not_real') == -1

    def test_all_have_non_negative_numbers(self):
        for name, num in LINUX_SYSCALLS.items():
            assert isinstance(num, int)
            assert num >= 0, f"Syscall {name} has negative number {num}"

    @pytest.mark.skipif(not _IS_LINUX, reason="Linux only")
    def test_syscall_getpid(self):
        from nexuslang.stdlib.kernel import syscall
        rt = MockRuntime()
        result = syscall(rt, 'getpid')
        assert result == os.getpid()


# ---------------------------------------------------------------------------
# User/group IDs
# ---------------------------------------------------------------------------

class TestUIDGID:
    @pytest.mark.skipif(not _IS_POSIX, reason="POSIX only")
    def test_uid(self):
        rt = MockRuntime()
        uid = get_uid(rt)
        assert uid == os.getuid()

    @pytest.mark.skipif(not _IS_POSIX, reason="POSIX only")
    def test_gid(self):
        from nexuslang.stdlib.kernel import get_gid
        rt = MockRuntime()
        gid = get_gid(rt)
        assert gid == os.getgid()


# ---------------------------------------------------------------------------
# Scheduler
# ---------------------------------------------------------------------------

class TestScheduler:
    def test_scheduler_constants(self):
        assert SCHED_OTHER == 0
        assert SCHED_FIFO == 1
        assert SCHED_RR == 2

    @pytest.mark.skipif(not _IS_LINUX, reason="Linux only")
    def test_get_scheduling_policy(self):
        from nexuslang.stdlib.kernel import get_scheduling_policy
        rt = MockRuntime()
        policy = get_scheduling_policy(rt, 0)
        assert isinstance(policy, int)
        assert policy >= 0

    @pytest.mark.skipif(not _IS_LINUX, reason="Linux only")
    def test_get_cpu_affinity(self):
        from nexuslang.stdlib.kernel import get_cpu_affinity
        rt = MockRuntime()
        mask = get_cpu_affinity(rt, 0)
        assert isinstance(mask, int)
        assert mask > 0  # At least one CPU must be in the affinity set

    @pytest.mark.skipif(not _IS_LINUX, reason="Linux only")
    def test_priority_min_max(self):
        from nexuslang.stdlib.kernel import get_scheduler_priority_min, get_scheduler_priority_max
        rt = MockRuntime()
        mn = get_scheduler_priority_min(rt, SCHED_OTHER)
        mx = get_scheduler_priority_max(rt, SCHED_OTHER)
        assert isinstance(mn, int)
        assert isinstance(mx, int)


# ---------------------------------------------------------------------------
# Kernel info (Linux only)
# ---------------------------------------------------------------------------

class TestKernelInfo:
    def test_kernel_version(self):
        rt = MockRuntime()
        v = kernel_version(rt)
        assert isinstance(v, str)
        assert len(v) > 0

    @pytest.mark.skipif(not _IS_LINUX, reason="Linux only")
    def test_system_uptime(self):
        from nexuslang.stdlib.kernel import get_system_uptime
        rt = MockRuntime()
        uptime = get_system_uptime(rt)
        assert isinstance(uptime, float)
        assert uptime > 0

    @pytest.mark.skipif(not _IS_LINUX, reason="Linux only")
    def test_memory_info(self):
        from nexuslang.stdlib.kernel import get_memory_info
        rt = MockRuntime()
        info = get_memory_info(rt)
        assert isinstance(info, dict)
        assert 'MemTotal' in info
        assert info['MemTotal'] > 0

    @pytest.mark.skipif(not _IS_LINUX, reason="Linux only")
    def test_cpu_info(self):
        from nexuslang.stdlib.kernel import get_cpu_info
        rt = MockRuntime()
        info = get_cpu_info(rt)
        assert isinstance(info, dict)
        assert info.get('processors', 0) >= 1


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

class TestRegisterKernelFunctions:
    def test_all_functions_registered(self):
        rt = MockRuntime()
        register_kernel_functions(rt)

        expected = [
            'create_process', 'wait_process', 'kill_process', 'terminate_process',
            'process_pid', 'process_exit_code', 'process_is_running',
            'get_process_id',
            'vmem_allocate', 'vmem_free', 'vmem_write', 'vmem_read',
            'vmem_address', 'vmem_size', 'get_page_size',
            'syscall', 'syscall_number',
            'get_uid', 'get_gid', 'get_euid', 'get_egid',
            'get_scheduling_policy', 'set_scheduling_policy',
            'kernel_version',
            'PROT_NONE', 'PROT_READ', 'PROT_WRITE', 'PROT_EXEC',
            'SCHED_OTHER', 'SCHED_FIFO', 'SCHED_RR',
            'SIGINT', 'SIGTERM', 'SIGKILL',
        ]
        for name in expected:
            assert name in rt._funcs, f"Missing: {name}"

    def test_prot_constant_values(self):
        rt = MockRuntime()
        register_kernel_functions(rt)
        assert rt._funcs['PROT_NONE']() == 0
        assert rt._funcs['PROT_READ']() == 1
        assert rt._funcs['PROT_WRITE']() == 2
        assert rt._funcs['PROT_EXEC']() == 4
