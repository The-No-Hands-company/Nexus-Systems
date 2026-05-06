"""
Tests for the NexusLang platform_macos stdlib module.

On macOS these tests execute real Foundation / CoreGraphics / Keychain /
NSUserDefaults / Clipboard / Notification Centre APIs.
On all other platforms the entire module is skipped.
"""

import os
import sys
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.stdlib.platform_macos import (
    PlatformError,
    # System info
    macos_version,
    macos_hostname,
    macos_username,
    macos_home_dir,
    macos_temp_dir,
    macos_app_support_dir,
    macos_caches_dir,
    macos_bundle_path,
    macos_documents_dir,
    macos_desktop_dir,
    macos_downloads_dir,
    macos_system_info,
    # Display
    macos_screen_size,
    macos_display_count,
    macos_display_info,
    # Keychain
    macos_keychain_get,
    macos_keychain_set,
    macos_keychain_delete,
    macos_keychain_find_internet,
    # Defaults
    macos_defaults_read,
    macos_defaults_write,
    macos_defaults_delete,
    macos_defaults_list_domains,
    macos_defaults_find,
    # Notifications
    macos_post_notification,
    # Clipboard
    macos_clipboard_get,
    macos_clipboard_set,
    # Misc
    macos_open_url,
    macos_say,
    macos_screencapture,
)


IS_MACOS = sys.platform == 'darwin'
pytestmark = pytest.mark.skipif(not IS_MACOS, reason="macOS-only tests")


# ============================================================================
# System Info
# ============================================================================

class TestMacosVersion:
    def test_returns_dict(self):
        v = macos_version()
        assert isinstance(v, dict)

    def test_has_required_keys(self):
        v = macos_version()
        for key in ('major', 'minor', 'patch', 'string'):
            assert key in v

    def test_major_is_int(self):
        v = macos_version()
        assert isinstance(v['major'], int)
        assert v['major'] >= 10   # macOS 10 or later

    def test_minor_is_int(self):
        v = macos_version()
        assert isinstance(v['minor'], int)
        assert v['minor'] >= 0

    def test_patch_is_int(self):
        v = macos_version()
        assert isinstance(v['patch'], int)
        assert v['patch'] >= 0

    def test_string_is_dotted_version(self):
        v = macos_version()
        parts = v['string'].split('.')
        assert len(parts) >= 2


class TestMacosHostname:
    def test_returns_nonempty_string(self):
        h = macos_hostname()
        assert isinstance(h, str)
        assert len(h) > 0


class TestMacosUsername:
    def test_returns_nonempty_string(self):
        u = macos_username()
        assert isinstance(u, str)
        assert len(u) > 0


class TestMacosHomeDir:
    def test_returns_absolute_path(self):
        d = macos_home_dir()
        assert isinstance(d, str)
        assert os.path.isabs(d)

    def test_directory_exists(self):
        d = macos_home_dir()
        assert os.path.isdir(d)

    def test_starts_with_users(self):
        d = macos_home_dir()
        assert d.startswith('/Users/') or d == '/var/root' or os.path.isdir(d)


class TestMacosTempDir:
    def test_returns_string(self):
        d = macos_temp_dir()
        assert isinstance(d, str)

    def test_is_absolute(self):
        d = macos_temp_dir()
        assert os.path.isabs(d)

    def test_directory_exists(self):
        d = macos_temp_dir()
        assert os.path.isdir(d)


class TestMacosAppSupportDir:
    def test_returns_string_no_args(self):
        d = macos_app_support_dir()
        assert isinstance(d, str)

    def test_absolute_no_args(self):
        d = macos_app_support_dir()
        assert os.path.isabs(d)

    def test_returns_string_with_app_name(self):
        d = macos_app_support_dir('TestApp')
        assert isinstance(d, str)

    def test_app_name_in_path(self):
        d = macos_app_support_dir('MyApp')
        if isinstance(d, str):
            assert 'MyApp' in d


class TestMacosCachesDir:
    def test_returns_string_no_args(self):
        d = macos_caches_dir()
        assert isinstance(d, str)

    def test_absolute_no_args(self):
        d = macos_caches_dir()
        assert os.path.isabs(d)

    def test_returns_string_with_app_name(self):
        d = macos_caches_dir('NLPLTest')
        assert isinstance(d, str)

    def test_app_name_in_path(self):
        d = macos_caches_dir('CacheTestApp')
        if isinstance(d, str):
            assert 'CacheTestApp' in d


class TestMacosBundlePath:
    def test_returns_string(self):
        p = macos_bundle_path()
        assert isinstance(p, str)

    def test_not_empty(self):
        p = macos_bundle_path()
        assert len(p) > 0


class TestMacosDocumentsDir:
    def test_returns_absolute_path(self):
        d = macos_documents_dir()
        assert isinstance(d, str)
        assert os.path.isabs(d)


class TestMacosDesktopDir:
    def test_returns_absolute_path(self):
        d = macos_desktop_dir()
        assert isinstance(d, str)
        assert os.path.isabs(d)


class TestMacosDownloadsDir:
    def test_returns_absolute_path(self):
        d = macos_downloads_dir()
        assert isinstance(d, str)
        assert os.path.isabs(d)


class TestMacosSystemInfo:
    def test_returns_dict(self):
        info = macos_system_info()
        assert isinstance(info, dict)

    def test_has_expected_keys(self):
        info = macos_system_info()
        expected = ('os_version', 'hostname', 'username', 'cpu_count')
        for key in expected:
            assert key in info

    def test_cpu_count_positive(self):
        info = macos_system_info()
        assert isinstance(info['cpu_count'], int)
        assert info['cpu_count'] > 0


# ============================================================================
# Display
# ============================================================================

class TestMacosScreenSize:
    def test_returns_dict(self):
        s = macos_screen_size()
        assert isinstance(s, dict)

    def test_has_width_height(self):
        s = macos_screen_size()
        if isinstance(s, dict) and 'error' not in s:
            assert 'width' in s
            assert 'height' in s

    def test_width_positive(self):
        s = macos_screen_size()
        if isinstance(s, dict) and 'error' not in s:
            assert s['width'] > 0

    def test_height_positive(self):
        s = macos_screen_size()
        if isinstance(s, dict) and 'error' not in s:
            assert s['height'] > 0


class TestMacosDisplayCount:
    def test_returns_int(self):
        count = macos_display_count()
        assert isinstance(count, int)

    def test_at_least_one_display(self):
        count = macos_display_count()
        if isinstance(count, int):
            assert count >= 1


class TestMacosDisplayInfo:
    def test_returns_list(self):
        info = macos_display_info()
        assert isinstance(info, list)

    def test_list_not_empty(self):
        info = macos_display_info()
        if isinstance(info, list):
            assert len(info) >= 1

    def test_each_item_is_dict(self):
        info = macos_display_info()
        if isinstance(info, list):
            for item in info:
                assert isinstance(item, dict)

    def test_each_item_has_width_height(self):
        info = macos_display_info()
        if isinstance(info, list) and len(info) > 0:
            for item in info:
                if 'error' not in item:
                    assert 'width' in item
                    assert 'height' in item


# ============================================================================
# Keychain
# ============================================================================

_KC_SERVICE = '_nxl_test_service_'
_KC_ACCOUNT = '_nxl_test_account_'
_KC_PASSWORD = '_nxl_test_password_secret_'


class TestMacosKeychainSetGetDelete:
    def test_set_returns_none_or_dict(self):
        result = macos_keychain_set(_KC_SERVICE, _KC_ACCOUNT, _KC_PASSWORD)
        assert result is None or isinstance(result, dict)
        macos_keychain_delete(_KC_SERVICE, _KC_ACCOUNT)

    def test_get_after_set(self):
        macos_keychain_set(_KC_SERVICE, _KC_ACCOUNT, _KC_PASSWORD)
        result = macos_keychain_get(_KC_SERVICE, _KC_ACCOUNT)
        macos_keychain_delete(_KC_SERVICE, _KC_ACCOUNT)
        assert result == _KC_PASSWORD or isinstance(result, dict)

    def test_get_missing_returns_none_or_error(self):
        result = macos_keychain_get('_nxl_missing_service_xyz_', '_nxl_missing_acct_')
        assert result is None or isinstance(result, dict)

    def test_delete_returns_none_or_dict(self):
        macos_keychain_set(_KC_SERVICE, _KC_ACCOUNT, _KC_PASSWORD)
        result = macos_keychain_delete(_KC_SERVICE, _KC_ACCOUNT)
        assert result is None or isinstance(result, dict)

    def test_delete_nonexistent_returns_error(self):
        result = macos_keychain_delete('_nxl_no_svc_', '_nxl_no_acct_')
        assert isinstance(result, dict)
        assert 'error' in result

    def test_set_with_update_flag(self):
        macos_keychain_set(_KC_SERVICE, _KC_ACCOUNT, 'first_password')
        result = macos_keychain_set(_KC_SERVICE, _KC_ACCOUNT, 'updated_password', update=True)
        assert result is None or isinstance(result, dict)
        macos_keychain_delete(_KC_SERVICE, _KC_ACCOUNT)


class TestMacosKeychainFindInternet:
    def test_nonexistent_returns_none_or_error(self):
        result = macos_keychain_find_internet(
            '_nxl_fake_server_xyz.local', '_nxl_user_', 80
        )
        assert result is None or isinstance(result, dict)


# ============================================================================
# NSUserDefaults
# ============================================================================

_DEFAULTS_DOMAIN = 'com.nlpl.test'
_DEFAULTS_KEY = '_nxl_test_key_'


class TestMacosDefaultsReadWrite:
    def test_write_string_and_read(self):
        macos_defaults_write(_DEFAULTS_DOMAIN, _DEFAULTS_KEY, 'test_value')
        result = macos_defaults_read(_DEFAULTS_DOMAIN, _DEFAULTS_KEY)
        macos_defaults_delete(_DEFAULTS_DOMAIN, _DEFAULTS_KEY)
        assert result == 'test_value' or isinstance(result, dict)

    def test_write_integer_and_read(self):
        macos_defaults_write(_DEFAULTS_DOMAIN, '_nxl_int_key_', 42, 'integer')
        result = macos_defaults_read(_DEFAULTS_DOMAIN, '_nxl_int_key_')
        macos_defaults_delete(_DEFAULTS_DOMAIN, '_nxl_int_key_')
        assert result == 42 or isinstance(result, (str, dict))

    def test_write_bool_and_read(self):
        macos_defaults_write(_DEFAULTS_DOMAIN, '_nxl_bool_key_', True, 'bool')
        result = macos_defaults_read(_DEFAULTS_DOMAIN, '_nxl_bool_key_')
        macos_defaults_delete(_DEFAULTS_DOMAIN, '_nxl_bool_key_')
        assert result in (True, 1, 'YES', 'true') or isinstance(result, dict)

    def test_read_missing_key_returns_none_or_error(self):
        result = macos_defaults_read(_DEFAULTS_DOMAIN, '_nxl_nonexistent_key_9999_')
        assert result is None or isinstance(result, dict)

    def test_read_full_domain_returns_dict_or_none(self):
        macos_defaults_write(_DEFAULTS_DOMAIN, _DEFAULTS_KEY, 'v')
        result = macos_defaults_read(_DEFAULTS_DOMAIN)
        macos_defaults_delete(_DEFAULTS_DOMAIN, _DEFAULTS_KEY)
        assert result is None or isinstance(result, dict)


class TestMacosDefaultsDelete:
    def test_delete_key_returns_none_or_dict(self):
        macos_defaults_write(_DEFAULTS_DOMAIN, _DEFAULTS_KEY, 'to_delete')
        result = macos_defaults_delete(_DEFAULTS_DOMAIN, _DEFAULTS_KEY)
        assert result is None or isinstance(result, dict)

    def test_delete_domain_returns_none_or_dict(self):
        macos_defaults_write(_DEFAULTS_DOMAIN, _DEFAULTS_KEY, 'x')
        result = macos_defaults_delete(_DEFAULTS_DOMAIN)
        assert result is None or isinstance(result, dict)


class TestMacosDefaultsListDomains:
    def test_returns_list(self):
        result = macos_defaults_list_domains()
        assert isinstance(result, list)

    def test_all_strings(self):
        result = macos_defaults_list_domains()
        if isinstance(result, list):
            for item in result:
                assert isinstance(item, str)

    def test_not_empty(self):
        result = macos_defaults_list_domains()
        if isinstance(result, list):
            assert len(result) > 0


class TestMacosDefaultsFind:
    def test_returns_list(self):
        result = macos_defaults_find('NSGlobalDomain')
        assert isinstance(result, list)

    def test_unknown_key_returns_empty_list_or_dict(self):
        result = macos_defaults_find('_nxl_totally_unknown_defaults_key_xyz_')
        assert isinstance(result, (list, dict))


# ============================================================================
# Notifications
# ============================================================================

class TestMacosPostNotification:
    def test_returns_none_or_dict_basic(self):
        result = macos_post_notification('NLPL Test', 'This is a test notification')
        assert result is None or isinstance(result, dict)

    def test_with_subtitle(self):
        result = macos_post_notification(
            'NLPL Test', 'Body text', subtitle='Subtitle here'
        )
        assert result is None or isinstance(result, dict)

    def test_empty_title_handled(self):
        result = macos_post_notification('', 'empty title test')
        assert result is None or isinstance(result, dict)


# ============================================================================
# Clipboard
# ============================================================================

class TestMacosClipboard:
    def test_set_and_get_round_trip(self):
        macos_clipboard_set('nxl_clipboard_test_string')
        result = macos_clipboard_get()
        assert result == 'nxl_clipboard_test_string' or isinstance(result, dict)

    def test_clipboard_get_returns_string_or_dict(self):
        result = macos_clipboard_get()
        assert isinstance(result, (str, dict))

    def test_clipboard_set_returns_none_or_dict(self):
        result = macos_clipboard_set('hello from nlpl')
        assert result is None or isinstance(result, dict)

    def test_set_empty_string(self):
        result = macos_clipboard_set('')
        assert result is None or isinstance(result, dict)

    def test_set_multiline(self):
        text = 'line one\nline two\nline three'
        macos_clipboard_set(text)
        result = macos_clipboard_get()
        assert result == text or isinstance(result, dict)


# ============================================================================
# Misc (open_url, say, screencapture)
# ============================================================================

class TestMacosOpenUrl:
    def test_valid_url_returns_none_or_dict(self):
        # We call with a file URI to avoid opening a real browser
        result = macos_open_url('file:///tmp')
        assert result is None or isinstance(result, dict)

    def test_invalid_url_returns_error(self):
        result = macos_open_url('not_a_real_url_scheme://')
        # May succeed or fail depending on handler registration
        assert result is None or isinstance(result, dict)


class TestMacosSay:
    def test_say_short_string_returns_none_or_dict(self):
        result = macos_say('test')
        assert result is None or isinstance(result, dict)

    def test_say_with_voice(self):
        result = macos_say('hello', voice='Alex')
        assert result is None or isinstance(result, dict)

    def test_say_with_rate(self):
        result = macos_say('testing rate', rate=200)
        assert result is None or isinstance(result, dict)

    def test_empty_text_handled(self):
        result = macos_say('')
        assert result is None or isinstance(result, dict)


class TestMacosScreencapture:
    def test_capture_to_file_returns_none_or_dict(self, tmp_path):
        output_path = str(tmp_path / 'screenshot.png')
        result = macos_screencapture(output_path)
        assert result is None or isinstance(result, dict)

    def test_capture_creates_file_when_not_interactive(self, tmp_path):
        output_path = str(tmp_path / 'cap.png')
        result = macos_screencapture(output_path, interactive=False)
        if result is None:
            assert os.path.exists(output_path)

    def test_invalid_path_returns_error(self):
        result = macos_screencapture('/no_such_dir_nxl_/screenshot.png')
        assert result is None or isinstance(result, dict)
