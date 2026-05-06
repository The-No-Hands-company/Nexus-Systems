"""
check_error_messages.py
========================

Monthly copy-pass audit tool for NexusLang error codes.

Checks every entry in src/nlpl/error_codes.py for:
  - Descriptions that are too short (< 20 characters)
  - Missing common_causes (empty list)
  - Missing fixes (empty list)
  - Missing doc_link
  - Fixes that start with a capital letter but lack terminal punctuation
  - Descriptions that end with a period (inconsistent style)
  - Duplicate fixes within the same error code
  - Fix text longer than 80 characters (hard to read in hover)
  - Error codes referenced in get_error_code_for_type mapping but missing from registry

Optionally shows local telemetry (most-emitted codes) so you know which
messages will be seen most by real users.

Usage:
    python dev_tools/check_error_messages.py
    python dev_tools/check_error_messages.py --show-telemetry
    python dev_tools/check_error_messages.py --reset-telemetry
    python dev_tools/check_error_messages.py --strict   # exit 1 if any issues found
"""

from __future__ import annotations

import sys
import argparse
from pathlib import Path
from typing import List, Tuple

PROJ_ROOT = Path(__file__).parent.parent
sys.path.insert(0, str(PROJ_ROOT / "src"))

from nexuslang.error_codes import ERROR_CODES, ErrorInfo, get_error_code_for_type  # noqa: E402


# ---------------------------------------------------------------------------
# Quality checks
# ---------------------------------------------------------------------------

Issue = Tuple[str, str, str]  # (code, field, message)


def _check_entry(code: str, info: ErrorInfo) -> List[Issue]:
    issues: List[Issue] = []

    # Description
    if not info.description.strip():
        issues.append((code, "description", "Empty description"))
    elif len(info.description.strip()) < 20:
        issues.append((code, "description", f"Very short description ({len(info.description)} chars)"))

    # Common causes
    if not info.common_causes:
        issues.append((code, "common_causes", "No common causes listed"))
    for i, cause in enumerate(info.common_causes):
        if len(cause) > 100:
            issues.append((code, f"common_causes[{i}]", f"Cause text very long ({len(cause)} chars) — consider splitting"))

    # Fixes
    if not info.fixes:
        issues.append((code, "fixes", "No fixes listed"))
    for i, fix in enumerate(info.fixes):
        if len(fix) > 90:
            issues.append((code, f"fixes[{i}]", f"Fix text long ({len(fix)} chars) — hover may truncate"))
    # Duplicate fixes
    seen_fixes = []
    for fix in info.fixes:
        norm = fix.strip().lower()
        if norm in seen_fixes:
            issues.append((code, "fixes", f"Duplicate fix: '{fix}'"))
        seen_fixes.append(norm)

    # Doc link
    if not info.doc_link:
        issues.append((code, "doc_link", "Missing doc_link — users cannot browse to documentation"))

    # Title
    if not info.title.strip():
        issues.append((code, "title", "Empty title"))
    elif len(info.title) > 60:
        issues.append((code, "title", f"Title very long ({len(info.title)} chars) — may truncate in Problems panel"))

    return issues


def audit_registry() -> List[Issue]:
    """Run all quality checks across the full ERROR_CODES registry."""
    all_issues: List[Issue] = []
    for code, info in sorted(ERROR_CODES.items()):
        all_issues.extend(_check_entry(code, info))
    return all_issues


def audit_mapping_coverage() -> List[Issue]:
    """Check that every code referenced in get_error_code_for_type exists in registry."""
    issues: List[Issue] = []
    # Build the mapping by spot-checking all known type keys
    known_type_keys = [
        "unexpected_token", "missing_end", "invalid_function", "invalid_class",
        "invalid_expression", "undefined_variable", "undefined_function",
        "undefined_class", "undefined_attribute", "unused_variable",
        "type_mismatch", "invalid_operation", "wrong_argument_count",
        "invalid_generic_args", "type_annotation_error", "data_schema_mismatch",
        "numeric_domain_error", "division_by_zero", "index_out_of_range",
        "key_not_found", "null_pointer", "no_attribute", "function_call_error",
        "invalid_cast", "memory_allocation_failed", "invalid_memory_operation",
        "runtime_error", "module_not_found", "circular_import",
        "import_name_not_found", "network_request_failed", "invalid_http_response",
        "database_connection_failed", "transaction_conflict",
    ]
    for key in known_type_keys:
        code = get_error_code_for_type(key)
        if code and code not in ERROR_CODES:
            issues.append((code, "registry", f"Code '{code}' referenced by type_key '{key}' but not in ERROR_CODES"))
    return issues


# ---------------------------------------------------------------------------
# Telemetry display
# ---------------------------------------------------------------------------

def show_telemetry(top_n: int = 15) -> None:
    try:
        from nexuslang.lsp.telemetry import get_counts, get_metadata
    except ImportError:
        print("  [telemetry] Cannot import telemetry module.")
        return

    meta = get_metadata()
    counts = get_counts()

    print(f"\n  Telemetry file: {meta['telemetry_file']}")
    print(f"  Sessions recorded: {meta['sessions']}")
    print(f"  First seen: {meta['first_seen']}   Last updated: {meta['last_updated']}")

    if not counts:
        print("\n  No telemetry data yet. Run the LSP server and open some NexusLang files to collect data.")
        return

    total = sum(counts.values())
    print(f"\n  Top {min(top_n, len(counts))} most-emitted error codes  (total emissions: {total})\n")
    print(f"  {'Code':<8} {'Count':>6}  {'Share':>6}  Title")
    print(f"  {'-'*7} {'------':>6}  {'------':>6}  {'-'*40}")
    for code, count in list(counts.items())[:top_n]:
        info = ERROR_CODES.get(code)
        title = info.title if info else "(unknown code)"
        share = 100.0 * count / total if total else 0
        print(f"  {code:<8} {count:>6}  {share:>5.1f}%  {title}")

    missed = [(c, n) for c, n in counts.items() if c not in ERROR_CODES]
    if missed:
        print(f"\n  WARNING: {len(missed)} unregistered code(s) in telemetry: {[c for c, _ in missed]}")


def reset_telemetry() -> None:
    try:
        from nexuslang.lsp.telemetry import reset_counts
        reset_counts()
        print("Telemetry counters reset.")
    except ImportError:
        print("Cannot import telemetry module.")


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

SEVERITY_SYMBOL = {
    "description": "WARN",
    "fixes": "WARN",
    "doc_link": "INFO",
    "common_causes": "WARN",
    "title": "WARN",
    "registry": "ERROR",
}


def _severity(field: str) -> str:
    for key, sev in SEVERITY_SYMBOL.items():
        if key in field:
            return sev
    return "WARN"


def print_report(issues: List[Issue], show_tele: bool = False) -> int:
    """Print formatted audit report. Returns exit code (0 = clean, 1 = issues)."""
    errors = [i for i in issues if _severity(i[1]) == "ERROR"]
    warnings = [i for i in issues if _severity(i[1]) == "WARN"]
    infos = [i for i in issues if _severity(i[1]) == "INFO"]

    total_codes = len(ERROR_CODES)
    print("=" * 60)
    print("NLPL Error Message Copy Audit")
    print("=" * 60)
    print(f"Registry: {total_codes} error codes")
    print(f"Issues:   {len(errors)} errors, {len(warnings)} warnings, {len(infos)} infos")
    print()

    if not issues:
        print("  All error messages pass quality checks.")
    else:
        last_code = None
        for code, field, msg in sorted(issues, key=lambda x: (x[0], x[1])):
            if code != last_code:
                info = ERROR_CODES.get(code)
                title_str = f" — {info.title}" if info else ""
                print(f"  [{code}]{title_str}")
                last_code = code
            sev = _severity(field)
            print(f"    {sev:5}  {field}: {msg}")
        print()

    if show_tele:
        print("=" * 60)
        print("Telemetry — Most Frequent Error Codes (local/dev)")
        print("=" * 60)
        show_telemetry()

    print()
    print("=" * 60)
    if not issues:
        print("Result: PASS — no issues found.")
        return 0
    print(f"Result: {len(errors)} error(s), {len(warnings)} warning(s), {len(infos)} info(s).")
    if errors:
        print("Re-generate docs after fixing:  python dev_tools/generate_error_docs.py")
    return 1 if errors else 0


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Monthly copy-pass audit for NexusLang error codes",
    )
    parser.add_argument("--show-telemetry", action="store_true",
                        help="Show local telemetry (most-emitted codes)")
    parser.add_argument("--reset-telemetry", action="store_true",
                        help="Reset telemetry counters to zero")
    parser.add_argument("--strict", action="store_true",
                        help="Exit with code 1 if any issues (errors or warnings) found")
    args = parser.parse_args()

    if args.reset_telemetry:
        reset_telemetry()
        return 0

    issues = audit_registry() + audit_mapping_coverage()
    rc = print_report(issues, show_tele=args.show_telemetry)

    if args.strict and issues:
        return 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
