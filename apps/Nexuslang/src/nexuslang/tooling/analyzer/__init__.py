""" 
NexusLang Static Analyzer
=========================

Production-grade static analysis for NexusLang code.

Catches bugs before runtime:
- Memory safety violations
- Null pointer dereferences
- Resource leaks
- Uninitialized variables
- Type mismatches
- Race conditions
- Dead code
- Performance issues
"""

from .analyzer import StaticAnalyzer
from .checks import *
from .report import AnalysisReport, Issue
from .autofix import AutoFixer, FixSuggestion, FixResult, TextEdit
from .ide_hooks import IDEHooks, LspFormatter, lsp_position, lsp_range, severity_to_lsp

__all__ = [
    'StaticAnalyzer',
    'AnalysisReport',
    'Issue',
    'AutoFixer',
    'FixSuggestion',
    'FixResult',
    'TextEdit',
    'IDEHooks',
    'LspFormatter',
    'lsp_position',
    'lsp_range',
    'severity_to_lsp',
]
