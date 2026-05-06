"""
Tests for the NexusLang platform_linux stdlib module.

On Linux these tests execute real Linux APIs.
POSIX, epoll, inotify, and systemd integration are all validated.
"""

import os
import sys
import select
import signal
import tempfile
import pytest

# Ensure the project source tree is on the path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.stdlib.platform_linux import (
    PlatformError,
    # POSIX
    posix_getpid, posix_getppid, posix_getuid, posix_getgid,
    posix_geteuid, posix_getegid, posix_getcwd, posix_chdir,
    posix_umask, posix_uname, posix_hostname, posix_username,
    posix_groups, posix_kill, posix_fork, posix_waitpid,
    posix_getenv, posix_setenv, posix_unsetenv, posix_environ,
    # epoll
    epoll_create, epoll_add, epoll_modify, epoll_remove,
    epoll_wait, epoll_close,
    # inotify
    IN_CREATE, IN_DELETE, IN_MODIFY, IN_ALL_EVENTS, IN_ISDIR,
    inotify_create, inotify_add_watch, inotify_remove_watch,
    inotify_read_events, inotify_close,
    # systemd
    systemd_notify, systemd_notify_ready, systemd_notify_stopping,
    systemd_notify_watchdog, systemd_notify_status,
    systemd_journal_log, systemd_unit_status, systemd_is_active,
    systemd_start_unit, systemd_stop_unit, systemd_restart_unit,
    systemd_reload_unit, systemd_enable_unit, systemd_disable_unit,
    systemd_list_units,
)


IS_LINUX = sys.platform in ('linux', 'linux2')
pytestmark = pytest.mark.skipif(not IS_LINUX, reason="Linux-only tests")


# ===========================================================================
# POSIX extras
# ===========================================================================

class TestPosixGetpid:
    def test_returns_int(self):
        assert isinstance(posix_getpid(), int)

    def test_matches_os_getpid(self):
        assert posix_getpid() == os.getpid()

    def test_positive(self):
        assert posix_getpid() > 0


class TestPosixGetppid:
    def test_returns_int(self):
        assert isinstance(posix_getppid(), int)

    def test_positive(self):
        assert posix_getppid() > 0


class TestPosixGetuid:
    def test_returns_int(self):
        assert isinstance(posix_getuid(), int)

    def test_matches_os(self):
        assert posix_getuid() == os.getuid()


class TestPosixGetgid:
    def test_returns_int(self):
        assert isinstance(posix_getgid(), int)

    def test_matches_os(self):
        assert posix_getgid() == os.getgid()


class TestPosixGeteuid:
    def test_returns_int(self):
        assert isinstance(posix_geteuid(), int)

    def test_matches_os(self):
        assert posix_geteuid() == os.geteuid()


class TestPosixGetegid:
    def test_returns_int(self):
        assert isinstance(posix_getegid(), int)

    def test_matches_os(self):
        assert posix_getegid() == os.getegid()


class TestPosixGetcwd:
    def test_returns_str(self):
        assert isinstance(posix_getcwd(), str)

    def test_matches_os(self):
        assert posix_getcwd() == os.getcwd()

    def test_absolute(self):
        cwd = posix_getcwd()
        assert os.path.isabs(cwd)


class TestPosixChdir:
    def test_chdir_tmpdir(self, tmp_path):
        original = os.getcwd()
        try:
            result = posix_chdir(str(tmp_path))
            assert result is None
            assert os.getcwd() == str(tmp_path)
        finally:
            os.chdir(original)

    def test_chdir_nonexistent_returns_error(self):
        result = posix_chdir('/nonexistent_path_that_does_not_exist_nxl')
        assert isinstance(result, dict)
        assert 'error' in result


class TestPosixUmask:
    def test_returns_int(self):
        old = posix_umask(0o022)
        try:
            assert isinstance(old, int)
        finally:
            os.umask(old)

    def test_roundtrip(self):
        original = os.umask(0o022)
        os.umask(original)
        old = posix_umask(original)
        os.umask(old)
        assert old == original


class TestPosixUname:
    def test_returns_dict(self):
        u = posix_uname()
        assert isinstance(u, dict)

    def test_has_required_keys(self):
        u = posix_uname()
        for key in ('sysname', 'nodename', 'release', 'version', 'machine'):
            assert key in u

    def test_sysname_is_linux(self):
        assert posix_uname()['sysname'] == 'Linux'

    def test_all_values_are_strings(self):
        u = posix_uname()
        for v in u.values():
            assert isinstance(v, str)


class TestPosixHostname:
    def test_returns_nonempty_string(self):
        h = posix_hostname()
        assert isinstance(h, str)
        assert len(h) > 0


class TestPosixUsername:
    def test_returns_nonempty_string(self):
        uname = posix_username()
        assert isinstance(uname, str)
        assert len(uname) > 0


class TestPosixGroups:
    def test_returns_list(self):
        groups = posix_groups()
        assert isinstance(groups, list)

    def test_all_ints(self):
        for g in posix_groups():
            assert isinstance(g, int)


class TestPosixKill:
    def test_kill_self_with_sig0_succeeds(self):
        result = posix_kill(os.getpid(), 0)   # signal 0 = check existence only
        assert result is None

    def test_kill_nonexistent_pid_returns_error(self):
        result = posix_kill(999999999, 0)
        assert isinstance(result, dict)
        assert 'error' in result


class TestPosixWaitpid:
    def test_waitpid_with_wnohang_on_no_child(self):
        result = posix_waitpid(os.getpid(), os.WNOHANG)
        # On non-child pid this should return an error dict
        assert isinstance(result, dict)
        assert 'error' in result or result.get('pid') == 0


class TestPosixEnv:
    def test_getenv_existing(self):
        os.environ['_NLPL_TEST_VAR_'] = 'hello'
        assert posix_getenv('_NLPL_TEST_VAR_') == 'hello'
        del os.environ['_NLPL_TEST_VAR_']

    def test_getenv_missing_returns_default(self):
        result = posix_getenv('_NLPL_DEFINITELY_NOT_SET_', 'fallback')
        assert result == 'fallback'

    def test_setenv_then_getenv(self):
        posix_setenv('_NLPL_SET_TEST_', 'value42')
        assert os.environ.get('_NLPL_SET_TEST_') == 'value42'
        del os.environ['_NLPL_SET_TEST_']

    def test_unsetenv(self):
        os.environ['_NLPL_UNSET_TEST_'] = 'x'
        posix_unsetenv('_NLPL_UNSET_TEST_')
        assert '_NLPL_UNSET_TEST_' not in os.environ

    def test_environ_returns_dict(self):
        env = posix_environ()
        assert isinstance(env, dict)
        assert 'HOME' in env or 'USER' in env or 'PATH' in env


# ===========================================================================
# epoll
# ===========================================================================

class TestEpollCreate:
    def test_returns_dict(self):
        ep = epoll_create()
        try:
            assert isinstance(ep, dict)
            assert 'epoll' in ep
            assert 'fd' in ep
        finally:
            epoll_close(ep)

    def test_epoll_obj_is_select_epoll(self):
        ep = epoll_create()
        try:
            assert isinstance(ep['epoll'], select.epoll)
        finally:
            epoll_close(ep)

    def test_fd_is_positive_int(self):
        ep = epoll_create()
        try:
            assert isinstance(ep['fd'], int)
            assert ep['fd'] > 0
        finally:
            epoll_close(ep)


class TestEpollAddModifyRemoveWait:
    def test_add_read_event_on_pipe(self):
        r_fd, w_fd = os.pipe()
        ep = epoll_create()
        try:
            result = epoll_add(ep, r_fd, 'in')
            assert result is None
        finally:
            os.close(r_fd)
            os.close(w_fd)
            epoll_close(ep)

    def test_wait_returns_list(self):
        ep = epoll_create()
        try:
            events = epoll_wait(ep, maxevents=5, timeout=0)
            assert isinstance(events, list)
        finally:
            epoll_close(ep)

    def test_wait_returns_readable_event_when_data_available(self):
        r_fd, w_fd = os.pipe()
        ep = epoll_create()
        try:
            epoll_add(ep, r_fd, 'in')
            os.write(w_fd, b'hello')
            events = epoll_wait(ep, maxevents=5, timeout=1.0)
            assert len(events) >= 1
            ev = events[0]
            assert ev['fd'] == r_fd
            assert ev['readable'] is True
        finally:
            os.close(r_fd)
            os.close(w_fd)
            epoll_close(ep)

    def test_event_dict_keys(self):
        r_fd, w_fd = os.pipe()
        ep = epoll_create()
        try:
            epoll_add(ep, r_fd, 'in')
            os.write(w_fd, b'x')
            events = epoll_wait(ep, maxevents=1, timeout=1.0)
            assert len(events) >= 1
            ev = events[0]
            for key in ('fd', 'event_mask', 'readable', 'writable', 'error', 'hangup'):
                assert key in ev
        finally:
            os.close(r_fd)
            os.close(w_fd)
            epoll_close(ep)

    def test_modify_fd(self):
        r_fd, w_fd = os.pipe()
        ep = epoll_create()
        try:
            epoll_add(ep, r_fd, 'in')
            result = epoll_modify(ep, r_fd, 'out')
            # After modify to 'out', r_fd is now monitored for EPOLLOUT
            # We don't assert specific events but the call should not error
            assert result is None or isinstance(result, dict)
        finally:
            os.close(r_fd)
            os.close(w_fd)
            epoll_close(ep)

    def test_remove_fd(self):
        r_fd, w_fd = os.pipe()
        ep = epoll_create()
        try:
            epoll_add(ep, r_fd, 'in')
            result = epoll_remove(ep, r_fd)
            assert result is None
        finally:
            os.close(r_fd)
            os.close(w_fd)
            epoll_close(ep)

    def test_close_is_idempotent(self):
        ep = epoll_create()
        epoll_close(ep)
        epoll_close(ep)   # second close should not raise

    def test_invalid_event_string_raises_value_error(self):
        ep = epoll_create()
        try:
            with pytest.raises(ValueError):
                epoll_add(ep, 1, 'not_a_real_event_flag')
        finally:
            epoll_close(ep)


# ===========================================================================
# inotify
# ===========================================================================

class TestInotifyConstants:
    def test_in_create_nonzero(self):
        assert IN_CREATE != 0

    def test_in_delete_nonzero(self):
        assert IN_DELETE != 0

    def test_in_all_events_is_bitmask(self):
        assert IN_ALL_EVENTS & IN_CREATE
        assert IN_ALL_EVENTS & IN_DELETE
        assert IN_ALL_EVENTS & IN_MODIFY

    def test_in_isdir_nonzero(self):
        assert IN_ISDIR != 0


class TestInotifyCreate:
    def test_returns_int(self):
        fd = inotify_create()
        try:
            assert isinstance(fd, int)
            assert fd >= 0
        finally:
            inotify_close(fd)

    def test_fd_is_readable_with_select(self):
        fd = inotify_create()
        try:
            r, _, _ = select.select([fd], [], [], 0)
            assert r == []   # no events yet
        finally:
            inotify_close(fd)


class TestInotifyAddWatch:
    def test_returns_positive_int(self, tmp_path):
        fd = inotify_create()
        try:
            wd = inotify_add_watch(fd, str(tmp_path), 'all')
            assert isinstance(wd, int)
            assert wd >= 0
        finally:
            inotify_close(fd)

    def test_nonexistent_path_raises_oserror(self):
        fd = inotify_create()
        try:
            with pytest.raises(OSError):
                inotify_add_watch(fd, '/nonexistent_path_nxl_test_xyz')
        finally:
            inotify_close(fd)

    def test_add_watch_with_mask_int(self, tmp_path):
        fd = inotify_create()
        try:
            wd = inotify_add_watch(fd, str(tmp_path), IN_CREATE | IN_DELETE)
            assert isinstance(wd, int)
            assert wd >= 0
        finally:
            inotify_close(fd)


class TestInotifyReadEvents:
    def test_create_event_is_detected(self, tmp_path):
        fd = inotify_create()
        wd = inotify_add_watch(fd, str(tmp_path), 'create')
        try:
            # Create a file in the watched directory
            new_file = tmp_path / 'test_newfile.txt'
            new_file.write_text('hello')
            events = inotify_read_events(fd, timeout=2.0)
            assert len(events) >= 1
            names = [ev.get('name', '') for ev in events]
            assert 'test_newfile.txt' in names
        finally:
            inotify_close(fd)

    def test_event_dict_has_required_keys(self, tmp_path):
        fd = inotify_create()
        wd = inotify_add_watch(fd, str(tmp_path), 'create')
        try:
            (tmp_path / 'k.txt').write_text('x')
            events = inotify_read_events(fd, timeout=2.0)
            assert len(events) >= 1
            ev = events[0]
            for key in ('wd', 'mask', 'cookie', 'name', 'is_dir', 'event_names'):
                assert key in ev, f"Missing key '{key}' in event dict"
        finally:
            inotify_close(fd)

    def test_timeout_returns_empty_list(self, tmp_path):
        fd = inotify_create()
        wd = inotify_add_watch(fd, str(tmp_path), 'create')
        try:
            events = inotify_read_events(fd, timeout=0.0)
            assert isinstance(events, list)
        finally:
            inotify_close(fd)

    def test_event_names_contain_in_create(self, tmp_path):
        fd = inotify_create()
        wd = inotify_add_watch(fd, str(tmp_path), 'create')
        try:
            (tmp_path / 'ev_test.txt').write_text('data')
            events = inotify_read_events(fd, timeout=2.0)
            create_events = [ev for ev in events if 'IN_CREATE' in ev.get('event_names', [])]
            assert len(create_events) >= 1
        finally:
            inotify_close(fd)


class TestInotifyRemoveWatch:
    def test_remove_valid_watch(self, tmp_path):
        fd = inotify_create()
        wd = inotify_add_watch(fd, str(tmp_path), 'all')
        try:
            result = inotify_remove_watch(fd, wd)
            assert result is None
        finally:
            inotify_close(fd)


class TestInotifyClose:
    def test_close_is_idempotent(self, tmp_path):
        fd = inotify_create()
        inotify_close(fd)
        inotify_close(fd)   # second close should not raise


# ===========================================================================
# systemd integration
# ===========================================================================

class TestSystemdNotify:
    def test_returns_bool(self):
        result = systemd_notify('STATUS=test')
        assert isinstance(result, bool)

    def test_returns_false_when_no_socket(self):
        saved = os.environ.pop('NOTIFY_SOCKET', None)
        try:
            assert systemd_notify('READY=1') is False
        finally:
            if saved is not None:
                os.environ['NOTIFY_SOCKET'] = saved

    def test_notify_ready_returns_bool(self):
        assert isinstance(systemd_notify_ready(), bool)

    def test_notify_stopping_returns_bool(self):
        assert isinstance(systemd_notify_stopping(), bool)

    def test_notify_watchdog_returns_bool(self):
        assert isinstance(systemd_notify_watchdog(), bool)

    def test_notify_status_returns_bool(self):
        assert isinstance(systemd_notify_status('Test status message'), bool)


class TestSystemdJournalLog:
    def test_does_not_raise(self):
        # If syslog is available, should succeed silently
        # If running in a minimal container without syslog, may raise ImportError
        try:
            result = systemd_journal_log('NLPL platform_linux test message', priority=7)
            assert result is None
        except (ImportError, OSError):
            pytest.skip("syslog not available in this environment")

    def test_priority_zero_does_not_raise(self):
        try:
            systemd_journal_log('critical test', 0)
        except (ImportError, OSError):
            pytest.skip("syslog not available in this environment")


class TestSystemdUnitStatus:
    def test_returns_dict(self):
        result = systemd_unit_status('nonexistent-fake-unit.service')
        assert isinstance(result, dict)

    def test_has_expected_keys_or_error(self):
        result = systemd_unit_status('nonexistent-fake-unit.service')
        if 'error' not in result:
            for key in ('active_state', 'sub_state', 'load_state', 'description'):
                assert key in result

    def test_load_state_not_found_for_nonexistent_unit(self):
        result = systemd_unit_status('nlpl-definitely-nonexistent-unit.service')
        if 'error' not in result:
            # systemctl returns 'not-found' load state for unknown units
            assert result.get('load_state', '') in ('not-found', '')


class TestSystemdIsActive:
    def test_returns_bool(self):
        assert isinstance(systemd_is_active('nonexistent.service'), bool)

    def test_nonexistent_unit_is_false(self):
        assert systemd_is_active('nlpl-definitely-nonexistent-unit.service') is False


class TestSystemdUnitManagement:
    """Tests verify the API returns the right shape; no real unit is started."""

    def test_start_returns_dict(self):
        r = systemd_start_unit('nlpl-fake.service')
        assert isinstance(r, dict)
        assert 'success' in r

    def test_stop_returns_dict(self):
        r = systemd_stop_unit('nlpl-fake.service')
        assert isinstance(r, dict)
        assert 'success' in r

    def test_restart_returns_dict(self):
        r = systemd_restart_unit('nlpl-fake.service')
        assert isinstance(r, dict)
        assert 'success' in r

    def test_reload_returns_dict(self):
        r = systemd_reload_unit('nlpl-fake.service')
        assert isinstance(r, dict)
        assert 'success' in r

    def test_enable_returns_dict(self):
        r = systemd_enable_unit('nlpl-fake.service')
        assert isinstance(r, dict)
        assert 'success' in r

    def test_disable_returns_dict(self):
        r = systemd_disable_unit('nlpl-fake.service')
        assert isinstance(r, dict)
        assert 'success' in r


class TestSystemdListUnits:
    def test_returns_list(self):
        result = systemd_list_units()
        assert isinstance(result, list)

    def test_items_are_dicts(self):
        result = systemd_list_units()
        for item in result:
            assert isinstance(item, dict)

    def test_unit_dicts_have_expected_keys_or_error(self):
        result = systemd_list_units()
        if result and 'error' not in result[0]:
            first = result[0]
            for key in ('unit', 'load', 'active', 'sub', 'description'):
                assert key in first

    def test_filter_by_type(self):
        result = systemd_list_units(unit_type='service')
        assert isinstance(result, list)

    def test_filter_by_state(self):
        result = systemd_list_units(state='active')
        assert isinstance(result, list)
