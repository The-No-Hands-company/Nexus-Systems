"""
NLPL Standard Library - Property Testing Module
Provides property-based testing: strategies/generators, property assertions,
counterexample shrinking, and test statistics.

All strategies are plain dicts with a 'type': 'strategy' discriminant and a
'kind' field specifying the generator kind.

Result of property_test():
    {'passed': bool, 'run_count': int, 'failure_count': int,
     'counterexample': list|None, 'shrunk_counterexample': list|None,
     'error': str|None}

Usage pattern:
    s = gen_integer(0, 100)
    result = property_test(lambda x: x >= 0, [s], count=200)
"""

from __future__ import annotations

import math
import random
import string
import traceback
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from nexuslang.runtime.runtime import Runtime

# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

_PRINTABLE = string.ascii_letters + string.digits + string.punctuation + " "
_ASCII_ALPHA = string.ascii_letters
_ASCII_ALPHANUM = string.ascii_letters + string.digits


def _make_strategy(kind: str, **params) -> dict:
    return {"type": "strategy", "kind": kind, **params}


def _draw(strategy: dict, rng: random.Random):
    """Recursively draw one value from a strategy using the given RNG."""
    if not isinstance(strategy, dict) or strategy.get("type") != "strategy":
        raise TypeError(f"Expected a strategy dict, got: {type(strategy).__name__}")

    kind = strategy["kind"]

    if kind == "integer":
        return rng.randint(strategy["min_val"], strategy["max_val"])

    if kind == "float":
        lo, hi = strategy["min_val"], strategy["max_val"]
        return lo + rng.random() * (hi - lo)

    if kind == "bool":
        return rng.random() < 0.5

    if kind == "none":
        return None

    if kind == "constant":
        return strategy["value"]

    if kind == "choice":
        values = strategy["values"]
        if not values:
            raise ValueError("gen_choice requires a non-empty list")
        return rng.choice(values)

    if kind == "from_list":
        values = strategy["values"]
        if not values:
            raise ValueError("gen_from_list requires a non-empty list")
        return rng.choice(values)

    if kind == "string":
        alphabet = strategy.get("alphabet") or _PRINTABLE
        lo = max(0, strategy["min_len"])
        hi = max(lo, strategy["max_len"])
        length = rng.randint(lo, hi)
        return "".join(rng.choice(alphabet) for _ in range(length))

    if kind == "text":
        lo = max(0, strategy["min_len"])
        hi = max(lo, strategy["max_len"])
        length = rng.randint(lo, hi)
        return "".join(rng.choice(_PRINTABLE) for _ in range(length))

    if kind == "list":
        lo = max(0, strategy["min_size"])
        hi = max(lo, strategy["max_size"])
        size = rng.randint(lo, hi)
        return [_draw(strategy["element_strategy"], rng) for _ in range(size)]

    if kind == "dict":
        lo = max(0, strategy["min_size"])
        hi = max(lo, strategy["max_size"])
        size = rng.randint(lo, hi)
        return {_draw(strategy["key_strategy"], rng): _draw(strategy["value_strategy"], rng)
                for _ in range(size)}

    if kind == "tuple":
        return [_draw(s, rng) for s in strategy["strategies"]]

    if kind == "one_of":
        strategies = strategy["strategies"]
        if not strategies:
            raise ValueError("gen_one_of requires at least one strategy")
        chosen = rng.choice(strategies)
        return _draw(chosen, rng)

    if kind == "optional":
        if rng.random() < 0.15:
            return None
        return _draw(strategy["inner"], rng)

    if kind == "filtered":
        inner = strategy["inner"]
        fn = strategy["fn"]
        max_attempts = strategy.get("max_attempts", 100)
        for _ in range(max_attempts):
            val = _draw(inner, rng)
            try:
                if fn(val):
                    return val
            except Exception:
                pass
        raise RuntimeError(
            f"strategy_filter: could not find a passing value in {max_attempts} attempts"
        )

    if kind == "mapped":
        inner = strategy["inner"]
        fn = strategy["fn"]
        val = _draw(inner, rng)
        return fn(val)

    if kind == "fixed_dict":
        return {k: _draw(v_strategy, rng) for k, v_strategy in strategy["pairs"].items()}

    if kind == "text_lines":
        line_count = strategy["count"]
        line_strat = _make_strategy("text", min_len=0, max_len=80)
        return [_draw(line_strat, rng) for _ in range(line_count)]

    raise ValueError(f"Unknown strategy kind: {kind!r}")


def _draw_all(strategies: list, rng: random.Random) -> list:
    return [_draw(s, rng) for s in strategies]


class _SkipExample(Exception):
    """Raised inside property functions to indicate the example should be skipped."""
    pass


def _call_property(fn, args: list):
    """Call fn with unpacked args. Returns (passed: bool, error: str|None)."""
    try:
        result = fn(*args)
        if result is None:
            return True, None
        return bool(result), None
    except _SkipExample:
        return None, "skip"
    except AssertionError as exc:
        return False, str(exc) or "AssertionError"
    except Exception as exc:
        return False, f"{type(exc).__name__}: {exc}"


# ---------------------------------------------------------------------------
# Shrinking
# ---------------------------------------------------------------------------

def _shrink_integer(val: int, lo: int, hi: int, fn) -> int:
    """Try to find a smaller integer in [lo, hi] that still causes failure."""
    best = val
    # Binary search toward lo
    low, high = lo, val
    while low < high:
        mid = (low + high) // 2
        ok, _ = _call_property(fn, [mid])
        if ok is False:
            best = mid
            high = mid
        else:
            low = mid + 1
    return best


def _shrink_list(val: list, fn) -> list:
    """Try to remove elements one by one to find smaller failing list."""
    best = val[:]
    changed = True
    while changed:
        changed = False
        i = 0
        while i < len(best):
            candidate = best[:i] + best[i + 1:]
            ok, _ = _call_property(fn, [candidate])
            if ok is False:
                best = candidate
                changed = True
            else:
                i += 1
    return best


def _shrink_string(val: str, fn) -> str:
    """Try to find shorter string that still causes failure."""
    # Reduce length
    best = val
    changed = True
    while changed:
        changed = False
        if len(best) == 0:
            break
        for i in range(len(best)):
            candidate = best[:i] + best[i + 1:]
            ok, _ = _call_property(fn, [candidate])
            if ok is False:
                best = candidate
                changed = True
                break
    return best


def _shrink_value(val, strategy: dict, fn) -> object:
    """Best-effort shrink of a value according to its strategy."""
    kind = strategy.get("kind", "")
    try:
        if kind == "integer":
            lo = strategy["min_val"]
            hi = strategy["max_val"]
            if isinstance(val, int):
                return _shrink_integer(val, lo, hi, fn)
        elif kind == "list":
            if isinstance(val, list):
                return _shrink_list(val, fn)
        elif kind in ("string", "text"):
            if isinstance(val, str):
                return _shrink_string(val, fn)
        elif kind == "float":
            lo = strategy["min_val"]
            if isinstance(val, float) and val > lo:
                # Try halving distance to lo
                for _ in range(20):
                    candidate = (val + lo) / 2
                    ok, _ = _call_property(fn, [candidate])
                    if ok is False:
                        val = candidate
                    else:
                        break
                return val
    except Exception:
        pass
    return val


def _shrink_counterexample(fn, strategies: list, failing_args: list) -> list:
    """Attempt to reduce counterexample to minimal form."""
    if len(strategies) == 1:
        shrunk = _shrink_value(
            failing_args[0], strategies[0], lambda v: fn(v)
        )
        return [shrunk]
    # Multi-arg: shrink each independently
    best = failing_args[:]
    for idx, (val, strat) in enumerate(zip(failing_args, strategies)):
        def make_tester(fixed_args, replace_idx):
            def tester(v):
                args = fixed_args[:]
                args[replace_idx] = v
                res, _ = _call_property(fn, args)
                return res
            return tester

        tester = make_tester(best[:], idx)
        best[idx] = _shrink_value(val, strat, tester)
    return best


# ---------------------------------------------------------------------------
# Public API — Generators
# ---------------------------------------------------------------------------

def gen_integer(min_val, max_val):
    """Return a strategy that generates integers in [min_val, max_val]."""
    min_val = int(min_val)
    max_val = int(max_val)
    if min_val > max_val:
        raise ValueError(f"gen_integer: min_val ({min_val}) > max_val ({max_val})")
    return _make_strategy("integer", min_val=min_val, max_val=max_val)


def gen_float(min_val, max_val):
    """Return a strategy that generates floats in [min_val, max_val)."""
    min_val = float(min_val)
    max_val = float(max_val)
    if min_val > max_val:
        raise ValueError(f"gen_float: min_val ({min_val}) > max_val ({max_val})")
    if math.isnan(min_val) or math.isnan(max_val):
        raise ValueError("gen_float: NaN bounds not allowed")
    return _make_strategy("float", min_val=min_val, max_val=max_val)


def gen_bool():
    """Return a strategy that generates True or False with equal probability."""
    return _make_strategy("bool")


def gen_none():
    """Return a strategy that always generates None."""
    return _make_strategy("none")


def gen_constant(value):
    """Return a strategy that always generates the given constant value."""
    return _make_strategy("constant", value=value)


def gen_choice(values):
    """Return a strategy that uniformly picks one value from the given list."""
    if not isinstance(values, (list, tuple)):
        raise TypeError("gen_choice: values must be a list or tuple")
    if len(values) == 0:
        raise ValueError("gen_choice: values must not be empty")
    return _make_strategy("choice", values=list(values))


def gen_from_list(values):
    """Alias for gen_choice — uniform random pick from a list of values."""
    if not isinstance(values, (list, tuple)):
        raise TypeError("gen_from_list: values must be a list or tuple")
    if len(values) == 0:
        raise ValueError("gen_from_list: values must not be empty")
    return _make_strategy("from_list", values=list(values))


def gen_string(min_len=0, max_len=20, alphabet=None):
    """Return a strategy that generates strings of printable characters.

    Args:
        min_len: minimum string length (default 0)
        max_len: maximum string length (default 20)
        alphabet: characters to use (default: all printable ASCII)
    """
    min_len = max(0, int(min_len))
    max_len = max(min_len, int(max_len))
    if alphabet is not None and not isinstance(alphabet, str):
        raise TypeError("gen_string: alphabet must be a string or None")
    return _make_strategy("string", min_len=min_len, max_len=max_len,
                           alphabet=alphabet)


def gen_text(min_len=0, max_len=50):
    """Return a strategy that generates printable text strings."""
    min_len = max(0, int(min_len))
    max_len = max(min_len, int(max_len))
    return _make_strategy("text", min_len=min_len, max_len=max_len)


def gen_list(element_strategy, min_size=0, max_size=10):
    """Return a strategy that generates lists of values from element_strategy.

    Args:
        element_strategy: strategy for list elements
        min_size: minimum list length (default 0)
        max_size: maximum list length (default 10)
    """
    if not isinstance(element_strategy, dict) or element_strategy.get("type") != "strategy":
        raise TypeError("gen_list: element_strategy must be a strategy dict")
    min_size = max(0, int(min_size))
    max_size = max(min_size, int(max_size))
    return _make_strategy("list", element_strategy=element_strategy,
                           min_size=min_size, max_size=max_size)


def gen_dict(key_strategy, value_strategy, min_size=0, max_size=5):
    """Return a strategy that generates dicts with keys/values from given strategies.

    Args:
        key_strategy: strategy for dict keys
        value_strategy: strategy for dict values
        min_size: minimum number of entries (default 0)
        max_size: maximum number of entries (default 5)
    """
    for name, s in [("key_strategy", key_strategy), ("value_strategy", value_strategy)]:
        if not isinstance(s, dict) or s.get("type") != "strategy":
            raise TypeError(f"gen_dict: {name} must be a strategy dict")
    min_size = max(0, int(min_size))
    max_size = max(min_size, int(max_size))
    return _make_strategy("dict", key_strategy=key_strategy,
                           value_strategy=value_strategy,
                           min_size=min_size, max_size=max_size)


def gen_tuple(strategies):
    """Return a strategy that generates fixed-length tuples (as lists) of values.

    Each element uses the corresponding strategy from the strategies list.

    Args:
        strategies: list of strategies, one per tuple position
    """
    if not isinstance(strategies, (list, tuple)):
        raise TypeError("gen_tuple: strategies must be a list")
    for i, s in enumerate(strategies):
        if not isinstance(s, dict) or s.get("type") != "strategy":
            raise TypeError(f"gen_tuple: strategies[{i}] must be a strategy dict")
    return _make_strategy("tuple", strategies=list(strategies))


def gen_one_of(strategies):
    """Return a strategy that randomly picks one strategy and draws from it.

    Args:
        strategies: list of strategies to choose among
    """
    if not isinstance(strategies, (list, tuple)):
        raise TypeError("gen_one_of: strategies must be a list")
    if len(strategies) == 0:
        raise ValueError("gen_one_of: strategies must not be empty")
    for i, s in enumerate(strategies):
        if not isinstance(s, dict) or s.get("type") != "strategy":
            raise TypeError(f"gen_one_of: strategies[{i}] must be a strategy dict")
    return _make_strategy("one_of", strategies=list(strategies))


def gen_optional(strategy):
    """Return a strategy that generates None about 15% of the time, otherwise draws from strategy.

    Args:
        strategy: inner strategy
    """
    if not isinstance(strategy, dict) or strategy.get("type") != "strategy":
        raise TypeError("gen_optional: argument must be a strategy dict")
    return _make_strategy("optional", inner=strategy)


def gen_fixed_dict(pairs):
    """Return a strategy that generates dicts with fixed keys and strategy-drawn values.

    Args:
        pairs: dict mapping key names to strategies
    """
    if not isinstance(pairs, dict):
        raise TypeError("gen_fixed_dict: pairs must be a dict")
    for k, v in pairs.items():
        if not isinstance(v, dict) or v.get("type") != "strategy":
            raise TypeError(f"gen_fixed_dict: pairs[{k!r}] must be a strategy dict")
    return _make_strategy("fixed_dict", pairs=dict(pairs))


def gen_text_lines(count=5):
    """Return a strategy that generates a list of 'count' lines of random text.

    Args:
        count: number of lines to generate (default 5)
    """
    count = max(0, int(count))
    return _make_strategy("text_lines", count=count)


# ---------------------------------------------------------------------------
# Strategy transforms
# ---------------------------------------------------------------------------

def strategy_filter(strategy, fn, max_attempts=100):
    """Return a strategy that only yields values for which fn(value) is truthy.

    Args:
        strategy: inner strategy
        fn: callable(value) -> bool
        max_attempts: how many rejection attempts before raising RuntimeError
    """
    if not isinstance(strategy, dict) or strategy.get("type") != "strategy":
        raise TypeError("strategy_filter: first argument must be a strategy dict")
    if not callable(fn):
        raise TypeError("strategy_filter: fn must be callable")
    max_attempts = max(1, int(max_attempts))
    return _make_strategy("filtered", inner=strategy, fn=fn,
                           max_attempts=max_attempts)


def strategy_map(strategy, fn):
    """Return a strategy that applies fn to drawn values.

    Args:
        strategy: inner strategy
        fn: callable(value) -> transformed_value
    """
    if not isinstance(strategy, dict) or strategy.get("type") != "strategy":
        raise TypeError("strategy_map: first argument must be a strategy dict")
    if not callable(fn):
        raise TypeError("strategy_map: fn must be callable")
    return _make_strategy("mapped", inner=strategy, fn=fn)


def strategy_draw(strategy, seed=None):
    """Draw a single value from a strategy.

    Args:
        strategy: strategy to draw from
        seed: optional random seed for reproducibility
    Returns:
        a drawn value
    """
    if not isinstance(strategy, dict) or strategy.get("type") != "strategy":
        raise TypeError("strategy_draw: argument must be a strategy dict")
    rng = random.Random(seed)
    return _draw(strategy, rng)


# ---------------------------------------------------------------------------
# Property test execution
# ---------------------------------------------------------------------------

def property_test(fn, strategies, count=100, seed=None):
    """Run fn against randomly generated arguments and report results.

    fn is called as fn(*args) where args[i] is drawn from strategies[i].
    fn should return True/falsy or raise an assertion error on failure.
    None return is treated as passed. Returning False is treated as failure.

    Args:
        fn: callable(arg0, arg1, ...) to test
        strategies: list of strategies, one per argument
        count: number of examples to run (default 100)
        seed: optional integer seed for reproducibility

    Returns:
        dict with keys:
            passed (bool), run_count (int), skip_count (int),
            failure_count (int), counterexample (list|None),
            shrunk_counterexample (list|None), error (str|None)
    """
    if not callable(fn):
        raise TypeError("property_test: fn must be callable")
    if not isinstance(strategies, (list, tuple)):
        raise TypeError("property_test: strategies must be a list")
    for i, s in enumerate(strategies):
        if not isinstance(s, dict) or s.get("type") != "strategy":
            raise TypeError(f"property_test: strategies[{i}] must be a strategy dict")
    count = max(1, int(count))

    rng = random.Random(seed)
    run_count = 0
    skip_count = 0
    failure_count = 0
    counterexample = None
    shrunk_counterexample = None
    last_error = None

    attempts = 0
    max_attempts = count * 10

    while run_count < count and attempts < max_attempts:
        attempts += 1
        try:
            args = _draw_all(list(strategies), rng)
        except Exception as exc:
            last_error = f"Strategy draw failed: {exc}"
            break

        ok, err = _call_property(fn, args)

        if ok is None and err == "skip":
            skip_count += 1
            continue

        run_count += 1

        if ok is False:
            failure_count += 1
            counterexample = args
            last_error = err
            # Attempt shrinking
            try:
                shrunk_counterexample = _shrink_counterexample(
                    fn, list(strategies), args
                )
            except Exception:
                shrunk_counterexample = args
            break

    return {
        "passed": failure_count == 0 and last_error is None,
        "run_count": run_count,
        "skip_count": skip_count,
        "failure_count": failure_count,
        "counterexample": counterexample,
        "shrunk_counterexample": shrunk_counterexample,
        "error": last_error,
    }


def property_assert(fn, strategies, count=100, seed=None):
    """Assert that property fn holds for all generated examples.

    Raises RuntimeError with details if a counterexample is found.

    Args:
        fn: property function (same as property_test)
        strategies: list of strategies
        count: number of examples (default 100)
        seed: optional seed
    """
    result = property_test(fn, strategies, count=count, seed=seed)
    if not result["passed"]:
        ce = result.get("shrunk_counterexample") or result.get("counterexample")
        err = result.get("error", "property failed")
        raise RuntimeError(
            f"Property falsified after {result['run_count']} examples.\n"
            f"Counterexample: {ce!r}\n"
            f"Error: {err}"
        )
    return True


def property_find_counterexample(fn, strategies, count=100, seed=None):
    """Return the first found counterexample, or None if none found.

    Args:
        fn: property function
        strategies: list of strategies
        count: number of examples to try (default 100)
        seed: optional seed

    Returns:
        list of arguments that cause fn to fail, or None
    """
    result = property_test(fn, strategies, count=count, seed=seed)
    if not result["passed"]:
        return result.get("shrunk_counterexample") or result.get("counterexample")
    return None


def property_find_example(fn, strategies, count=100, seed=None):
    """Return the first generated example for which fn returns truthy.

    Args:
        fn: predicate function
        strategies: list of strategies
        count: number of examples to try (default 100)
        seed: optional seed

    Returns:
        list of arguments for which fn passed, or None
    """
    if not callable(fn):
        raise TypeError("property_find_example: fn must be callable")
    if not isinstance(strategies, (list, tuple)):
        raise TypeError("property_find_example: strategies must be a list")
    count = max(1, int(count))
    rng = random.Random(seed)
    for _ in range(count):
        try:
            args = _draw_all(list(strategies), rng)
            ok, _ = _call_property(fn, args)
            if ok is True:
                return args
        except Exception:
            pass
    return None


def property_statistics(fn, strategies, count=100, seed=None):
    """Collect statistics about a property over generated examples.

    Args:
        fn: property function (should return True/False or raise)
        strategies: list of strategies
        count: number of examples (default 100)
        seed: optional seed

    Returns:
        dict with: total (int), passed (int), failed (int), skipped (int),
                   pass_rate (float), fail_rate (float)
    """
    if not callable(fn):
        raise TypeError("property_statistics: fn must be callable")
    if not isinstance(strategies, (list, tuple)):
        raise TypeError("property_statistics: strategies must be a list")
    count = max(1, int(count))
    rng = random.Random(seed)

    passed = 0
    failed = 0
    skipped = 0

    for _ in range(count):
        try:
            args = _draw_all(list(strategies), rng)
            ok, err = _call_property(fn, args)
            if ok is None and err == "skip":
                skipped += 1
            elif ok is False:
                failed += 1
            else:
                passed += 1
        except Exception:
            failed += 1

    total = passed + failed
    return {
        "total": total,
        "passed": passed,
        "failed": failed,
        "skipped": skipped,
        "pass_rate": passed / total if total > 0 else 1.0,
        "fail_rate": failed / total if total > 0 else 0.0,
    }


def property_shrink(fn, strategies, failing_args):
    """Attempt to minimize a counterexample by shrinking each argument.

    Args:
        fn: property function that fails on failing_args
        strategies: list of strategies (one per argument)
        failing_args: list of argument values that cause the property to fail

    Returns:
        list of minimized argument values, or failing_args if shrinking fails
    """
    if not callable(fn):
        raise TypeError("property_shrink: fn must be callable")
    if not isinstance(strategies, (list, tuple)):
        raise TypeError("property_shrink: strategies must be a list")
    if not isinstance(failing_args, (list, tuple)):
        raise TypeError("property_shrink: failing_args must be a list")
    try:
        return _shrink_counterexample(fn, list(strategies), list(failing_args))
    except Exception:
        return list(failing_args)


def property_report(result):
    """Format a property_test result dict into a human-readable report string.

    Args:
        result: dict returned by property_test()

    Returns:
        str — multi-line report
    """
    if not isinstance(result, dict):
        raise TypeError("property_report: result must be a dict from property_test()")
    lines = []
    status = "PASSED" if result.get("passed") else "FAILED"
    lines.append(f"Property test: {status}")
    lines.append(f"  Examples run    : {result.get('run_count', 0)}")
    lines.append(f"  Skipped         : {result.get('skip_count', 0)}")
    lines.append(f"  Failures        : {result.get('failure_count', 0)}")
    if not result.get("passed"):
        ce = result.get("shrunk_counterexample") or result.get("counterexample")
        if ce is not None:
            lines.append(f"  Counterexample  : {ce!r}")
        err = result.get("error")
        if err:
            lines.append(f"  Error           : {err}")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Assumption helper
# ---------------------------------------------------------------------------

def property_assume(condition):
    """Skip the current generated example if condition is falsy.

    Call this at the start of a property function to filter invalid inputs.
    Raises _SkipExample (caught internally by property_test) when condition
    is False.

    Args:
        condition: boolean — if falsy, example is skipped
    """
    if not condition:
        raise _SkipExample()


# ---------------------------------------------------------------------------
# Convenience combinators
# ---------------------------------------------------------------------------

def gen_positive_integer(max_val=1000):
    """Shortcut: generate integers in [1, max_val]."""
    max_val = max(1, int(max_val))
    return gen_integer(1, max_val)


def gen_non_negative_integer(max_val=1000):
    """Shortcut: generate integers in [0, max_val]."""
    max_val = max(0, int(max_val))
    return gen_integer(0, max_val)


def gen_ascii_string(min_len=1, max_len=20):
    """Shortcut: generate ASCII alphanumeric strings."""
    min_len = max(0, int(min_len))
    max_len = max(min_len, int(max_len))
    return gen_string(min_len=min_len, max_len=max_len, alphabet=_ASCII_ALPHANUM)


def gen_non_empty_list(element_strategy, max_size=10):
    """Shortcut: generate non-empty lists (min_size=1)."""
    if not isinstance(element_strategy, dict) or element_strategy.get("type") != "strategy":
        raise TypeError("gen_non_empty_list: element_strategy must be a strategy dict")
    max_size = max(1, int(max_size))
    return gen_list(element_strategy, min_size=1, max_size=max_size)


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

def register_property_testing_functions(runtime: "Runtime") -> None:
    """Register all property testing functions with the NexusLang runtime."""

    # Generators
    runtime.register_function("gen_integer", gen_integer)
    runtime.register_function("gen_float", gen_float)
    runtime.register_function("gen_bool", gen_bool)
    runtime.register_function("gen_none", gen_none)
    runtime.register_function("gen_constant", gen_constant)
    runtime.register_function("gen_choice", gen_choice)
    runtime.register_function("gen_from_list", gen_from_list)
    runtime.register_function("gen_string", gen_string)
    runtime.register_function("gen_text", gen_text)
    runtime.register_function("gen_list", gen_list)
    runtime.register_function("gen_dict", gen_dict)
    runtime.register_function("gen_tuple", gen_tuple)
    runtime.register_function("gen_one_of", gen_one_of)
    runtime.register_function("gen_optional", gen_optional)
    runtime.register_function("gen_fixed_dict", gen_fixed_dict)
    runtime.register_function("gen_text_lines", gen_text_lines)
    runtime.register_function("gen_positive_integer", gen_positive_integer)
    runtime.register_function("gen_non_negative_integer", gen_non_negative_integer)
    runtime.register_function("gen_ascii_string", gen_ascii_string)
    runtime.register_function("gen_non_empty_list", gen_non_empty_list)

    # Strategy transforms
    runtime.register_function("strategy_filter", strategy_filter)
    runtime.register_function("strategy_map", strategy_map)
    runtime.register_function("strategy_draw", strategy_draw)

    # Property tests
    runtime.register_function("property_test", property_test)
    runtime.register_function("property_assert", property_assert)
    runtime.register_function("property_find_counterexample", property_find_counterexample)
    runtime.register_function("property_find_example", property_find_example)
    runtime.register_function("property_statistics", property_statistics)
    runtime.register_function("property_shrink", property_shrink)
    runtime.register_function("property_report", property_report)
    runtime.register_function("property_assume", property_assume)

    # Module aliases
    runtime.register_module("property_testing")
    runtime.register_module("prop_test")
