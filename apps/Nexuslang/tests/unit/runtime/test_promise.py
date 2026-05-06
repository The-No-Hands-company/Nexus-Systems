"""Tests for Promise<T> async utilities."""

import pytest
import time
from nexuslang.stdlib.asyncio_utils.promise import Promise, async_task


class TestPromiseBasics:
    """Test basic Promise creation and execution."""

    def test_promise_resolve(self):
        """Test Promise resolves with a value."""
        def executor(resolve, reject):
            resolve(42)

        promise = Promise(executor)
        result = promise.get()
        assert result == 42
        assert promise.is_fulfilled()
        assert not promise.is_rejected()

    def test_promise_reject(self):
        """Test Promise rejects with an error."""
        def executor(resolve, reject):
            reject("Error occurred")

        promise = Promise(executor)
        with pytest.raises(Exception, match="Error occurred"):
            promise.get()
        assert promise.is_rejected()
        assert not promise.is_fulfilled()

    def test_promise_static_resolve(self):
        """Test Promise.resolve() creates fulfilled promise."""
        promise = Promise.resolve(100)
        assert promise.get() == 100
        assert promise.is_fulfilled()

    def test_promise_static_reject(self):
        """Test Promise.reject() creates rejected promise."""
        promise = Promise.reject("Static error")
        with pytest.raises(Exception, match="Static error"):
            promise.get()
        assert promise.is_rejected()


class TestPromiseChaining:
    """Test Promise then/catch/finally chaining."""

    def test_then_chain(self):
        """Test .then() chains transformations."""
        def executor(resolve, reject):
            resolve(10)

        promise = Promise(executor)
        result = promise.then(lambda x: x * 2).then(lambda x: x + 5).get()
        assert result == 25

    def test_catch_handles_rejection(self):
        """Test .catch() handles rejected promises."""
        def executor(resolve, reject):
            reject("Initial error")

        promise = Promise(executor)
        result = promise.catch(lambda err: f"Handled: {err}").get()
        assert result == "Handled: Initial error"

    def test_catch_not_called_on_success(self):
        """Test .catch() is skipped for fulfilled promises."""
        def executor(resolve, reject):
            resolve(42)

        called = [False]
        def catch_handler(err):
            called[0] = True
            return "Should not be called"

        result = promise = Promise(executor).catch(catch_handler).get()
        assert result == 42
        assert not called[0]

    def test_finally_always_called(self):
        """Test .finally() runs regardless of success/failure."""
        called = [0]
        def finally_handler():
            called[0] += 1

        # Success case
        Promise.resolve(1).finally_(finally_handler).get()
        assert called[0] == 1

        # Failure case
        try:
            Promise.reject("error").finally_(finally_handler).get()
        except:
            pass
        assert called[0] == 2

    def test_mixed_chain(self):
        """Test complex chain with then/catch/finally."""
        def executor(resolve, reject):
            resolve(5)

        finalized = [False]
        def finalize():
            finalized[0] = True

        result = (
            Promise(executor)
            .then(lambda x: x * 2)
            .catch(lambda err: 0)
            .then(lambda x: x + 10)
            .finally_(finalize)
            .get()
        )
        assert result == 20
        assert finalized[0]


class TestPromiseStatic:
    """Test Promise.all() and Promise.race()."""

    def test_promise_all_success(self):
        """Test Promise.all() with all fulfilled promises."""
        promises = [
            Promise.resolve(1),
            Promise.resolve(2),
            Promise.resolve(3),
        ]
        result = Promise.all(promises).get()
        assert result == [1, 2, 3]

    def test_promise_all_reject(self):
        """Test Promise.all() rejects if any promise rejects."""
        promises = [
            Promise.resolve(1),
            Promise.reject("Error"),
            Promise.resolve(3),
        ]
        with pytest.raises(Exception, match="Error"):
            Promise.all(promises).get()

    def test_promise_race_first_wins(self):
        """Test Promise.race() resolves with first completed promise."""
        import threading
        
        # Use a more reliable approach - one promise delays, other resolves immediately
        result = Promise.race([
            Promise.resolve("immediate"),
            Promise(lambda resolve, reject: (time.sleep(0.5), resolve("delayed"))),
        ]).get(timeout=2)
        
        # The immediate one should win
        assert result == "immediate"


class TestAsyncTask:
    """Test async_task wrapper for async functions."""

    def test_async_task_execution(self):
        """Test async_task runs function asynchronously."""
        def compute(x, y):
            return x + y

        promise = async_task(compute, 10, 20)
        result = promise.get()
        assert result == 30

    def test_async_task_with_exception(self):
        """Test async_task handles exceptions."""
        def failing_task():
            raise ValueError("Task failed")

        promise = async_task(failing_task)
        with pytest.raises(Exception, match="Task failed"):
            promise.get()

    def test_async_task_chaining(self):
        """Test async_task result can be chained."""
        def compute(x):
            return x * 2

        result = async_task(compute, 5).then(lambda x: x + 10).get()
        assert result == 20


class TestPromiseEdgeCases:
    """Test Promise edge cases and error conditions."""

    def test_get_timeout(self):
        """Test Promise.get() timeout with async task."""
        # Use async_task which actually runs in thread pool
        def slow_task():
            time.sleep(2)
            return "done"

        promise = async_task(slow_task)
        with pytest.raises(TimeoutError):
            promise.get(timeout=0.1)

    def test_executor_exception(self):
        """Test Promise handles executor exceptions."""
        def bad_executor(resolve, reject):
            raise RuntimeError("Executor failed")

        promise = Promise(bad_executor)
        with pytest.raises(Exception, match="Executor failed"):
            promise.get()

    def test_double_resolve_ignored(self):
        """Test resolving a promise twice ignores second call."""
        def executor(resolve, reject):
            resolve(1)
            resolve(2)  # Should be ignored

        promise = Promise(executor)
        assert promise.get() == 1

    def test_resolve_after_reject_ignored(self):
        """Test resolve after reject is ignored."""
        def executor(resolve, reject):
            reject("error")
            resolve(42)  # Should be ignored

        promise = Promise(executor)
        with pytest.raises(Exception, match="error"):
            promise.get()

    def test_empty_all(self):
        """Test Promise.all([]) resolves to empty list."""
        result = Promise.all([]).get()
        assert result == []

    def test_race_with_one_promise(self):
        """Test Promise.race() with single promise."""
        result = Promise.race([Promise.resolve(42)]).get()
        assert result == 42
