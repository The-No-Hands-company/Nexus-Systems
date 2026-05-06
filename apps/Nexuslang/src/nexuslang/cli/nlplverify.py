"""
nlpl.cli.nlplverify
====================

``nlpl-verify`` command-line interface.

Statically analyses NexusLang source files for design-by-contract
annotations, generates a ``FormalSpec`` per file, and optionally
discharges proof obligations using the Z3 SMT solver.

Usage::

    nlpl-verify path/to/file.nlpl [path/to/other.nlpl ...]

    nlpl-verify --help

Exit codes:
    0  All conditions proved or no conditions found.
    1  One or more conditions failed (counter-example found) or a
       file could not be parsed.
    2  Invalid CLI invocation (missing arguments, bad flags).
"""

from __future__ import annotations

import argparse
import os
import sys
from typing import List, Optional


def _run_verify(
    source_paths: List[str],
    output_json: Optional[str],
    output_text: Optional[str],
    use_z3: bool,
    z3_timeout_ms: int,
    fail_on_unverified: bool,
    quiet: bool,
) -> int:
    """Core verification logic.

    Args:
        source_paths       (List[str]): NexusLang source files to verify.
        output_json        (str|None): Path to write JSON report.
        output_text        (str|None): Path to write text report.
        use_z3             (bool): Attempt Z3 proof discharge.
        z3_timeout_ms      (int): Per-VC Z3 timeout in milliseconds.
        fail_on_unverified (bool): Exit 1 if any VC remains unverified.
        quiet              (bool): Suppress text output to stdout.

    Returns:
        int: Exit code (0 = success, 1 = failures, 2 = invocation error).
    """
    from nexuslang.verification.reporter import verify_files
    from nexuslang.verification.z3_backend import Z3_AVAILABLE

    if not quiet and use_z3 and not Z3_AVAILABLE:
        print(
            "Note: Z3 SMT solver not installed.  Install z3-solver for formal "
            "proof discharge.  Conditions will be listed as UNVERIFIED.",
            file=sys.stderr,
        )

    report = verify_files(source_paths, use_z3=use_z3, z3_timeout_ms=z3_timeout_ms)

    if not quiet:
        print(report.summary())

    if output_json:
        report.write_json(output_json)
        if not quiet:
            print(f"JSON report written to: {output_json}", file=sys.stderr)

    if output_text:
        report.write_text(output_text)
        if not quiet:
            print(f"Text report written to: {output_text}", file=sys.stderr)

    # Exit code logic
    if report.has_failures:
        return 1
    if fail_on_unverified and report.total_unverified > 0:
        if not quiet:
            print(
                f"Exit 1: {report.total_unverified} conditions unverified "
                "(use --no-fail-unverified to allow this).",
                file=sys.stderr,
            )
        return 1
    return 0


def _cli(argv: Optional[List[str]] = None) -> int:
    """Parse CLI arguments and invoke the verifier.

    Args:
        argv: Argument list (defaults to sys.argv[1:]).

    Returns:
        int: Exit code.
    """
    parser = argparse.ArgumentParser(
        prog="nlpl-verify",
        description=(
            "Formally verify NexusLang source files by collecting design-by-contract "
            "annotations and optionally discharging proof obligations via Z3."
        ),
    )
    parser.add_argument(
        "files",
        nargs="+",
        metavar="FILE",
        help="One or more .nlpl source files to verify.",
    )
    parser.add_argument(
        "--json",
        metavar="PATH",
        default=None,
        help="Write JSON verification report to PATH.",
    )
    parser.add_argument(
        "--text",
        metavar="PATH",
        default=None,
        help="Write text verification report to PATH.",
    )
    parser.add_argument(
        "--no-z3",
        action="store_true",
        default=False,
        help="Disable Z3 proof discharge (collect VCs only, all UNVERIFIED).",
    )
    parser.add_argument(
        "--z3-timeout",
        metavar="MS",
        type=int,
        default=5_000,
        help="Per-VC Z3 solver timeout in milliseconds (default: 5000).",
    )
    parser.add_argument(
        "--fail-unverified",
        action="store_true",
        default=False,
        help="Exit 1 if any VC remains unverified (not just on failures).",
    )
    parser.add_argument(
        "--quiet",
        "-q",
        action="store_true",
        default=False,
        help="Suppress stdout output (errors and warnings still go to stderr).",
    )

    args = parser.parse_args(argv)

    # Validate files exist
    missing = [f for f in args.files if not os.path.isfile(f)]
    if missing:
        for f in missing:
            print(f"nlpl-verify: file not found: {f}", file=sys.stderr)
        return 2

    return _run_verify(
        source_paths=args.files,
        output_json=args.json,
        output_text=args.text,
        use_z3=not args.no_z3,
        z3_timeout_ms=args.z3_timeout,
        fail_on_unverified=args.fail_unverified,
        quiet=args.quiet,
    )


def main() -> None:
    """Entry point for the ``nlpl-verify`` console script."""
    sys.exit(_cli())


if __name__ == "__main__":
    main()
