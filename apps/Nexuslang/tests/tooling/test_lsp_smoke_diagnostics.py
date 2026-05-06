"""
LSP smoke tests - publishDiagnostics payload
=============================================

Automates the manual VS Code smoke-tests from the LSP checklist:
  - Problems panel ``code`` column: every diagnostic from the server carries
    a ``code`` field in E### format.
  - Hover data: every code-bearing diagnostic also has ``data.explainHint``
    and ``source == "nlpl"``.
  - Clean file: no spurious diagnostics on a valid NexusLang file.

These tests spawn an actual LSP server subprocess (stdin/stdout) and
communicate using the LSP wire protocol, matching exactly what the VS Code
client does.  No mocking.

Checklist coverage (now automated):
  [x] Problems panel shows ``code`` column          (test_syntax_error_has_code)
  [x] Hover ``data.explainHint`` present            (test_syntax_error_has_explain_hint)
  [x] ``source`` always "nlpl"                      (test_all_diagnostics_source_nxl)
  [x] Clean file emits zero diagnostics             (test_valid_file_zero_diagnostics)
  [x] All codes match E### pattern                  (test_code_format_e_pattern)
"""

import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path
from typing import List, Dict, Optional

import pytest

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

WORKSPACE_ROOT = Path(__file__).resolve().parent.parent.parent  # tests/tooling -> tests -> NexusLang/
SRC_DIR = WORKSPACE_ROOT / "src"
FIXTURE_ERROR = WORKSPACE_ROOT / "test_programs" / "regression" / "lsp_smoke_fixture.nxl"
FIXTURE_VALID = WORKSPACE_ROOT / "test_programs" / "regression" / "error_tests" / "test_basic_errors.nxl"

# Fallback valid NexusLang text used when the fixture file doesn't exist yet
_VALID_NLPL_FALLBACK = 'set x to 42\nprint text x\n'
# Error NexusLang text that the parser must reject (E001)
_ERROR_NLPL = 'set greeting to "hello"\nprint text greeting\n\nset broken to\n'

E_CODE_PATTERN = re.compile(r'^E\d{3}$')


class _LspClient:
    """Minimal synchronous LSP wire client for testing."""

    def __init__(self):
        import os
        env = os.environ.copy()
        # Ensure the src/ directory is on PYTHONPATH so 'nlpl' package is importable.
        # We use the absolute resolved path so the child process can always find the
        # package regardless of cwd or how pytest was invoked (e.g. without
        # PYTHONPATH set in the shell environment — conftest.py only patches
        # sys.path in-process, not os.environ, so we must do it explicitly here).
        src_abs = str(SRC_DIR.resolve())
        existing = env.get("PYTHONPATH", "")
        env["PYTHONPATH"] = src_abs + (os.pathsep + existing if existing else "")
        # close_fds=True ensures pytest's captured file descriptors are not
        # inherited by the child process, preventing broken-pipe exits when
        # pytest's capture plugin replaces the process-level stdout/stderr FDs.
        self.proc = subprocess.Popen(
            [sys.executable, "-m", "nexuslang.lsp", "--stdio"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=str(WORKSPACE_ROOT),
            env=env,
            close_fds=True,
        )
        self._notifications: List[Dict] = []
        self._next_id = 1

    # ------------------------------------------------------------------
    # Wire-level helpers
    # ------------------------------------------------------------------

    def _send(self, message: dict) -> None:
        content = json.dumps(message)
        header = f"Content-Length: {len(content)}\r\n\r\n"
        self.proc.stdin.write((header + content).encode("utf-8"))
        self.proc.stdin.flush()

    def _read_one(self, timeout: float = 5.0) -> Optional[dict]:
        """Read exactly one LSP message from stdout using unbuffered os.read()."""
        import select as _select
        fd = self.proc.stdout.fileno()

        def _read_bytes(n: int) -> Optional[bytes]:
            """Read exactly n bytes, respecting the deadline."""
            data = b""
            deadline = time.monotonic() + timeout
            while len(data) < n:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return None
                ready, _, _ = _select.select([fd], [], [], min(0.1, remaining))
                if not ready:
                    continue
                chunk = os.read(fd, n - len(data))
                if not chunk:
                    return None
                data += chunk
            return data

        # Read header byte by byte until \r\n\r\n
        deadline = time.monotonic() + timeout
        buf = b""
        while b"\r\n\r\n" not in buf:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None
            ready, _, _ = _select.select([fd], [], [], min(0.1, remaining))
            if not ready:
                continue
            ch = os.read(fd, 1)
            if not ch:
                return None
            buf += ch

        header_part, rest = buf.split(b"\r\n\r\n", 1)
        content_length = 0
        for line in header_part.split(b"\r\n"):
            if line.lower().startswith(b"content-length:"):
                content_length = int(line.split(b":", 1)[1].strip())
                break
        if content_length == 0:
            return None

        # Read remaining body bytes
        to_read = content_length - len(rest)
        if to_read > 0:
            extra = _read_bytes(to_read)
            if extra is None:
                return None
            body = rest + extra
        else:
            body = rest[:content_length]

        try:
            return json.loads(body.decode("utf-8"))
        except json.JSONDecodeError:
            return None

    def _drain(self, timeout: float = 1.5) -> List[dict]:
        """Read all available messages within *timeout* seconds."""
        messages = []
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            msg = self._read_one(timeout=max(0.05, remaining))
            if msg is None:
                break
            messages.append(msg)
        return messages

    # ------------------------------------------------------------------
    # High-level LSP operations
    # ------------------------------------------------------------------

    def initialize(self) -> None:
        self._send(
            {
                "jsonrpc": "2.0",
                "id": self._next_id,
                "method": "initialize",
                "params": {
                    "processId": self.proc.pid,
                    "rootUri": f"file://{WORKSPACE_ROOT}",
                    "capabilities": {},
                },
            }
        )
        self._next_id += 1
        # Consume the initialize response
        self._drain(timeout=3.0)
        self._send({"jsonrpc": "2.0", "method": "initialized", "params": {}})

    def did_open(self, uri: str, text: str) -> List[Dict]:
        """Send didOpen and collect all publishDiagnostics notifications."""
        self._send(
            {
                "jsonrpc": "2.0",
                "method": "textDocument/didOpen",
                "params": {
                    "textDocument": {
                        "uri": uri,
                        "languageId": "nlpl",
                        "version": 1,
                        "text": text,
                    }
                },
            }
        )
        # Collect for up to 3 seconds - server may emit one notification
        messages = self._drain(timeout=3.0)
        diagnostics = []
        for msg in messages:
            if msg.get("method") == "textDocument/publishDiagnostics":
                diagnostics.extend(msg["params"].get("diagnostics", []))
        return diagnostics

    def shutdown(self) -> None:
        try:
            self._send({"jsonrpc": "2.0", "id": self._next_id, "method": "shutdown", "params": None})
            self._next_id += 1
            self._drain(timeout=1.0)
            self._send({"jsonrpc": "2.0", "method": "exit", "params": None})
            self.proc.wait(timeout=3)
        except Exception:
            self.proc.terminate()


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture(scope="module")
def lsp_client():
    """Module-scoped LSP client: start once, share across tests in this file."""
    client = _LspClient()
    # Poll with retries — pytest's capture plugin can delay server startup.
    deadline = time.monotonic() + 3.0
    while time.monotonic() < deadline:
        if client.proc.poll() is None:
            break
        time.sleep(0.05)
    if client.proc.poll() is not None:
        err = client.proc.stderr.read().decode(errors="replace")
        pytest.fail(f"LSP server failed to start (rc={client.proc.returncode}): {err[:400]}")
    client.initialize()
    yield client
    client.shutdown()


@pytest.fixture(scope="module")
def error_diagnostics(lsp_client):
    """Diagnostics produced when opening a file with a deliberate syntax error."""
    text = _ERROR_NLPL
    uri = f"file://{WORKSPACE_ROOT}/test_programs/regression/.lsp_smoke_error_virtual.nxl"
    diags = lsp_client.did_open(uri, text)
    return diags


@pytest.fixture(scope="module")
def valid_diagnostics(lsp_client):
    """Diagnostics produced when opening a valid NexusLang file."""
    if FIXTURE_VALID.exists():
        text = FIXTURE_VALID.read_text(encoding="utf-8")
    else:
        text = _VALID_NLPL_FALLBACK
    uri = f"file://{WORKSPACE_ROOT}/test_programs/regression/.lsp_smoke_valid_virtual.nxl"
    diags = lsp_client.did_open(uri, text)
    return diags


# ---------------------------------------------------------------------------
# Tests: syntax-error file (E001)
# ---------------------------------------------------------------------------

class TestSyntaxErrorDiagnostics:
    """Verify the payload shape of diagnostics emitted for a syntax error."""

    def test_server_emits_at_least_one_diagnostic(self, error_diagnostics):
        """LSP server must publish diagnostics when it detects a syntax error."""
        assert len(error_diagnostics) >= 1, (
            "Expected at least one diagnostic for the deliberately broken NexusLang snippet. "
            "Check that the LSP server can parse and detect syntax errors."
        )

    def test_syntax_error_has_code(self, error_diagnostics):
        """Every diagnostic the server emits must carry a 'code' field.

        This is the automated equivalent of the manual check:
         'Problems panel shows the code column (e.g. E001)'.
        """
        for diag in error_diagnostics:
            assert "code" in diag, (
                f"Diagnostic missing 'code' field: {diag.get('message', '?')} "
                "(Problems panel would show no error code)"
            )

    def test_code_format_e_pattern(self, error_diagnostics):
        """Codes must match the E### pattern (e.g. E001, E100)."""
        for diag in error_diagnostics:
            code = diag.get("code", "")
            assert E_CODE_PATTERN.match(str(code)), (
                f"Diagnostic code '{code}' does not match E### pattern: {diag.get('message', '?')}"
            )

    def test_all_diagnostics_source_nxl(self, error_diagnostics):
        """'source' must be 'nlpl' on every diagnostic.

        This ensures the VS Code hover provider filter works
        (it matches source === 'nlpl').
        """
        for diag in error_diagnostics:
            assert diag.get("source") == "nlpl", (
                f"Diagnostic source is '{diag.get('source')}', expected 'nlpl': "
                f"{diag.get('message', '?')}"
            )

    def test_syntax_error_has_explain_hint(self, error_diagnostics):
        """data.explainHint must be present on every code-bearing diagnostic.

        This is the automated equivalent of the manual hover smoke-test:
        'Hover over squiggle, confirm explainHint renders'.
        """
        for diag in error_diagnostics:
            if "code" not in diag:
                continue
            data = diag.get("data", {})
            assert "explainHint" in data, (
                f"Diagnostic '{diag['code']}' missing data.explainHint "
                "(hover panel would show no explain command)"
            )
            hint = data["explainHint"]
            assert diag["code"] in hint, (
                f"explainHint '{hint}' does not contain the code '{diag['code']}'"
            )

    def test_syntax_error_has_data_title(self, error_diagnostics):
        """data.title must be present on every code-bearing diagnostic."""
        for diag in error_diagnostics:
            if "code" not in diag:
                continue
            data = diag.get("data", {})
            assert "title" in data, (
                f"Diagnostic '{diag['code']}' missing data.title"
            )
            assert isinstance(data["title"], str) and data["title"], (
                f"data.title must be a non-empty string for '{diag['code']}'"
            )

    def test_syntax_error_has_data_category(self, error_diagnostics):
        """data.category must be present on every code-bearing diagnostic."""
        for diag in error_diagnostics:
            if "code" not in diag:
                continue
            data = diag.get("data", {})
            assert "category" in data, (
                f"Diagnostic '{diag['code']}' missing data.category"
            )

    def test_syntax_error_range_valid(self, error_diagnostics):
        """range must contain non-negative integer positions."""
        for diag in error_diagnostics:
            r = diag.get("range", {})
            for pos_key in ("start", "end"):
                pos = r.get(pos_key, {})
                assert pos.get("line", -1) >= 0, f"range.{pos_key}.line is negative"
                assert pos.get("character", -1) >= 0, f"range.{pos_key}.character is negative"


# ---------------------------------------------------------------------------
# Tests: valid file (regression - no spurious diagnostics)
# ---------------------------------------------------------------------------

class TestCleanFileDiagnostics:
    """Regression: a valid NexusLang file must not produce spurious diagnostics."""

    def test_valid_file_zero_diagnostics(self, valid_diagnostics):
        """Opening a valid NexusLang file must produce zero diagnostics.

        This is the 'regression check: diagnostics stable on unchanged code'
        item from the checklist.  If this fails, the server is producing
        false-positive errors that would pollute the Problems panel.
        """
        if valid_diagnostics:
            codes = [d.get("code", "?") for d in valid_diagnostics]
            messages = [d.get("message", "?") for d in valid_diagnostics]
            pytest.fail(
                f"Valid NexusLang file produced {len(valid_diagnostics)} unexpected diagnostic(s).\n"
                f"Codes: {codes}\n"
                f"Messages: {messages}"
            )


# ---------------------------------------------------------------------------
# Tests: invariants that hold regardless of file content
# ---------------------------------------------------------------------------

class TestDiagnosticInvariants:
    """Cross-file invariants derived from the LSP spec and our error contract."""

    def test_code_bearing_diagnostics_have_explain_hint(
        self, error_diagnostics, valid_diagnostics
    ):
        """Any diagnostic with a 'code' field must also have data.explainHint."""
        all_diags = error_diagnostics + valid_diagnostics
        for diag in all_diags:
            if "code" not in diag:
                continue
            data = diag.get("data", {})
            assert "explainHint" in data, (
                f"Diagnostic {diag['code']} has 'code' but no 'data.explainHint'"
            )

    def test_no_diagnostic_source_is_empty(
        self, error_diagnostics, valid_diagnostics
    ):
        """'source' must never be empty or missing."""
        all_diags = error_diagnostics + valid_diagnostics
        for diag in all_diags:
            src = diag.get("source", "")
            assert src, f"Diagnostic has empty/missing source: {diag.get('message', '?')}"

    def test_no_unknown_source_prefix(
        self, error_diagnostics, valid_diagnostics
    ):
        """Old fragmented source strings ('nlpl-parser', 'nlpl-typechecker', …)
        must not appear — all diagnostics now use source='nlpl'."""
        all_diags = error_diagnostics + valid_diagnostics
        forbidden_prefixes = ("nlpl-parser", "nlpl-typechecker", "nlpl-imports")
        for diag in all_diags:
            src = diag.get("source", "")
            for prefix in forbidden_prefixes:
                assert not src.startswith(prefix), (
                    f"Diagnostic still uses old source '{src}'. "
                    "All sources should be 'nlpl' after normalization."
                )
