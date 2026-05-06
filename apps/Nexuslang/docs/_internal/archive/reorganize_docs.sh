#!/bin/bash
# Documentation reorganization script

# Create necessary directories
mkdir -p 2_language_basics/features
mkdir -p 3_core_concepts/advanced_features
mkdir -p 4_architecture/implementation_details
mkdir -p 7_development/guides
mkdir -p 9_status_reports/sessions
mkdir -p 10_assessments/audits
mkdir -p reference
mkdir -p archive

# Move language feature documents
mv -v ENUM_TYPES.md 2_language_basics/features/ 2>/dev/null || true
mv -v SWITCH_STATEMENT.md 2_language_basics/features/ 2>/dev/null || true
mv -v FUNCTION_POINTERS.md 2_language_basics/features/ 2>/dev/null || true
mv -v NESTED_STRUCTS.md 2_language_basics/features/ 2>/dev/null || true

# Move architecture/implementation documents
mv -v MEMORY_MANAGEMENT.md 4_architecture/implementation_details/ 2>/dev/null || true
mv -v INTERPRETER_VS_COMPILER.md 4_architecture/ 2>/dev/null || true
mv -v async_await_implementation_plan.md 4_architecture/implementation_details/ 2>/dev/null || true
mv -v exception_handling_status.md 4_architecture/implementation_details/ 2>/dev/null || true

# Move development guides
mv -v OPTIMIZATION_GUIDE.md 7_development/guides/ 2>/dev/null || true
mv -v OPTIMIZATION_IMPLEMENTATION.md 7_development/guides/ 2>/dev/null || true
mv -v SECURITY_GUIDE.md 7_development/guides/ 2>/dev/null || true
mv -v CROSS_PLATFORM_GUIDE.md 7_development/guides/ 2>/dev/null || true
mv -v BEST_PRACTICES_ANSWERS.md 7_development/guides/ 2>/dev/null || true
mv -v NLPLLINT_README.md 7_development/ 2>/dev/null || true
mv -v DEVELOPMENT_TOOLS_STATUS.md 7_development/ 2>/dev/null || true

# Move session reports
mv -v LLVM_COMPILATION_SESSION3_SUMMARY.md 9_status_reports/sessions/ 2>/dev/null || true
mv -v SESSION_SUMMARY_SWITCH_ENUM_FUNCPTR.md 9_status_reports/sessions/ 2>/dev/null || true
mv -v REORGANIZATION_SUMMARY.md 9_status_reports/ 2>/dev/null || true

# Move assessment/audit documents
mv -v CURRENT_STATUS_ANALYSIS.md 10_assessments/ 2>/dev/null || true
mv -v FEATURE_COMPLETENESS_GAP_ANALYSIS.md 10_assessments/ 2>/dev/null || true
mv -v FINAL_AUDIT_2026_01_26.md 10_assessments/audits/ 2>/dev/null || true
mv -v PRODUCTION_GRADE_AUDIT.md 10_assessments/audits/ 2>/dev/null || true
mv -v PRODUCTION_GRADE_COMPLETE.md 10_assessments/audits/ 2>/dev/null || true
mv -v NLPL_COMPETITIVE_ADVANTAGES.md 10_assessments/ 2>/dev/null || true
mv -v NLPL_DEVELOPMENT_ANALYSIS.md 10_assessments/ 2>/dev/null || true
mv -v NLPL_IDENTITY_CORRECTED.md 10_assessments/ 2>/dev/null || true
mv -v PROJECT_DEEP_DIVE_2026.md 10_assessments/ 2>/dev/null || true
mv -v CROSS_PLATFORM_CLARIFICATION.md 10_assessments/ 2>/dev/null || true

# Move reference documents
mv -v FFI_QUICK_REFERENCE.md reference/ 2>/dev/null || true
mv -v MULTI_LEVEL_QUICK_REFERENCE.md reference/ 2>/dev/null || true
mv -v STDLIB_API_REFERENCE.md reference/ 2>/dev/null || true

# Move status/progress documents
mv -v STATUS.md 9_status_reports/ 2>/dev/null || true
mv -v NLPL_PROGRESS_VISUAL_SUMMARY.md 9_status_reports/ 2>/dev/null || true

# Rename folders for better clarity
[ -d "session_reports" ] && [ ! -d "9_status_reports/archived_sessions" ] && mv -v session_reports 9_status_reports/archived_sessions
[ -d "guides" ] && [ ! -d "7_development/archived_guides" ] && mv -v guides 7_development/archived_guides
[ -d "engine" ] && [ ! -d "archive/engine_docs" ] && mv -v engine archive/engine_docs
[ -d "build" ] && [ ! -d "archive/build_docs" ] && mv -v build archive/build_docs

echo "Documentation reorganization complete!"
