#!/usr/bin/env python3
"""
Restructure docs/ into a user-facing language documentation layout.

New structure:
  docs/
    README.md                  -- documentation home / navigation index
    getting-started/           -- installation, first program, overview
    guide/                     -- language programming guide (how to write NLPL)
    reference/                 -- formal spec, grammar, error codes
      stdlib/                  -- stdlib API reference per module
    tooling/                   -- LSP, debugger, REPL, build system, formatter
    contributing/              -- contributor / developer guide
    _internal/                 -- all dev-internal content (status reports,
                                  assessments, completion reports, planning)

Files already in their correct final location are not moved.
"""

import os
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def mkdir(p: Path):
    p.mkdir(parents=True, exist_ok=True)


def mv(src_rel: str, dst_rel: str):
    src = DOCS / src_rel
    dst = DOCS / dst_rel
    if not src.exists():
        print(f"  MISSING: {src_rel}")
        return
    mkdir(dst.parent)
    shutil.move(str(src), str(dst))
    print(f"  mv  {src_rel:<70}  ->  {dst_rel}")


def mv_dir_contents(src_dir: str, dst_dir: str):
    """Move all files from src_dir into dst_dir (flat, one level deep)."""
    src = DOCS / src_dir
    if not src.exists():
        print(f"  MISSING dir: {src_dir}")
        return
    dst = DOCS / dst_dir
    mkdir(dst)
    for f in src.iterdir():
        if f.is_file():
            mv(f"{src_dir}/{f.name}", f"{dst_dir}/{f.name}")
        elif f.is_dir():
            # Recurse one level
            sub_dst = dst / f.name
            mkdir(sub_dst)
            for ff in f.iterdir():
                if ff.is_file():
                    mv(f"{src_dir}/{f.name}/{ff.name}",
                       f"{dst_dir}/{f.name}/{ff.name}")


def rmdir_if_empty(rel: str):
    p = DOCS / rel
    if p.exists() and p.is_dir():
        try:
            p.rmdir()
            print(f"  rmdir {rel}")
        except OSError:
            remaining = list(p.rglob("*"))
            print(f"  NOTE: {rel} still has {len(remaining)} items, not removed")


# ---------------------------------------------------------------------------
# Create new directory skeleton
# ---------------------------------------------------------------------------

print("\n=== directory skeleton ===")
for d in [
    "getting-started",
    "guide",
    "reference/stdlib",
    "tooling",
    "contributing",
    "_internal/status-reports",
    "_internal/planning",
    "_internal/assessments",
    "_internal/project-status",
    "_internal/completion-reports",
    "_internal/archive",
]:
    mkdir(DOCS / d)
    print(f"  mkdir docs/{d}")


# ---------------------------------------------------------------------------
# getting-started/
# (1_introduction/ → getting-started/, renaming to kebab-case)
# ---------------------------------------------------------------------------

print("\n=== getting-started/ ===")
mv("1_introduction/overview.md",              "getting-started/overview.md")
mv("1_introduction/philosophy.md",            "getting-started/philosophy.md")
mv("1_introduction/getting_started.md",       "getting-started/installation.md")
mv("1_introduction/key_features.md",          "getting-started/key-features.md")
mv("1_introduction/MULTI_LEVEL_ARCHITECTURE.md", "getting-started/architecture-overview.md")
mv("1_introduction/MULTI_LEVEL_SUMMARY.md",   "_internal/archive/MULTI_LEVEL_SUMMARY.md")
mv("1_introduction/NLPL_COMPETITIVE_ADVANTAGES.md", "_internal/assessments/NLPL_COMPETITIVE_ADVANTAGES_intro.md")
mv("1_introduction/CONCURRENCY_LEVELS.md",    "guide/concurrency-levels.md")
rmdir_if_empty("1_introduction")


# ---------------------------------------------------------------------------
# guide/  (language programming guide — how to write NLPL)
# ---------------------------------------------------------------------------

print("\n=== guide/ ===")

# 2_language_basics → guide/
mv("2_language_basics/syntax_overview.md",     "guide/syntax.md")
mv("2_language_basics/variables_and_objects.md","guide/variables.md")
mv("2_language_basics/commands_and_phrases.md", "guide/functions.md")
mv("2_language_basics/scoping_and_blocks.md",   "guide/scoping.md")

# 2_language_basics/features/ → guide/
features_dir = DOCS / "2_language_basics/features"
if features_dir.exists():
    for f in features_dir.iterdir():
        if f.is_file():
            name = f.name.lower().replace("_", "-")
            mv(f"2_language_basics/features/{f.name}", f"guide/{name}")
    rmdir_if_empty("2_language_basics/features")
rmdir_if_empty("2_language_basics")

# 3_core_concepts → guide/
mv("3_core_concepts/pattern_matching.md",      "guide/pattern-matching.md")
mv("3_core_concepts/struct_union.md",           "guide/structs-and-unions.md")
mv("3_core_concepts/ffi.md",                   "guide/ffi.md")
mv("3_core_concepts/inline_assembly.md",        "guide/inline-assembly.md")
mv("3_core_concepts/inline_assembly_design.md", "guide/inline-assembly-design.md")
mv("3_core_concepts/file_operations.md",        "guide/file-io.md")
mv("3_core_concepts/networking.md",             "guide/networking.md")
mv("3_core_concepts/database_operations.md",    "guide/database.md")
mv("3_core_concepts/events_and_triggers.md",    "guide/events.md")
mv("3_core_concepts/behaviors.md",              "guide/behaviors.md")
mv("3_core_concepts/repeat_while_loop_feature.md", "guide/loops.md")
mv("3_core_concepts/time_and_frames.md",        "guide/time-and-frames.md")
mv("3_core_concepts/MULTI_LEVEL_EXAMPLES.md",   "guide/multi-level-examples.md")

# game_objects — keep in guide (it's a domain-neutral concept example)
mv("3_core_concepts/game_objects.md",           "guide/entity-system.md")

# lsp_integration — user-facing tooling overview
mv("3_core_concepts/lsp_integration.md",        "tooling/lsp-integration.md")

# find_references — dev-internal planning
mv("3_core_concepts/find_references_ast_plan.md","_internal/archive/find_references_ast_plan.md")
mv("3_core_concepts/find_references.md",        "_internal/archive/find_references.md")
rmdir_if_empty("3_core_concepts")

# 5_type_system → guide/ + reference/
mv("5_type_system/type_system.md",              "guide/type-system.md")
mv("5_type_system/type_system_summary.md",      "guide/type-system-summary.md")
mv("5_type_system/QUICK_REFERENCE.md",          "reference/type-system-quick-reference.md")
mv("5_type_system/generic_type_system_completion.md","_internal/completion-reports/generic_type_system_completion.md")
mv("5_type_system/TYPE_SYSTEM_COMPLETION.md",   "_internal/completion-reports/TYPE_SYSTEM_COMPLETION.md")
mv("5_type_system/rc_implementation_design.md", "_internal/archive/rc_implementation_design.md")
rmdir_if_empty("5_type_system")

# 6_module_system → guide/
mv("6_module_system/module_system.md",          "guide/modules.md")
mv("6_module_system/module_system_enhancements.md","guide/module-enhancements.md")
mv("6_module_system/module_system_summary.md",  "_internal/archive/module_system_summary.md")
rmdir_if_empty("6_module_system")


# ---------------------------------------------------------------------------
# reference/
# ---------------------------------------------------------------------------

print("\n=== reference/ ===")
mv("4_architecture/language_specification.md",  "reference/language-spec.md")
mv("4_architecture/syntax_design.md",           "reference/syntax-grammar.md")
mv("4_architecture/INTERPRETER_VS_COMPILER.md", "_internal/assessments/INTERPRETER_VS_COMPILER.md")
mv("4_architecture/backend_strategy.md",        "_internal/archive/backend_strategy.md")
mv("4_architecture/optimization_guide.md",      "reference/optimization-guide.md")
mv("4_architecture/optimization_pipeline.md",   "_internal/archive/optimization_pipeline.md")
mv("4_architecture/compiler_architecture.md",   "contributing/architecture.md")
rmdir_if_empty("4_architecture")

# reference/ already has 4 files — normalize names
mv("reference/STDLIB_API_REFERENCE.md",         "reference/stdlib/index.md")
mv("reference/FFI_QUICK_REFERENCE.md",          "reference/stdlib/ffi.md")
mv("reference/MULTI_LEVEL_QUICK_REFERENCE.md",  "reference/multi-level-quick-reference.md")
mv("reference/VERSIONING_STRATEGY.md",          "reference/versioning.md")

# 7_development — user-facing reference content
mv("7_development/error_codes_reference.md",    "reference/error-codes.md")
mv("7_development/COMMON_SYNTAX_ERRORS.md",     "reference/common-syntax-errors.md")


# ---------------------------------------------------------------------------
# tooling/
# ---------------------------------------------------------------------------

print("\n=== tooling/ ===")
mv("7_development/lsp.md",                      "tooling/lsp.md")
mv("7_development/lsp_installation.md",         "tooling/lsp-installation.md")
mv("7_development/LSP_FEATURES.md",             "tooling/lsp-features.md")
mv("7_development/LSP_QUICK_REFERENCE.md",      "tooling/lsp-quick-reference.md")
mv("7_development/debugger.md",                 "tooling/debugger.md")
mv("7_development/debugger_quick_reference.md", "tooling/debugger-quick-reference.md")
mv("7_development/DEBUGGER_QUICK_START.md",     "tooling/debugger-quick-start.md")
mv("7_development/repl.md",                     "tooling/repl.md")
mv("7_development/repl_quick_reference.md",     "tooling/repl-quick-reference.md")
mv("7_development/formatter_implementation.md", "tooling/formatter.md")
mv("7_development/tooling_guide.md",            "tooling/tooling-guide.md")
mv("7_development/vscode_extension_guide.md",   "tooling/vscode-extension.md")
mv("7_development/marketplace_publishing_guide.md","tooling/marketplace-publishing.md")
mv("7_development/performance_guide.md",        "tooling/performance-guide.md")
mv("7_development/NLPLLINT_README.md",          "tooling/linter.md")

# build_system/ → tooling/ (user-facing files)
mv("build_system/BUILD_TOOL_GUIDE.md",          "tooling/build-tool.md")
mv("build_system/NLPL_TOML_SPECIFICATION.md",   "tooling/nlpl-toml.md")
mv("build_system/INCREMENTAL_COMPILATION.md",   "tooling/incremental-compilation.md")
# Progress/completion files from build_system go to _internal
mv("build_system/BUILD_SYSTEM_COMPLETE.md",     "_internal/completion-reports/BUILD_SYSTEM_COMPLETE.md")
mv("build_system/BUILD_SYSTEM_PROGRESS.md",     "_internal/project-status/BUILD_SYSTEM_PROGRESS.md")
mv("build_system/TASK_3_COMPLETE.md",           "_internal/completion-reports/TASK_3_COMPLETE.md")
rmdir_if_empty("build_system")


# ---------------------------------------------------------------------------
# contributing/  (developer / contributor guide)
# ---------------------------------------------------------------------------

print("\n=== contributing/ ===")
mv("7_development/style_guide.md",              "contributing/style-guide.md")
mv("7_development/DEVELOPMENT_SETUP.md",        "contributing/development-setup.md")
mv("7_development/compiler_guide.md",           "contributing/compiler-guide.md")
mv("7_development/incremental_parsing.md",      "contributing/incremental-parsing.md")
mv("7_development/cross_platform_inline_assembly.md","contributing/cross-platform-assembly.md")
mv("7_development/persistent_cache_plan.md",    "contributing/persistent-cache.md")
mv("7_development/performance_optimizations.md","contributing/performance-optimizations.md")
mv("7_development/scope_optimizer_guide.md",    "contributing/scope-optimizer.md")
mv("7_development/ast_cache_quickref.md",       "contributing/ast-cache.md")
mv("7_development/build_system_improvements.md","contributing/build-system-improvements.md")
mv("7_development/build_system_quick_reference.md","contributing/build-system-quick-ref.md")
mv("7_development/ci_test_hardening_plan.md",   "contributing/ci-test-hardening.md")
mv("7_development/ci_tooling_versions.md",      "contributing/ci-tooling-versions.md")
mv("7_development/DEVELOPMENT_TOOLS_STATUS.md", "contributing/tools-status.md")
mv("7_development/VSCODE_EXTENSION_STRUCTURE_ANALYSIS.md","contributing/vscode-extension-analysis.md")
mv("7_development/VSCODE_LSP_TESTING_GUIDE.md","contributing/vscode-lsp-testing.md")

# The rest of 7_development that are dev session reports → _internal
remaining_dev = list((DOCS / "7_development").rglob("*.md")) if (DOCS / "7_development").exists() else []
print(f"\n  Remaining in 7_development/: {len(remaining_dev)} files → _internal/status-reports/")
for f in remaining_dev:
    if f.is_file():
        rel = f.relative_to(DOCS)
        mv(str(rel), f"_internal/status-reports/{f.name}")

rmdir_if_empty("7_development/archived_guides")
rmdir_if_empty("7_development")


# ---------------------------------------------------------------------------
# _internal/  (all dev-internal content)
# ---------------------------------------------------------------------------

print("\n=== _internal/ status reports ===")
mv_dir_contents("9_status_reports", "_internal/status-reports")
rmdir_if_empty("9_status_reports")

print("\n=== _internal/ planning ===")
mv_dir_contents("8_planning", "_internal/planning")
rmdir_if_empty("8_planning")

print("\n=== _internal/ assessments ===")
mv_dir_contents("10_assessments", "_internal/assessments")
rmdir_if_empty("10_assessments")

print("\n=== _internal/ project-status ===")
mv_dir_contents("project_status", "_internal/project-status")
rmdir_if_empty("project_status")

print("\n=== _internal/ completion-reports ===")
mv_dir_contents("completion-reports", "_internal/completion-reports")
rmdir_if_empty("completion-reports")

print("\n=== _internal/ archive (existing + early design) ===")
# Move existing archive/ contents
if (DOCS / "archive").exists():
    mv_dir_contents("archive", "_internal/archive")
    rmdir_if_empty("archive")

# Move "Creating a Truly NexusLang/" to _internal/archive/early-design
early_design_src = DOCS / "Creating a Truly NexusLang"
if early_design_src.exists():
    mkdir(DOCS / "_internal/archive/early-design")
    for f in early_design_src.iterdir():
        mv(f"Creating a Truly NexusLang/{f.name}",
           f"_internal/archive/early-design/{f.name}")
    try:
        early_design_src.rmdir()
        print(f"  rmdir 'Creating a Truly NexusLang/'")
    except OSError:
        print(f"  NOTE: early-design dir not empty after migration")


# ---------------------------------------------------------------------------
# Clean up root docs/ files
# ---------------------------------------------------------------------------

print("\n=== docs/ root cleanup ===")
mv("_ORGANIZATION_GUIDE.md",                "_internal/archive/_ORGANIZATION_GUIDE.md")
# docs/README.md and docs/reorganize_docs.sh stay (README is the home page)
if (DOCS / "reorganize_docs.sh").exists():
    shutil.move(str(DOCS / "reorganize_docs.sh"),
                str(DOCS / "_internal/archive/reorganize_docs.sh"))
    print("  mv reorganize_docs.sh -> _internal/archive/")


# ---------------------------------------------------------------------------
# Write docs/README.md as navigation home
# ---------------------------------------------------------------------------

print("\n=== writing docs/README.md ===")
README = """\
# NexusLang Documentation

Welcome to the NexusLang documentation. NexusLang is a general-purpose programming language
designed to read like natural English while offering full systems-level capabilities.

## Contents

### Getting Started
- [Overview](getting-started/overview.md)
- [Philosophy](getting-started/philosophy.md)
- [Installation & First Steps](getting-started/installation.md)
- [Key Features](getting-started/key-features.md)
- [Architecture Overview](getting-started/architecture-overview.md)

### Language Guide
- [Syntax](guide/syntax.md)
- [Variables & Objects](guide/variables.md)
- [Functions](guide/functions.md)
- [Scoping & Blocks](guide/scoping.md)
- [Pattern Matching](guide/pattern-matching.md)
- [Structs & Unions](guide/structs-and-unions.md)
- [Type System](guide/type-system.md)
- [Modules](guide/modules.md)
- [Concurrency](guide/concurrency-levels.md)
- [FFI](guide/ffi.md)
- [Inline Assembly](guide/inline-assembly.md)
- [File I/O](guide/file-io.md)
- [Networking](guide/networking.md)
- [Database](guide/database.md)
- [Loops](guide/loops.md)
- [Events](guide/events.md)

### Language Reference
- [Language Specification](reference/language-spec.md)
- [Syntax Grammar](reference/syntax-grammar.md)
- [Error Codes](reference/error-codes.md)
- [Common Syntax Errors](reference/common-syntax-errors.md)
- [Optimization Guide](reference/optimization-guide.md)
- [Type System Quick Reference](reference/type-system-quick-reference.md)
- [Standard Library](reference/stdlib/index.md)
  - [FFI](reference/stdlib/ffi.md)

### Tooling
- [LSP Server](tooling/lsp.md)
- [Debugger](tooling/debugger.md)
- [REPL](tooling/repl.md)
- [Build System](tooling/build-tool.md)
- [nlpl.toml Specification](tooling/nlpl-toml.md)
- [Formatter](tooling/formatter.md)
- [Linter](tooling/linter.md)
- [VS Code Extension](tooling/vscode-extension.md)
- [Performance Guide](tooling/performance-guide.md)

### Contributing
- [Architecture](contributing/architecture.md)
- [Style Guide](contributing/style-guide.md)
- [Development Setup](contributing/development-setup.md)
- [Compiler Guide](contributing/compiler-guide.md)
- [Performance Optimizations](contributing/performance-optimizations.md)
"""
(DOCS / "README.md").write_text(README)
print("  wrote docs/README.md")


# ---------------------------------------------------------------------------
# Final summary
# ---------------------------------------------------------------------------

print("\n=== summary ===")
for top in sorted(DOCS.iterdir()):
    if top.is_dir():
        count = len(list(top.rglob("*.md")))
        print(f"  docs/{top.name}/  ({count} files)")
all_md = list(DOCS.rglob("*.md"))
print(f"\n  Total markdown files: {len(all_md)}")
