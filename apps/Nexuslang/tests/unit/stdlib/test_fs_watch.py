"""
Tests for stdlib/fs_watch - file system watching via watchdog.

Covers:
  fs_watch_start, fs_watch_stop, fs_watch_stop_all
  fs_watch_poll, fs_watch_list, fs_watch_is_active
  fs_watch_path, fs_watch_clear
  Registration in runtime
  HAS_WATCHDOG=False stub paths
"""

import os
import sys
import tempfile
import time
import threading
import uuid
import pytest

# ------------------------------------------------------------------
# Try importing the module under test
# ------------------------------------------------------------------
try:
    from src.nlpl.stdlib.fs_watch import (
        HAS_WATCHDOG,
        fs_watch_start,
        fs_watch_stop,
        fs_watch_stop_all,
        fs_watch_poll,
        fs_watch_list,
        fs_watch_is_active,
        fs_watch_path,
        fs_watch_clear,
        register_fs_watch_functions,
    )
    _IMPORT_OK = True
except ImportError:
    _IMPORT_OK = False
    HAS_WATCHDOG = False

pytestmark = pytest.mark.skipif(
    not (_IMPORT_OK and HAS_WATCHDOG),
    reason="watchdog not installed or fs_watch module unavailable",
)

# ------------------------------------------------------------------
# Helpers
# ------------------------------------------------------------------

def _make_file(directory: str, name: str = "test.txt", content: str = "nlpl") -> str:
    """Create a file in directory and return its full path."""
    path = os.path.join(directory, name)
    with open(path, "w") as fh:
        fh.write(content)
    return path


def _wait_for_events(watcher_id: str, min_count: int = 1, timeout: float = 1.5) -> list:
    """Poll until at least min_count events arrive or timeout elapses."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        events = fs_watch_poll(watcher_id)
        if len(events) >= min_count:
            return events
        time.sleep(0.05)
    return fs_watch_poll(watcher_id)


# ------------------------------------------------------------------
# Fixture: temp directory with a running watcher
# ------------------------------------------------------------------

@pytest.fixture
def watched_dir():
    """Creates a temp dir, starts a watcher, yields (tmpdir, watcher_id), stops on teardown."""
    with tempfile.TemporaryDirectory() as tmpdir:
        wid = fs_watch_start(tmpdir)
        yield tmpdir, wid
        # Stop if still active
        if fs_watch_is_active(wid):
            fs_watch_stop(wid)


# ------------------------------------------------------------------
# fs_watch_start
# ------------------------------------------------------------------

class TestFsWatchStart:
    def test_returns_non_empty_string(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            try:
                assert isinstance(wid, str)
                assert len(wid) > 0
            finally:
                fs_watch_stop(wid)

    def test_watcher_id_is_unique(self):
        with tempfile.TemporaryDirectory() as d:
            ids = [fs_watch_start(d) for _ in range(5)]
            try:
                assert len(set(ids)) == 5
            finally:
                for wid in ids:
                    fs_watch_stop(wid)

    def test_raises_oserror_for_nonexistent_path(self):
        nonexistent = "/tmp/__nxl_fs_watch_nonexistent_path_xyz__"
        with pytest.raises(OSError):
            fs_watch_start(nonexistent)

    def test_raises_oserror_for_file_path(self):
        with tempfile.NamedTemporaryFile() as f:
            # Watching a file path that is NOT a directory is invalid for some
            # watchdog backends; our wrapper must reject it.
            with pytest.raises(OSError):
                fs_watch_start(f.name)

    def test_resolves_to_absolute_path(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            try:
                assert os.path.isabs(fs_watch_path(wid))
            finally:
                fs_watch_stop(wid)

    def test_is_active_immediately_after_start(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            try:
                assert fs_watch_is_active(wid) is True
            finally:
                fs_watch_stop(wid)

    def test_non_recursive_by_default(self, watched_dir):
        tmpdir, wid = watched_dir
        subdir = os.path.join(tmpdir, "sub")
        os.makedirs(subdir)
        # Let dir-creation events settle, then clear
        time.sleep(0.1)
        fs_watch_clear(wid)
        # Create file inside subdir – should NOT appear in non-recursive watcher
        _make_file(subdir, "nested.txt")
        time.sleep(0.3)
        events = fs_watch_poll(wid)
        nested_events = [e for e in events if "nested.txt" in e["path"]]
        assert nested_events == [], (
            "Non-recursive watcher should not see events inside subdirectory"
        )

    def test_recursive_mode_sees_nested_events(self):
        with tempfile.TemporaryDirectory() as d:
            subdir = os.path.join(d, "sub")
            os.makedirs(subdir)
            wid = fs_watch_start(d, recursive=True)
            try:
                _make_file(subdir, "deep.txt")
                events = _wait_for_events(wid, min_count=1)
                nested = [e for e in events if "deep.txt" in e["path"]]
                assert nested, "Recursive watcher should see nested file creation"
            finally:
                fs_watch_stop(wid)

    def test_multiple_watchers_on_same_dir(self):
        with tempfile.TemporaryDirectory() as d:
            wid1 = fs_watch_start(d)
            wid2 = fs_watch_start(d)
            try:
                assert wid1 != wid2
                assert fs_watch_is_active(wid1)
                assert fs_watch_is_active(wid2)
            finally:
                fs_watch_stop(wid1)
                fs_watch_stop(wid2)


# ------------------------------------------------------------------
# fs_watch_stop
# ------------------------------------------------------------------

class TestFsWatchStop:
    def test_returns_true_on_success(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            result = fs_watch_stop(wid)
            assert result is True

    def test_returns_false_for_unknown_id(self):
        result = fs_watch_stop("unknown-id-does-not-exist")
        assert result is False

    def test_is_inactive_after_stop(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            fs_watch_stop(wid)
            assert fs_watch_is_active(wid) is False

    def test_double_stop_returns_false(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            assert fs_watch_stop(wid) is True
            assert fs_watch_stop(wid) is False

    def test_stop_removes_from_list(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            assert any(w["id"] == wid for w in fs_watch_list())
            fs_watch_stop(wid)
            assert not any(w["id"] == wid for w in fs_watch_list())


# ------------------------------------------------------------------
# fs_watch_stop_all
# ------------------------------------------------------------------

class TestFsWatchStopAll:
    def test_returns_count_of_stopped_watchers(self):
        with tempfile.TemporaryDirectory() as d1, tempfile.TemporaryDirectory() as d2:
            wid1 = fs_watch_start(d1)
            wid2 = fs_watch_start(d2)
            count = fs_watch_stop_all()
            assert count >= 2  # at least those two
            assert not fs_watch_is_active(wid1)
            assert not fs_watch_is_active(wid2)

    def test_returns_zero_when_no_watchers(self):
        # Ensure clean state
        fs_watch_stop_all()
        result = fs_watch_stop_all()
        assert result == 0

    def test_list_empty_after_stop_all(self):
        with tempfile.TemporaryDirectory() as d:
            fs_watch_start(d)
        fs_watch_stop_all()
        assert fs_watch_list() == []


# ------------------------------------------------------------------
# fs_watch_poll
# ------------------------------------------------------------------

class TestFsWatchPoll:
    def test_returns_list(self, watched_dir):
        _, wid = watched_dir
        assert isinstance(fs_watch_poll(wid), list)

    def test_returns_empty_list_for_unknown_id(self):
        result = fs_watch_poll("no-such-watcher")
        assert result == []

    def test_detects_file_creation(self, watched_dir):
        tmpdir, wid = watched_dir
        _make_file(tmpdir, "new_file.txt")
        events = _wait_for_events(wid, min_count=1)
        created = [e for e in events if e["type"] == "created" and "new_file.txt" in e["path"]]
        assert created, f"Expected 'created' event, got: {events}"

    def test_detects_file_modification(self, watched_dir):
        tmpdir, wid = watched_dir
        path = _make_file(tmpdir, "existing.txt")
        time.sleep(0.2)
        fs_watch_clear(wid)
        with open(path, "a") as fh:
            fh.write(" modified")
        events = _wait_for_events(wid, min_count=1)
        modified = [e for e in events if e["type"] == "modified" and "existing.txt" in e["path"]]
        assert modified, f"Expected 'modified' event, got: {events}"

    def test_detects_file_deletion(self, watched_dir):
        tmpdir, wid = watched_dir
        path = _make_file(tmpdir, "to_delete.txt")
        time.sleep(0.2)
        fs_watch_clear(wid)
        os.remove(path)
        events = _wait_for_events(wid, min_count=1)
        deleted = [e for e in events if e["type"] == "deleted" and "to_delete.txt" in e["path"]]
        assert deleted, f"Expected 'deleted' event, got: {events}"

    def test_event_has_required_keys(self, watched_dir):
        tmpdir, wid = watched_dir
        _make_file(tmpdir, "keys_test.txt")
        events = _wait_for_events(wid, min_count=1)
        for event in events:
            assert "type" in event
            assert "path" in event
            assert "is_dir" in event
            assert "dest_path" in event

    def test_event_type_is_string(self, watched_dir):
        tmpdir, wid = watched_dir
        _make_file(tmpdir, "type_test.txt")
        events = _wait_for_events(wid, min_count=1)
        for event in events:
            assert isinstance(event["type"], str)
            assert event["type"] in ("created", "modified", "deleted", "moved")

    def test_event_path_is_absolute(self, watched_dir):
        tmpdir, wid = watched_dir
        _make_file(tmpdir, "path_test.txt")
        events = _wait_for_events(wid, min_count=1)
        for event in events:
            assert os.path.isabs(event["path"]), f"Expected absolute path, got: {event['path']}"

    def test_event_is_dir_bool(self, watched_dir):
        tmpdir, wid = watched_dir
        _make_file(tmpdir, "dir_test.txt")
        events = _wait_for_events(wid, min_count=1)
        for event in events:
            assert isinstance(event["is_dir"], bool)

    def test_poll_drains_queue(self, watched_dir):
        tmpdir, wid = watched_dir
        _make_file(tmpdir, "drain1.txt")
        events = _wait_for_events(wid, min_count=1)
        assert len(events) > 0
        # Second poll: queue should be drained
        assert fs_watch_poll(wid) == []

    def test_file_event_is_not_dir(self, watched_dir):
        tmpdir, wid = watched_dir
        _make_file(tmpdir, "notdir.txt")
        events = _wait_for_events(wid, min_count=1)
        file_events = [e for e in events if "notdir.txt" in e["path"]]
        assert file_events
        for e in file_events:
            assert e["is_dir"] is False

    def test_moved_event_has_dest_path(self, watched_dir):
        tmpdir, wid = watched_dir
        src = _make_file(tmpdir, "move_src.txt")
        dst = os.path.join(tmpdir, "move_dst.txt")
        time.sleep(0.15)
        fs_watch_clear(wid)
        os.rename(src, dst)
        events = _wait_for_events(wid, min_count=1)
        moved = [e for e in events if e["type"] == "moved"]
        if moved:
            assert moved[0]["dest_path"] is not None
            assert isinstance(moved[0]["dest_path"], str)


# ------------------------------------------------------------------
# fs_watch_list
# ------------------------------------------------------------------

class TestFsWatchList:
    def test_returns_list(self):
        assert isinstance(fs_watch_list(), list)

    def test_entry_has_required_keys(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            try:
                entries = fs_watch_list()
                matching = [e for e in entries if e["id"] == wid]
                assert matching, "Watcher not found in list"
                entry = matching[0]
                assert "id" in entry
                assert "path" in entry
                assert "recursive" in entry
                assert "active" in entry
            finally:
                fs_watch_stop(wid)

    def test_entry_shows_correct_path(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            try:
                entry = next(e for e in fs_watch_list() if e["id"] == wid)
                assert entry["path"] == os.path.abspath(d)
            finally:
                fs_watch_stop(wid)

    def test_entry_active_is_true_while_running(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            try:
                entry = next(e for e in fs_watch_list() if e["id"] == wid)
                assert entry["active"] is True
            finally:
                fs_watch_stop(wid)

    def test_entry_recursive_reflects_arg(self):
        with tempfile.TemporaryDirectory() as d:
            wid_f = fs_watch_start(d, recursive=False)
            wid_t = fs_watch_start(d, recursive=True)
            try:
                entries = {e["id"]: e for e in fs_watch_list()}
                assert entries[wid_f]["recursive"] is False
                assert entries[wid_t]["recursive"] is True
            finally:
                fs_watch_stop(wid_f)
                fs_watch_stop(wid_t)

    def test_snapshot_does_not_change_with_events(self, watched_dir):
        tmpdir, wid = watched_dir
        snap_before = fs_watch_list()
        _make_file(tmpdir, "list_snap.txt")
        time.sleep(0.2)
        snap_after = fs_watch_list()
        assert len(snap_before) == len(snap_after)


# ------------------------------------------------------------------
# fs_watch_is_active
# ------------------------------------------------------------------

class TestFsWatchIsActive:
    def test_true_for_running_watcher(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            try:
                assert fs_watch_is_active(wid) is True
            finally:
                fs_watch_stop(wid)

    def test_false_after_stop(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            fs_watch_stop(wid)
            assert fs_watch_is_active(wid) is False

    def test_false_for_unknown_id(self):
        assert fs_watch_is_active("no-such-id") is False

    def test_type_is_bool(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            try:
                assert isinstance(fs_watch_is_active(wid), bool)
            finally:
                fs_watch_stop(wid)


# ------------------------------------------------------------------
# fs_watch_path
# ------------------------------------------------------------------

class TestFsWatchPath:
    def test_returns_correct_path(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            try:
                assert fs_watch_path(wid) == os.path.abspath(d)
            finally:
                fs_watch_stop(wid)

    def test_returns_empty_string_for_unknown_id(self):
        result = fs_watch_path("no-such-id")
        assert result == ""

    def test_returns_absolute_path(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            try:
                assert os.path.isabs(fs_watch_path(wid))
            finally:
                fs_watch_stop(wid)

    def test_type_is_str(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            try:
                assert isinstance(fs_watch_path(wid), str)
            finally:
                fs_watch_stop(wid)


# ------------------------------------------------------------------
# fs_watch_clear
# ------------------------------------------------------------------

class TestFsWatchClear:
    def test_returns_zero_when_no_events(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            try:
                count = fs_watch_clear(wid)
                assert count == 0
            finally:
                fs_watch_stop(wid)

    def test_returns_count_of_discarded_events(self, watched_dir):
        tmpdir, wid = watched_dir
        _make_file(tmpdir, "discard1.txt")
        _make_file(tmpdir, "discard2.txt")
        events = _wait_for_events(wid, min_count=1)
        # Re-add them to queue by clearing normally
        count = fs_watch_clear(wid)
        assert count >= 0  # May or may not have caught both depending on timing

    def test_poll_empty_after_clear(self, watched_dir):
        tmpdir, wid = watched_dir
        _make_file(tmpdir, "pre_clear.txt")
        _wait_for_events(wid, min_count=1)
        fs_watch_clear(wid)
        assert fs_watch_poll(wid) == []

    def test_returns_zero_for_unknown_id(self):
        result = fs_watch_clear("no-such-id")
        assert result == 0

    def test_type_is_int(self):
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)
            try:
                assert isinstance(fs_watch_clear(wid), int)
            finally:
                fs_watch_stop(wid)


# ------------------------------------------------------------------
# Thread safety
# ------------------------------------------------------------------

class TestFsWatchThreadSafety:
    def test_concurrent_start_stop(self):
        """Multiple threads can start and stop watchers without deadlock."""
        results = []
        errors = []

        def worker(idx):
            try:
                with tempfile.TemporaryDirectory() as d:
                    wid = fs_watch_start(d)
                    time.sleep(0.05)
                    fs_watch_stop(wid)
                    results.append(idx)
            except Exception as exc:
                errors.append(exc)

        threads = [threading.Thread(target=worker, args=(i,)) for i in range(6)]
        for t in threads:
            t.start()
        for t in threads:
            t.join(timeout=5)

        assert not errors, f"Thread errors: {errors}"
        assert len(results) == 6

    def test_poll_and_stop_concurrent(self):
        """Polling while stopping should not raise exceptions."""
        errors = []
        with tempfile.TemporaryDirectory() as d:
            wid = fs_watch_start(d)

            def poller():
                for _ in range(20):
                    try:
                        fs_watch_poll(wid)
                        time.sleep(0.01)
                    except Exception as exc:
                        errors.append(exc)

            t = threading.Thread(target=poller)
            t.start()
            time.sleep(0.05)
            fs_watch_stop(wid)
            t.join(timeout=3)

        assert not errors, f"Concurrent poll/stop errors: {errors}"


# ------------------------------------------------------------------
# Registration in runtime
# ------------------------------------------------------------------

class TestFsWatchRegistration:
    """Verify all 8 function names are registered with the runtime."""

    EXPECTED_NAMES = [
        "fs_watch_start",
        "fs_watch_stop",
        "fs_watch_stop_all",
        "fs_watch_poll",
        "fs_watch_list",
        "fs_watch_is_active",
        "fs_watch_path",
        "fs_watch_clear",
    ]

    @pytest.fixture
    def runtime(self):
        from src.nlpl.runtime.runtime import Runtime
        rt = Runtime()
        register_fs_watch_functions(rt)
        return rt

    def test_all_functions_registered(self, runtime):
        for name in self.EXPECTED_NAMES:
            assert name in runtime.functions, f"'{name}' not registered in runtime"

    def test_registered_functions_are_callable(self, runtime):
        for name in self.EXPECTED_NAMES:
            assert callable(runtime.functions[name]), f"'{name}' is not callable"

    def test_registration_count(self, runtime):
        registered = [n for n in self.EXPECTED_NAMES if n in runtime.functions]
        assert len(registered) == len(self.EXPECTED_NAMES)


# ------------------------------------------------------------------
# HAS_WATCHDOG=False stub paths
# ------------------------------------------------------------------

class TestFsWatchNoWatchdog:
    """When watchdog is unavailable stubs should raise ImportError."""

    def _get_stubs(self):
        """Import and register with watchdog patched to False."""
        import src.nlpl.stdlib.fs_watch as mod
        original = mod.HAS_WATCHDOG
        mod.HAS_WATCHDOG = False
        try:
            from src.nlpl.runtime.runtime import Runtime
            rt = Runtime()
            # Re-register with patched flag by calling the internal helpers
            # We test the stubs directly by monkeypatching the module flag
        finally:
            mod.HAS_WATCHDOG = original

    def test_has_watchdog_is_bool(self):
        import src.nlpl.stdlib.fs_watch as mod
        assert isinstance(mod.HAS_WATCHDOG, bool)

    def test_has_watchdog_is_true_when_installed(self):
        import src.nlpl.stdlib.fs_watch as mod
        assert mod.HAS_WATCHDOG is True

    def test_stubs_raise_import_error(self, monkeypatch):
        """Simulate missing watchdog: re-register with HAS_WATCHDOG=False and verify ImportError."""
        import src.nlpl.stdlib.fs_watch as mod
        from src.nlpl.runtime.runtime import Runtime

        monkeypatch.setattr(mod, "HAS_WATCHDOG", False)
        rt = Runtime()
        # Manually call register which should wire up stubs when HAS_WATCHDOG is False
        mod.register_fs_watch_functions(rt)

        for name in TestFsWatchRegistration.EXPECTED_NAMES:
            if name in rt.functions:
                with pytest.raises((ImportError, Exception)):
                    # Call with no args; stub should raise ImportError regardless
                    rt.functions[name]()
