"""Focused coverage tests for interpreter scope optimization helpers."""

from types import SimpleNamespace

from nexuslang.interpreter.scope_optimizer import (
    ScopeCache,
    OptimizedScopeLookup,
    enable_scope_optimization,
    disable_scope_optimization,
)


class DummyInterpreter:
    def __init__(self):
        self.current_scope = [{"outer": 1}, {"inner": 2}]

    def get_variable(self, name):
        for scope in reversed(self.current_scope):
            if name in scope:
                return scope[name]
        raise KeyError(name)

    def set_variable(self, name, value):
        self.current_scope[-1][name] = value
        return value

    def enter_scope(self):
        self.current_scope.append({})

    def exit_scope(self):
        if len(self.current_scope) > 1:
            self.current_scope.pop()


class CachedLookupInterpreter(OptimizedScopeLookup, DummyInterpreter):
    def __init__(self, enable_scope_cache: bool):
        super().__init__(enable_scope_cache=enable_scope_cache)
        self._original_get_variable = DummyInterpreter.get_variable.__get__(self, DummyInterpreter)


def test_scope_cache_tracks_hits_misses_and_shadowing():
    scope_stack = [{"x": "outer"}, {"x": "inner", "y": 2}]
    cache = ScopeCache(scope_stack)

    assert cache.get_variable("x") == "inner"
    assert cache.get_variable("x") == "inner"
    assert cache.get_variable("missing") is None
    assert cache.has_variable("y") is True
    assert cache.has_variable("missing") is False

    stats = cache.get_stats()
    assert stats["hits"] >= 2
    assert stats["misses"] >= 1
    assert stats["rebuilds"] >= 1
    assert stats["total_lookups"] == stats["hits"] + stats["misses"]


def test_scope_cache_invalidates_on_scope_depth_change_and_updates_set():
    scope_stack = [{"a": 1}]
    cache = ScopeCache(scope_stack)

    assert cache.get_variable("a") == 1
    scope_stack.append({"b": 2})

    assert cache.get_variable("b") == 2
    cache.set_variable("c", 3)
    assert cache.get_variable("c") == 3

    cache.enter_scope()
    cache.exit_scope()
    cache.invalidate()

    stats = cache.get_stats()
    assert stats["invalidations"] >= 3


def test_optimized_scope_lookup_mixin_paths_with_cache_enabled_and_disabled(capsys):
    cached = CachedLookupInterpreter(enable_scope_cache=True)
    plain = CachedLookupInterpreter(enable_scope_cache=False)

    cached._cached_set_variable("value", 10)
    assert cached._cached_get_variable("value") == 10

    cached._cached_enter_scope()
    cached._cached_set_variable("nested", 11)
    assert cached._cached_get_variable("nested") == 11
    cached._cached_exit_scope()

    assert plain._cached_set_variable("plain", 20) == 20
    assert plain._cached_get_variable("plain") == 20
    assert plain.get_scope_cache_stats() is None
    plain.print_scope_cache_stats()
    assert "not enabled" in capsys.readouterr().out


def test_enable_and_disable_scope_optimization_monkeypatches_methods():
    interpreter = DummyInterpreter()

    original_methods = (
        interpreter.get_variable,
        interpreter.set_variable,
        interpreter.enter_scope,
        interpreter.exit_scope,
    )

    enable_scope_optimization(interpreter)

    interpreter.set_variable("patched", 42)
    assert interpreter.get_variable("patched") == 42
    interpreter.enter_scope()
    interpreter.set_variable("deeper", 99)
    assert interpreter.get_variable("deeper") == 99
    interpreter.exit_scope()

    disable_scope_optimization(interpreter)

    assert not hasattr(interpreter, "_scope_cache")
    assert interpreter.get_variable.__func__ is original_methods[0].__func__
    assert interpreter.set_variable.__func__ is original_methods[1].__func__
    assert interpreter.enter_scope.__func__ is original_methods[2].__func__
    assert interpreter.exit_scope.__func__ is original_methods[3].__func__


def test_enable_scope_optimization_is_idempotent_when_already_enabled():
    interpreter = DummyInterpreter()
    enable_scope_optimization(interpreter)
    cache_obj = interpreter._scope_cache

    enable_scope_optimization(interpreter)

    assert interpreter._scope_cache is cache_obj
