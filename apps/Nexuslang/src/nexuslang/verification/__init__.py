"""
nlpl.verification
=================

Formal verification infrastructure for NexusLang programs.

This package provides static analysis of Design-by-Contract annotations
(``require``, ``ensure``, ``guarantee``, ``invariant``) and
``spec`` blocks.

Public API::

    from nexuslang.verification import ConstraintCollector, VerificationReport, verify_file

Modules:
    specification       -- Data model: VerificationCondition, FormalSpec, etc.
    constraint_collector -- AST walker that extracts VCs from annotated programs.
    z3_backend          -- Optional SMT solver integration (requires Z3).
    reporter            -- Text/JSON report generation.
"""

from nexuslang.verification.specification import (
    VerificationStatus,
    VerificationCondition,
    FormalSpec,
)
from nexuslang.verification.constraint_collector import ConstraintCollector
from nexuslang.verification.reporter import VerificationReport, verify_file

__all__ = [
    "VerificationStatus",
    "VerificationCondition",
    "FormalSpec",
    "ConstraintCollector",
    "VerificationReport",
    "verify_file",
]
