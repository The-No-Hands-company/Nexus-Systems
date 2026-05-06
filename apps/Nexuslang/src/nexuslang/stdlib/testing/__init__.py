"""
Testing framework module for NexusLang.
Provides assertion functions and test running capabilities.
"""

from typing import Any, Callable, List, Dict, Optional
from ...runtime.runtime import Runtime
import traceback
import time

class TestResult:
    """Represents the result of a test."""
    def __init__(self, name: str, passed: bool, error: Optional[str] = None, duration: float = 0.0):
        self.name = name
        self.passed = passed
        self.error = error
        self.duration = duration
    
    def __str__(self):
        status = " PASS" if self.passed else " FAIL"
        result = f"{status} {self.name} ({self.duration:.3f}s)"
        if self.error:
            result += f"\n  Error: {self.error}"
        return result

class TestSuite:
    """Manages a collection of tests."""
    def __init__(self, name: str):
        self.name = name
        self.tests: List[Callable] = []
        self.results: List[TestResult] = []
        self.setup_func: Optional[Callable] = None
        self.teardown_func: Optional[Callable] = None
    
    def add_test(self, test_func: Callable, name: str):
        """Add a test to the suite."""
        self.tests.append((name, test_func))
    
    def run(self) -> Dict[str, Any]:
        """Run all tests in the suite."""
        print(f"\n{'='*60}")
        print(f"Running test suite: {self.name}")
        print(f"{'='*60}\n")
        
        passed = 0
        failed = 0
        total_time = 0.0
        
        for test_name, test_func in self.tests:
            # Run setup if defined
            if self.setup_func:
                try:
                    self.setup_func()
                except Exception as e:
                    result = TestResult(test_name, False, f"Setup failed: {e}")
                    self.results.append(result)
                    print(result)
                    failed += 1
                    continue
            
            # Run the test
            start_time = time.time()
            try:
                test_func()
                duration = time.time() - start_time
                result = TestResult(test_name, True, duration=duration)
                self.results.append(result)
                print(result)
                passed += 1
                total_time += duration
            except AssertionError as e:
                duration = time.time() - start_time
                result = TestResult(test_name, False, str(e), duration)
                self.results.append(result)
                print(result)
                failed += 1
                total_time += duration
            except Exception as e:
                duration = time.time() - start_time
                error_msg = f"{type(e).__name__}: {e}"
                result = TestResult(test_name, False, error_msg, duration)
                self.results.append(result)
                print(result)
                failed += 1
                total_time += duration
            
            # Run teardown if defined
            if self.teardown_func:
                try:
                    self.teardown_func()
                except Exception as e:
                    print(f"  Warning: Teardown failed: {e}")
        
        # Print summary
        total = passed + failed
        pass_rate = (passed / total * 100) if total > 0 else 0
        
        print(f"\n{'='*60}")
        print(f"Test Results: {passed}/{total} passed ({pass_rate:.1f}%)")
        print(f"Total time: {total_time:.3f}s")
        print(f"{'='*60}\n")
        
        return {
            'total': total,
            'passed': passed,
            'failed': failed,
            'pass_rate': pass_rate,
            'duration': total_time
        }

class AssertionError(Exception):
    """Custom assertion error."""
    pass

# Global test suite registry
_test_suites: Dict[str, TestSuite] = {}
_current_suite: Optional[TestSuite] = None


def assert_true(value: Any, message: str = ""):
    """Assert that a value is true."""
    if not value:
        raise AssertionError(f"Expected true, got {value}. {message}")


def assert_false(value: Any, message: str = ""):
    """Assert that a value is false."""
    if value:
        raise AssertionError(f"Expected false, got {value}. {message}")


def assert_equal(actual: Any, expected: Any, message: str = ""):
    """Assert that two values are equal."""
    if actual != expected:
        raise AssertionError(f"Expected {expected}, got {actual}. {message}")


def assert_not_equal(actual: Any, expected: Any, message: str = ""):
    """Assert that two values are not equal."""
    if actual == expected:
        raise AssertionError(f"Expected values to be different, both are {actual}. {message}")


def assert_greater(actual: Any, expected: Any, message: str = ""):
    """Assert that actual > expected."""
    if not actual > expected:
        raise AssertionError(f"Expected {actual} > {expected}. {message}")


def assert_less(actual: Any, expected: Any, message: str = ""):
    """Assert that actual < expected."""
    if not actual < expected:
        raise AssertionError(f"Expected {actual} < {expected}. {message}")


def assert_greater_equal(actual: Any, expected: Any, message: str = ""):
    """Assert that actual >= expected."""
    if not actual >= expected:
        raise AssertionError(f"Expected {actual} >= {expected}. {message}")


def assert_less_equal(actual: Any, expected: Any, message: str = ""):
    """Assert that actual <= expected."""
    if not actual <= expected:
        raise AssertionError(f"Expected {actual} <= {expected}. {message}")


def assert_null(value: Any, message: str = ""):
    """Assert that a value is null/None."""
    if value is not None:
        raise AssertionError(f"Expected null, got {value}. {message}")


def assert_not_null(value: Any, message: str = ""):
    """Assert that a value is not null/None."""
    if value is None:
        raise AssertionError(f"Expected non-null value. {message}")


def assert_contains(container: Any, item: Any, message: str = ""):
    """Assert that container contains item."""
    if item not in container:
        raise AssertionError(f"Expected {container} to contain {item}. {message}")


def assert_not_contains(container: Any, item: Any, message: str = ""):
    """Assert that container does not contain item."""
    if item in container:
        raise AssertionError(f"Expected {container} not to contain {item}. {message}")


def assert_type(value: Any, expected_type: type, message: str = ""):
    """Assert that value is of expected type."""
    if not isinstance(value, expected_type):
        raise AssertionError(f"Expected type {expected_type.__name__}, got {type(value).__name__}. {message}")


def assert_raises(func: Callable, *args, **kwargs):
    """Assert that function raises an exception."""
    try:
        func(*args, **kwargs)
        raise AssertionError(f"Expected {func.__name__} to raise an exception, but it didn't")
    except Exception:
        pass


def create_test_suite(name: str) -> TestSuite:
    """Create a new test suite."""
    suite = TestSuite(name)
    _test_suites[name] = suite
    global _current_suite
    _current_suite = suite
    return suite


def run_test_suite(name: str) -> Dict[str, Any]:
    """Run a test suite by name."""
    if name not in _test_suites:
        raise ValueError(f"Test suite '{name}' not found")
    return _test_suites[name].run()


def run_all_tests() -> Dict[str, Any]:
    """Run all registered test suites."""
    total_passed = 0
    total_failed = 0
    total_time = 0.0

    for suite_name in _test_suites:
        results = run_test_suite(suite_name)
        total_passed += results['passed']
        total_failed += results['failed']
        total_time += results['duration']

    total = total_passed + total_failed
    pass_rate = (total_passed / total * 100) if total > 0 else 0

    print(f"\n{'='*60}")
    print(f"OVERALL RESULTS: {total_passed}/{total} tests passed ({pass_rate:.1f}%)")
    print(f"Total time: {total_time:.3f}s")
    print(f"{'='*60}\n")

    return {
        'total': total,
        'passed': total_passed,
        'failed': total_failed,
        'pass_rate': pass_rate,
        'duration': total_time
    }


def benchmark(func: Callable, iterations: int = 1000) -> float:
    """Benchmark a function and return average execution time."""
    start = time.time()
    for _ in range(iterations):
        func()
    duration = time.time() - start
    avg = duration / iterations
    print(f"Benchmark: {iterations} iterations in {duration:.3f}s (avg: {avg*1000:.3f}ms)")
    return avg


def _register_assertion_functions(runtime: Runtime) -> None:
    runtime.register_function("assert_true", assert_true)
    runtime.register_function("assert_false", assert_false)
    runtime.register_function("assert_equal", assert_equal)
    runtime.register_function("assert_not_equal", assert_not_equal)
    runtime.register_function("assert_greater", assert_greater)
    runtime.register_function("assert_less", assert_less)
    runtime.register_function("assert_greater_equal", assert_greater_equal)
    runtime.register_function("assert_less_equal", assert_less_equal)
    runtime.register_function("assert_null", assert_null)
    runtime.register_function("assert_not_null", assert_not_null)
    runtime.register_function("assert_contains", assert_contains)
    runtime.register_function("assert_not_contains", assert_not_contains)
    runtime.register_function("assert_type", assert_type)
    runtime.register_function("assert_raises", assert_raises)


def _register_assertion_aliases(runtime: Runtime) -> None:
    runtime.register_function("assertTrue", assert_true)
    runtime.register_function("assertFalse", assert_false)
    runtime.register_function("assertEqual", assert_equal)
    runtime.register_function("assertNotEqual", assert_not_equal)
    runtime.register_function("assertGreater", assert_greater)
    runtime.register_function("assertLess", assert_less)


def _register_test_management_functions(runtime: Runtime) -> None:
    runtime.register_function("create_test_suite", create_test_suite)
    runtime.register_function("run_test_suite", run_test_suite)
    runtime.register_function("run_all_tests", run_all_tests)
    runtime.register_function("benchmark", benchmark)

def register_testing_functions(runtime: Runtime) -> None:
    """Register testing functions with the runtime."""
    _register_assertion_functions(runtime)
    _register_assertion_aliases(runtime)
    _register_test_management_functions(runtime)
