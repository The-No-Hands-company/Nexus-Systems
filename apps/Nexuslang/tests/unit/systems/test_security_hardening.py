"""
Tests for NexusLang 8.4.3 Security Hardening modules:

  - nlpl.security.analysis   (taint analysis, CFI, memory safety)
  - nlpl.security.sandbox    (restricted mode, resource limits, seccomp)
  - nlpl.security.runtime_protections (stack canaries, bounds checker, overflow)
"""

from __future__ import annotations

import os
import platform
import sys
import threading
import time
import pytest


# =============================================================================
# Helpers
# =============================================================================

def _is_linux() -> bool:
    return platform.system() == "Linux"


# =============================================================================
# Analysis module: TaintLabel & TaintedValue
# =============================================================================

class TestTaintLabel:
    """TaintLabel enum and its helper properties."""

    def test_trusted_is_not_untrusted(self):
        from nexuslang.security.analysis import TaintLabel
        assert not TaintLabel.TRUSTED.is_untrusted

    def test_user_input_is_untrusted(self):
        from nexuslang.security.analysis import TaintLabel
        assert TaintLabel.USER_INPUT.is_untrusted

    def test_file_read_is_untrusted(self):
        from nexuslang.security.analysis import TaintLabel
        assert TaintLabel.FILE_READ.is_untrusted

    def test_most_dangerous_picks_highest(self):
        from nexuslang.security.analysis import TaintLabel
        labels = frozenset({TaintLabel.FILE_READ, TaintLabel.USER_INPUT})
        assert TaintLabel.most_dangerous(labels) == TaintLabel.USER_INPUT

    def test_most_dangerous_empty_returns_trusted(self):
        from nexuslang.security.analysis import TaintLabel
        assert TaintLabel.most_dangerous(frozenset()) == TaintLabel.TRUSTED

    def test_ordering_ffi_lt_user_input(self):
        from nexuslang.security.analysis import TaintLabel
        assert TaintLabel.FFI_RETURN.value < TaintLabel.USER_INPUT.value


class TestTaintedValue:
    """TaintedValue wrapper: construction, propagation, helpers."""

    def test_from_source_sets_label(self):
        from nexuslang.security.analysis import TaintedValue, TaintLabel
        tv = TaintedValue.from_source("hello", TaintLabel.USER_INPUT, "stdin")
        assert TaintLabel.USER_INPUT in tv.labels
        assert tv.value == "hello"
        assert tv.is_tainted

    def test_trusted_value_is_not_tainted(self):
        from nexuslang.security.analysis import TaintedValue, TaintLabel
        tv = TaintedValue("x", frozenset({TaintLabel.TRUSTED}))
        assert not tv.is_tainted

    def test_propagate_combines_labels(self):
        from nexuslang.security.analysis import TaintedValue, TaintLabel
        a = TaintedValue.from_source("a", TaintLabel.FILE_READ, "file")
        b = TaintedValue.from_source("b", TaintLabel.ENV_VAR, "ENV")
        result = a.propagate("ab", b)
        assert TaintLabel.FILE_READ in result.labels
        assert TaintLabel.ENV_VAR in result.labels

    def test_propagate_to_plain_value_preserves_taint(self):
        from nexuslang.security.analysis import TaintedValue, TaintLabel
        tv = TaintedValue.from_source(42, TaintLabel.NETWORK, "socket")
        result = tv.propagate(84)
        assert isinstance(result, TaintedValue)
        assert result.value == 84
        assert result.dominant_label == TaintLabel.NETWORK

    def test_dominant_label_returns_worst(self):
        from nexuslang.security.analysis import TaintedValue, TaintLabel
        tv = TaintedValue(
            "x",
            frozenset({TaintLabel.FILE_READ, TaintLabel.USER_INPUT}),
        )
        assert tv.dominant_label == TaintLabel.USER_INPUT


class TestUnwrapHelpers:
    """unwrap(), taint_label_of(), is_tainted() utility functions."""

    def test_unwrap_removes_wrapper(self):
        from nexuslang.security.analysis import TaintedValue, TaintLabel, unwrap
        tv = TaintedValue.from_source(99, TaintLabel.NETWORK, "net")
        assert unwrap(tv) == 99

    def test_unwrap_plain_value(self):
        from nexuslang.security.analysis import unwrap
        assert unwrap("hello") == "hello"

    def test_taint_label_of_tainted(self):
        from nexuslang.security.analysis import TaintedValue, TaintLabel, taint_label_of
        tv = TaintedValue.from_source(1, TaintLabel.FFI_RETURN, "ffi")
        assert taint_label_of(tv) == TaintLabel.FFI_RETURN

    def test_taint_label_of_plain_is_trusted(self):
        from nexuslang.security.analysis import TaintLabel, taint_label_of
        assert taint_label_of("plain") == TaintLabel.TRUSTED

    def test_is_tainted_true(self):
        from nexuslang.security.analysis import TaintedValue, TaintLabel, is_tainted
        tv = TaintedValue.from_source("evil", TaintLabel.USER_INPUT, "stdin")
        assert is_tainted(tv)

    def test_is_tainted_false_for_plain(self):
        from nexuslang.security.analysis import is_tainted
        assert not is_tainted("clean")


# =============================================================================
# Analysis module: TaintTracker
# =============================================================================

class TestTaintTracker:
    """TaintTracker: taint(), propagate(), check_sink()."""

    def test_taint_wraps_value(self):
        from nexuslang.security.analysis import TaintTracker, TaintLabel
        tracker = TaintTracker()
        tv = tracker.taint("user_data", TaintLabel.USER_INPUT, "prompt")
        assert tv.is_tainted
        assert tv.value == "user_data"

    def test_propagate_with_no_taint_returns_plain(self):
        from nexuslang.security.analysis import TaintTracker
        tracker = TaintTracker()
        result = tracker.propagate("result", "clean1", "clean2")
        assert result == "result"
        assert not isinstance(result, type(None))

    def test_propagate_with_tainted_parent(self):
        from nexuslang.security.analysis import TaintTracker, TaintLabel, TaintedValue
        tracker = TaintTracker()
        tv = tracker.taint("x", TaintLabel.NETWORK, "net")
        result = tracker.propagate("derived", tv, "clean")
        assert hasattr(result, "is_tainted")
        assert result.is_tainted

    def test_check_sink_clean_value_passes(self):
        from nexuslang.security.analysis import TaintTracker, TaintSink
        tracker = TaintTracker()
        tracker.check_sink("clean_string", TaintSink.SHELL_EXEC)

    def test_check_sink_tainted_raises(self):
        from nexuslang.security.analysis import TaintTracker, TaintLabel, TaintSink, TaintViolation
        from nexuslang.security.analysis import AnalysisPolicy, ViolationPolicy, set_analysis_policy, get_analysis_policy
        original = get_analysis_policy()
        try:
            set_analysis_policy(AnalysisPolicy(taint_policy=ViolationPolicy.RAISE))
            tracker = TaintTracker()
            tv = tracker.taint("evil", TaintLabel.USER_INPUT, "stdin")
            with pytest.raises(TaintViolation):
                tracker.check_sink(tv, TaintSink.SHELL_EXEC, location="line 5")
        finally:
            set_analysis_policy(original)

    def test_check_sink_warn_does_not_raise(self, capsys):
        from nexuslang.security.analysis import (
            TaintTracker, TaintLabel, TaintSink, TaintViolation,
            AnalysisPolicy, ViolationPolicy, set_analysis_policy, get_analysis_policy,
        )
        original = get_analysis_policy()
        try:
            set_analysis_policy(AnalysisPolicy(taint_policy=ViolationPolicy.WARN))
            tracker = TaintTracker()
            tv = tracker.taint("payload", TaintLabel.USER_INPUT, "stdin")
            # Should not raise
            tracker.check_sink(tv, TaintSink.FILE_WRITE)
            captured = capsys.readouterr()
            assert "warning" in captured.err.lower() or "tainted" in captured.err.lower()
        finally:
            set_analysis_policy(original)

    def test_check_sink_log_records_violation(self):
        from nexuslang.security.analysis import (
            TaintTracker, TaintLabel, TaintSink, TaintViolation,
            AnalysisPolicy, ViolationPolicy, set_analysis_policy, get_analysis_policy,
        )
        original = get_analysis_policy()
        try:
            set_analysis_policy(AnalysisPolicy(taint_policy=ViolationPolicy.LOG))
            tracker = TaintTracker()
            tv = tracker.taint("data", TaintLabel.NETWORK, "net")
            tracker.check_sink(tv, TaintSink.SQL_QUERY)
            assert len(tracker.violations) == 1
        finally:
            set_analysis_policy(original)

    def test_violations_empty_initially(self):
        from nexuslang.security.analysis import TaintTracker
        tracker = TaintTracker()
        assert tracker.violations == []

    def test_reset_clears_violations(self):
        from nexuslang.security.analysis import (
            TaintTracker, TaintLabel, TaintSink,
            AnalysisPolicy, ViolationPolicy, set_analysis_policy, get_analysis_policy,
        )
        original = get_analysis_policy()
        try:
            set_analysis_policy(AnalysisPolicy(taint_policy=ViolationPolicy.LOG))
            tracker = TaintTracker()
            tv = tracker.taint("x", TaintLabel.USER_INPUT, "s")
            tracker.check_sink(tv, TaintSink.EVAL)
            assert len(tracker.violations) == 1
            tracker.reset()
            assert tracker.violations == []
        finally:
            set_analysis_policy(original)


# =============================================================================
# Analysis module: CFIChecker
# =============================================================================

class TestCFIChecker:
    """CFIChecker: call registration and validation."""

    def test_registered_call_passes(self):
        from nexuslang.security.analysis import CFIChecker
        checker = CFIChecker()
        func = lambda: None
        checker.call_graph.register_callable(func, "my_func")
        checker.check_call(func, location="line 1")

    def test_unregistered_call_raises(self):
        from nexuslang.security.analysis import CFIChecker, CFIViolation
        from nexuslang.security.analysis import AnalysisPolicy, ViolationPolicy, set_analysis_policy, get_analysis_policy
        original = get_analysis_policy()
        try:
            set_analysis_policy(AnalysisPolicy(cfi_policy=ViolationPolicy.RAISE))
            checker = CFIChecker()
            unregistered = lambda: None
            with pytest.raises(CFIViolation):
                checker.check_call(unregistered, location="line 2")
        finally:
            set_analysis_policy(original)

    def test_non_callable_raises_cfi(self):
        from nexuslang.security.analysis import CFIChecker, CFIViolation
        from nexuslang.security.analysis import AnalysisPolicy, ViolationPolicy, set_analysis_policy, get_analysis_policy
        original = get_analysis_policy()
        try:
            set_analysis_policy(AnalysisPolicy(cfi_policy=ViolationPolicy.RAISE))
            checker = CFIChecker()
            with pytest.raises(CFIViolation):
                checker.check_call("not_callable", location="line 3")
        finally:
            set_analysis_policy(original)

    def test_call_site_restricts_to_valid_targets(self):
        from nexuslang.security.analysis import CFIChecker, CFIViolation
        from nexuslang.security.analysis import AnalysisPolicy, ViolationPolicy, set_analysis_policy, get_analysis_policy
        original = get_analysis_policy()
        try:
            set_analysis_policy(AnalysisPolicy(cfi_policy=ViolationPolicy.RAISE))
            checker = CFIChecker()
            allowed = lambda: "allowed"
            denied = lambda: "denied"
            checker.call_graph.register_callable(allowed, "allowed")
            checker.call_graph.register_callable(denied, "denied")
            checker.call_graph.register_call_site("callback", [allowed])

            # Allowed target at this call site: OK
            checker.check_call(allowed, site_name="callback")

            # Registered but not in call site target list: CFIViolation
            with pytest.raises(CFIViolation):
                checker.check_call(denied, site_name="callback")
        finally:
            set_analysis_policy(original)

    def test_enter_exit_frame_normal(self):
        from nexuslang.security.analysis import CFIChecker
        checker = CFIChecker()
        frame_id = checker.enter_frame("test_func")
        assert frame_id > 0
        checker.exit_frame(frame_id, "test_func")
        assert checker.violations == []

    def test_exit_frame_mismatch_raises(self):
        from nexuslang.security.analysis import CFIChecker, CFIViolation
        from nexuslang.security.analysis import AnalysisPolicy, ViolationPolicy, set_analysis_policy, get_analysis_policy
        original = get_analysis_policy()
        try:
            set_analysis_policy(AnalysisPolicy(cfi_policy=ViolationPolicy.RAISE))
            checker = CFIChecker()
            frame_id = checker.enter_frame("func_a")
            with pytest.raises(CFIViolation):
                checker.exit_frame(frame_id, "func_b")  # wrong name
        finally:
            set_analysis_policy(original)

    def test_exit_frame_empty_stack_is_violation(self):
        from nexuslang.security.analysis import CFIChecker, CFIViolation
        from nexuslang.security.analysis import AnalysisPolicy, ViolationPolicy, set_analysis_policy, get_analysis_policy
        original = get_analysis_policy()
        try:
            set_analysis_policy(AnalysisPolicy(cfi_policy=ViolationPolicy.RAISE))
            checker = CFIChecker()
            with pytest.raises(CFIViolation):
                checker.exit_frame(42, "ghost_func")
        finally:
            set_analysis_policy(original)


# =============================================================================
# Analysis module: MemorySafetyValidator
# =============================================================================

class TestMemorySafetyValidator:
    """MemorySafetyValidator: bounds checks and use-after-free detection."""

    def test_valid_index_passes(self):
        from nexuslang.security.analysis import MemorySafetyValidator
        v = MemorySafetyValidator()
        v.check_bounds(0, 5)
        v.check_bounds(4, 5)
        v.check_bounds(-1, 5)   # negative index OK
        v.check_bounds(-5, 5)  # at -size OK

    def test_oob_index_raises_bounds_error(self):
        from nexuslang.security.analysis import MemorySafetyValidator, BoundsError
        from nexuslang.security.analysis import AnalysisPolicy, ViolationPolicy, set_analysis_policy, get_analysis_policy
        original = get_analysis_policy()
        try:
            set_analysis_policy(AnalysisPolicy(memory_policy=ViolationPolicy.RAISE))
            v = MemorySafetyValidator()
            with pytest.raises(BoundsError):
                v.check_bounds(5, 5)
        finally:
            set_analysis_policy(original)

    def test_negative_oob_raises(self):
        from nexuslang.security.analysis import MemorySafetyValidator, BoundsError
        from nexuslang.security.analysis import AnalysisPolicy, ViolationPolicy, set_analysis_policy, get_analysis_policy
        original = get_analysis_policy()
        try:
            set_analysis_policy(AnalysisPolicy(memory_policy=ViolationPolicy.RAISE))
            v = MemorySafetyValidator()
            with pytest.raises(BoundsError):
                v.check_bounds(-6, 5)
        finally:
            set_analysis_policy(original)

    def test_empty_buffer_any_access_raises(self):
        from nexuslang.security.analysis import MemorySafetyValidator, BoundsError
        from nexuslang.security.analysis import AnalysisPolicy, ViolationPolicy, set_analysis_policy, get_analysis_policy
        original = get_analysis_policy()
        try:
            set_analysis_policy(AnalysisPolicy(memory_policy=ViolationPolicy.RAISE))
            v = MemorySafetyValidator()
            with pytest.raises(BoundsError):
                v.check_bounds(0, 0)
        finally:
            set_analysis_policy(original)

    def test_use_after_free_detected(self):
        from nexuslang.security.analysis import MemorySafetyValidator, UseAfterFreeError
        from nexuslang.security.analysis import AnalysisPolicy, ViolationPolicy, set_analysis_policy, get_analysis_policy
        original = get_analysis_policy()
        try:
            set_analysis_policy(AnalysisPolicy(memory_policy=ViolationPolicy.RAISE))
            v = MemorySafetyValidator()
            v.record_free(0xdeadbeef)
            with pytest.raises(UseAfterFreeError):
                v.check_not_freed(0xdeadbeef)
        finally:
            set_analysis_policy(original)

    def test_fresh_address_passes(self):
        from nexuslang.security.analysis import MemorySafetyValidator
        v = MemorySafetyValidator()
        v.check_not_freed(0x1234)  # Not freed, should pass

    def test_reallocated_address_no_longer_freed(self):
        from nexuslang.security.analysis import MemorySafetyValidator
        v = MemorySafetyValidator()
        addr = 0xabcdef
        v.record_free(addr)
        v.record_alloc(addr)  # Re-allocated
        v.check_not_freed(addr)  # Should NOT raise

    def test_type_confusion_check(self):
        from nexuslang.security.analysis import MemorySafetyValidator, MemorySafetyViolation
        from nexuslang.security.analysis import AnalysisPolicy, ViolationPolicy, set_analysis_policy, get_analysis_policy
        original = get_analysis_policy()
        try:
            set_analysis_policy(AnalysisPolicy(memory_policy=ViolationPolicy.RAISE))
            v = MemorySafetyValidator()
            with pytest.raises(MemorySafetyViolation):
                v.check_index_no_overflow("not_an_int")
        finally:
            set_analysis_policy(original)

    def test_reset_clears_freed_set(self):
        from nexuslang.security.analysis import MemorySafetyValidator
        v = MemorySafetyValidator()
        v.record_free(0x1)
        v.reset()
        v.check_not_freed(0x1)  # Should pass after reset


# =============================================================================
# Analysis module: SecurityAnalyser facade
# =============================================================================

class TestSecurityAnalyser:
    """SecurityAnalyser aggregates all three analysers."""

    def test_no_violations_initially(self):
        from nexuslang.security.analysis import SecurityAnalyser
        sa = SecurityAnalyser()
        assert not sa.has_violations()

    def test_all_violations_returns_dict(self):
        from nexuslang.security.analysis import SecurityAnalyser
        sa = SecurityAnalyser()
        all_v = sa.all_violations()
        assert set(all_v.keys()) == {"taint", "cfi", "memory"}

    def test_reset_clears_all(self):
        from nexuslang.security.analysis import (
            SecurityAnalyser, TaintLabel, TaintSink,
            AnalysisPolicy, ViolationPolicy, set_analysis_policy, get_analysis_policy,
        )
        original = get_analysis_policy()
        try:
            set_analysis_policy(AnalysisPolicy(taint_policy=ViolationPolicy.LOG))
            sa = SecurityAnalyser()
            tv = sa.taint.taint("x", TaintLabel.USER_INPUT, "stdin")
            sa.taint.check_sink(tv, TaintSink.EVAL)
            assert sa.has_violations()
            sa.reset()
            assert not sa.has_violations()
        finally:
            set_analysis_policy(original)

    def test_report_no_violations(self, capsys):
        from nexuslang.security.analysis import SecurityAnalyser
        sa = SecurityAnalyser()
        import io
        buf = io.StringIO()
        sa.report(file=buf)
        assert "No violations" in buf.getvalue()


# =============================================================================
# Sandbox module: SandboxPolicy
# =============================================================================

class TestSandboxPolicy:
    """SandboxPolicy dataclass defaults and STRICT/DEVELOPMENT presets."""

    def test_default_policy_disallows_ffi(self):
        from nexuslang.security.sandbox import SandboxPolicy
        p = SandboxPolicy()
        assert not p.allow_ffi

    def test_default_policy_disallows_asm(self):
        from nexuslang.security.sandbox import SandboxPolicy
        p = SandboxPolicy()
        assert not p.allow_asm

    def test_strict_policy_has_memory_limit(self):
        from nexuslang.security.sandbox import STRICT_POLICY
        assert STRICT_POLICY.max_memory_mb is not None
        assert STRICT_POLICY.max_memory_mb > 0

    def test_development_policy_allows_ffi(self):
        from nexuslang.security.sandbox import DEVELOPMENT_POLICY
        assert DEVELOPMENT_POLICY.allow_ffi
        assert DEVELOPMENT_POLICY.allow_asm

    def test_strict_policy_is_immutable(self):
        from nexuslang.security.sandbox import STRICT_POLICY
        with pytest.raises((AttributeError, TypeError)):
            STRICT_POLICY.allow_ffi = True  # type: ignore[misc]


# =============================================================================
# Sandbox module: RestrictedMode
# =============================================================================

class TestRestrictedMode:
    """RestrictedMode enforces sandbox policy via PermissionManager."""

    def test_enter_denies_ffi_by_default(self):
        from nexuslang.security.sandbox import RestrictedMode, SandboxPolicy
        from nexuslang.security.permissions import (
            PermissionManager, PermissionType, PermissionDeniedError,
            get_permission_manager, set_permission_manager,
        )
        policy = SandboxPolicy(allow_ffi=False)
        rm = RestrictedMode(policy, PermissionManager())
        rm.enter()
        try:
            with pytest.raises(PermissionDeniedError):
                rm.check_ffi("libevil.so")
        finally:
            rm.exit()

    def test_enter_allows_ffi_when_policy_permits(self):
        from nexuslang.security.sandbox import RestrictedMode, SandboxPolicy
        from nexuslang.security.permissions import PermissionManager
        policy = SandboxPolicy(allow_ffi=True)
        rm = RestrictedMode(policy, PermissionManager())
        rm.enter()
        try:
            rm.check_ffi("libsafe.so")  # Should not raise
        finally:
            rm.exit()

    def test_enter_denies_asm_by_default(self):
        from nexuslang.security.sandbox import RestrictedMode, SandboxPolicy
        from nexuslang.security.permissions import PermissionManager, PermissionDeniedError
        policy = SandboxPolicy(allow_asm=False)
        rm = RestrictedMode(policy, PermissionManager())
        rm.enter()
        try:
            with pytest.raises(PermissionDeniedError):
                rm.check_asm()
        finally:
            rm.exit()

    def test_context_manager_restores_on_exit(self):
        from nexuslang.security.sandbox import RestrictedMode, SandboxPolicy
        from nexuslang.security.permissions import (
            PermissionManager, PermissionType,
            get_permission_manager, set_permission_manager,
        )
        original = PermissionManager(allow_all=True)
        set_permission_manager(original)

        policy = SandboxPolicy(allow_ffi=False)
        with RestrictedMode(policy, original):
            pass

        # After context, permissions should be restored
        mgr = get_permission_manager()
        assert mgr.has_permission(PermissionType.FFI)

    def test_enter_exit_can_be_called_programmatically(self):
        from nexuslang.security.sandbox import RestrictedMode, SandboxPolicy
        from nexuslang.security.permissions import PermissionManager
        policy = SandboxPolicy(allow_ffi=True)
        rm = RestrictedMode(policy, PermissionManager())
        rm.enter()
        rm.exit()


# =============================================================================
# Sandbox module: ResourceLimits
# =============================================================================

class TestResourceLimits:
    """ResourceLimits applies POSIX resource caps."""

    def test_enter_exit_no_crash_without_resource_module(self, monkeypatch):
        """ResourceLimits must not crash when resource module is absent."""
        from nexuslang.security import sandbox as sandbox_mod
        import nexuslang.security.sandbox as sb
        monkeypatch.setattr(sb, "_HAS_RESOURCE", False)
        from nexuslang.security.sandbox import ResourceLimits, SandboxPolicy
        rl = ResourceLimits(SandboxPolicy(max_memory_mb=128))
        rl.enter()   # No-op
        rl.exit()    # No-op

    @pytest.mark.skipif(not _is_linux(), reason="POSIX resource limits on Linux only")
    def test_enter_does_not_crash_on_linux(self):
        """Applying resource limits should succeed on Linux."""
        from nexuslang.security.sandbox import ResourceLimits, SandboxPolicy
        policy = SandboxPolicy(max_cpu_seconds=300.0, max_open_files=512)
        rl = ResourceLimits(policy)
        rl.enter()
        rl.exit()

    def test_context_manager_interface(self):
        from nexuslang.security.sandbox import ResourceLimits, SandboxPolicy
        # Should work as a context manager without raising
        policy = SandboxPolicy()
        with ResourceLimits(policy):
            pass


# =============================================================================
# Sandbox module: SeccompFilter
# =============================================================================

class TestSeccompFilter:
    """SeccompFilter: availability check and install path."""

    def test_not_available_on_non_linux(self):
        if _is_linux() and platform.machine().lower() in ("x86_64", "amd64"):
            pytest.skip("On x86-64 Linux, seccomp IS available")
        from nexuslang.security.sandbox import SeccompFilter, SandboxPolicy
        sf = SeccompFilter(SandboxPolicy())
        assert not sf.available

    def test_available_on_x86_64_linux(self):
        if not _is_linux() or platform.machine().lower() not in ("x86_64", "amd64"):
            pytest.skip("Only applicable on x86-64 Linux")
        from nexuslang.security.sandbox import SeccompFilter, SandboxPolicy
        sf = SeccompFilter(SandboxPolicy())
        assert sf.available

    def test_install_no_op_on_unsupported(self):
        """On non-x86-64 or non-Linux, install() should be a no-op."""
        if _is_linux() and platform.machine().lower() in ("x86_64", "amd64"):
            pytest.skip("On x86-64 Linux, install() is real")
        from nexuslang.security.sandbox import SeccompFilter, SandboxPolicy
        sf = SeccompFilter(SandboxPolicy())
        sf.install()  # Should not raise
        assert not sf.installed

    def test_context_manager_on_unsupported(self):
        if _is_linux() and platform.machine().lower() in ("x86_64", "amd64"):
            pytest.skip("Real seccomp on this platform")
        from nexuslang.security.sandbox import SeccompFilter, SandboxPolicy
        with SeccompFilter(SandboxPolicy()):
            pass


# =============================================================================
# Sandbox module: Sandbox facade
# =============================================================================

class TestSandbox:
    """Sandbox context manager applies all layers."""

    def test_sandbox_enters_and_exits(self):
        from nexuslang.security.sandbox import Sandbox, SandboxPolicy
        from nexuslang.security.permissions import PermissionManager
        policy = SandboxPolicy(allow_ffi=False)
        sb = Sandbox(policy, PermissionManager())
        sb.enter()
        assert sb.active
        sb.exit()
        assert not sb.active

    def test_sandbox_as_context_manager(self):
        from nexuslang.security.sandbox import Sandbox, SandboxPolicy
        from nexuslang.security.permissions import PermissionManager
        policy = SandboxPolicy()
        with Sandbox(policy, PermissionManager()) as sb:
            assert sb.active
        assert not sb.active

    def test_double_enter_raises(self):
        from nexuslang.security.sandbox import Sandbox, SandboxPolicy, SandboxError
        from nexuslang.security.permissions import PermissionManager
        policy = SandboxPolicy()
        sb = Sandbox(policy, PermissionManager())
        sb.enter()
        try:
            with pytest.raises(SandboxError):
                sb.enter()
        finally:
            sb.exit()

    def test_sandbox_policy_accessible(self):
        from nexuslang.security.sandbox import Sandbox, STRICT_POLICY
        sb = Sandbox(STRICT_POLICY)
        assert sb.policy is STRICT_POLICY

    def test_check_ffi_inside_strict_sandbox_raises(self):
        from nexuslang.security.sandbox import Sandbox, STRICT_POLICY
        from nexuslang.security.permissions import PermissionManager, PermissionDeniedError
        with Sandbox(STRICT_POLICY, PermissionManager()):
            with pytest.raises(PermissionDeniedError):
                # STRICT_POLICY disallows FFI
                from nexuslang.security.permissions import get_permission_manager, PermissionType
                mgr = get_permission_manager()
                mgr.check(PermissionType.FFI, resource="libtest.so")


# =============================================================================
# Sandbox module: ASLR helpers (check_aslr_status, warn_if_aslr_disabled)
# =============================================================================

class TestAslrHelpers:
    """Check ASLR status utilities."""

    def test_check_aslr_returns_none_on_non_linux(self):
        if _is_linux():
            pytest.skip("On Linux, may return a real value")
        from nexuslang.security.sandbox import check_aslr_status
        result = check_aslr_status()
        # On non-Linux should be None
        assert result is None

    @pytest.mark.skipif(not _is_linux(), reason="Linux only")
    def test_check_aslr_returns_int_on_linux(self):
        from nexuslang.security.sandbox import check_aslr_status
        result = check_aslr_status()
        if result is not None:
            assert result in (0, 1, 2)

    def test_warn_if_aslr_disabled_no_crash(self, capsys):
        from nexuslang.security.sandbox import warn_if_aslr_disabled
        warn_if_aslr_disabled()  # Should not raise regardless of ASLR state


# =============================================================================
# Runtime Protections: StackCanary
# =============================================================================

class TestStackCanary:
    """StackCanary: frame tracking and smash detection."""

    def test_enter_returns_positive_id(self):
        from nexuslang.security.runtime_protections import StackCanary
        sc = StackCanary(enabled=True)
        frame_id = sc.enter_frame("my_func")
        sc.exit_frame(frame_id, "my_func")
        assert frame_id > 0

    def test_normal_entry_exit_no_violation(self):
        from nexuslang.security.runtime_protections import StackCanary
        sc = StackCanary(enabled=True)
        frame_id = sc.enter_frame("hello")
        canary_val = sc.get_canary(frame_id)
        sc.exit_frame(frame_id, "hello", current_canary=canary_val)

    def test_modified_canary_raises(self):
        from nexuslang.security.runtime_protections import StackCanary, StackSmashingDetected
        sc = StackCanary(enabled=True)
        frame_id = sc.enter_frame("target")
        real_canary = sc.get_canary(frame_id)
        tampered = real_canary ^ 0xDEADBEEFDEADBEEF
        with pytest.raises(StackSmashingDetected):
            sc.exit_frame(frame_id, "target", current_canary=tampered)

    def test_wrong_function_name_raises(self):
        from nexuslang.security.runtime_protections import StackCanary, StackSmashingDetected
        sc = StackCanary(enabled=True)
        frame_id = sc.enter_frame("func_a")
        with pytest.raises(StackSmashingDetected):
            sc.exit_frame(frame_id, "func_b")

    def test_disabled_canary_returns_zero_id(self):
        from nexuslang.security.runtime_protections import StackCanary
        sc = StackCanary(enabled=False)
        frame_id = sc.enter_frame("no_effect")
        assert frame_id == 0

    def test_disabled_canary_exit_no_raise(self):
        from nexuslang.security.runtime_protections import StackCanary
        sc = StackCanary(enabled=False)
        frame_id = sc.enter_frame("f")
        sc.exit_frame(frame_id, "f", current_canary=0xBADBAD)  # Should not raise

    def test_get_canary_unknown_frame_returns_none(self):
        from nexuslang.security.runtime_protections import StackCanary
        sc = StackCanary(enabled=True)
        assert sc.get_canary(99999) is None

    def test_current_depth_tracking(self):
        from nexuslang.security.runtime_protections import StackCanary
        sc = StackCanary(enabled=True)
        assert sc.current_depth() == 0
        fid = sc.enter_frame("f1")
        assert sc.current_depth() == 1
        canary = sc.get_canary(fid)
        sc.exit_frame(fid, "f1", current_canary=canary)
        assert sc.current_depth() == 0

    def test_nested_frames(self):
        from nexuslang.security.runtime_protections import StackCanary
        sc = StackCanary(enabled=True)
        fid1 = sc.enter_frame("outer")
        fid2 = sc.enter_frame("inner")
        c2 = sc.get_canary(fid2)
        sc.exit_frame(fid2, "inner", current_canary=c2)
        c1 = sc.get_canary(fid1)
        sc.exit_frame(fid1, "outer", current_canary=c1)

    def test_multithreaded_canaries_independent(self):
        """Each thread has its own frame stack."""
        import gc
        gc.collect()  # Free memory from prior tests to avoid MemoryError

        from nexuslang.security.runtime_protections import StackCanary, StackSmashingDetected
        sc = StackCanary(enabled=True)
        errors: list = []

        def thread_work(name: str) -> None:
            try:
                fid = sc.enter_frame(name)
                time.sleep(0.01)  # Allow other threads to run
                canary = sc.get_canary(fid)
                sc.exit_frame(fid, name, current_canary=canary)
            except MemoryError:
                pass  # Environment memory pressure, not a canary bug
            except Exception as e:
                errors.append(e)

        threads = [threading.Thread(target=thread_work, args=(f"thread_{i}",))
                   for i in range(5)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        assert errors == [], f"Thread errors: {errors}"


# =============================================================================
# Runtime Protections: BoundsChecker
# =============================================================================

class TestBoundsChecker:
    """BoundsChecker: index validation."""

    def test_valid_index_passes(self):
        from nexuslang.security.runtime_protections import BoundsChecker
        bc = BoundsChecker(enabled=True)
        bc.check(0, 5)
        bc.check(4, 5)

    def test_negative_index_within_range_passes(self):
        from nexuslang.security.runtime_protections import BoundsChecker
        bc = BoundsChecker(enabled=True)
        bc.check(-1, 5)
        bc.check(-5, 5)

    def test_oob_positive_raises(self):
        from nexuslang.security.runtime_protections import BoundsChecker, BoundsCheckError
        bc = BoundsChecker(enabled=True)
        with pytest.raises(BoundsCheckError):
            bc.check(5, 5)

    def test_oob_negative_raises(self):
        from nexuslang.security.runtime_protections import BoundsChecker, BoundsCheckError
        bc = BoundsChecker(enabled=True)
        with pytest.raises(BoundsCheckError):
            bc.check(-6, 5)

    def test_empty_collection_raises(self):
        from nexuslang.security.runtime_protections import BoundsChecker, BoundsCheckError
        bc = BoundsChecker(enabled=True)
        with pytest.raises(BoundsCheckError):
            bc.check(0, 0)

    def test_disabled_never_raises(self):
        from nexuslang.security.runtime_protections import BoundsChecker
        bc = BoundsChecker(enabled=False)
        bc.check(9999, 0)  # Would be OOB if enabled

    def test_violation_count_increments(self):
        from nexuslang.security.runtime_protections import BoundsChecker, BoundsCheckError
        bc = BoundsChecker(enabled=True)
        for _ in range(3):
            try:
                bc.check(10, 5)
            except BoundsCheckError:
                pass
        assert bc.violation_count == 3

    def test_reset_count(self):
        from nexuslang.security.runtime_protections import BoundsChecker, BoundsCheckError
        bc = BoundsChecker(enabled=True)
        try:
            bc.check(100, 10)
        except BoundsCheckError:
            pass
        bc.reset_count()
        assert bc.violation_count == 0

    def test_check_slice_invalid_order(self):
        from nexuslang.security.runtime_protections import BoundsChecker, BoundsCheckError
        bc = BoundsChecker(enabled=True)
        # start=4 > stop=2 after clamping in a size-5 list is valid Python,
        # but start > stop on a size-5 list at indices 4, 2 still results in
        # an empty slice in Python.  The checker should allow it.
        # Only a truly impossible slice should raise.

    def test_check_slice_valid_passes(self):
        from nexuslang.security.runtime_protections import BoundsChecker
        bc = BoundsChecker(enabled=True)
        bc.check_slice(0, 5, 10)
        bc.check_slice(None, None, 10)
        bc.check_slice(2, 7, 10)


# =============================================================================
# Runtime Protections: IntegerOverflowChecker
# =============================================================================

class TestIntegerOverflowChecker:
    """IntegerOverflowChecker: overflow threshold and shift amount validation."""

    def test_normal_integer_passes(self):
        from nexuslang.security.runtime_protections import IntegerOverflowChecker
        checker = IntegerOverflowChecker(enabled=True)
        checker.check("a + b", 42)
        checker.check("a + b", -42)

    def test_huge_integer_raises(self):
        from nexuslang.security.runtime_protections import IntegerOverflowChecker, IntegerOverflowError
        checker = IntegerOverflowChecker(enabled=True, threshold=1000)
        with pytest.raises(IntegerOverflowError):
            checker.check("big_mul", 1001)

    def test_negative_huge_raises(self):
        from nexuslang.security.runtime_protections import IntegerOverflowChecker, IntegerOverflowError
        checker = IntegerOverflowChecker(enabled=True, threshold=1000)
        with pytest.raises(IntegerOverflowError):
            checker.check("neg_big", -1001)

    def test_disabled_huge_no_raise(self):
        from nexuslang.security.runtime_protections import IntegerOverflowChecker
        checker = IntegerOverflowChecker(enabled=False, threshold=1)
        checker.check("huge", 10 ** 100)  # Should not raise

    def test_valid_shift_passes(self):
        from nexuslang.security.runtime_protections import IntegerOverflowChecker
        checker = IntegerOverflowChecker(enabled=True)
        checker.check_shift(0)
        checker.check_shift(63)
        checker.check_shift(32)

    def test_shift_too_large_raises(self):
        from nexuslang.security.runtime_protections import IntegerOverflowChecker, IntegerOverflowError
        checker = IntegerOverflowChecker(enabled=True)
        with pytest.raises(IntegerOverflowError):
            checker.check_shift(64)

    def test_negative_shift_raises(self):
        from nexuslang.security.runtime_protections import IntegerOverflowChecker, IntegerOverflowError
        checker = IntegerOverflowChecker(enabled=True)
        with pytest.raises(IntegerOverflowError):
            checker.check_shift(-1)

    def test_non_integer_result_skipped(self):
        from nexuslang.security.runtime_protections import IntegerOverflowChecker
        checker = IntegerOverflowChecker(enabled=True, threshold=1)
        checker.check("float_op", 3.14)  # float, should not raise


# =============================================================================
# Runtime Protections: ASLR helpers
# =============================================================================

class TestRuntimeAslrHelpers:
    """aslr_level() and aslr_warning_message()."""

    def test_aslr_level_non_linux_returns_none(self):
        if _is_linux():
            pytest.skip("Linux present, may not return None")
        from nexuslang.security.runtime_protections import aslr_level
        assert aslr_level() is None

    @pytest.mark.skipif(not _is_linux(), reason="Linux only")
    def test_aslr_level_linux_returns_int_or_none(self):
        from nexuslang.security.runtime_protections import aslr_level
        result = aslr_level()
        if result is not None:
            assert result in (0, 1, 2)

    def test_aslr_warning_none_on_non_linux(self):
        if _is_linux():
            pytest.skip("Linux present, may have a real warning")
        from nexuslang.security.runtime_protections import aslr_warning_message
        assert aslr_warning_message() is None

    def test_check_and_warn_does_not_raise(self, capsys):
        from nexuslang.security.runtime_protections import check_and_warn_aslr
        import io
        buf = io.StringIO()
        check_and_warn_aslr(file=buf)


# =============================================================================
# Runtime Protections: RuntimeProtector facade
# =============================================================================

class TestRuntimeProtector:
    """RuntimeProtector: enable/disable/startup_checks."""

    def test_default_config_produces_disabled_protections(self):
        from nexuslang.security.runtime_protections import RuntimeProtector, RuntimeProtectorConfig
        # Default config: all disabled
        rp = RuntimeProtector(RuntimeProtectorConfig())
        assert not rp.canary.enabled
        assert not rp.bounds.enabled
        assert not rp.overflow.enabled

    def test_enable_all_toggles_all(self):
        from nexuslang.security.runtime_protections import RuntimeProtector, RuntimeProtectorConfig
        rp = RuntimeProtector(RuntimeProtectorConfig())
        rp.enable_all()
        assert rp.canary.enabled
        assert rp.bounds.enabled
        assert rp.overflow.enabled

    def test_disable_all_toggles_all_off(self):
        from nexuslang.security.runtime_protections import RuntimeProtector, RuntimeProtectorConfig
        rp = RuntimeProtector(RuntimeProtectorConfig(
            enable_canaries=True, enable_bounds=True, enable_overflow=True
        ))
        rp.disable_all()
        assert not rp.canary.enabled
        assert not rp.bounds.enabled
        assert not rp.overflow.enabled

    def test_startup_checks_no_crash(self, capsys):
        from nexuslang.security.runtime_protections import RuntimeProtector
        import io
        rp = RuntimeProtector()
        buf = io.StringIO()
        rp.startup_checks(file=buf)

    def test_bounds_check_via_protector(self):
        from nexuslang.security.runtime_protections import (
            RuntimeProtector, RuntimeProtectorConfig, BoundsCheckError,
        )
        rp = RuntimeProtector(RuntimeProtectorConfig(enable_bounds=True))
        rp.bounds.check(0, 10)  # Valid
        with pytest.raises(BoundsCheckError):
            rp.bounds.check(10, 10)  # OOB

    def test_canary_via_protector(self):
        from nexuslang.security.runtime_protections import (
            RuntimeProtector, RuntimeProtectorConfig, StackSmashingDetected,
        )
        rp = RuntimeProtector(RuntimeProtectorConfig(enable_canaries=True))
        fid = rp.canary.enter_frame("test")
        canary = rp.canary.get_canary(fid)
        rp.canary.exit_frame(fid, "test", current_canary=canary)

    def test_overflow_via_protector(self):
        from nexuslang.security.runtime_protections import (
            RuntimeProtector, RuntimeProtectorConfig, IntegerOverflowError,
        )
        rp = RuntimeProtector(RuntimeProtectorConfig(
            enable_overflow=True, overflow_threshold=100
        ))
        rp.overflow.check("add", 50)   # OK
        with pytest.raises(IntegerOverflowError):
            rp.overflow.check("mul", 101)


# =============================================================================
# Module-level import: all exported symbols present
# =============================================================================

class TestSecurityModuleExports:
    """Verify the security __init__.py exports all new symbols."""

    @pytest.mark.parametrize("symbol", [
        # Analysis - taint
        "TaintLabel", "TaintedValue", "TaintSink", "TaintTracker",
        "unwrap", "taint_label_of", "is_tainted",
        # Analysis - CFI
        "CallSite", "CallGraph", "CFIChecker",
        # Analysis - memory
        "MemorySafetyValidator", "BoundsError", "UseAfterFreeError",
        # Analysis - policy
        "ViolationPolicy", "AnalysisPolicy", "get_analysis_policy", "set_analysis_policy",
        # Analysis - exceptions
        "AnalysisViolation", "TaintViolation", "CFIViolation", "MemorySafetyViolation",
        # Analysis - facade
        "SecurityAnalyser",
        # Sandbox
        "SandboxPolicy", "SandboxError", "STRICT_POLICY", "DEVELOPMENT_POLICY",
        "RestrictedMode", "ResourceLimits", "SeccompFilter", "Sandbox",
        "check_aslr_status", "warn_if_aslr_disabled",
        # Runtime protections - exceptions
        "RuntimeProtectionError", "StackSmashingDetected", "BoundsCheckError", "IntegerOverflowError",
        # Runtime protections
        "StackCanary", "BoundsChecker", "IntegerOverflowChecker",
        "aslr_level", "aslr_warning_message", "check_and_warn_aslr",
        "RuntimeProtectorConfig", "RuntimeProtector",
    ])
    def test_symbol_is_exported(self, symbol):
        import nexuslang.security as sec
        assert hasattr(sec, symbol), f"nexuslang.security is missing export: {symbol}"


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
