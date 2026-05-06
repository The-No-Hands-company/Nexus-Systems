"""
Tests for the NexusLang logging_utils stdlib module.

Coverage
--------
- Level constants
- Root-logger convenience helpers (log_debug/info/warning/error/critical)
- Named loggers (isolation, create-on-demand)
- Level set/get (int and string inputs)
- Format strings
- Enable / disable
- Handler management (console, stderr, file)
- log_to (generic named-logger helper)
- In-memory record buffer (log_get_records / log_clear_records)
- log_configure one-call setup
- log_get_all_loggers
- Registration (all expected function names present in runtime)
"""

import os
import sys
import tempfile
from io import StringIO

import pytest

# Import the module directly so we can manipulate the registry in tests
from nexuslang.stdlib.logging_utils import (
    LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_CRITICAL,
    _LOGGERS, _LOG_LOCK, _get_or_create, _NLPLLogger, _resolve_level,
    log_debug, log_info, log_warning, log_error, log_critical,
    log_get_logger, log_set_level, log_get_level, log_set_format,
    log_enable, log_disable, log_get_all_loggers,
    log_add_console_handler, log_add_stderr_handler, log_add_file_handler,
    log_remove_handlers,
    log_to,
    log_get_records, log_clear_records,
    log_configure,
    set_log_level, add_file_handler, remove_all_handlers, set_log_format,
    log_exception,
    register_logging_functions,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

class _FakeRuntime:
    """Minimal runtime stub that accumulates registered functions."""

    def __init__(self):
        self.functions = {}

    def register_function(self, name, fn):
        self.functions[name] = fn


def _fresh_logger(name: str) -> "_NLPLLogger":
    """Create (or reset) a named logger for an isolated test."""
    with _LOG_LOCK:
        logger = _NLPLLogger(name)
        _LOGGERS[name] = logger
    return logger


def _capturing_handler(buf):
    """Return a handler that appends formatted strings to *buf* (a list)."""
    def _h(formatted):
        buf.append(formatted)
    return _h


# ---------------------------------------------------------------------------
# Level constants
# ---------------------------------------------------------------------------

class TestLevelConstants:
    def test_values_are_integers(self):
        assert isinstance(LOG_DEBUG, int)
        assert isinstance(LOG_INFO, int)
        assert isinstance(LOG_WARNING, int)
        assert isinstance(LOG_ERROR, int)
        assert isinstance(LOG_CRITICAL, int)

    def test_ascending_severity(self):
        assert LOG_DEBUG < LOG_INFO < LOG_WARNING < LOG_ERROR < LOG_CRITICAL

    def test_exact_values(self):
        assert LOG_DEBUG == 10
        assert LOG_INFO == 20
        assert LOG_WARNING == 30
        assert LOG_ERROR == 40
        assert LOG_CRITICAL == 50


# ---------------------------------------------------------------------------
# _resolve_level helper
# ---------------------------------------------------------------------------

class TestResolveLevel:
    def test_int_passthrough(self):
        assert _resolve_level(10) == 10
        assert _resolve_level(55) == 55

    def test_uppercase_string(self):
        assert _resolve_level("DEBUG") == LOG_DEBUG
        assert _resolve_level("INFO") == LOG_INFO
        assert _resolve_level("WARNING") == LOG_WARNING
        assert _resolve_level("ERROR") == LOG_ERROR
        assert _resolve_level("CRITICAL") == LOG_CRITICAL

    def test_lowercase_string(self):
        assert _resolve_level("debug") == LOG_DEBUG
        assert _resolve_level("critical") == LOG_CRITICAL

    def test_mixed_case_string(self):
        assert _resolve_level("Warning") == LOG_WARNING

    def test_numeric_string(self):
        assert _resolve_level("30") == 30

    def test_unknown_string_defaults_to_debug(self):
        assert _resolve_level("VERBOSE") == LOG_DEBUG


# ---------------------------------------------------------------------------
# Root-logger convenience helpers
# ---------------------------------------------------------------------------

class TestRootLoggerConvenience:
    def setup_method(self):
        """Clear root logger records and attach a capture handler."""
        self._root = _get_or_create("root")
        self._root.records.clear()
        self._buf = []
        # Remove existing handlers so stdout doesn't get cluttered in CI
        self._root.handlers.clear()
        self._root.handlers.append(_capturing_handler(self._buf))
        self._root.level = LOG_DEBUG
        self._root.enabled = True

    def test_log_debug_creates_record(self):
        log_debug("hello debug")
        assert len(self._root.records) == 1
        assert self._root.records[-1]["level"] == "DEBUG"
        assert self._root.records[-1]["message"] == "hello debug"

    def test_log_info_creates_record(self):
        log_info("info message")
        assert self._root.records[-1]["level"] == "INFO"

    def test_log_warning_creates_record(self):
        log_warning("warn")
        assert self._root.records[-1]["level"] == "WARNING"

    def test_log_error_creates_record(self):
        log_error("err")
        assert self._root.records[-1]["level"] == "ERROR"

    def test_log_critical_creates_record(self):
        log_critical("crit")
        assert self._root.records[-1]["level"] == "CRITICAL"

    def test_handler_receives_formatted_string(self):
        log_info("check handler")
        assert len(self._buf) == 1
        assert "check handler" in self._buf[0]

    def test_record_has_required_keys(self):
        log_info("keys test")
        rec = self._root.records[-1]
        for key in ("level", "level_no", "name", "message", "formatted", "time"):
            assert key in rec, f"Missing key: {key}"

    def test_record_name_is_root(self):
        log_info("name test")
        assert self._root.records[-1]["name"] == "root"

    def test_record_level_no_matches_constant(self):
        log_info("level_no test")
        assert self._root.records[-1]["level_no"] == LOG_INFO


# ---------------------------------------------------------------------------
# Named logger management
# ---------------------------------------------------------------------------

class TestNamedLoggers:
    def test_log_get_logger_returns_name(self):
        name = log_get_logger("test_named_aa")
        assert name == "test_named_aa"

    def test_log_get_logger_creates_entry_in_registry(self):
        log_get_logger("test_named_bb")
        assert "test_named_bb" in _LOGGERS

    def test_log_get_logger_idempotent(self):
        log_get_logger("test_named_cc")
        log_get_logger("test_named_cc")
        assert _LOGGERS.get("test_named_cc") is not None

    def test_named_loggers_are_isolated(self):
        a = _fresh_logger("test_iso_a")
        b = _fresh_logger("test_iso_b")
        buf_a, buf_b = [], []
        a.handlers = [_capturing_handler(buf_a)]
        b.handlers = [_capturing_handler(buf_b)]
        a.log(LOG_INFO, "for a")
        assert len(buf_a) == 1
        assert len(buf_b) == 0

    def test_log_get_all_loggers_includes_root(self):
        result = log_get_all_loggers()
        assert "root" in result

    def test_log_get_all_loggers_is_sorted(self):
        result = log_get_all_loggers()
        assert result == sorted(result)

    def test_log_get_all_loggers_returns_newly_created(self):
        log_get_logger("test_gal_zz")
        assert "test_gal_zz" in log_get_all_loggers()


# ---------------------------------------------------------------------------
# Level set / get
# ---------------------------------------------------------------------------

class TestLogSetGetLevel:
    def setup_method(self):
        self._logger = _fresh_logger("test_level_logger")

    def test_set_level_by_constant(self):
        log_set_level("test_level_logger", LOG_WARNING)
        assert self._logger.level == LOG_WARNING

    def test_set_level_by_string(self):
        log_set_level("test_level_logger", "error")
        assert self._logger.level == LOG_ERROR

    def test_get_level_returns_name(self):
        log_set_level("test_level_logger", LOG_INFO)
        assert log_get_level("test_level_logger") == "INFO"

    def test_get_level_after_string_set(self):
        log_set_level("test_level_logger", "critical")
        assert log_get_level("test_level_logger") == "CRITICAL"

    def test_level_threshold_filters_messages(self):
        buf = []
        self._logger.handlers = [_capturing_handler(buf)]
        log_set_level("test_level_logger", LOG_WARNING)
        log_to("test_level_logger", LOG_DEBUG, "should be filtered")
        log_to("test_level_logger", LOG_WARNING, "should pass")
        assert len(buf) == 1
        assert "should pass" in buf[0]


# ---------------------------------------------------------------------------
# Format strings
# ---------------------------------------------------------------------------

class TestLogSetFormat:
    def setup_method(self):
        self._logger = _fresh_logger("test_fmt_logger")
        self._buf = []
        self._logger.handlers = [_capturing_handler(self._buf)]
        self._logger.level = LOG_DEBUG

    def test_default_format_includes_level(self):
        self._logger.log(LOG_INFO, "msg")
        assert "INFO" in self._buf[-1]

    def test_default_format_includes_name(self):
        self._logger.log(LOG_INFO, "msg")
        assert "test_fmt_logger" in self._buf[-1]

    def test_custom_format_applied(self):
        log_set_format("test_fmt_logger", "{level}: {message}")
        self._logger.log(LOG_INFO, "custom")
        assert self._buf[-1] == "INFO: custom"

    def test_custom_format_without_name_token(self):
        log_set_format("test_fmt_logger", ">> {message}")
        self._logger.log(LOG_DEBUG, "stripped")
        assert self._buf[-1] == ">> stripped"

    def test_format_reset_to_default(self):
        from nexuslang.stdlib.logging_utils import _DEFAULT_FORMAT
        log_set_format("test_fmt_logger", _DEFAULT_FORMAT)
        self._logger.log(LOG_INFO, "reset")
        assert "test_fmt_logger" in self._buf[-1]


# ---------------------------------------------------------------------------
# Enable / disable
# ---------------------------------------------------------------------------

class TestLogEnableDisable:
    def setup_method(self):
        self._logger = _fresh_logger("test_toggle")
        self._buf = []
        self._logger.handlers = [_capturing_handler(self._buf)]
        self._logger.level = LOG_DEBUG

    def test_disabled_logger_suppresses_output(self):
        log_disable("test_toggle")
        self._logger.log(LOG_ERROR, "suppressed")
        assert len(self._buf) == 0

    def test_disabled_logger_no_records(self):
        log_disable("test_toggle")
        self._logger.log(LOG_ERROR, "suppressed")
        assert len(self._logger.records) == 0

    def test_re_enable_restores_logging(self):
        log_disable("test_toggle")
        log_enable("test_toggle")
        self._logger.log(LOG_INFO, "restored")
        assert len(self._buf) == 1

    def test_enable_on_already_enabled_is_safe(self):
        log_enable("test_toggle")
        log_enable("test_toggle")
        assert self._logger.enabled is True


# ---------------------------------------------------------------------------
# Handler management
# ---------------------------------------------------------------------------

class TestHandlerManagement:
    def setup_method(self):
        self._logger = _fresh_logger("test_handlers")
        self._logger.level = LOG_DEBUG

    def test_log_remove_handlers_returns_count(self):
        buf = []
        self._logger.handlers = [_capturing_handler(buf), _capturing_handler(buf)]
        count = log_remove_handlers("test_handlers")
        assert count == 2

    def test_log_remove_handlers_clears_list(self):
        buf = []
        self._logger.handlers = [_capturing_handler(buf)]
        log_remove_handlers("test_handlers")
        assert len(self._logger.handlers) == 0

    def test_log_remove_handlers_on_empty_returns_zero(self):
        self._logger.handlers = []
        assert log_remove_handlers("test_handlers") == 0

    def test_log_add_console_handler_adds_to_list(self):
        before = len(self._logger.handlers)
        log_add_console_handler("test_handlers")
        assert len(self._logger.handlers) == before + 1

    def test_log_add_stderr_handler_adds_to_list(self):
        before = len(self._logger.handlers)
        log_add_stderr_handler("test_handlers")
        assert len(self._logger.handlers) == before + 1

    def test_log_add_file_handler_creates_file(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "log_test.txt")
            log_add_file_handler("test_handlers", path)
            assert os.path.isfile(path)

    def test_log_add_file_handler_writes_on_log(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "write_test.txt")
            log_add_file_handler("test_handlers", path)
            self._logger.log(LOG_INFO, "written to file")
            with open(path, encoding="utf-8") as fh:
                content = fh.read()
            assert "written to file" in content

    def test_log_add_file_handler_appends(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "append_test.txt")
            log_add_file_handler("test_handlers", path)
            self._logger.log(LOG_INFO, "first")
            log_add_file_handler("test_handlers", path)
            self._logger.log(LOG_INFO, "second")
            with open(path, encoding="utf-8") as fh:
                lines = [l for l in fh.readlines() if l.strip()]
            assert len(lines) >= 2

    def test_log_add_file_handler_creates_parent_dirs(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "sub", "dir", "nested.log")
            log_add_file_handler("test_handlers", path)
            assert os.path.isfile(path)


# ---------------------------------------------------------------------------
# log_to generic helper
# ---------------------------------------------------------------------------

class TestLogTo:
    def setup_method(self):
        self._logger = _fresh_logger("test_log_to")
        self._buf = []
        self._logger.handlers = [_capturing_handler(self._buf)]
        self._logger.level = LOG_DEBUG

    def test_log_to_with_int_level(self):
        log_to("test_log_to", LOG_INFO, "via int")
        assert len(self._buf) == 1
        assert "via int" in self._buf[0]

    def test_log_to_with_string_level(self):
        log_to("test_log_to", "warning", "via string")
        assert len(self._logger.records) == 1
        assert self._logger.records[-1]["level"] == "WARNING"

    def test_log_to_filtered_by_threshold(self):
        self._logger.level = LOG_ERROR
        log_to("test_log_to", LOG_DEBUG, "too low")
        assert len(self._buf) == 0

    def test_log_to_creates_logger_if_absent(self):
        unique = "test_log_to_absent_xyz"
        if unique in _LOGGERS:
            del _LOGGERS[unique]
        log_to(unique, LOG_INFO, "create on demand")
        assert unique in _LOGGERS


# ---------------------------------------------------------------------------
# In-memory record buffer
# ---------------------------------------------------------------------------

class TestRecordBuffer:
    def setup_method(self):
        self._logger = _fresh_logger("test_records")
        self._buf = []
        self._logger.handlers = [_capturing_handler(self._buf)]
        self._logger.level = LOG_DEBUG

    def test_records_are_dicts(self):
        self._logger.log(LOG_INFO, "dict test")
        rec = list(self._logger.records)[-1]
        assert isinstance(rec, dict)

    def test_record_has_all_keys(self):
        self._logger.log(LOG_INFO, "keys test")
        rec = list(self._logger.records)[-1]
        for key in ("level", "level_no", "name", "message", "formatted", "time"):
            assert key in rec

    def test_log_get_records_returns_list(self):
        result = log_get_records("test_records", 10)
        assert isinstance(result, list)

    def test_log_get_records_max_count(self):
        for i in range(20):
            self._logger.log(LOG_DEBUG, f"msg {i}")
        result = log_get_records("test_records", 5)
        assert len(result) <= 5

    def test_log_get_records_returns_most_recent(self):
        for i in range(10):
            self._logger.log(LOG_INFO, f"item {i}")
        result = log_get_records("test_records", 3)
        assert result[-1]["message"] == "item 9"

    def test_log_get_records_unknown_logger_returns_empty(self):
        result = log_get_records("nonexistent_logger_xyz", 10)
        assert result == []

    def test_log_clear_records_returns_count(self):
        for i in range(5):
            self._logger.log(LOG_INFO, f"x {i}")
        count = log_clear_records("test_records")
        assert count == 5

    def test_log_clear_records_empties_buffer(self):
        self._logger.log(LOG_INFO, "clear me")
        log_clear_records("test_records")
        assert len(self._logger.records) == 0

    def test_log_clear_records_unknown_logger_returns_zero(self):
        assert log_clear_records("nonexistent_xyz") == 0


# ---------------------------------------------------------------------------
# log_configure
# ---------------------------------------------------------------------------

class TestLogConfigure:
    def setup_method(self):
        self._logger = _fresh_logger("test_configure")
        self._buf = []
        self._logger.handlers = [_capturing_handler(self._buf)]

    def test_configure_sets_level_by_int(self):
        log_configure(LOG_ERROR, name="test_configure")
        assert self._logger.level == LOG_ERROR

    def test_configure_sets_level_by_string(self):
        log_configure("warning", name="test_configure")
        assert self._logger.level == LOG_WARNING

    def test_configure_sets_format(self):
        log_configure(LOG_DEBUG, fmt="{level}: {message}", name="test_configure")
        assert self._logger.format_str == "{level}: {message}"

    def test_configure_without_name_uses_root(self):
        root = _get_or_create("root")
        original_level = root.level
        log_configure(LOG_ERROR)
        assert root.level == LOG_ERROR
        root.level = original_level  # restore

    def test_configure_does_not_remove_handlers(self):
        self._logger.handlers = [_capturing_handler(self._buf)]
        log_configure(LOG_DEBUG, name="test_configure")
        assert len(self._logger.handlers) == 1


# ---------------------------------------------------------------------------
# Backward-compatible aliases
# ---------------------------------------------------------------------------

class TestBackwardCompatAliases:
    def setup_method(self):
        root = _get_or_create("root")
        root.records.clear()
        self._root_level_backup = root.level
        root.level = LOG_DEBUG
        root.enabled = True

    def teardown_method(self):
        _get_or_create("root").level = self._root_level_backup

    def test_set_log_level_updates_root(self):
        root = _get_or_create("root")
        set_log_level("ERROR")
        assert root.level == LOG_ERROR

    def test_set_log_level_string_case_insensitive(self):
        set_log_level("info")
        assert _get_or_create("root").level == LOG_INFO

    def test_add_file_handler_returns_true_on_success(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "compat.log")
            assert add_file_handler(path) is True

    def test_add_file_handler_returns_false_on_invalid_path(self):
        # Use a null byte — always an invalid path on Linux regardless of permissions
        result = add_file_handler("/tmp/invalid\x00path.log")
        assert result is False

    def test_remove_all_handlers_clears_root(self):
        root = _get_or_create("root")
        root.handlers.append(_capturing_handler([]))
        remove_all_handlers()
        assert len(root.handlers) == 0

    def test_set_log_format_changes_root_format(self):
        set_log_format("{level}: {message}")
        assert _get_or_create("root").format_str == "{level}: {message}"

    def test_log_exception_logs_at_warning(self):
        root = _get_or_create("root")
        root.records.clear()
        root.level = LOG_DEBUG
        try:
            raise ValueError("boom")
        except ValueError:
            log_exception("oops")
        assert len(root.records) == 1
        assert root.records[0]["level"] == "WARNING"


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

class TestRegistration:
    """All expected function names end up in the runtime."""

    EXPECTED = {
        "LOG_DEBUG", "LOG_INFO", "LOG_WARNING", "LOG_ERROR", "LOG_CRITICAL",
        "log_debug", "log_info", "log_warning", "log_error", "log_critical",
        "log_get_logger", "log_set_level", "log_get_level", "log_set_format",
        "log_enable", "log_disable", "log_get_all_loggers",
        "log_add_console_handler", "log_add_stderr_handler",
        "log_add_file_handler", "log_remove_handlers",
        "log_to",
        "log_get_records", "log_clear_records",
        "log_configure",
        # backward compat
        "set_log_level", "add_file_handler", "remove_all_handlers",
        "set_log_format", "log_exception",
    }

    def setup_method(self):
        self._rt = _FakeRuntime()
        register_logging_functions(self._rt)

    def test_all_expected_names_registered(self):
        missing = self.EXPECTED - set(self._rt.functions.keys())
        assert not missing, f"Missing registered functions: {missing}"

    def test_registered_functions_are_callable(self):
        for name, fn in self._rt.functions.items():
            assert callable(fn), f"{name!r} is not callable"

    def test_log_debug_callable_from_runtime(self):
        self._rt.functions["log_debug"]("test from runtime")

    def test_log_info_callable_from_runtime(self):
        self._rt.functions["log_info"]("info from runtime")

    def test_log_configure_callable_from_runtime(self):
        self._rt.functions["log_configure"](LOG_INFO)
