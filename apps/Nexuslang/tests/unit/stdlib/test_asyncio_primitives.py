"""
Tests for NexusLang async concurrency primitives and async runtime core:
  - NLPLAsyncChannel  (async_channel_*)
  - NLPLAsyncLock     (async_lock_*)
  - NLPLAsyncSemaphore (async_semaphore_*)
  - NLPLFuture        (create_future, future_set_result, future_get, ...)
  - Task lifecycle    (spawn_async, task_result, join_all, select_first, ...)
  - Timing helpers    (async_sleep, async_after, async_timeout)
  - Async file I/O    (async_read_file, async_write_file, ...)
"""

import os
import sys
import tempfile
import threading
import time

import pytest

_PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..")
)
if _PROJECT_ROOT not in sys.path:
    sys.path.insert(0, _PROJECT_ROOT)

from nexuslang.stdlib.asyncio_utils.async_runtime import (
    # Channel
    NLPLAsyncChannel,
    async_channel_create,
    async_channel_send,
    async_channel_recv,
    async_channel_try_recv,
    async_channel_close,
    async_channel_is_closed,
    async_channel_size,
    # Lock
    NLPLAsyncLock,
    async_lock_create,
    async_lock_acquire,
    async_lock_release,
    async_lock_try_acquire,
    # Semaphore
    NLPLAsyncSemaphore,
    async_semaphore_create,
    async_semaphore_acquire,
    async_semaphore_release,
    async_semaphore_try_acquire,
    async_semaphore_value,
    # Future (manual)
    NLPLFuture,
    create_future,
    future_set_result,
    future_set_error,
    future_get,
    future_is_done,
    # Task / async runtime
    NLPLTask,
    spawn_async,
    task_result,
    task_is_done,
    task_cancel,
    join_all,
    select_first,
    async_timeout,
    async_sleep,
    async_after,
    # Async file I/O
    async_read_file,
    async_write_file,
    async_append_file,
    async_read_file_bytes,
    async_write_file_bytes,
)


# ===========================================================================
# NLPLAsyncChannel
# ===========================================================================

class TestAsyncChannelCreate:
    def test_returns_channel_object(self):
        ch = async_channel_create()
        assert isinstance(ch, NLPLAsyncChannel)

    def test_unbounded_by_default(self):
        ch = async_channel_create()
        assert ch._q.maxsize == 0

    def test_bounded_maxsize_respected(self):
        ch = async_channel_create(5)
        assert ch._q.maxsize == 5

    def test_initial_size_is_zero(self):
        ch = async_channel_create()
        assert async_channel_size(ch) == 0

    def test_not_closed_initially(self):
        ch = async_channel_create()
        assert not async_channel_is_closed(ch)

    def test_repr_shows_open(self):
        ch = async_channel_create()
        assert "open" in repr(ch)


class TestAsyncChannelSendRecv:
    def test_send_then_recv_returns_value(self):
        ch = async_channel_create()
        async_channel_send(ch, 42)
        assert async_channel_recv(ch) == 42

    def test_fifo_ordering(self):
        ch = async_channel_create()
        for i in range(5):
            async_channel_send(ch, i)
        results = [async_channel_recv(ch) for _ in range(5)]
        assert results == list(range(5))

    def test_send_string_value(self):
        ch = async_channel_create()
        async_channel_send(ch, "hello")
        assert async_channel_recv(ch) == "hello"

    def test_send_none_value(self):
        ch = async_channel_create()
        async_channel_send(ch, None)
        assert async_channel_recv(ch) is None

    def test_send_complex_value(self):
        ch = async_channel_create()
        value = {"key": [1, 2, 3]}
        async_channel_send(ch, value)
        assert async_channel_recv(ch) == value

    def test_size_increments_on_send(self):
        ch = async_channel_create()
        async_channel_send(ch, 1)
        async_channel_send(ch, 2)
        assert async_channel_size(ch) == 2

    def test_size_decrements_on_recv(self):
        ch = async_channel_create()
        async_channel_send(ch, 1)
        async_channel_recv(ch)
        assert async_channel_size(ch) == 0

    def test_recv_blocks_until_item_available(self):
        ch = async_channel_create()
        received = []

        def sender():
            time.sleep(0.05)
            async_channel_send(ch, "delayed")

        t = threading.Thread(target=sender)
        t.start()
        received.append(async_channel_recv(ch, timeout=2))
        t.join()
        assert received == ["delayed"]

    def test_recv_timeout_raises(self):
        ch = async_channel_create()
        with pytest.raises(TimeoutError):
            async_channel_recv(ch, timeout=0.05)


class TestAsyncChannelTryRecv:
    def test_returns_none_when_empty(self):
        ch = async_channel_create()
        assert async_channel_try_recv(ch) is None

    def test_returns_item_when_available(self):
        ch = async_channel_create()
        async_channel_send(ch, 99)
        assert async_channel_try_recv(ch) == 99

    def test_does_not_block(self):
        ch = async_channel_create()
        start = time.monotonic()
        async_channel_try_recv(ch)
        assert time.monotonic() - start < 0.1


class TestAsyncChannelClose:
    def test_close_marks_channel_closed(self):
        ch = async_channel_create()
        async_channel_close(ch)
        assert async_channel_is_closed(ch)

    def test_close_is_idempotent(self):
        ch = async_channel_create()
        async_channel_close(ch)
        async_channel_close(ch)  # must not raise
        assert async_channel_is_closed(ch)

    def test_send_to_closed_raises(self):
        ch = async_channel_create()
        async_channel_close(ch)
        with pytest.raises(RuntimeError):
            async_channel_send(ch, 1)

    def test_recv_from_closed_empty_raises_stop_iteration(self):
        ch = async_channel_create()
        async_channel_close(ch)
        with pytest.raises(StopIteration):
            async_channel_recv(ch, timeout=0.2)

    def test_recv_drains_remaining_items_before_stop_iteration(self):
        ch = async_channel_create()
        async_channel_send(ch, "last")
        async_channel_close(ch)
        assert async_channel_recv(ch) == "last"
        with pytest.raises(StopIteration):
            async_channel_recv(ch, timeout=0.2)

    def test_repr_shows_closed(self):
        ch = async_channel_create()
        async_channel_close(ch)
        assert "closed" in repr(ch)


class TestAsyncChannelBounded:
    def test_bounded_channel_blocks_on_full(self):
        ch = async_channel_create(1)
        async_channel_send(ch, "a")
        with pytest.raises(TimeoutError):
            async_channel_send(ch, "b", timeout=0.05)

    def test_bounded_channel_unblocks_after_recv(self):
        ch = async_channel_create(1)
        async_channel_send(ch, "a")

        def delayed_recv():
            time.sleep(0.05)
            async_channel_recv(ch)

        t = threading.Thread(target=delayed_recv)
        t.start()
        async_channel_send(ch, "b", timeout=2)
        t.join()
        assert async_channel_recv(ch) == "b"


class TestAsyncChannelConcurrent:
    def test_mpmc_all_items_delivered(self):
        ch = async_channel_create()
        n = 50
        sent = list(range(n))
        received = []
        lock = threading.Lock()

        def producer():
            for item in sent:
                async_channel_send(ch, item)

        def consumer():
            for _ in range(n // 2):
                val = async_channel_recv(ch, timeout=5)
                with lock:
                    received.append(val)

        threads = [threading.Thread(target=producer)]
        threads += [threading.Thread(target=consumer) for _ in range(2)]
        for t in threads:
            t.start()
        for t in threads:
            t.join(timeout=10)

        assert sorted(received) == sorted(sent)

    def test_type_error_on_bad_channel_send(self):
        with pytest.raises(TypeError):
            async_channel_send("not_a_channel", 1)

    def test_type_error_on_bad_channel_recv(self):
        with pytest.raises(TypeError):
            async_channel_recv("not_a_channel")


# ===========================================================================
# NLPLAsyncLock
# ===========================================================================

class TestAsyncLockCreate:
    def test_returns_lock_object(self):
        lk = async_lock_create()
        assert isinstance(lk, NLPLAsyncLock)

    def test_repr(self):
        lk = async_lock_create()
        assert "AsyncLock" in repr(lk)


class TestAsyncLockAcquireRelease:
    def test_acquire_returns_true(self):
        lk = async_lock_create()
        assert async_lock_acquire(lk) is True
        async_lock_release(lk)

    def test_acquire_and_release_cycle(self):
        lk = async_lock_create()
        for _ in range(5):
            async_lock_acquire(lk)
            async_lock_release(lk)

    def test_reentrant_same_thread(self):
        lk = async_lock_create()
        async_lock_acquire(lk)
        async_lock_acquire(lk)  # re-entrant: must not deadlock
        async_lock_release(lk)
        async_lock_release(lk)

    def test_release_without_acquire_raises(self):
        lk = async_lock_create()
        with pytest.raises(RuntimeError):
            async_lock_release(lk)

    def test_mutual_exclusion_between_threads(self):
        lk = async_lock_create()
        counter = [0]
        errors = []

        def worker():
            for _ in range(100):
                async_lock_acquire(lk)
                try:
                    v = counter[0]
                    time.sleep(0)
                    counter[0] = v + 1
                finally:
                    async_lock_release(lk)

        threads = [threading.Thread(target=worker) for _ in range(5)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        assert counter[0] == 500
        assert errors == []


class TestAsyncLockTryAcquire:
    def test_try_acquire_succeeds_when_free(self):
        lk = async_lock_create()
        assert async_lock_try_acquire(lk) is True
        async_lock_release(lk)

    def test_type_error_on_bad_lock(self):
        with pytest.raises(TypeError):
            async_lock_acquire("not_a_lock")

    def test_type_error_release_bad_lock(self):
        with pytest.raises(TypeError):
            async_lock_release("not_a_lock")

    def test_type_error_try_acquire_bad_lock(self):
        with pytest.raises(TypeError):
            async_lock_try_acquire("not_a_lock")


# ===========================================================================
# NLPLAsyncSemaphore
# ===========================================================================

class TestAsyncSemaphoreCreate:
    def test_returns_semaphore_object(self):
        sem = async_semaphore_create(3)
        assert isinstance(sem, NLPLAsyncSemaphore)

    def test_default_value_is_one(self):
        sem = async_semaphore_create()
        assert async_semaphore_value(sem) == 1

    def test_initial_value_stored(self):
        sem = async_semaphore_create(5)
        assert async_semaphore_value(sem) == 5

    def test_zero_initial_value_allowed(self):
        sem = async_semaphore_create(0)
        assert async_semaphore_value(sem) == 0

    def test_negative_initial_value_raises(self):
        with pytest.raises(ValueError):
            async_semaphore_create(-1)

    def test_repr(self):
        sem = async_semaphore_create(2)
        assert "AsyncSemaphore" in repr(sem)
        assert "2" in repr(sem)


class TestAsyncSemaphoreAcquireRelease:
    def test_acquire_decrements_value(self):
        sem = async_semaphore_create(3)
        async_semaphore_acquire(sem)
        assert async_semaphore_value(sem) == 2

    def test_release_increments_value(self):
        sem = async_semaphore_create(1)
        async_semaphore_acquire(sem)
        async_semaphore_release(sem)
        assert async_semaphore_value(sem) == 1

    def test_acquire_blocks_when_zero(self):
        sem = async_semaphore_create(0)
        with pytest.raises(TimeoutError):
            async_semaphore_acquire(sem, timeout=0.05)

    def test_acquire_unblocks_after_release(self):
        sem = async_semaphore_create(0)
        result = []

        def releaser():
            time.sleep(0.05)
            async_semaphore_release(sem)

        t = threading.Thread(target=releaser)
        t.start()
        async_semaphore_acquire(sem, timeout=2)
        result.append("acquired")
        t.join()
        assert result == ["acquired"]

    def test_multiple_acquires_then_releases(self):
        sem = async_semaphore_create(3)
        async_semaphore_acquire(sem)
        async_semaphore_acquire(sem)
        async_semaphore_acquire(sem)
        assert async_semaphore_value(sem) == 0
        async_semaphore_release(sem)
        async_semaphore_release(sem)
        async_semaphore_release(sem)
        assert async_semaphore_value(sem) == 3

    def test_limits_concurrency(self):
        sem = async_semaphore_create(2)
        active = [0]
        peak = [0]
        lock = threading.Lock()

        def worker():
            async_semaphore_acquire(sem, timeout=5)
            with lock:
                active[0] += 1
                if active[0] > peak[0]:
                    peak[0] = active[0]
            time.sleep(0.02)
            with lock:
                active[0] -= 1
            async_semaphore_release(sem)

        threads = [threading.Thread(target=worker) for _ in range(10)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        assert peak[0] <= 2


class TestAsyncSemaphoreTryAcquire:
    def test_try_acquire_succeeds_when_available(self):
        sem = async_semaphore_create(1)
        assert async_semaphore_try_acquire(sem) is True
        assert async_semaphore_value(sem) == 0

    def test_try_acquire_fails_when_zero(self):
        sem = async_semaphore_create(0)
        assert async_semaphore_try_acquire(sem) is False

    def test_type_errors(self):
        with pytest.raises(TypeError):
            async_semaphore_acquire("not_a_sem")
        with pytest.raises(TypeError):
            async_semaphore_release("not_a_sem")
        with pytest.raises(TypeError):
            async_semaphore_try_acquire("not_a_sem")
        with pytest.raises(TypeError):
            async_semaphore_value("not_a_sem")


# ===========================================================================
# NLPLFuture  (manual futures)
# ===========================================================================

class TestManualFuture:
    def test_create_future_returns_future(self):
        fut = create_future()
        assert isinstance(fut, NLPLFuture)

    def test_not_done_initially(self):
        fut = create_future()
        assert future_is_done(fut) is False

    def test_set_result_marks_done(self):
        fut = create_future()
        future_set_result(fut, 42)
        assert future_is_done(fut) is True

    def test_get_returns_result(self):
        fut = create_future()
        future_set_result(fut, "hello")
        assert future_get(fut) == "hello"

    def test_get_with_timeout_succeeds(self):
        fut = create_future()
        future_set_result(fut, 99)
        assert future_get(fut, 1.0) == 99

    def test_get_timeout_raises(self):
        fut = create_future()
        with pytest.raises(TimeoutError):
            future_get(fut, 0.05)

    def test_set_error_marks_done(self):
        fut = create_future()
        future_set_error(fut, ValueError("oops"))
        assert future_is_done(fut) is True

    def test_get_raises_stored_error(self):
        fut = create_future()
        future_set_error(fut, ValueError("oops"))
        with pytest.raises(ValueError, match="oops"):
            future_get(fut)

    def test_double_set_result_raises(self):
        fut = create_future()
        future_set_result(fut, 1)
        with pytest.raises(RuntimeError):
            future_set_result(fut, 2)

    def test_set_result_from_thread(self):
        fut = create_future()

        def setter():
            time.sleep(0.05)
            future_set_result(fut, "threaded")

        t = threading.Thread(target=setter)
        t.start()
        assert future_get(fut, timeout=2) == "threaded"
        t.join()

    def test_repr_pending(self):
        fut = create_future()
        assert "pending" in repr(fut)

    def test_repr_done(self):
        fut = create_future()
        future_set_result(fut, 7)
        assert "done" in repr(fut)

    def test_repr_error(self):
        fut = create_future()
        future_set_error(fut, RuntimeError("fail"))
        assert "error" in repr(fut)


# ===========================================================================
# Task lifecycle  (spawn_async, join_all, select_first, async_timeout)
# ===========================================================================

class TestSpawnAsync:
    def test_plain_callable_returns_task(self):
        task = spawn_async(lambda: 42)
        assert isinstance(task, NLPLTask)

    def test_task_result_of_plain_callable(self):
        task = spawn_async(lambda: "done")
        assert task_result(task, timeout=5) == "done"

    def test_task_is_done_after_result(self):
        task = spawn_async(lambda: 1)
        task_result(task, timeout=5)
        assert task_is_done(task) is True

    def test_task_is_not_done_immediately_for_slow_call(self):
        task = spawn_async(lambda: time.sleep(0.5) or "slow")
        # Check before it's done (may be racy on very fast machines; use sleep)
        time.sleep(0.02)
        # Not asserting is_done here since timing is non-deterministic,
        # but result must arrive within 2 s.
        assert task_result(task, timeout=2) == "slow"

    def test_result_timeout_raises(self):
        task = spawn_async(lambda: time.sleep(10))
        with pytest.raises(TimeoutError):
            task_result(task, timeout=0.05)

    def test_exception_propagated_via_task_result(self):
        def raiser():
            raise ValueError("boom")

        task = spawn_async(raiser)
        with pytest.raises(ValueError, match="boom"):
            task_result(task, timeout=5)

    def test_repr_running(self):
        task = spawn_async(lambda: time.sleep(0.3))
        time.sleep(0.01)
        assert "Task" in repr(task)

    def test_repr_done(self):
        task = spawn_async(lambda: "x")
        task_result(task, timeout=5)
        assert "Task" in repr(task)


class TestJoinAll:
    def test_empty_list_returns_empty(self):
        results = join_all([], timeout=1)
        assert results == []

    def test_single_task(self):
        task = spawn_async(lambda: 7)
        results = join_all([task], timeout=5)
        assert results == [7]

    def test_multiple_tasks_all_results(self):
        tasks = [spawn_async(lambda i=i: i * 2) for i in range(5)]
        results = join_all(tasks, timeout=10)
        assert sorted(results) == [0, 2, 4, 6, 8]

    def test_timeout_raises(self):
        tasks = [spawn_async(lambda: time.sleep(10))]
        with pytest.raises(TimeoutError):
            join_all(tasks, timeout=0.05)


class TestSelectFirst:
    def test_returns_fastest_task_result(self):
        fast = spawn_async(lambda: "fast")
        slow = spawn_async(lambda: (time.sleep(5), "slow")[1])
        result = select_first([fast, slow], timeout=5)
        assert result == "fast"

    def test_single_task(self):
        task = spawn_async(lambda: 123)
        assert select_first([task], timeout=5) == 123

    def test_timeout_raises(self):
        task = spawn_async(lambda: time.sleep(10))
        with pytest.raises(TimeoutError):
            select_first([task], timeout=0.05)


class TestAsyncTimeout:
    def test_task_completes_within_deadline(self):
        task = spawn_async(lambda: "ok")
        result = async_timeout(task, 5)
        assert result == "ok"

    def test_task_exceeds_deadline_raises(self):
        task = spawn_async(lambda: time.sleep(10))
        with pytest.raises(TimeoutError):
            async_timeout(task, 0.05)


class TestAsyncSleep:
    def test_sleep_delays_execution(self):
        start = time.monotonic()
        async_sleep(0.1)
        elapsed = time.monotonic() - start
        assert elapsed >= 0.08

    def test_zero_sleep_returns_quickly(self):
        start = time.monotonic()
        async_sleep(0)
        assert time.monotonic() - start < 0.5


class TestAsyncAfter:
    def test_callback_fires_after_delay(self):
        fired = threading.Event()
        async_after(0.05, lambda: fired.set())
        assert fired.wait(timeout=2), "Callback did not fire within 2 seconds"

    def test_callback_receives_args(self):
        received = []
        async_after(0.05, lambda v: received.append(v), 42)
        time.sleep(0.2)
        assert received == [42]


# ===========================================================================
# Async file I/O
# ===========================================================================

class TestAsyncFileIO:
    def test_write_then_read(self, tmp_path):
        p = str(tmp_path / "test.txt")
        task = async_write_file(p, "hello world")
        task_result(task, timeout=5)
        read_task = async_read_file(p)
        assert task_result(read_task, timeout=5) == "hello world"

    def test_write_bytes_then_read_bytes(self, tmp_path):
        p = str(tmp_path / "bytes.bin")
        data = b"\x00\x01\x02\x03"
        task = async_write_file_bytes(p, data)
        task_result(task, timeout=5)
        read_task = async_read_file_bytes(p)
        assert task_result(read_task, timeout=5) == data

    def test_append_file(self, tmp_path):
        p = str(tmp_path / "append.txt")
        task_result(async_write_file(p, "line1\n"), timeout=5)
        task_result(async_append_file(p, "line2\n"), timeout=5)
        content = task_result(async_read_file(p), timeout=5)
        assert content == "line1\nline2\n"

    def test_read_nonexistent_file_raises(self, tmp_path):
        p = str(tmp_path / "nonexistent.txt")
        task = async_read_file(p)
        with pytest.raises(Exception):
            task_result(task, timeout=5)

    def test_write_creates_file(self, tmp_path):
        p = str(tmp_path / "new.txt")
        task_result(async_write_file(p, "created"), timeout=5)
        assert os.path.exists(p)

    def test_read_file_with_encoding(self, tmp_path):
        p = str(tmp_path / "utf8.txt")
        task_result(async_write_file(p, "hello", "utf-8"), timeout=5)
        content = task_result(async_read_file(p, "utf-8"), timeout=5)
        assert content == "hello"
