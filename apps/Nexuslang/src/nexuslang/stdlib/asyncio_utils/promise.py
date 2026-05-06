"""
Promise<T> type for NexusLang standard library.

Represents an asynchronous computation that will eventually produce a value.
Supports async/await patterns and then/catch chaining.
"""

from typing import Any, Callable, Optional as PyOptional
from concurrent.futures import ThreadPoolExecutor, Future
import threading


class PromiseState:
    """Enum for Promise states."""
    PENDING = "pending"
    FULFILLED = "fulfilled"
    REJECTED = "rejected"


class Promise:
    """Represents an asynchronous computation."""
    
    # Shared thread pool for all promises
    _executor = None
    _executor_lock = threading.Lock()
    
    @classmethod
    def get_executor(cls) -> ThreadPoolExecutor:
        """Get the shared thread pool executor."""
        if cls._executor is None:
            with cls._executor_lock:
                if cls._executor is None:
                    cls._executor = ThreadPoolExecutor(max_workers=10)
        return cls._executor
    
    def __init__(self, executor_fn: Callable[[Callable, Callable], None] = None):
        """
        Create a new Promise.
        
        Args:
            executor_fn: Function that takes (resolve, reject) callbacks
        """
        self.state = PromiseState.PENDING
        self._value: Any = None
        self._error: Any = None
        self._then_callbacks: list = []
        self._catch_callbacks: list = []
        self._finally_callbacks: list = []
        self._lock = threading.Lock()
        
        if executor_fn:
            try:
                executor_fn(self._resolve, self._reject)
            except Exception as e:
                self._reject(e)
    
    def _resolve(self, value: Any):
        """Mark promise as fulfilled with a value."""
        with self._lock:
            if self.state != PromiseState.PENDING:
                return
            
            self.state = PromiseState.FULFILLED
            self._value = value
            
            # Execute then callbacks
            for callback in self._then_callbacks:
                try:
                    callback(value)
                except Exception as e:
                    # Errors in then callbacks are caught by catch handlers
                    for catch_cb in self._catch_callbacks:
                        try:
                            catch_cb(e)
                        except:
                            pass
            
            # Execute finally callbacks
            for callback in self._finally_callbacks:
                try:
                    callback()
                except:
                    pass
    
    def _reject(self, error: Any):
        """Mark promise as rejected with an error."""
        with self._lock:
            if self.state != PromiseState.PENDING:
                return
            
            self.state = PromiseState.REJECTED
            self._error = error
            
            # Execute catch callbacks
            if self._catch_callbacks:
                for callback in self._catch_callbacks:
                    try:
                        callback(error)
                    except:
                        pass
            
            # Execute finally callbacks
            for callback in self._finally_callbacks:
                try:
                    callback()
                except:
                    pass
    
    def then(self, on_fulfilled: Callable, on_rejected: Callable = None) -> 'Promise':
        """
        Add a fulfillment handler.
        
        Args:
            on_fulfilled: Called when promise is fulfilled
            on_rejected: Called when promise is rejected (optional)
        
        Returns:
            New promise that resolves to the callback's return value
        """
        new_promise = Promise()
        
        def handle_fulfillment(value):
            try:
                result = on_fulfilled(value)
                if isinstance(result, Promise):
                    result.then(new_promise._resolve, new_promise._reject)
                else:
                    new_promise._resolve(result)
            except Exception as e:
                new_promise._reject(e)
        
        def handle_rejection(error):
            if on_rejected:
                try:
                    result = on_rejected(error)
                    new_promise._resolve(result)
                except Exception as e:
                    new_promise._reject(e)
            else:
                new_promise._reject(error)
        
        with self._lock:
            if self.state == PromiseState.FULFILLED:
                handle_fulfillment(self._value)
            elif self.state == PromiseState.REJECTED:
                handle_rejection(self._error)
            else:
                self._then_callbacks.append(handle_fulfillment)
                if on_rejected:
                    self._catch_callbacks.append(handle_rejection)
                else:
                    self._catch_callbacks.append(lambda e: new_promise._reject(e))
        
        return new_promise
    
    def catch(self, on_rejected: Callable) -> 'Promise':
        """
        Add an error handler.
        
        Args:
            on_rejected: Called when promise is rejected
        
        Returns:
            New promise
        """
        return self.then(lambda x: x, on_rejected)
    
    def finally_(self, on_finally: Callable) -> 'Promise':
        """
        Add a handler that runs regardless of success or failure.
        
        Args:
            on_finally: Called when promise settles
        
        Returns:
            New promise
        """
        new_promise = Promise()
        
        def handle_finally():
            try:
                on_finally()
                if self.state == PromiseState.FULFILLED:
                    new_promise._resolve(self._value)
                else:
                    new_promise._reject(self._error)
            except Exception as e:
                new_promise._reject(e)
        
        with self._lock:
            if self.state != PromiseState.PENDING:
                handle_finally()
            else:
                self._finally_callbacks.append(handle_finally)
        
        return new_promise
    
    def get(self, timeout: PyOptional[float] = None) -> Any:
        """
        Block and wait for the promise to resolve (sync operation).
        
        Args:
            timeout: Maximum time to wait in seconds
        
        Returns:
            The resolved value
        
        Raises:
            Exception if promise is rejected or timeout occurs
        """
        import time
        start_time = time.time()
        
        while self.state == PromiseState.PENDING:
            if timeout and (time.time() - start_time) > timeout:
                raise TimeoutError("Promise timed out")
            time.sleep(0.01)
        
        if self.state == PromiseState.FULFILLED:
            return self._value
        else:
            raise Exception(f"Promise rejected: {self._error}")
    
    def is_pending(self) -> bool:
        """Check if promise is pending."""
        return self.state == PromiseState.PENDING
    
    def is_fulfilled(self) -> bool:
        """Check if promise is fulfilled."""
        return self.state == PromiseState.FULFILLED
    
    def is_rejected(self) -> bool:
        """Check if promise is rejected."""
        return self.state == PromiseState.REJECTED
    
    @staticmethod
    def resolve(value: Any) -> 'Promise':
        """Create a fulfilled promise."""
        promise = Promise()
        promise._resolve(value)
        return promise
    
    @staticmethod
    def reject(error: Any) -> 'Promise':
        """Create a rejected promise."""
        promise = Promise()
        promise._reject(error)
        return promise
    
    @staticmethod
    def all(promises: list) -> 'Promise':
        """
        Wait for all promises to resolve.
        
        Returns:
            Promise that resolves to list of all values
        """
        result_promise = Promise()
        results = [None] * len(promises)
        completed = [0]  # Mutable counter
        lock = threading.Lock()
        
        if not promises:
            result_promise._resolve([])
            return result_promise
        
        def check_complete():
            with lock:
                if completed[0] == len(promises):
                    result_promise._resolve(results)
        
        for i, p in enumerate(promises):
            def make_handler(index):
                def handler(value):
                    results[index] = value
                    completed[0] += 1
                    check_complete()
                return handler
            
            def error_handler(error):
                result_promise._reject(error)
            
            p.then(make_handler(i), error_handler)
        
        return result_promise
    
    @staticmethod
    def race(promises: list) -> 'Promise':
        """
        Wait for the first promise to resolve or reject.
        
        Returns:
            Promise that resolves/rejects with the first settled promise's value/error
        """
        result_promise = Promise()
        settled = [False]
        lock = threading.Lock()
        
        def resolve_handler(value):
            with lock:
                if not settled[0]:
                    settled[0] = True
                    result_promise._resolve(value)
        
        def reject_handler(error):
            with lock:
                if not settled[0]:
                    settled[0] = True
                    result_promise._reject(error)
        
        for p in promises:
            p.then(resolve_handler, reject_handler)
        
        return result_promise
    
    def __repr__(self):
        if self.state == PromiseState.FULFILLED:
            return f"Promise<fulfilled: {self._value!r}>"
        elif self.state == PromiseState.REJECTED:
            return f"Promise<rejected: {self._error!r}>"
        return "Promise<pending>"


def create_promise(executor_fn: Callable) -> Promise:
    """Create a new Promise."""
    return Promise(executor_fn)


def promise_resolve(value: Any) -> Promise:
    """Create a resolved promise."""
    return Promise.resolve(value)


def promise_reject(error: Any) -> Promise:
    """Create a rejected promise."""
    return Promise.reject(error)


def async_task(fn: Callable, *args, **kwargs) -> Promise:
    """
    Run a function asynchronously and return a Promise.
    
    Args:
        fn: Function to run
        *args: Positional arguments
        **kwargs: Keyword arguments
    
    Returns:
        Promise that resolves to the function's return value
    """
    promise = Promise()
    
    def run_task():
        try:
            result = fn(*args, **kwargs)
            promise._resolve(result)
        except Exception as e:
            promise._reject(str(e))
    
    executor = Promise.get_executor()
    executor.submit(run_task)
    
    return promise


def register_promise_functions(runtime):
    """Register Promise type functions with the runtime."""
    
    def create_promise_wrapper(executor_fn):
        """Create a new Promise."""
        return create_promise(executor_fn)
    
    def promise_then(promise, on_fulfilled, on_rejected=None):
        """Add then handler."""
        if isinstance(promise, Promise):
            return promise.then(on_fulfilled, on_rejected)
        raise ValueError("Not a Promise")
    
    def promise_catch(promise, on_rejected):
        """Add catch handler."""
        if isinstance(promise, Promise):
            return promise.catch(on_rejected)
        raise ValueError("Not a Promise")
    
    def promise_finally(promise, on_finally):
        """Add finally handler."""
        if isinstance(promise, Promise):
            return promise.finally_(on_finally)
        raise ValueError("Not a Promise")
    
    def promise_get(promise, timeout=None):
        """Block and wait for promise."""
        if isinstance(promise, Promise):
            return promise.get(timeout)
        raise ValueError("Not a Promise")
    
    def promise_all_wrapper(promises):
        """Wait for all promises."""
        return Promise.all(promises)
    
    def promise_race_wrapper(promises):
        """Race promises."""
        return Promise.race(promises)
    
    def is_pending(promise):
        """Check if pending."""
        if isinstance(promise, Promise):
            return promise.is_pending()
        return False
    
    def is_fulfilled(promise):
        """Check if fulfilled."""
        if isinstance(promise, Promise):
            return promise.is_fulfilled()
        return False
    
    def is_rejected(promise):
        """Check if rejected."""
        if isinstance(promise, Promise):
            return promise.is_rejected()
        return False
    
    # Register functions
    runtime.register_function("Promise", create_promise_wrapper)
    runtime.register_function("promise_resolve", promise_resolve)
    runtime.register_function("promise_reject", promise_reject)
    runtime.register_function("async_task", async_task)
    runtime.register_function("promise_then", promise_then)
    runtime.register_function("promise_catch", promise_catch)
    runtime.register_function("promise_finally", promise_finally)
    runtime.register_function("promise_get", promise_get)
    runtime.register_function("promise_all", promise_all_wrapper)
    runtime.register_function("promise_race", promise_race_wrapper)
    runtime.register_function("promise_is_pending", is_pending)
    runtime.register_function("promise_is_fulfilled", is_fulfilled)
    runtime.register_function("promise_is_rejected", is_rejected)
