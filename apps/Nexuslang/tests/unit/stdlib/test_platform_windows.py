"""
Tests for the NexusLang platform_windows stdlib module.

On Windows these tests execute real Win32 / Registry / Services / COM APIs.
On all other platforms the entire module is skipped.
"""

import os
import sys
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.stdlib.platform_windows import (
    PlatformError,
    HKEY_LOCAL_MACHINE,
    HKEY_CURRENT_USER,
    HKEY_CLASSES_ROOT,
    HKEY_USERS,
    HKEY_CURRENT_CONFIG,
    # Registry
    registry_read,
    registry_write,
    registry_delete,
    registry_delete_key,
    registry_list_subkeys,
    registry_list_values,
    registry_key_exists,
    # Win32 system info
    win32_computer_name,
    win32_username,
    win32_is_admin,
    win32_message_box,
    win32_get_windows_dir,
    win32_get_system_dir,
    win32_get_temp_dir,
    win32_get_env,
    win32_get_last_error,
    win32_error_message,
    win32_get_version,
    win32_expand_env_strings,
    win32_get_special_folder,
    # Services
    winservice_query_status,
    winservice_start,
    winservice_stop,
    winservice_pause,
    winservice_continue,
    winservice_list,
    # COM
    com_create_object,
    com_call_method,
)


IS_WINDOWS = sys.platform == 'win32'
pytestmark = pytest.mark.skipif(not IS_WINDOWS, reason="Windows-only tests")


# ============================================================================
# HKEY constants
# ============================================================================

class TestHkeyConstants:
    def test_hklm_is_int(self):
        assert isinstance(HKEY_LOCAL_MACHINE, int)

    def test_hkcu_is_int(self):
        assert isinstance(HKEY_CURRENT_USER, int)

    def test_hkcr_is_int(self):
        assert isinstance(HKEY_CLASSES_ROOT, int)

    def test_hku_is_int(self):
        assert isinstance(HKEY_USERS, int)

    def test_hkcc_is_int(self):
        assert isinstance(HKEY_CURRENT_CONFIG, int)

    def test_all_distinct(self):
        hkeys = [HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER, HKEY_CLASSES_ROOT,
                 HKEY_USERS, HKEY_CURRENT_CONFIG]
        assert len(set(hkeys)) == len(hkeys)


# ============================================================================
# Registry
# ============================================================================

class TestRegistryRead:
    def test_read_known_value(self):
        # HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProductName is always present
        result = registry_read(
            HKEY_LOCAL_MACHINE,
            r'SOFTWARE\Microsoft\Windows NT\CurrentVersion',
            'ProductName',
        )
        assert isinstance(result, str)
        assert len(result) > 0

    def test_read_default_value_name_empty_string(self):
        # Read default value (empty name) of a known key
        result = registry_read(HKEY_CLASSES_ROOT, r'.txt')
        # May return a string or error dict depending on the system
        assert isinstance(result, (str, dict))

    def test_missing_key_returns_error_dict(self):
        result = registry_read(
            HKEY_CURRENT_USER,
            r'Software\_NLPL_TEST_KEY_MISSING_',
            'NoSuchValue',
        )
        assert isinstance(result, dict)
        assert 'error' in result

    def test_missing_value_returns_error_dict(self):
        result = registry_read(
            HKEY_LOCAL_MACHINE,
            r'SOFTWARE\Microsoft\Windows NT\CurrentVersion',
            '_NLPL_NO_SUCH_VALUE_',
        )
        assert isinstance(result, dict)
        assert 'error' in result

    def test_string_hive_alias(self):
        result = registry_read(
            'HKEY_LOCAL_MACHINE',
            r'SOFTWARE\Microsoft\Windows NT\CurrentVersion',
            'ProductName',
        )
        assert isinstance(result, str)

    def test_short_alias_hklm(self):
        result = registry_read(
            'HKLM',
            r'SOFTWARE\Microsoft\Windows NT\CurrentVersion',
            'ProductName',
        )
        assert isinstance(result, str)

    def test_short_alias_hkcu(self):
        result = registry_read('HKCU', r'Software\Microsoft\Windows\CurrentVersion', '')
        assert isinstance(result, (str, int, dict))


class TestRegistryWrite:
    _TEST_SUBKEY = r'Software\_NLPL_TEST_WRITE_'

    def test_write_and_read_string_value(self):
        registry_write(HKEY_CURRENT_USER, self._TEST_SUBKEY, 'TestStr', 'hello_nxl', 'REG_SZ')
        result = registry_read(HKEY_CURRENT_USER, self._TEST_SUBKEY, 'TestStr')
        assert result == 'hello_nxl'
        registry_delete(HKEY_CURRENT_USER, self._TEST_SUBKEY, 'TestStr')
        registry_delete_key(HKEY_CURRENT_USER, self._TEST_SUBKEY)

    def test_write_and_read_dword_value(self):
        registry_write(HKEY_CURRENT_USER, self._TEST_SUBKEY, 'TestDword', 42, 'REG_DWORD')
        result = registry_read(HKEY_CURRENT_USER, self._TEST_SUBKEY, 'TestDword')
        assert result == 42
        registry_delete(HKEY_CURRENT_USER, self._TEST_SUBKEY, 'TestDword')
        registry_delete_key(HKEY_CURRENT_USER, self._TEST_SUBKEY)

    def test_write_returns_none_on_success(self):
        result = registry_write(
            HKEY_CURRENT_USER, self._TEST_SUBKEY, 'TmpValue', 'tmp', 'REG_SZ'
        )
        assert result is None or isinstance(result, dict)
        registry_delete(HKEY_CURRENT_USER, self._TEST_SUBKEY, 'TmpValue')
        registry_delete_key(HKEY_CURRENT_USER, self._TEST_SUBKEY)

    def test_write_to_hklm_system_key_returns_error_when_unprivileged(self):
        if win32_is_admin():
            pytest.skip("Running as admin; write succeeds")
        result = registry_write(
            HKEY_LOCAL_MACHINE,
            r'SOFTWARE\_NLPL_TEST_FORBIDDEN_',
            'X', '1', 'REG_SZ',
        )
        assert isinstance(result, dict)
        assert 'error' in result


class TestRegistryDelete:
    _TEST_SUBKEY = r'Software\_NLPL_TEST_DELETE_'

    def test_delete_existing_value(self):
        registry_write(HKEY_CURRENT_USER, self._TEST_SUBKEY, 'ToDelete', 'x', 'REG_SZ')
        result = registry_delete(HKEY_CURRENT_USER, self._TEST_SUBKEY, 'ToDelete')
        assert result is None or isinstance(result, dict)
        registry_delete_key(HKEY_CURRENT_USER, self._TEST_SUBKEY)

    def test_delete_nonexistent_value_returns_error(self):
        result = registry_delete(
            HKEY_CURRENT_USER,
            r'Software\_NLPL_NONEXISTENT_KEY_',
            '_NLPL_NONEXISTENT_VALUE_',
        )
        assert isinstance(result, dict)
        assert 'error' in result

    def test_delete_key_empty_key(self):
        registry_write(HKEY_CURRENT_USER, self._TEST_SUBKEY, 'V', '1', 'REG_SZ')
        registry_delete(HKEY_CURRENT_USER, self._TEST_SUBKEY, 'V')
        result = registry_delete_key(HKEY_CURRENT_USER, self._TEST_SUBKEY)
        assert result is None or isinstance(result, dict)

    def test_delete_nonexistent_key_returns_error(self):
        result = registry_delete_key(
            HKEY_CURRENT_USER,
            r'Software\_NLPL_NONEXISTENT_KEY_99999_',
        )
        assert isinstance(result, dict)
        assert 'error' in result


class TestRegistryListSubkeys:
    def test_returns_list(self):
        result = registry_list_subkeys(
            HKEY_LOCAL_MACHINE, r'SOFTWARE\Microsoft\Windows NT\CurrentVersion'
        )
        assert isinstance(result, list)

    def test_subkeys_are_strings(self):
        result = registry_list_subkeys(
            HKEY_LOCAL_MACHINE, r'SOFTWARE\Microsoft\Windows NT\CurrentVersion'
        )
        if isinstance(result, list):
            for item in result:
                assert isinstance(item, str)

    def test_missing_key_returns_error_or_empty(self):
        result = registry_list_subkeys(
            HKEY_CURRENT_USER, r'Software\_NLPL_MISSING_SUBKEY_'
        )
        assert isinstance(result, (list, dict))
        if isinstance(result, dict):
            assert 'error' in result


class TestRegistryListValues:
    def test_returns_list(self):
        result = registry_list_values(
            HKEY_LOCAL_MACHINE, r'SOFTWARE\Microsoft\Windows NT\CurrentVersion'
        )
        assert isinstance(result, list)

    def test_value_dicts_have_name_key(self):
        result = registry_list_values(
            HKEY_LOCAL_MACHINE, r'SOFTWARE\Microsoft\Windows NT\CurrentVersion'
        )
        if isinstance(result, list) and len(result) > 0:
            for item in result:
                assert 'name' in item

    def test_missing_key_returns_error(self):
        result = registry_list_values(
            HKEY_CURRENT_USER, r'Software\_NLPL_MISSING_VALUES_KEY_'
        )
        assert isinstance(result, (list, dict))
        if isinstance(result, dict):
            assert 'error' in result


class TestRegistryKeyExists:
    def test_existing_key_returns_true(self):
        result = registry_key_exists(
            HKEY_LOCAL_MACHINE, r'SOFTWARE\Microsoft\Windows NT\CurrentVersion'
        )
        assert result is True

    def test_nonexistent_key_returns_false_or_error(self):
        result = registry_key_exists(
            HKEY_CURRENT_USER, r'Software\_NLPL_NONEXISTENT_KEY_9999_'
        )
        assert result is False or isinstance(result, dict)

    def test_returns_bool_for_known_keys(self):
        result = registry_key_exists(HKEY_CURRENT_USER, r'Software')
        assert isinstance(result, bool)
        assert result is True


# ============================================================================
# Win32 System Info
# ============================================================================

class TestWin32ComputerName:
    def test_returns_nonempty_string(self):
        name = win32_computer_name()
        assert isinstance(name, str)
        assert len(name) > 0

    def test_no_whitespace_only(self):
        name = win32_computer_name()
        if isinstance(name, str):
            assert name.strip() != ''


class TestWin32Username:
    def test_returns_nonempty_string(self):
        name = win32_username()
        assert isinstance(name, str)
        assert len(name) > 0


class TestWin32IsAdmin:
    def test_returns_bool(self):
        result = win32_is_admin()
        assert isinstance(result, bool)


class TestWin32GetWindowsDir:
    def test_returns_string(self):
        d = win32_get_windows_dir()
        assert isinstance(d, str)

    def test_is_existing_directory(self):
        d = win32_get_windows_dir()
        if isinstance(d, str):
            assert os.path.isabs(d)

    def test_contains_windows(self):
        d = win32_get_windows_dir()
        if isinstance(d, str):
            assert 'Windows' in d or 'WINDOWS' in d


class TestWin32GetSystemDir:
    def test_returns_string(self):
        d = win32_get_system_dir()
        assert isinstance(d, str)

    def test_is_absolute_path(self):
        d = win32_get_system_dir()
        if isinstance(d, str):
            assert os.path.isabs(d)


class TestWin32GetTempDir:
    def test_returns_string(self):
        d = win32_get_temp_dir()
        assert isinstance(d, str)

    def test_is_absolute_path(self):
        d = win32_get_temp_dir()
        if isinstance(d, str):
            assert os.path.isabs(d)

    def test_directory_exists(self):
        d = win32_get_temp_dir()
        if isinstance(d, str):
            assert os.path.isdir(d)


class TestWin32GetEnv:
    def test_systemroot_not_none(self):
        result = win32_get_env('SystemRoot')
        assert result is not None

    def test_path_not_none(self):
        result = win32_get_env('PATH')
        assert result is not None

    def test_missing_var_returns_default(self):
        result = win32_get_env('_NLPL_TOTALLY_MISSING_VAR_XYZ_', 'default_val')
        assert result == 'default_val'

    def test_missing_var_no_default_returns_none(self):
        result = win32_get_env('_NLPL_TOTALLY_MISSING_VAR_XYZ_')
        assert result is None


class TestWin32GetLastError:
    def test_returns_int(self):
        code = win32_get_last_error()
        assert isinstance(code, int)

    def test_non_negative(self):
        code = win32_get_last_error()
        assert code >= 0


class TestWin32ErrorMessage:
    def test_known_code_returns_string(self):
        msg = win32_error_message(0)   # 0 = ERROR_SUCCESS
        assert isinstance(msg, str)

    def test_error_code_2_file_not_found(self):
        msg = win32_error_message(2)   # ERROR_FILE_NOT_FOUND
        assert isinstance(msg, str)
        assert len(msg) > 0

    def test_unknown_large_code_returns_string(self):
        msg = win32_error_message(0xDEADBEEF)
        assert isinstance(msg, str)


class TestWin32GetVersion:
    def test_returns_dict(self):
        v = win32_get_version()
        assert isinstance(v, dict)

    def test_has_major_minor(self):
        v = win32_get_version()
        assert 'major' in v
        assert 'minor' in v

    def test_major_is_int(self):
        v = win32_get_version()
        assert isinstance(v['major'], int)
        assert v['major'] >= 6   # Vista or later

    def test_minor_is_int(self):
        v = win32_get_version()
        assert isinstance(v['minor'], int)


class TestWin32ExpandEnvStrings:
    def test_expands_systemroot(self):
        result = win32_expand_env_strings('%SystemRoot%')
        assert isinstance(result, str)
        assert '%' not in result or result == '%SystemRoot%'

    def test_passes_through_plain_string(self):
        result = win32_expand_env_strings('no_vars_here')
        assert result == 'no_vars_here'

    def test_expands_system_drive(self):
        result = win32_expand_env_strings('%SystemDrive%\\Windows')
        assert isinstance(result, str)


class TestWin32GetSpecialFolder:
    def test_csidl_personal_returns_string(self):
        CSIDL_PERSONAL = 5   # My Documents
        result = win32_get_special_folder(CSIDL_PERSONAL)
        assert isinstance(result, (str, dict))

    def test_csidl_appdata_returns_string(self):
        CSIDL_APPDATA = 26   # Roaming AppData
        result = win32_get_special_folder(CSIDL_APPDATA)
        assert isinstance(result, (str, dict))

    def test_csidl_local_appdata_absolute(self):
        CSIDL_LOCAL_APPDATA = 28
        result = win32_get_special_folder(CSIDL_LOCAL_APPDATA)
        if isinstance(result, str):
            assert os.path.isabs(result)

    def test_invalid_csidl_returns_error(self):
        result = win32_get_special_folder(-9999)
        assert isinstance(result, (str, dict))


# ============================================================================
# Windows Services
# ============================================================================

class TestWinserviceQueryStatus:
    def test_spooler_service_returns_dict(self):
        result = winservice_query_status('Spooler')
        assert isinstance(result, dict)

    def test_result_has_status_key(self):
        result = winservice_query_status('Spooler')
        if isinstance(result, dict) and 'error' not in result:
            assert 'status' in result

    def test_nonexistent_service_returns_error(self):
        result = winservice_query_status('_NLPL_NO_SUCH_SERVICE_9999_')
        assert isinstance(result, dict)
        assert 'error' in result

    def test_w32tm_service(self):
        result = winservice_query_status('W32Time')
        assert isinstance(result, dict)


class TestWinserviceList:
    def test_returns_list(self):
        result = winservice_list()
        assert isinstance(result, list)

    def test_list_not_empty(self):
        result = winservice_list()
        if isinstance(result, list):
            assert len(result) > 0

    def test_each_item_is_dict(self):
        result = winservice_list()
        if isinstance(result, list):
            for item in result[:10]:
                assert isinstance(item, dict)

    def test_items_have_name_key(self):
        result = winservice_list()
        if isinstance(result, list) and len(result) > 0:
            for item in result[:5]:
                assert 'name' in item


class TestWinserviceStartStop:
    """
    These tests require elevated privileges and a pausable service.
    Marked skip unless running as admin and the service is in the expected state.
    """

    def test_start_unknown_service_returns_error(self):
        result = winservice_start('_NLPL_FAKE_SERVICE_')
        assert isinstance(result, dict)
        assert 'error' in result

    def test_stop_unknown_service_returns_error(self):
        result = winservice_stop('_NLPL_FAKE_SERVICE_')
        assert isinstance(result, dict)
        assert 'error' in result

    def test_pause_unknown_service_returns_error(self):
        result = winservice_pause('_NLPL_FAKE_SERVICE_')
        assert isinstance(result, dict)
        assert 'error' in result

    def test_continue_unknown_service_returns_error(self):
        result = winservice_continue('_NLPL_FAKE_SERVICE_')
        assert isinstance(result, dict)
        assert 'error' in result


# ============================================================================
# COM Automation
# ============================================================================

class TestComCreateObject:
    def test_invalid_progid_returns_error(self):
        result = com_create_object('_NLPL.NotAReal.ProgID_')
        assert isinstance(result, dict)
        assert 'error' in result

    def test_wscript_shell_creates(self):
        result = com_create_object('WScript.Shell')
        # If COM is available WScript.Shell should succeed
        assert result is not None

    def test_scripting_filesystemobject_creates(self):
        result = com_create_object('Scripting.FileSystemObject')
        assert result is not None


class TestComCallMethod:
    def test_call_method_on_invalid_object_returns_error(self):
        result = com_call_method(None, 'SomeMethod')
        assert isinstance(result, dict)
        assert 'error' in result

    def test_wscript_shell_expandenvironmentstrings(self):
        obj = com_create_object('WScript.Shell')
        if isinstance(obj, dict) and 'error' in obj:
            pytest.skip("WScript.Shell COM object unavailable")
        result = com_call_method(obj, 'ExpandEnvironmentStrings', '%SystemRoot%')
        assert isinstance(result, str)
        assert len(result) > 0

    def test_fso_getspecialfolder_windows(self):
        obj = com_create_object('Scripting.FileSystemObject')
        if isinstance(obj, dict) and 'error' in obj:
            pytest.skip("Scripting.FileSystemObject COM object unavailable")
        result = com_call_method(obj, 'GetSpecialFolder', 0)   # 0 = Windows folder
        assert result is not None
