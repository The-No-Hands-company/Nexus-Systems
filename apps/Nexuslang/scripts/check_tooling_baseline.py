#!/usr/bin/env python3
"""
CI Gating Script for Phase 3 Tooling Baseline
==============================================

Validates that current LSP and codegen performance metrics stay within
acceptable deviation thresholds from the established baseline.

Usage:
    python scripts/check_tooling_baseline.py [--baseline BASELINE_FILE] [--iterations N] [--strict]

Exit codes:
    0 - All metrics within thresholds
    1 - One or more metrics exceeded thresholds
    2 - Baseline file not found
    3 - Benchmark execution failed
"""

import sys
import json
import argparse
import subprocess
from pathlib import Path
from typing import Dict, Optional, Tuple


# Default thresholds for regression detection
THRESHOLDS = {
    # LSP diagnostics latency thresholds (in milliseconds)
    # p95 should not exceed this factor above baseline p95
    "lsp_latency_p95_multiplier": 1.5,  # Allow 50% increase
    "lsp_latency_p95_absolute_ms": 3.0,  # But never exceed 3.0 ms absolute
    
    # Codegen latency thresholds (in milliseconds)
    "codegen_latency_p95_multiplier": 1.3,  # Allow 30% increase
    "codegen_latency_p95_absolute_ms": 0.5,  # But never exceed 0.5 ms absolute
    
    # Diagnostic count should remain stable (allow 0 difference)
    "diagnostic_count_tolerance": 0,
}


def load_baseline(baseline_path: Path) -> Optional[Dict]:
    """Load baseline metrics from JSON file."""
    try:
        with open(baseline_path, 'r') as f:
            baseline = json.load(f)
        return baseline
    except FileNotFoundError:
        print(f"ERROR: Baseline file not found: {baseline_path}", file=sys.stderr)
        return None
    except json.JSONDecodeError as e:
        print(f"ERROR: Failed to parse baseline JSON: {e}", file=sys.stderr)
        return None


def run_benchmarks(iterations: int) -> Optional[Dict]:
    """Run tooling benchmarks and return metrics."""
    benchmark_script = Path(__file__).parent.parent / "benchmarks" / "benchmark_phase3_tooling.py"
    
    if not benchmark_script.exists():
        print(f"ERROR: Benchmark script not found: {benchmark_script}", file=sys.stderr)
        return None
    
    try:
        result = subprocess.run(
            [sys.executable, str(benchmark_script), "--iterations", str(iterations), "--json"],
            capture_output=True,
            text=True,
            timeout=300,  # 5 minute timeout
        )
        
        if result.returncode != 0:
            print(f"ERROR: Benchmark execution failed:\n{result.stderr}", file=sys.stderr)
            return None
        
        # Parse JSON output from last line
        for line in reversed(result.stdout.split('\n')):
            if line.strip().startswith('{'):
                try:
                    return json.loads(line)
                except json.JSONDecodeError:
                    continue
        
        print("ERROR: No valid JSON output from benchmark", file=sys.stderr)
        return None
        
    except subprocess.TimeoutExpired:
        print("ERROR: Benchmark execution timed out", file=sys.stderr)
        return None
    except Exception as e:
        print(f"ERROR: Failed to execute benchmarks: {e}", file=sys.stderr)
        return None


def extract_lsp_metrics(metrics: Dict) -> Dict[str, float]:
    """Extract LSP diagnostic latency metrics from benchmark results."""
    lsp_metrics = {}
    lsp_diagnostics = metrics.get("lsp_diagnostics", {})
    
    for program_name, program_metrics in lsp_diagnostics.items():
        latency_data = program_metrics.get("latency_ms", {})
        p95 = latency_data.get("p95")
        
        if p95 is not None:
            lsp_metrics[f"{program_name}_p95"] = p95
    
    return lsp_metrics


def extract_codegen_metrics(metrics: Dict) -> Dict[str, float]:
    """Extract codegen latency metrics from benchmark results."""
    codegen_metrics = {}
    codegen_data = metrics.get("codegen_metrics", {})
    
    for scenario_name, scenario_metrics in codegen_data.items():
        latency_data = scenario_metrics.get("latency_ms", {})
        p95 = latency_data.get("p95")
        
        if p95 is not None:
            codegen_metrics[f"{scenario_name}_p95"] = p95
    
    return codegen_metrics


def check_lsp_metrics(baseline: Dict, current: Dict, thresholds: Dict) -> Tuple[bool, list]:
    """Check LSP metrics for regressions."""
    baseline_metrics = extract_lsp_metrics(baseline)
    current_metrics = extract_lsp_metrics(current)
    
    issues = []
    
    for metric_name, current_value in current_metrics.items():
        baseline_value = baseline_metrics.get(metric_name)
        
        if baseline_value is None:
            continue
        
        # Check multiplier threshold
        max_allowed_multiplier = thresholds["lsp_latency_p95_multiplier"]
        max_allowed_absolute = thresholds["lsp_latency_p95_absolute_ms"]
        
        max_allowed = min(
            baseline_value * max_allowed_multiplier,
            max_allowed_absolute,
        )
        
        if current_value > max_allowed:
            regression_pct = ((current_value - baseline_value) / baseline_value) * 100
            issues.append(
                f"LSP {metric_name}: {current_value:.3f}ms (baseline {baseline_value:.3f}ms, "
                f"+{regression_pct:.1f}%) exceeds threshold {max_allowed:.3f}ms"
            )
    
    return len(issues) == 0, issues


def check_codegen_metrics(baseline: Dict, current: Dict, thresholds: Dict) -> Tuple[bool, list]:
    """Check codegen metrics for regressions."""
    baseline_metrics = extract_codegen_metrics(baseline)
    current_metrics = extract_codegen_metrics(current)
    
    issues = []
    
    for metric_name, current_value in current_metrics.items():
        baseline_value = baseline_metrics.get(metric_name)
        
        if baseline_value is None:
            continue
        
        # Check multiplier threshold
        max_allowed_multiplier = thresholds["codegen_latency_p95_multiplier"]
        max_allowed_absolute = thresholds["codegen_latency_p95_absolute_ms"]
        
        max_allowed = min(
            baseline_value * max_allowed_multiplier,
            max_allowed_absolute,
        )
        
        if current_value > max_allowed:
            regression_pct = ((current_value - baseline_value) / baseline_value) * 100
            issues.append(
                f"Codegen {metric_name}: {current_value:.3f}ms (baseline {baseline_value:.3f}ms, "
                f"+{regression_pct:.1f}%) exceeds threshold {max_allowed:.3f}ms"
            )
    
    return len(issues) == 0, issues


def check_diagnostic_counts(baseline: Dict, current: Dict, thresholds: Dict) -> Tuple[bool, list]:
    """Check that diagnostic counts remain stable."""
    issues = []
    tolerance = thresholds["diagnostic_count_tolerance"]
    
    baseline_diags = baseline.get("lsp_diagnostics", {})
    current_diags = current.get("lsp_diagnostics", {})
    
    for program_name in baseline_diags:
        baseline_counts = baseline_diags[program_name].get("diagnostic_count_samples", [])
        current_counts = current_diags.get(program_name, {}).get("diagnostic_count_samples", [])
        
        if not baseline_counts or not current_counts:
            continue
        
        baseline_mean = sum(baseline_counts) / len(baseline_counts)
        current_mean = sum(current_counts) / len(current_counts)
        
        if abs(current_mean - baseline_mean) > tolerance:
            issues.append(
                f"Diagnostic count for '{program_name}': expected ~{baseline_mean:.1f}, "
                f"got {current_mean:.1f} (tolerance: {tolerance})"
            )
    
    return len(issues) == 0, issues


def main():
    """Main CI gating logic."""
    parser = argparse.ArgumentParser(
        description="Validate tooling metrics against phase3-tooling-baseline.json"
    )
    parser.add_argument(
        "--baseline",
        default="benchmarks/phase3-tooling-baseline.json",
        help="Path to baseline metrics JSON (default: benchmarks/phase3-tooling-baseline.json)",
    )
    parser.add_argument(
        "--iterations",
        type=int,
        default=3,
        help="Number of iterations for benchmark runs (default: 3)",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Use stricter thresholds (multiplier 1.2, absolute limits 2.0ms / 0.3ms)",
    )
    
    args = parser.parse_args()
    
    baseline_path = Path(args.baseline)
    
    # Load baseline
    print(f"Loading baseline from {baseline_path}...", file=sys.stderr)
    baseline = load_baseline(baseline_path)
    if baseline is None:
        return 2
    
    # Override thresholds if strict mode
    thresholds = THRESHOLDS.copy()
    if args.strict:
        thresholds["lsp_latency_p95_multiplier"] = 1.2
        thresholds["lsp_latency_p95_absolute_ms"] = 2.0
        thresholds["codegen_latency_p95_multiplier"] = 1.2
        thresholds["codegen_latency_p95_absolute_ms"] = 0.3
    
    # Run benchmarks
    print(f"Running benchmarks ({args.iterations} iterations)...", file=sys.stderr)
    current = run_benchmarks(args.iterations)
    if current is None:
        return 3
    
    # Check metrics
    print("\nValidating metrics against baseline:", file=sys.stderr)
    print("-" * 70, file=sys.stderr)
    
    all_pass = True
    all_issues = []
    
    # Check LSP metrics
    lsp_pass, lsp_issues = check_lsp_metrics(baseline, current, thresholds)
    if lsp_issues:
        print("\nLSP Diagnostics Latency:", file=sys.stderr)
        for issue in lsp_issues:
            print(f"  FAIL: {issue}", file=sys.stderr)
            all_issues.append(issue)
        all_pass = False
    else:
        print("\nLSP Diagnostics Latency: PASS", file=sys.stderr)
    
    # Check codegen metrics
    codegen_pass, codegen_issues = check_codegen_metrics(baseline, current, thresholds)
    if codegen_issues:
        print("\nCodegen Latency:", file=sys.stderr)
        for issue in codegen_issues:
            print(f"  FAIL: {issue}", file=sys.stderr)
            all_issues.append(issue)
        all_pass = False
    else:
        print("\nCodegen Latency: PASS", file=sys.stderr)
    
    # Check diagnostic counts
    diag_pass, diag_issues = check_diagnostic_counts(baseline, current, thresholds)
    if diag_issues:
        print("\nDiagnostic Counts:", file=sys.stderr)
        for issue in diag_issues:
            print(f"  WARN: {issue}", file=sys.stderr)
    else:
        print("\nDiagnostic Counts: PASS", file=sys.stderr)
    
    print("-" * 70, file=sys.stderr)
    
    if all_pass:
        print("\n✓ All metrics within acceptable thresholds", file=sys.stderr)
        
        # Print summary as JSON for CI consumption
        summary = {
            "status": "pass",
            "lsp_check": "pass" if lsp_pass else "fail",
            "codegen_check": "pass" if codegen_pass else "fail",
            "diagnostic_check": "pass" if diag_pass else "fail",
        }
        print(json.dumps(summary))
        return 0
    else:
        print(f"\n✗ {len(all_issues)} metric(s) exceeded thresholds", file=sys.stderr)
        
        # Print summary as JSON for CI consumption
        summary = {
            "status": "fail",
            "issues": all_issues,
            "lsp_check": "pass" if lsp_pass else "fail",
            "codegen_check": "pass" if codegen_pass else "fail",
            "diagnostic_check": "pass" if diag_pass else "fail",
        }
        print(json.dumps(summary))
        return 1


if __name__ == "__main__":
    sys.exit(main())
