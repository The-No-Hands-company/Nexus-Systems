"""
Tests for the NexusLang stdlib http module.

Split into two groups:
1. Offline tests (always run): URL utilities, HTTPResponse class,
   response helper functions.
2. Network tests (opt-in): real HTTP requests using httpbin or similar.
   Activated by setting environment variable NLPL_TEST_NETWORK=1.

To enable network tests:
  NLPL_TEST_NETWORK=1 python -m pytest tests/unit/stdlib/test_http_client.py -v
"""

import os
import sys
import json
import pytest

_PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..")
)
if _PROJECT_ROOT not in sys.path:
    sys.path.insert(0, _PROJECT_ROOT)

from nexuslang.stdlib.http import (
    HTTPResponse,
    url_encode,
    url_decode,
    url_parse,
    url_join,
    get_response_text,
    get_response_status,
    get_response_headers,
    is_response_ok,
    http_get,
    http_post,
)

_NETWORK = pytest.mark.skipif(
    not os.environ.get("NLPL_TEST_NETWORK"),
    reason="Set NLPL_TEST_NETWORK=1 to enable live network tests",
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _make_response(status: int = 200, body: bytes = b"hello", url: str = "http://example.com") -> HTTPResponse:
    return HTTPResponse(status_code=status, headers={"Content-Type": "text/plain"}, body=body, url=url)


# ===========================================================================
# HTTPResponse class (offline — constructed from raw parts)
# ===========================================================================


class TestHTTPResponse:
    def test_status_code(self):
        r = _make_response(status=200)
        assert r.status_code == 200

    def test_body_accessible(self):
        r = _make_response(body=b"hello world")
        assert r.body == b"hello world"

    def test_text_property(self):
        r = _make_response(body=b"hello world")
        assert r.text == "hello world"

    def test_text_cached(self):
        r = _make_response(body=b"cached")
        _ = r.text
        assert r._text == "cached"

    def test_headers_accessible(self):
        r = HTTPResponse(200, {"X-Custom": "value"}, b"", "http://x.com")
        assert r.headers["X-Custom"] == "value"

    def test_url_stored(self):
        r = _make_response(url="http://test.example.org/path")
        assert r.url == "http://test.example.org/path"

    def test_ok_true_for_2xx(self):
        for code in (200, 201, 204, 299):
            assert _make_response(status=code).ok is True

    def test_ok_false_for_4xx(self):
        for code in (400, 401, 403, 404, 500):
            assert _make_response(status=code).ok is False

    def test_json_property(self):
        body = json.dumps({"key": "value", "n": 42}).encode()
        r = _make_response(body=body)
        data = r.json
        assert data["key"] == "value" and data["n"] == 42

    def test_unicode_body(self):
        text = "Unicode: \u00e9\u00e0\u4e2d"
        r = _make_response(body=text.encode("utf-8"))
        assert r.text == text


# ===========================================================================
# Response helper functions (offline)
# ===========================================================================


class TestResponseHelpers:
    def test_get_response_text(self):
        r = _make_response(body=b"body text")
        assert get_response_text(r) == "body text"

    def test_get_response_status(self):
        assert get_response_status(_make_response(status=404)) == 404

    def test_get_response_headers(self):
        headers = {"Content-Type": "application/json", "X-Rate-Limit": "100"}
        r = HTTPResponse(200, headers, b"", "http://x.com")
        assert get_response_headers(r) == headers

    def test_is_response_ok_true(self):
        assert is_response_ok(_make_response(status=200)) is True

    def test_is_response_ok_false(self):
        assert is_response_ok(_make_response(status=500)) is False


# ===========================================================================
# URL utilities (offline — pure string manipulation)
# ===========================================================================


class TestURLEncode:
    def test_dict_params(self):
        result = url_encode({"key": "value", "n": "42"})
        assert "key=value" in result
        assert "n=42" in result

    def test_special_chars_encoded(self):
        result = url_encode("hello world")
        assert " " not in result

    def test_slash_encoded_in_plain_string(self):
        result = url_encode("a/b")
        assert "/" not in result or result == "a%2Fb"

    def test_empty_dict(self):
        result = url_encode({})
        assert isinstance(result, str)


class TestURLDecode:
    def test_plain_encoded_string(self):
        assert url_decode("hello+world") == "hello world"

    def test_query_string_to_dict(self):
        result = url_decode("key=value&n=42")
        assert isinstance(result, dict)
        assert result["key"] == "value"

    def test_roundtrip_plain(self):
        original = "hello world"
        assert url_decode(url_encode(original)) == original


class TestURLParse:
    def test_scheme(self):
        parsed = url_parse("https://example.com/path?q=1#frag")
        assert parsed["scheme"] == "https"

    def test_netloc(self):
        parsed = url_parse("https://example.com/path")
        assert parsed["netloc"] == "example.com"

    def test_path(self):
        parsed = url_parse("https://example.com/some/path")
        assert parsed["path"] == "/some/path"

    def test_query(self):
        parsed = url_parse("https://example.com/path?foo=bar&baz=1")
        assert "foo=bar" in parsed["query"]

    def test_hostname(self):
        parsed = url_parse("https://example.com/")
        assert parsed["hostname"] == "example.com"

    def test_port(self):
        parsed = url_parse("http://localhost:8080/")
        assert parsed["port"] == 8080

    def test_returns_dict_with_required_keys(self):
        parsed = url_parse("https://example.com/")
        for key in ("scheme", "netloc", "path", "query", "fragment", "hostname"):
            assert key in parsed


class TestURLJoin:
    def test_basic_join(self):
        assert url_join("https://example.com/", "path") == "https://example.com/path"

    def test_absolute_path_overrides(self):
        result = url_join("https://example.com/base/", "/other")
        assert result == "https://example.com/other"

    def test_relative_path(self):
        result = url_join("https://example.com/base/", "sub")
        assert result == "https://example.com/base/sub"

    def test_preserves_scheme(self):
        result = url_join("http://example.com/", "page")
        assert result.startswith("http://")

    def test_empty_path(self):
        base = "https://example.com/path"
        assert url_join(base, "") == base


# ===========================================================================
# Validate url_encode / url_decode interop
# ===========================================================================


class TestURLEncodeDecodeInterop:
    def test_dict_roundtrip(self):
        params = {"name": "Alice Smith", "age": "30", "city": "New York"}
        encoded = url_encode(params)
        decoded = url_decode(encoded)
        assert isinstance(decoded, dict)
        assert decoded["name"] == "Alice Smith"
        assert decoded["age"] == "30"


# ===========================================================================
# Live network tests (opt-in via NLPL_TEST_NETWORK=1)
# ===========================================================================


@_NETWORK
class TestHTTPGet:
    def test_get_ok_status(self):
        r = http_get("https://httpbin.org/get", timeout=10)
        assert r.status_code == 200

    def test_get_ok_property(self):
        r = http_get("https://httpbin.org/get", timeout=10)
        assert r.ok is True

    def test_get_json_response(self):
        r = http_get("https://httpbin.org/json", timeout=10)
        assert isinstance(r.json, dict)

    def test_get_status_404(self):
        r = http_get("https://httpbin.org/status/404", timeout=10)
        assert r.status_code == 404
        assert r.ok is False


@_NETWORK
class TestHTTPPost:
    def test_post_with_json_data(self):
        payload = {"message": "hello", "value": 42}
        r = http_post("https://httpbin.org/post", data=payload, timeout=10)
        assert r.status_code == 200
        body = r.json
        assert "json" in body

    def test_post_reflects_payload(self):
        payload = {"test": "data"}
        r = http_post("https://httpbin.org/post", data=payload, timeout=10)
        body = r.json
        assert body["json"]["test"] == "data"
