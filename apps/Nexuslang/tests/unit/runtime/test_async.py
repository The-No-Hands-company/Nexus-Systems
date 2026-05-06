"""Tests for the async/await runtime module."""
import pytest
import sys
import os
import time
import threading

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from nexuslang.stdlib.asyncio_utils.async_runtime import (
    NLPLFuture,
    NLPLTask,
    spawn_async,
    task_result,
    task_is_done,
    task_cancel,
    join_all,
    select_first,
    async_sleep,
    async_after,
    create_future,
    future_set_result,
    future_set_error,
    future_get,
    future_is_done,
    async_write_file,
    async_read_file,
    register_async_runtime_functions,
)


# ---------------------------------------------------------------------------
# MockRuntime
# ---------------------------------------------------------------------------

class MockRuntime:
    def __init__(self):
        self._funcs = {}

    def register_function(self, name, fn):
        self._funcs[name] = fn


# ---------------------------------------------------------------------------
# NLPLFuture
# ---------------------------------------------------------------------------

class TestNLPLFuture:
    def test_set_and_get_result(self):
        f = NLPLFuture()
        assert not f.is_done()
        f.set_result(42)
        assert f.is_done()
        assert f.get() == 42

    def test_set_error_raises_on_get(self):
        f = NLPLFuture()
        f.set_error(RuntimeError("test error"))
        assert f.is_done()
        with pytest.raises(RuntimeError, match="test error"):
            f.get()

    def test_get_with_timeout_raises(self):
        f = NLPLFuture()
        with pytest.raises(TimeoutError):
            f.get(timeout=0.01)

    def test_get_waits_for_result(self):
        f = NLPLFuture()

        def set_later():
            time.sleep(0.05)
            f.set_result("done")

        t = threading.Thread(target=set_later, daemon=True)
        t.start()
        result = f.get(timeout=1.0)
        assert result == "done"

    def test_double_set_result_raises(self):
        f = NLPLFuture()
        f.set_result(1)
        with pytest.raises(RuntimeError, match="already"):
            f.set_result(2)


# ---------------------------------------------------------------------------
# spawn_async / task helpers
# ---------------------------------------------------------------------------

class TestSpawnAsync:
    def test_spawn_and_get_result(self):
        task = spawn_async(lambda: 99)
        result = task_result(task, timeout=2.0)
        assert result == 99

    def test_task_is_done(self):
        task = spawn_async(lambda: 'hello')
        task_result(task, timeout=2.0)  # wait for completion
        assert task_is_done(task)

    def test_spawn_multiple_tasks(self):
        tasks = [spawn_async(lambda i=i: i * 2) for i in range(5)]
        results = [task_result(t, timeout=2.0) for t in tasks]
        assert sorted(results) == [0, 2, 4, 6, 8]

    def test_task_with_args(self):
        task = spawn_async(lambda a, b: a + b, 3, 4)
        result = task_result(task, timeout=2.0)
        assert result == 7

    def test_spawn_exception_propagates(self):
        def raises():
            raise ValueError("async error")

        task = spawn_async(raises)
        with pytest.raises(ValueError, match="async error"):
            task_result(task, timeout=2.0)


# ---------------------------------------------------------------------------
# join_all / select_first
# ---------------------------------------------------------------------------

class TestJoinAll:
    def test_join_all_returns_all_results(self):
        tasks = [spawn_async(lambda v=v: v) for v in [10, 20, 30]]
        results = join_all(tasks, timeout=5.0)
        assert sorted(results) == [10, 20, 30]

    def test_join_all_empty(self):
        results = join_all([], timeout=1.0)
        assert results == []

    def test_join_all_timeout(self):
        def slow():
            time.sleep(10)
            return 1

        tasks = [spawn_async(slow)]
        with pytest.raises(TimeoutError):
            join_all(tasks, timeout=0.05)

    def test_select_first(self):
        def fast():
            time.sleep(0.05)
            return 'fast'

        def slow():
            time.sleep(60.0)
            return 'slow'

        tasks = [spawn_async(slow), spawn_async(fast)]
        first = select_first(tasks, timeout=10.0)
        assert first == 'fast'


# ---------------------------------------------------------------------------
# async_sleep / async_after
# ---------------------------------------------------------------------------

class TestAsyncSleepAfter:
    def test_async_sleep(self):
        start = time.time()
        async_sleep(0.05)
        elapsed = time.time() - start
        assert elapsed >= 0.04  # at least 40ms

    def test_async_after(self):
        result_holder = []

        def callback():
            result_holder.append('called')

        task = async_after(0.05, callback)
        # Wait for the delayed callback to finish
        task_result(task, timeout=5.0)
        assert 'called' in result_holder


# ---------------------------------------------------------------------------
# create_future / future helpers
# ---------------------------------------------------------------------------

class TestFutureHelpers:
    def test_create_set_get(self):
        f = create_future()
        assert not future_is_done(f)
        future_set_result(f, 'value')
        assert future_is_done(f)
        assert future_get(f, timeout=1.0) == 'value'

    def test_future_set_error(self):
        f = create_future()
        future_set_error(f, ValueError("test"))
        with pytest.raises(ValueError):
            future_get(f, timeout=1.0)


# ---------------------------------------------------------------------------
# Async file I/O
# ---------------------------------------------------------------------------

class TestAsyncFileIO:
    def test_write_and_read(self, tmp_path):
        path = str(tmp_path / 'test.txt')
        task_w = async_write_file(path, 'hello async world')
        task_result(task_w, timeout=5.0)

        task_r = async_read_file(path)
        content = task_result(task_r, timeout=5.0)
        assert content == 'hello async world'

    def test_read_nonexistent_raises(self, tmp_path):
        path = str(tmp_path / 'does_not_exist.txt')
        task = async_read_file(path)
        with pytest.raises(Exception):
            task_result(task, timeout=5.0)


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

class TestRegisterAsyncRuntimeFunctions:
    def test_all_functions_registered(self):
        rt = MockRuntime()
        register_async_runtime_functions(rt)

        expected = [
            'spawn_async', 'task_result', 'task_is_done', 'task_cancel',
            'join_all', 'select_first', 'async_timeout', 'async_sleep', 'async_after',
            'create_future', 'future_set_result', 'future_set_error',
            'future_get', 'future_is_done',
            'async_read_file', 'async_read_file_bytes',
            'async_write_file', 'async_write_file_bytes', 'async_append_file',
        ]
        for name in expected:
            assert name in rt._funcs, f"Missing function: {name}"

    def test_registered_spawn_async_works(self):
        rt = MockRuntime()
        register_async_runtime_functions(rt)
        spawn = rt._funcs['spawn_async']
        get = rt._funcs['task_result']
        # Registered functions receive runtime as the first argument
        task = spawn(rt, lambda: 'registered test')
        result = get(rt, task, 2.0)
        assert result == 'registered test'
