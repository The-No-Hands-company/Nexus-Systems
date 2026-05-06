#!/usr/bin/env python3
"""
Comprehensive FFI Test Runner
Tests all FFI features and identifies edge cases
"""

import subprocess
import sys
import os
from pathlib import Path

# Add src to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

def test_ffi_program(program_path, expected_patterns=None):
    """Test compiling and running an FFI program."""
    print(f"\n{'='*60}")
    print(f"Testing: {program_path}")
    print('='*60)
    
    # Get output name
    prog_name = Path(program_path).stem
    output = f"test_{prog_name}"
    
    # Compile
    compile_cmd = [
        'python', 'nlplc_llvm.py',
        program_path,
        '-o', output
    ]
    
    print(f"Compiling: {' '.join(compile_cmd)}")
    result = subprocess.run(compile_cmd, capture_output=True, text=True, timeout=10)
    
    if result.returncode != 0:
        print(f" COMPILATION FAILED")
        print(f"STDOUT: {result.stdout}")
        print(f"STDERR: {result.stderr}")
        return False
    
    print(" Compilation successful")
    
    # Check if executable exists
    if not os.path.exists(output):
        print(f" Executable not found: {output}")
        return False
    
    # Run
    print(f"Running: ./{output}")
    run_result = subprocess.run([f"./{output}"], capture_output=True, text=True, timeout=5)
    
    print(f"Exit code: {run_result.returncode}")
    print(f"Output:\n{run_result.stdout}")
    
    if run_result.stderr:
        print(f"Stderr:\n{run_result.stderr}")
    
    # Check expected patterns
    if expected_patterns:
        for pattern in expected_patterns:
            if pattern not in run_result.stdout:
                print(f" Expected pattern not found: {pattern}")
                return False
    
    # Cleanup
    if os.path.exists(output):
        os.remove(output)
    
    print(" Test passed")
    return True


def main():
    """Run all FFI tests."""
    os.chdir('/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Active/NLPL')
    
    test_cases = [
        # Basic FFI
        ('test_programs/ffi/test_ffi_basic.nxl', ['Hello from NLPL']),
        
        # Structs
        ('test_programs/ffi/test_ffi_struct.nxl', ['Point:', 'Sum:', 'Distance:']),
        
        # Math
        ('test_programs/ffi/test_ffi_math.nxl', None),
        
        # Callbacks
        ('test_programs/ffi/test_ffi_callback_working.nxl', ['Testing callback', 'Callback called']),
        
        # Variadic
        ('test_programs/ffi/test_variadic_printf.nxl', None),
    ]
    
    passed = 0
    failed = 0
    
    for test_file, expected in test_cases:
        try:
            if test_ffi_program(test_file, expected):
                passed += 1
            else:
                failed += 1
        except Exception as e:
            print(f" Test error: {e}")
            failed += 1
    
    print(f"\n{'='*60}")
    print(f"FFI Test Results: {passed} passed, {failed} failed")
    print('='*60)
    
    return 0 if failed == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
