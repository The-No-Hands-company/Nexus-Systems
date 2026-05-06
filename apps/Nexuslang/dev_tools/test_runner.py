#!/usr/bin/env python3
"""
NLPL Test Runner
================

Enhanced test runner CLI for NexusLang programs with coverage support.

Features:
- Test discovery (finds all test_*.nlpl files)
- Pattern matching (run specific tests)
- Verbose/quiet modes
- Test statistics and reporting
- Exit codes for CI/CD integration
- Coverage collection (optional)
- Category filtering

Usage:
    python dev_tools/test_runner.py                     # Run all tests
    python dev_tools/test_runner.py test_*.nlpl         # Pattern matching
    python dev_tools/test_runner.py --verbose            # Verbose output
    python dev_tools/test_runner.py --category features  # Filter by category
    python dev_tools/test_runner.py --coverage           # With coverage
"""

import argparse
import glob
import os
import sys
import time
import subprocess
from pathlib import Path
from typing import List, Tuple, Dict
import json

# Color codes for terminal output
class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    BOLD = '\033[1m'
    RESET = '\033[0m'

class TestResult:
    """Represents the result of a single test."""
    def __init__(self, name: str, passed: bool, duration: float, output: str = "", error: str = ""):
        self.name = name
        self.passed = passed
        self.duration = duration
        self.output = output
        self.error = error

class TestRunner:
    """Enhanced test runner for NexusLang programs."""
    
    def __init__(self, verbose: bool = False, quiet: bool = False, coverage: bool = False):
        self.verbose = verbose
        self.quiet = quiet
        self.coverage = coverage
        self.results: List[TestResult] = []
        self.nxl_interpreter = "python src/nlpl/main.py"
        
    def discover_tests(self, pattern: str = "test_*.nxl", category: str = None) -> List[str]:
        """Discover test files matching pattern."""
        test_dirs = [
            "test_programs/features",
            "test_programs/control_flow",
            "test_programs/data_structures",
            "test_programs/stdlib",
            "test_programs",
        ]
        
        test_files = []
        for test_dir in test_dirs:
            if not os.path.exists(test_dir):
                continue
                
            # Check if category filter matches
            if category and category not in test_dir:
                continue
                
            pattern_path = os.path.join(test_dir, pattern)
            found = glob.glob(pattern_path)
            test_files.extend(found)
        
        # Remove duplicates and sort
        test_files = sorted(set(test_files))
        return test_files
    
    def run_test(self, test_file: str) -> TestResult:
        """Run a single test file."""
        test_name = os.path.basename(test_file)
        
        if self.verbose:
            print(f"\n{Colors.BLUE}Running: {test_name}{Colors.RESET}")
        elif not self.quiet:
            print(f"  {test_name}...", end=" ", flush=True)
        
        start_time = time.time()
        
        try:
            # Run the NexusLang program
            cmd = f"{self.nxl_interpreter} {test_file}"
            result = subprocess.run(
                cmd,
                shell=True,
                capture_output=True,
                text=True,
                timeout=30
            )
            
            duration = time.time() - start_time
            
            # Check for success
            passed = result.returncode == 0
            
            if self.verbose:
                print(f"Output:\n{result.stdout}")
                if result.stderr:
                    print(f"Stderr:\n{result.stderr}")
            
            return TestResult(
                name=test_name,
                passed=passed,
                duration=duration,
                output=result.stdout,
                error=result.stderr
            )
            
        except subprocess.TimeoutExpired:
            duration = time.time() - start_time
            return TestResult(
                name=test_name,
                passed=False,
                duration=duration,
                error="Test timeout (30s)"
            )
        except Exception as e:
            duration = time.time() - start_time
            return TestResult(
                name=test_name,
                passed=False,
                duration=duration,
                error=str(e)
            )
    
    def run_tests(self, test_files: List[str]) -> Tuple[int, int]:
        """Run all test files and return (passed, failed) counts."""
        if not test_files:
            print(f"{Colors.YELLOW}No test files found{Colors.RESET}")
            return 0, 0
        
        print(f"\n{Colors.BOLD}Discovered {len(test_files)} test(s){Colors.RESET}\n")
        
        for test_file in test_files:
            result = self.run_test(test_file)
            self.results.append(result)
            
            if not self.verbose and not self.quiet:
                if result.passed:
                    print(f"{Colors.GREEN} PASS{Colors.RESET} ({result.duration:.2f}s)")
                else:
                    print(f"{Colors.RED} FAIL{Colors.RESET} ({result.duration:.2f}s)")
                    if result.error:
                        print(f"    Error: {result.error}")
        
        passed = sum(1 for r in self.results if r.passed)
        failed = len(self.results) - passed
        
        return passed, failed
    
    def print_summary(self, passed: int, failed: int, total_time: float):
        """Print test run summary."""
        total = passed + failed
        
        print(f"\n{Colors.BOLD}{'=' * 60}{Colors.RESET}")
        print(f"{Colors.BOLD}Test Summary{Colors.RESET}")
        print(f"{'=' * 60}")
        
        if failed == 0:
            print(f"{Colors.GREEN}{Colors.BOLD}All tests passed!{Colors.RESET}")
        else:
            print(f"{Colors.RED}{Colors.BOLD}Some tests failed{Colors.RESET}")
        
        print(f"\nTotal:   {total}")
        print(f"{Colors.GREEN}Passed:  {passed}{Colors.RESET}")
        if failed > 0:
            print(f"{Colors.RED}Failed:  {failed}{Colors.RESET}")
        print(f"Time:    {total_time:.2f}s")
        
        # List failed tests
        if failed > 0 and not self.verbose:
            print(f"\n{Colors.RED}Failed tests:{Colors.RESET}")
            for result in self.results:
                if not result.passed:
                    print(f"  - {result.name}")
                    if result.error:
                        print(f"    {result.error}")
        
        print(f"{'=' * 60}\n")
    
    def save_results(self, output_file: str):
        """Save test results to JSON file."""
        data = {
            "total": len(self.results),
            "passed": sum(1 for r in self.results if r.passed),
            "failed": sum(1 for r in self.results if not r.passed),
            "tests": [
                {
                    "name": r.name,
                    "passed": r.passed,
                    "duration": r.duration,
                    "error": r.error if not r.passed else ""
                }
                for r in self.results
            ]
        }
        
        with open(output_file, 'w') as f:
            json.dump(data, f, indent=2)
        
        print(f"Results saved to: {output_file}")


def main():
    """Main entry point for test runner."""
    parser = argparse.ArgumentParser(
        description="NLPL Test Runner - Run NexusLang test programs",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument(
        "pattern",
        nargs="?",
        default="test_*.nxl",
        help="Test file pattern (default: test_*.nxl)"
    )
    
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Verbose output (show test output)"
    )
    
    parser.add_argument(
        "-q", "--quiet",
        action="store_true",
        help="Quiet mode (minimal output)"
    )
    
    parser.add_argument(
        "-c", "--category",
        help="Filter tests by category (features, control_flow, stdlib, etc.)"
    )
    
    parser.add_argument(
        "--coverage",
        action="store_true",
        help="Collect coverage information (not yet implemented)"
    )
    
    parser.add_argument(
        "-o", "--output",
        help="Save results to JSON file"
    )
    
    args = parser.parse_args()
    
    # Create test runner
    runner = TestRunner(
        verbose=args.verbose,
        quiet=args.quiet,
        coverage=args.coverage
    )
    
    # Discover tests
    test_files = runner.discover_tests(args.pattern, args.category)
    
    # Run tests
    start_time = time.time()
    passed, failed = runner.run_tests(test_files)
    total_time = time.time() - start_time
    
    # Print summary
    runner.print_summary(passed, failed, total_time)
    
    # Save results if requested
    if args.output:
        runner.save_results(args.output)
    
    # Exit with appropriate code
    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
