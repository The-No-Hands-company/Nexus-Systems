"""
NLPL File System Watching Module.

Provides cross-platform file and directory watching via the watchdog library
(inotify on Linux, FSEvents on macOS, ReadDirectoryChangesW on Windows).

Uses a thread-safe event queue architecture: watchdog runs in a background
OS thread; events are staged into per-watcher queues; NexusLang programs drain
queues with fs_watch_poll() from the interpreter thread without risk of
re-entry.

Registered functions (callable from NexusLang programs):

    fs_watch_start(path, recursive=False) -> String
        Start watching *path* (must be a directory).
        Returns a watcher_id string used to identify this watcher.
        Raises ImportError if watchdog is not installed.
        Raises OSError if path does not exist or is not a directory.

    fs_watch_stop(watcher_id) -> Boolean
        Stop and remove the watcher identified by *watcher_id*.
        Returns True on success, False if watcher_id is unknown.

    fs_watch_stop_all() -> Integer
        Stop and remove all active watchers.
        Returns the count of watchers that were stopped.

    fs_watch_poll(watcher_id) -> List
        Drain and return all pending events for *watcher_id*.
        Returns an empty list if the watcher is unknown or has no events.
        Each event is a Dictionary:
            {
              "type":      "created" | "modified" | "deleted" | "moved",
              "path":      "/absolute/path/to/target",
              "is_dir":    True | False,
              "dest_path": "/absolute/new/path"  # only for "moved", else None
            }

    fs_watch_list() -> List
        Return a List of Dictionaries describing all active watchers:
            {
              "id":        "<watcher_id>",
              "path":      "/watched/path",
              "recursive": True | False,
              "active":    True | False
            }

    fs_watch_is_active(watcher_id) -> Boolean
        True when the watcher exists and its background observer is alive.

    fs_watch_path(watcher_id) -> String
        Return the path registered for *watcher_id*, or "" if unknown.

    fs_watch_clear(watcher_id) -> Integer
        Discard all pending events for *watcher_id* without returning them.
        Returns the number of events discarded.
"""

from __future__ import annotations

import os
import queue
import threading
import uuid
from typing import Any, Dict, List, Optional

try:
    from watchdog.observers import Observer
    from watchdog.events import (
        FileSystemEventHandler,
        FileCreatedEvent,
        FileModifiedEvent,
        FileDeletedEvent,
        FileMovedEvent,
        DirCreatedEvent,
        DirModifiedEvent,
        DirDeletedEvent,
        DirMovedEvent,
    )
    HAS_WATCHDOG = True
except ImportError:
    HAS_WATCHDOG = False

    # Provide a minimal stub so that class definitions below don't
    # raise NameError at module-import time on systems without watchdog.
    class FileSystemEventHandler:  # type: ignore[no-redef]
        pass

    class Observer:  # type: ignore[no-redef]
        pass


# ---------------------------------------------------------------------------
# Internal registry
# ---------------------------------------------------------------------------

# watcher_id -> _WatcherEntry
_WATCHERS: Dict[str, "_WatcherEntry"] = {}
_WATCHERS_LOCK = threading.Lock()


class _WatcherEntry:
    """Holds state for one active watcher."""

    def __init__(self, watcher_id: str, path: str, recursive: bool) -> None:
        self.watcher_id = watcher_id
        self.path = path
        self.recursive = recursive
        self.event_queue: queue.Queue = queue.Queue()
        self.observer: Optional[Any] = None   # watchdog Observer

    @property
    def active(self) -> bool:
        return self.observer is not None and self.observer.is_alive()


class _QueueHandler(FileSystemEventHandler):
    """Watchdog event handler that enqueues events as plain dicts."""

    def __init__(self, event_queue: queue.Queue) -> None:
        super().__init__()
        self._queue = event_queue

    def _enqueue(self, event_type: str, event: Any) -> None:
        dest = getattr(event, "dest_path", None)
        self._queue.put_nowait({
            "type": event_type,
            "path": os.path.abspath(event.src_path),
            "is_dir": event.is_directory,
            "dest_path": os.path.abspath(dest) if dest else None,
        })

    def on_created(self, event):  # noqa: D401
        self._enqueue("created", event)

    def on_modified(self, event):  # noqa: D401
        self._enqueue("modified", event)

    def on_deleted(self, event):  # noqa: D401
        self._enqueue("deleted", event)

    def on_moved(self, event):  # noqa: D401
        self._enqueue("moved", event)


# ---------------------------------------------------------------------------
# Public functions
# ---------------------------------------------------------------------------

def fs_watch_start(path: str, recursive: bool = False) -> str:
    """Start watching *path* (must be a directory).  Returns a watcher_id string."""
    if not HAS_WATCHDOG:
        raise ImportError("watchdog package required: pip install watchdog")

    abs_path = os.path.abspath(path)
    if not os.path.exists(abs_path):
        raise OSError(f"fs_watch_start: path does not exist: {abs_path!r}")
    if not os.path.isdir(abs_path):
        raise OSError(f"fs_watch_start: path is not a directory: {abs_path!r}")

    watcher_id = str(uuid.uuid4())
    entry = _WatcherEntry(watcher_id, abs_path, bool(recursive))

    handler = _QueueHandler(entry.event_queue)
    observer = Observer()
    observer.schedule(handler, abs_path, recursive=bool(recursive))
    observer.start()
    entry.observer = observer

    with _WATCHERS_LOCK:
        _WATCHERS[watcher_id] = entry

    return watcher_id


def fs_watch_stop(watcher_id: str) -> bool:
    """Stop and remove the watcher with *watcher_id*.  Returns True on success."""
    with _WATCHERS_LOCK:
        entry = _WATCHERS.pop(watcher_id, None)

    if entry is None:
        return False

    try:
        if entry.observer is not None:
            entry.observer.stop()
            entry.observer.join(timeout=2.0)
    except Exception:
        pass
    return True


def fs_watch_stop_all() -> int:
    """Stop all active watchers.  Returns count stopped."""
    with _WATCHERS_LOCK:
        ids = list(_WATCHERS.keys())

    count = 0
    for watcher_id in ids:
        if fs_watch_stop(watcher_id):
            count += 1
    return count


def fs_watch_poll(watcher_id: str) -> List[Dict[str, Any]]:
    """Drain and return all pending events for *watcher_id*.

    Returns [] if the watcher is unknown.
    """
    with _WATCHERS_LOCK:
        entry = _WATCHERS.get(watcher_id)

    if entry is None:
        return []

    events: List[Dict[str, Any]] = []
    try:
        while True:
            events.append(entry.event_queue.get_nowait())
    except queue.Empty:
        pass
    return events


def fs_watch_list() -> List[Dict[str, Any]]:
    """Return a list of dicts describing all active watchers."""
    with _WATCHERS_LOCK:
        snapshot = list(_WATCHERS.values())

    return [
        {
            "id": e.watcher_id,
            "path": e.path,
            "recursive": e.recursive,
            "active": e.active,
        }
        for e in snapshot
    ]


def fs_watch_is_active(watcher_id: str) -> bool:
    """True when the watcher exists and its observer thread is alive."""
    with _WATCHERS_LOCK:
        entry = _WATCHERS.get(watcher_id)
    return entry is not None and entry.active


def fs_watch_path(watcher_id: str) -> str:
    """Return the watched path for *watcher_id*, or empty string if unknown."""
    with _WATCHERS_LOCK:
        entry = _WATCHERS.get(watcher_id)
    return entry.path if entry is not None else ""


def fs_watch_clear(watcher_id: str) -> int:
    """Discard all pending events for *watcher_id*.  Returns count discarded."""
    with _WATCHERS_LOCK:
        entry = _WATCHERS.get(watcher_id)

    if entry is None:
        return 0

    count = 0
    try:
        while True:
            entry.event_queue.get_nowait()
            count += 1
    except queue.Empty:
        pass
    return count


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

def register_fs_watch_functions(runtime) -> None:
    """Register file system watching functions with the NexusLang runtime."""
    if HAS_WATCHDOG:
        runtime.register_function("fs_watch_start", fs_watch_start)
        runtime.register_function("fs_watch_stop", fs_watch_stop)
        runtime.register_function("fs_watch_stop_all", fs_watch_stop_all)
        runtime.register_function("fs_watch_poll", fs_watch_poll)
        runtime.register_function("fs_watch_list", fs_watch_list)
        runtime.register_function("fs_watch_is_active", fs_watch_is_active)
        runtime.register_function("fs_watch_path", fs_watch_path)
        runtime.register_function("fs_watch_clear", fs_watch_clear)
    else:
        # Register stubs that raise a clear ImportError so NexusLang programs get
        # a useful message rather than "function not found".
        def _no_watchdog(*args, **kwargs):
            raise ImportError("watchdog package required: pip install watchdog")

        for name in (
            "fs_watch_start", "fs_watch_stop", "fs_watch_stop_all",
            "fs_watch_poll", "fs_watch_list", "fs_watch_is_active",
            "fs_watch_path", "fs_watch_clear",
        ):
            runtime.register_function(name, _no_watchdog)


__all__ = [
    "fs_watch_start",
    "fs_watch_stop",
    "fs_watch_stop_all",
    "fs_watch_poll",
    "fs_watch_list",
    "fs_watch_is_active",
    "fs_watch_path",
    "fs_watch_clear",
    "register_fs_watch_functions",
    "HAS_WATCHDOG",
]
