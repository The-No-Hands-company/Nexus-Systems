"""
Tests for NexusLang security system.
"""

import pytest
import os
import tempfile
from nexuslang.security import (
    PermissionManager,
    PermissionType,
    PermissionDeniedError,
    validate_path,
    PathTraversalError,
    safe_execute,
    CommandInjectionError,
    validate_email,
    validate_url,
    escape_html,
    sanitize_sql_identifier,
    ValidationError,
)


class TestPermissionManager:
    """Test the permission system."""
    
    def test_deny_by_default(self):
        """Test that all permissions are denied by default."""
        manager = PermissionManager()
        
        with pytest.raises(PermissionDeniedError):
            manager.check(PermissionType.READ, "/tmp/test.txt")
        
        with pytest.raises(PermissionDeniedError):
            manager.check(PermissionType.NET, "example.com")
    
    def test_grant_permission(self):
        """Test granting a permission."""
        manager = PermissionManager()
        manager.grant(PermissionType.READ)
        
        # Should not raise
        assert manager.check(PermissionType.READ, "/tmp/test.txt")
    
    def test_scoped_permission(self):
        """Test permissions with scope restrictions."""
        manager = PermissionManager()
        manager.grant(PermissionType.READ, ["/home/user/data/"])
        
        # Should allow within scope
        assert manager.check(PermissionType.READ, "/home/user/data/file.txt")
        
        # Should deny outside scope
        with pytest.raises(PermissionDeniedError):
            manager.check(PermissionType.READ, "/etc/passwd")
    
    def test_has_permission_no_throw(self):
        """Test non-throwing permission check."""
        manager = PermissionManager()
        
        # Should return False, not raise
        assert not manager.has_permission(PermissionType.WRITE, "/tmp/test.txt")
        
        manager.grant(PermissionType.WRITE)
        assert manager.has_permission(PermissionType.WRITE, "/tmp/test.txt")
    
    def test_revoke_permission(self):
        """Test revoking a permission."""
        manager = PermissionManager()
        manager.grant(PermissionType.FFI)
        assert manager.has_permission(PermissionType.FFI)
        
        manager.revoke(PermissionType.FFI)
        assert not manager.has_permission(PermissionType.FFI)
    
    def test_allow_all(self):
        """Test granting all permissions."""
        manager = PermissionManager(allow_all=True)
        
        # All permissions should be granted
        assert manager.has_permission(PermissionType.READ)
        assert manager.has_permission(PermissionType.WRITE)
        assert manager.has_permission(PermissionType.NET)
        assert manager.has_permission(PermissionType.FFI)


class TestPathValidation:
    """Test path validation utilities."""
    
    def test_normalize_path(self):
        """Test path normalization."""
        from nexuslang.security import normalize_path
        
        # Should resolve .. and .
        path = normalize_path("./test/../file.txt")
        assert ".." not in path
    
    def test_path_traversal_detection(self):
        """Test detection of path traversal attempts."""
        dangerous_paths = [
            "../etc/passwd",
            "../../secret.txt",
            "test/../../../etc/passwd",
        ]
        
        for path in dangerous_paths:
            with pytest.raises(PathTraversalError):
                validate_path(path, allow_absolute=False)
    
    def test_null_byte_rejection(self):
        """Test rejection of null bytes in paths."""
        with pytest.raises(PathTraversalError):
            validate_path("test\x00.txt")
    
    def test_allowed_directories(self):
        """Test whitelisting of allowed directories."""
        with tempfile.TemporaryDirectory() as tmpdir:
            allowed_dirs = [tmpdir]
            
            # Should allow within tmpdir
            test_file = os.path.join(tmpdir, "test.txt")
            validated = validate_path(test_file, allowed_dirs=allowed_dirs, allow_absolute=True)
            assert validated.startswith(tmpdir)
            
            # Should reject outside tmpdir
            with pytest.raises(PathTraversalError):
                validate_path("/etc/passwd", allowed_dirs=allowed_dirs, allow_absolute=True)
    
    def test_safe_filename(self):
        """Test filename sanitization."""
        from nexuslang.security import get_safe_filename
        
        # Should remove dangerous characters
        assert get_safe_filename("../../../etc/passwd") == "passwd"
        assert get_safe_filename("test<script>.html") == "test_script_.html"
        assert get_safe_filename(".hidden") == "_hidden"


class TestSafeSubprocess:
    """Test safe subprocess execution."""
    
    def test_no_shell_expansion(self):
        """Test that shell metacharacters don't work."""
        # This should fail because shell expansion doesn't happen
        with pytest.raises(CommandInjectionError):
            safe_execute("ls; rm -rf /", [])
    
    def test_safe_execution(self):
        """Test executing a safe command."""
        # Execute echo (should be safe on all systems)
        result = safe_execute("echo", ["hello"], capture_output=True)
        assert result.stdout.strip() == "hello"
    
    def test_whitelist_enforcement(self):
        """Test program whitelist."""
        allowed = ["ls", "echo"]
        
        # Should allow whitelisted program
        result = safe_execute("echo", ["test"], allowed_programs=allowed)
        assert result.returncode == 0
        
        # Should deny non-whitelisted program
        with pytest.raises(CommandInjectionError):
            safe_execute("rm", ["-rf", "/"], allowed_programs=allowed)
    
    def test_argument_separation(self):
        """Test that arguments are properly separated."""
        # This tests that "hello world" is passed as one argument
        result = safe_execute("echo", ["hello world"], capture_output=True)
        assert result.stdout.strip() == "hello world"


class TestInputValidation:
    """Test input validation functions."""
    
    def test_email_validation(self):
        """Test email address validation."""
        valid_emails = [
            "test@example.com",
            "user.name+tag@example.co.uk",
            "test123@test-domain.com",
        ]
        
        invalid_emails = [
            "not-an-email",
            "@example.com",
            "test@",
            "test @example.com",
        ]
        
        for email in valid_emails:
            assert validate_email(email), f"Should accept {email}"
        
        for email in invalid_emails:
            assert not validate_email(email), f"Should reject {email}"
    
    def test_url_validation(self):
        """Test URL validation."""
        valid_urls = [
            "http://example.com",
            "https://example.com/path",
            "ftp://files.example.com",
        ]
        
        invalid_urls = [
            "not a url",
            "example.com",
            "ht!tp://bad.com",
        ]
        
        for url in valid_urls:
            assert validate_url(url), f"Should accept {url}"
        
        for url in invalid_urls:
            assert not validate_url(url), f"Should reject {url}"
    
    def test_url_scheme_restriction(self):
        """Test URL scheme whitelisting."""
        assert validate_url("https://example.com", ["https"])
        assert not validate_url("ftp://example.com", ["http", "https"])
    
    def test_sql_identifier_sanitization(self):
        """Test SQL identifier sanitization."""
        # Valid identifiers
        assert sanitize_sql_identifier("table_name") == "table_name"
        assert sanitize_sql_identifier("column123") == "column123"
        
        # Invalid identifiers
        with pytest.raises(ValidationError):
            sanitize_sql_identifier("table; DROP TABLE users;")
        
        with pytest.raises(ValidationError):
            sanitize_sql_identifier("123invalid")
        
        # SQL keywords
        with pytest.raises(ValidationError):
            sanitize_sql_identifier("select")


class TestOutputSanitization:
    """Test output sanitization functions."""
    
    def test_html_escaping(self):
        """Test HTML entity escaping."""
        dangerous_html = '<script>alert("XSS")</script>'
        escaped = escape_html(dangerous_html)
        
        # Should not contain < or >
        assert '<' not in escaped
        assert '>' not in escaped
        assert '&lt;' in escaped
        assert '&gt;' in escaped
    
    def test_html_escape_completeness(self):
        """Test that all dangerous characters are escaped."""
        test_cases = {
            '&': '&amp;',
            '<': '&lt;',
            '>': '&gt;',
            '"': '&quot;',
            "'": '&#x27;',
            '/': '&#x2F;',
        }
        
        for char, expected in test_cases.items():
            assert expected in escape_html(char)


class TestRateLimit:
    """Test rate limiting functionality."""
    
    def test_rate_limit_enforcement(self):
        """Test that rate limit is enforced."""
        from nexuslang.security import check_rate_limit
        
        # Should allow first 3 calls
        assert check_rate_limit("user1", max_calls=3, window_seconds=60)
        assert check_rate_limit("user1", max_calls=3, window_seconds=60)
        assert check_rate_limit("user1", max_calls=3, window_seconds=60)
        
        # Should deny 4th call
        assert not check_rate_limit("user1", max_calls=3, window_seconds=60)
    
    def test_rate_limit_per_identifier(self):
        """Test that rate limits are per-identifier."""
        from nexuslang.security import check_rate_limit
        
        # Reset by using new identifiers
        assert check_rate_limit("user_a", max_calls=2, window_seconds=60)
        assert check_rate_limit("user_a", max_calls=2, window_seconds=60)
        
        # Different user should have separate limit
        assert check_rate_limit("user_b", max_calls=2, window_seconds=60)
        assert check_rate_limit("user_b", max_calls=2, window_seconds=60)


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
