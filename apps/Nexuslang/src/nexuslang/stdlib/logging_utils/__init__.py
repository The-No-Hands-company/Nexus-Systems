"""
NLPL Structured Logging Module.

Provides a hierarchical, handler-based logging system.  A special "root"
logger is created at import time with a console handler so that log_info()
works with zero configuration.

Log Levels (numeric, ascending severity)
-----------------------------------------
  LOG_DEBUG    = 10
  LOG_INFO     = 20
  LOG_WARNING  = 30
  LOG_ERROR    = 40
  LOG_CRITICAL = 50

Architecture
------------
Each named logger owns:
  - level threshold  (messages below threshold are silently dropped)
  - list of handlers  (console stdout/stderr, file, or any callable)
  - format string     (tokens: {time}, {level}, {name}, {message})
  - in-memory record buffer  (circular deque, max 1000 entries)
  - enabled flag

Registered NexusLang functions
--------------------------
Convenience (root logger):
    log_debug(message)
    log_info(message)
    log_warning(message)
    log_error(message)
    log_critical(message)

Logger management:
    log_get_logger(name)             -> String  (creates if absent)
    log_set_level(name, level)       -> None    (int or string)
    log_get_level(name)              -> String  ("DEBUG", "INFO", ...)
    log_set_format(name, fmt)        -> None
    log_enable(name)                 -> None
    log_disable(name)                -> None
    log_get_all_loggers()            -> List of String

Handler management:
    log_add_console_handler(name)    -> None  (stdout)
    log_add_stderr_handler(name)     -> None  (stderr)
    log_add_file_handler(name, path) -> None  (append mode)
    log_remove_handlers(name)        -> Integer

Named-logger generic helper:
    log_to(name, level, message)     -> None

In-memory buffer:
    log_get_records(name, max=100)   -> List of Dict
    log_clear_records(name)          -> Integer

Convenience configuration:
    log_configure(level, fmt, name)  -> None

Backward-compatible aliases (from the original stub):
    set_log_level(level)             -> None  (root logger)
    add_file_handler(path, level)    -> bool  (root logger)
    remove_all_handlers()            -> None  (root logger)
    set_log_format(fmt)              -> None  (root logger)
    log_exception(message)           -> None  (root logger, WARNING level)
"""

from __future__ import annotations

import datetime
import os
import sys
import threading
import traceback
from collections import deque
from typing import Any, Dict, List, Union


# ---------------------------------------------------------------------------
# Level constants
# ---------------------------------------------------------------------------

LOG_DEBUG: int = 10
LOG_INFO: int = 20
LOG_WARNING: int = 30
LOG_ERROR: int = 40
LOG_CRITICAL: int = 50

_LEVEL_NAME: Dict[int, str] = {
    LOG_DEBUG: "DEBUG",
    LOG_INFO: "INFO",
    LOG_WARNING: "WARNING",
    LOG_ERROR: "ERROR",
    LOG_CRITICAL: "CRITICAL",
}

_LEVEL_FROM_NAME: Dict[str, int] = {v: k for k, v in _LEVEL_NAME.items()}

_DEFAULT_FORMAT = "[{time}] {level:<8} [{name}] {message}"
_MAX_RECORDS = 1000

# ---------------------------------------------------------------------------
# Internal registry
# ---------------------------------------------------------------------------

_LOGGERS: Dict[str, "_NLPLLogger"] = {}
_LOG_LOCK = threading.Lock()


class _NLPLLogger:
    """State for one named logger."""

    def __init__(self, name: str) -> None:
        self.name: str = name
        self.level: int = LOG_DEBUG
        self.enabled: bool = True
        self.format_str: str = _DEFAULT_FORMAT
        self.handlers: List[Any] = []
        self.records: deque = deque(maxlen=_MAX_RECORDS)

    def _format_record(self, level: int, message: str) -> str:
        now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        level_name = _LEVEL_NAME.get(level, str(level))
        return self.format_str.format(
            time=now,
            level=level_name,
            name=self.name,
            message=message,
        )

    def log(self, level: int, message: str) -> None:
        if not self.enabled or level < self.level:
            return
        formatted = self._format_record(level, message)
        record: Dict[str, Any] = {
            "level": _LEVEL_NAME.get(level, str(level)),
            "level_no": level,
            "name": self.name,
            "message": message,
            "formatted": formatted,
            "time": datetime.datetime.now().isoformat(),
        }
        self.records.append(record)
        for handler in self.handlers:
            try:
                handler(formatted)
            except Exception:
                pass


def _resolve_level(level: Union[int, str]) -> int:
    """Convert int or name string to numeric level."""
    if isinstance(level, int):
        return level
    upper = str(level).upper()
    if upper in _LEVEL_FROM_NAME:
        return _LEVEL_FROM_NAME[upper]
    try:
        return int(level)
    except (TypeError, ValueError):
        return LOG_DEBUG


def _get_or_create(name: str) -> "_NLPLLogger":
    with _LOG_LOCK:
        if name not in _LOGGERS:
            _LOGGERS[name] = _NLPLLogger(name)
        return _LOGGERS[name]


# ---------------------------------------------------------------------------
# Handler factories
# ---------------------------------------------------------------------------

def _make_console_handler(stream: Any = None) -> Any:
    _stream = stream or sys.stdout

    def _handler(formatted: str) -> None:
        print(formatted, file=_stream, flush=True)

    return _handler


def _make_file_handler(path: str) -> Any:
    abs_path = os.path.abspath(path)
    parent = os.path.dirname(abs_path)
    if parent:
        os.makedirs(parent, exist_ok=True)

    def _handler(formatted: str) -> None:
        with open(abs_path, "a", encoding="utf-8") as fh:
            fh.write(formatted + "\n")

    return _handler


# ---------------------------------------------------------------------------
# Initialise root logger with a default console handler
# ---------------------------------------------------------------------------

def _init_root() -> None:
    root = _NLPLLogger("root")
    root.level = LOG_DEBUG
    root.handlers.append(_make_console_handler())
    _LOGGERS["root"] = root


_init_root()


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

# Convenience helpers (root logger) -----------------------------------------

def log_debug(message: str) -> None:
    """Log at DEBUG level on the root logger."""
    _LOGGERS["root"].log(LOG_DEBUG, str(message))


def log_info(message: str) -> None:
    """Log at INFO level on the root logger."""
    _LOGGERS["root"].log(LOG_INFO, str(message))


def log_warning(message: str) -> None:
    """Log at WARNING level on the root logger."""
    _LOGGERS["root"].log(LOG_WARNING, str(message))


def log_error(message: str) -> None:
    """Log at ERROR level on the root logger."""
    _LOGGERS["root"].log(LOG_ERROR, str(message))


def log_critical(message: str) -> None:
    """Log at CRITICAL level on the root logger."""
    _LOGGERS["root"].log(LOG_CRITICAL, str(message))


# Logger management ----------------------------------------------------------

def log_get_logger(name: str) -> str:
    """Get or create a named logger.  Returns the logger name."""
    _get_or_create(str(name))
    return str(name)


def log_set_level(name: str, level: Union[int, str] = LOG_DEBUG) -> None:
    """Set minimum severity threshold for logger *name*.

    *level* may be an integer (LOG_DEBUG … LOG_CRITICAL) or a name string
    ("debug", "info", "warning", "error", "critical", case-insensitive).
    """
    _get_or_create(str(name)).level = _resolve_level(level)


def log_get_level(name: str) -> str:
    """Return current level name ("DEBUG", "INFO", …) for logger *name*."""
    logger = _get_or_create(str(name))
    return _LEVEL_NAME.get(logger.level, str(logger.level))


def log_set_format(name: str, fmt: str = _DEFAULT_FORMAT) -> None:
    """Set the format string for logger *name*.

    Available tokens: {time}, {level}, {name}, {message}
    Example: "[{time}] {level}: {message}"
    """
    _get_or_create(str(name)).format_str = str(fmt)


def log_enable(name: str) -> None:
    """Re-enable a previously disabled logger."""
    _get_or_create(str(name)).enabled = True


def log_disable(name: str) -> None:
    """Silence a logger without removing it from the registry."""
    _get_or_create(str(name)).enabled = False


def log_get_all_loggers() -> List[str]:
    """Return a sorted list of all registered logger names."""
    with _LOG_LOCK:
        return sorted(_LOGGERS.keys())


# Handler management ---------------------------------------------------------

def log_add_console_handler(name: str) -> None:
    """Add a stdout console handler to logger *name*."""
    _get_or_create(str(name)).handlers.append(_make_console_handler(sys.stdout))


def log_add_stderr_handler(name: str) -> None:
    """Add a stderr console handler to logger *name*."""
    _get_or_create(str(name)).handlers.append(_make_console_handler(sys.stderr))


def log_add_file_handler(name: str, path: str) -> None:
    """Add an append-mode file handler to logger *name*.

    Parent directories are created automatically.
    Raises OSError if the file cannot be opened for writing.
    """
    abs_path = os.path.abspath(str(path))
    parent = os.path.dirname(abs_path)
    if parent:
        os.makedirs(parent, exist_ok=True)
    with open(abs_path, "a", encoding="utf-8"):
        pass  # validate write access before registering
    _get_or_create(str(name)).handlers.append(_make_file_handler(abs_path))


def log_remove_handlers(name: str) -> int:
    """Remove all handlers from logger *name*.  Returns the number removed."""
    logger = _get_or_create(str(name))
    count = len(logger.handlers)
    logger.handlers.clear()
    return count


# Generic named-logger helper ------------------------------------------------

def log_to(name: str, level: Union[int, str], message: str) -> None:
    """Log *message* at *level* on the named logger *name*.

    *level* may be an integer or string ("debug", "INFO", etc.).
    """
    _get_or_create(str(name)).log(_resolve_level(level), str(message))


# In-memory record buffer ----------------------------------------------------

def log_get_records(name: str, max_count: int = 100) -> List[Dict]:
    """Return up to *max_count* most-recent log records for logger *name*.

    Each record is a Dict:
        {
          "level":     "INFO",
          "level_no":  20,
          "name":      "myapp",
          "message":   "...",
          "formatted": "[2026-02-26 ...] INFO ...",
          "time":      "2026-02-26T..."
        }
    Returns [] for unknown loggers.
    """
    with _LOG_LOCK:
        logger = _LOGGERS.get(str(name))
    if logger is None:
        return []
    records = list(logger.records)
    if max_count > 0:
        records = records[-int(max_count):]
    return records


def log_clear_records(name: str) -> int:
    """Discard all buffered records for logger *name*.  Returns count discarded."""
    with _LOG_LOCK:
        logger = _LOGGERS.get(str(name))
    if logger is None:
        return 0
    count = len(logger.records)
    logger.records.clear()
    return count


# Convenience configuration --------------------------------------------------

def log_configure(
    level: Union[int, str] = LOG_INFO,
    fmt: str = _DEFAULT_FORMAT,
    name: str = "root",
) -> None:
    """One-call configuration for logger *name*.

    Sets level and format string without touching existing handlers.
    Example:
        log_configure with level: "info" and fmt: "{level}: {message}"
    """
    logger = _get_or_create(str(name))
    logger.level = _resolve_level(level)
    logger.format_str = str(fmt)


# ---------------------------------------------------------------------------
# Backward-compatible aliases (original stub API)
# ---------------------------------------------------------------------------

def set_log_level(level: str) -> None:
    """Legacy: set level on the root logger.  Use log_set_level instead."""
    _LOGGERS["root"].level = _resolve_level(level)


def add_file_handler(filepath: str, level: str = "DEBUG") -> bool:
    """Legacy: add file handler to the root logger.

    Returns False if the parent directory does not exist (does NOT create
    directories, unlike log_add_file_handler).
    """
    try:
        abs_path = os.path.abspath(str(filepath))
        parent = os.path.dirname(abs_path)
        if not os.path.isdir(parent):
            return False
        with open(abs_path, "a", encoding="utf-8"):
            pass
        _get_or_create("root").handlers.append(_make_file_handler(abs_path))
        return True
    except Exception as exc:
        print(f"Failed to add file handler: {exc}", file=sys.stderr)
        return False


def remove_all_handlers() -> None:
    """Legacy: remove all handlers from the root logger."""
    log_remove_handlers("root")


# set_log_format on root logger (original single-logger behaviour)
def set_log_format(format_string: str) -> None:
    """Legacy: set format on the root logger."""
    _LOGGERS["root"].format_str = str(format_string)


def log_exception(message: str = "Exception occurred") -> None:
    """Log a WARNING with the current traceback appended."""
    tb = traceback.format_exc()
    full_message = f"{message}\n{tb}" if tb.strip() != "NoneType: None" else message
    _LOGGERS["root"].log(LOG_WARNING, full_message)


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

def register_logging_functions(runtime: Any) -> None:
    """Register all logging functions with the NexusLang runtime."""
    # Level constants
    runtime.register_function("LOG_DEBUG", lambda: LOG_DEBUG)
    runtime.register_function("LOG_INFO", lambda: LOG_INFO)
    runtime.register_function("LOG_WARNING", lambda: LOG_WARNING)
    runtime.register_function("LOG_ERROR", lambda: LOG_ERROR)
    runtime.register_function("LOG_CRITICAL", lambda: LOG_CRITICAL)

    # Convenience root-logger helpers
    runtime.register_function("log_debug", log_debug)
    runtime.register_function("log_info", log_info)
    runtime.register_function("log_warning", log_warning)
    runtime.register_function("log_error", log_error)
    runtime.register_function("log_critical", log_critical)

    # Logger management
    runtime.register_function("log_get_logger", log_get_logger)
    runtime.register_function("log_set_level", log_set_level)
    runtime.register_function("log_get_level", log_get_level)
    runtime.register_function("log_set_format", log_set_format)
    runtime.register_function("log_enable", log_enable)
    runtime.register_function("log_disable", log_disable)
    runtime.register_function("log_get_all_loggers", log_get_all_loggers)

    # Handler management
    runtime.register_function("log_add_console_handler", log_add_console_handler)
    runtime.register_function("log_add_stderr_handler", log_add_stderr_handler)
    runtime.register_function("log_add_file_handler", log_add_file_handler)
    runtime.register_function("log_remove_handlers", log_remove_handlers)

    # Named-logger generic helper
    runtime.register_function("log_to", log_to)

    # In-memory buffer
    runtime.register_function("log_get_records", log_get_records)
    runtime.register_function("log_clear_records", log_clear_records)

    # Convenience configuration
    runtime.register_function("log_configure", log_configure)

    # Backward-compatible aliases
    runtime.register_function("set_log_level", set_log_level)
    runtime.register_function("add_file_handler", add_file_handler)
    runtime.register_function("remove_all_handlers", remove_all_handlers)
    runtime.register_function("set_log_format", set_log_format)
    runtime.register_function("log_exception", log_exception)


__all__ = [
    "LOG_DEBUG", "LOG_INFO", "LOG_WARNING", "LOG_ERROR", "LOG_CRITICAL",
    "log_debug", "log_info", "log_warning", "log_error", "log_critical",
    "log_get_logger", "log_set_level", "log_get_level", "log_set_format",
    "log_enable", "log_disable", "log_get_all_loggers",
    "log_add_console_handler", "log_add_stderr_handler", "log_add_file_handler",
    "log_remove_handlers", "log_to",
    "log_get_records", "log_clear_records", "log_configure",
    # backward compat
    "set_log_level", "add_file_handler", "remove_all_handlers",
    "set_log_format", "log_exception",
    "register_logging_functions",
]
