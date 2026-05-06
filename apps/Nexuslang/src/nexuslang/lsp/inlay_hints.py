"""
Inlay Hints Provider
====================

Provides inline type and parameter name hints displayed inside the editor.

Two kinds of hints are generated:

1. Type hints (kind=1):
   Display the inferred type of a variable right after its name in a
   declaration.  Example:

       set counter to 0         ->  set counter: Integer to 0
       set name to "Alice"      ->  set name: String to "Alice"
       set pi to 3.14159        ->  set pi: Float to 3.14159
       set flags to [1, 2]      ->  set flags: List to [1, 2]

2. Parameter name hints (kind=2):
   Display the expected parameter name before each positional argument
   at a call site when the function signature is known.  Example:

       greet with "World"       ->  greet with name: "World"
       add with 3 and 5         ->  add with a: 3 and b: 5

Hints are computed by lightweight regex scanning; no full parse is needed.
"""

from __future__ import annotations

import re
import logging
from typing import Dict, List, Optional


logger = logging.getLogger(__name__)


# LSP InlayHintKind constants
INLAY_HINT_TYPE = 1       # Type annotation
INLAY_HINT_PARAMETER = 2  # Parameter name


# ---------------------------------------------------------------------------
# Literal type patterns — tried in order; first match wins
# ---------------------------------------------------------------------------
_LITERAL_PATTERNS: List[tuple] = [
    (re.compile(r"^-?\d+\.\d+(?:[eE][+-]?\d+)?$"), "Float"),
    (re.compile(r"^-?\d+$"), "Integer"),
    (re.compile(r'^"[^"]*"$'), "String"),
    (re.compile(r"^'[^']*'$", re.DOTALL), "String"),
    (re.compile(r"^(true|false)$", re.IGNORECASE), "Boolean"),
    (re.compile(r"^\["), "List"),
    (re.compile(r"^\{"), "Dict"),
]

# Keyword + type combos that already encode a type in their syntax
_RC_PATTERN = re.compile(r"^Rc\s+of\b", re.IGNORECASE)
_ARC_PATTERN = re.compile(r"^Arc\s+of\b", re.IGNORECASE)
_WEAK_PATTERN = re.compile(r"^Weak\s+of\b", re.IGNORECASE)
_BOX_PATTERN = re.compile(r"^box_new\b", re.IGNORECASE)


class InlayHintsProvider:
    """
    Provides inlay hints for NexusLang documents.

    Usage:
        provider = InlayHintsProvider(server)
        hints = provider.get_inlay_hints(uri, text, range_)
    """

    def __init__(self, server):
        self.server = server
        # Cache: func_name -> [param_name, ...]  (populated lazily per document)
        self._param_cache: Dict[str, List[str]] = {}

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def get_inlay_hints(
        self,
        uri: str,
        text: str,
        range_: Optional[Dict] = None,
    ) -> List[Dict]:
        """
        Return inlay hints for the document (or a restricted range).

        Args:
            uri:    Document URI (unused, reserved for workspace-index lookup).
            text:   Full document text.
            range_: Optional LSP Range; restricts hints to those lines.

        Returns:
            List of LSP InlayHint objects.
        """
        lines = text.split("\n")

        # Rebuild per-document param cache from scratch for each request so
        # edits are reflected immediately.
        self._param_cache = self._build_param_cache(lines)

        start_line = range_["start"]["line"] if range_ else 0
        end_line = range_["end"]["line"] if range_ else len(lines) - 1

        hints: List[Dict] = []
        for line_num in range(start_line, min(end_line + 1, len(lines))):
            hints.extend(self._hints_for_line(lines[line_num], line_num))

        return hints

    # ------------------------------------------------------------------
    # Per-line scanning
    # ------------------------------------------------------------------

    def _hints_for_line(self, line: str, line_num: int) -> List[Dict]:
        hints: List[Dict] = []

        # Skip comments and blank lines
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            return hints

        # 1. Variable declaration type hint ---------------------------------
        # Pattern: (optional indent) set VARNAME to VALUE
        m = re.match(r"^(\s*)set\s+(\w+)\s+to\s+(.+)$", line, re.IGNORECASE)
        if m:
            indent_len = len(m.group(1))
            var_name = m.group(2)
            value_str = m.group(3).strip()
            # Strip trailing comment from value
            value_str = re.sub(r"\s*#.*$", "", value_str).strip()

            inferred = self._infer_type(value_str)
            if inferred:
                # Insert hint immediately after the variable name
                name_col = indent_len + len("set ")
                hints.append(
                    _make_hint(
                        line=line_num,
                        character=name_col + len(var_name),
                        label=f": {inferred}",
                        kind=INLAY_HINT_TYPE,
                        padding_left=False,
                        padding_right=True,
                    )
                )
            # We still fall through to check for call-site hints on the same
            # line (e.g. "set x to greet with name")

        # 2. Call-site parameter name hints ---------------------------------
        # Pattern: (set X to)? FUNCNAME with ARG (and ARG)*
        # Skip lines that are function *definitions*
        if re.match(
            r"^\s*(function|class|struct|if|while|for|match|require|ensure|describe|test|it)\b",
            line,
            re.IGNORECASE,
        ):
            return hints

        # Match: optional "set X to" prefix then FUNCNAME with ...
        call_m = re.search(
            r"(?:^|to\s+)(\w+)\s+with\s+(.+?)(?:\s*#.*)?$",
            line,
            re.IGNORECASE,
        )
        if call_m:
            func_name = call_m.group(1)
            args_str = call_m.group(2).strip()
            params = self._param_cache.get(func_name.lower(), [])

            if params:
                hints.extend(
                    self._parameter_hints(
                        line=line,
                        line_num=line_num,
                        func_name=func_name,
                        args_str=args_str,
                        params=params,
                        call_match=call_m,
                    )
                )

        return hints

    def _parameter_hints(
        self,
        line: str,
        line_num: int,
        func_name: str,
        args_str: str,
        params: List[str],
        call_match: re.Match,
    ) -> List[Dict]:
        hints: List[Dict] = []

        # Split args on ' and ' (NLPL multi-arg separator) or ','
        args = re.split(r"\s+and\s+|,\s*", args_str)

        # Find where the args section starts in `line`
        with_match = re.search(r"\bwith\b", line, re.IGNORECASE)
        if not with_match:
            return hints

        after_with = with_match.end()

        for i, (arg, param_name) in enumerate(zip(args, params)):
            arg = arg.strip()
            if not arg:
                continue
            # Skip if the argument already contains ':' (named arg syntax)
            if ":" in arg:
                continue
            # Skip stdlib-style single-arg calls where the hint is noise
            if len(params) == 1 and len(params[0]) <= 3:
                continue

            # Find the character position of this argument in `line`
            arg_col = _find_arg_col(line, after_with, i, args)
            if arg_col is None:
                continue

            hints.append(
                _make_hint(
                    line=line_num,
                    character=arg_col,
                    label=f"{param_name}: ",
                    kind=INLAY_HINT_PARAMETER,
                    padding_left=False,
                    padding_right=False,
                )
            )

        return hints

    # ------------------------------------------------------------------
    # Type inference
    # ------------------------------------------------------------------

    def _infer_type(self, value: str) -> Optional[str]:
        """Return a type name for a literal value string, or None."""
        if not value:
            return None
        # Smart pointer constructors
        if _RC_PATTERN.match(value):
            return "Rc"
        if _ARC_PATTERN.match(value):
            return "Arc"
        if _WEAK_PATTERN.match(value):
            return "Weak"
        if _BOX_PATTERN.match(value):
            return "Box"
        for pattern, type_name in _LITERAL_PATTERNS:
            if pattern.match(value):
                return type_name
        return None

    # ------------------------------------------------------------------
    # Param cache
    # ------------------------------------------------------------------

    def _build_param_cache(self, lines: List[str]) -> Dict[str, List[str]]:
        """
        Scan all function definitions in the document and return a mapping
        { lowercase_func_name: [param1, param2, ...] }.
        """
        cache: Dict[str, List[str]] = {}
        sig_pattern = re.compile(
            r"^\s*function\s+(\w+)\s+(?:with\s+|that\s+takes?\s+)?(.+?)(?:\s+returns?\b.*)?$",
            re.IGNORECASE,
        )
        for line in lines:
            m = sig_pattern.match(line)
            if not m:
                continue
            func_name = m.group(1).lower()
            sig = m.group(2).strip()
            # Remove 'returns ...' tail
            sig = re.sub(r"\s+returns?\s+.*$", "", sig, flags=re.IGNORECASE)
            params: List[str] = []
            for part in re.split(r"\s+and\s+|,\s*", sig):
                part = part.strip()
                pm = re.match(r"(\w+)(?:\s+as\s+.*)?", part, re.IGNORECASE)
                if pm and pm.group(1).lower() not in (
                    "with",
                    "that",
                    "takes",
                    "returns",
                ):
                    params.append(pm.group(1))
            if params:
                cache[func_name] = params

        # Also incorporate workspace index (cross-file functions)
        if hasattr(self.server, "workspace_index") and self.server.workspace_index:
            try:
                for sym in self.server.workspace_index.get_all_symbols():
                    if sym.kind not in ("function", "method"):
                        continue
                    key = sym.name.lower()
                    if key in cache or not sym.signature:
                        continue
                    sig = re.sub(
                        r"\s+returns?\s+.*$", "", sym.signature, flags=re.IGNORECASE
                    )
                    params = []
                    for part in re.split(r"\s+and\s+|,\s*", sig):
                        part = part.strip()
                        pm = re.match(r"(\w+)(?:\s+as\s+.*)?", part, re.IGNORECASE)
                        if pm:
                            params.append(pm.group(1))
                    if params:
                        cache[key] = params
            except Exception:
                logger.debug("Skipping workspace-index parameter cache enrichment", exc_info=True)

        return cache


# ---------------------------------------------------------------------------
# Module-level helpers
# ---------------------------------------------------------------------------


def _make_hint(
    line: int,
    character: int,
    label: str,
    kind: int,
    padding_left: bool,
    padding_right: bool,
) -> Dict:
    return {
        "position": {"line": line, "character": character},
        "label": label,
        "kind": kind,
        "paddingLeft": padding_left,
        "paddingRight": padding_right,
    }


def _find_arg_col(
    line: str, after_with: int, arg_index: int, args: List[str]
) -> Optional[int]:
    """
    Find the character column of the arg at `arg_index` in `line`.

    `after_with` is the column right after 'with' in `line`.
    `args`       are the already-split argument strings.

    Returns the column, or None if the search fails.
    """
    try:
        pos = after_with
        for i in range(arg_index):
            # Skip past the previous arg and its 'and'/',' separator
            arg_text = args[i].strip()
            idx = line.find(arg_text, pos)
            if idx == -1:
                return None
            pos = idx + len(arg_text)
            # Skip separator
            sep = re.search(r"\s+and\s+|,\s*", line[pos:], re.IGNORECASE)
            if sep:
                pos += sep.end()

        # Skip leading whitespace
        while pos < len(line) and line[pos] == " ":
            pos += 1
        return pos
    except Exception:
        return None
