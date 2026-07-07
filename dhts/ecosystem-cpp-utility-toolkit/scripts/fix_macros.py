#!/usr/bin/env python3
"""Fix all header-cpp mismatches by ensuring headers declare what .cpp implements."""

import os, re

BASE = "/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/dhts/dht_nexus_debug"
SRC = f"{BASE}/src/utility"
INC = f"{BASE}/include/nexus/utility"

# Files where the .h is macro-only and .cpp should be bare include
MACRO_ONLY = {
    'build/compiler_diagnostics',
    'build/feature_detector',
    'build/platform_detector',
    'build/static_analyzer_hints',
    'build/warning_disable',
    'codegen/generated_code_marker',
}

def find_methods(cpp_content):
    """Extract method signatures from .cpp file"""
    methods = []
    # Match: ReturnType ClassName::methodName(params) { ... }
    # or: ClassName::ClassName(params) { ... }
    for m in re.finditer(r'(?:([\w:<>,\s*&]+)\s+)?(\w+)::(\w+)\s*\(([^)]*)\)\s*(?:const\s*)?', cpp_content):
        methods.append(m.group(0))
    return methods

fixed = 0
for root, dirs, files in os.walk(SRC):
    for fname in files:
        if not fname.endswith('.cpp'): continue
        cpp_path = os.path.join(root, fname)
        
        # Get category + name
        rel = os.path.relpath(cpp_path, SRC)
        cat_name = rel.replace('.cpp', '')
        cat, name = os.path.split(cat_name)
        
        h_path = os.path.join(INC, cat, f'{name}.h')
        if not os.path.exists(h_path): continue
        
        with open(cpp_path) as f:
            cpp = f.read()
        with open(h_path) as f:
            h = f.read()
        
        # Check if macro-only
        if f'{cat}/{name}' in MACRO_ONLY:
            # Simplify .cpp to bare include
            new_cpp = f'// {name}.h is a macro-only header (no class needed)\n#include "nexus/utility/{cat}/{name}.h"\n'
            if cpp != new_cpp:
                with open(cpp_path, 'w') as f: f.write(new_cpp)
                print(f'  SIMPLIFIED: {cat}/{name}.cpp (macro-only header)')
                fixed += 1
            continue
        
        # Check for method mismatches
        cpp_methods = find_methods(cpp)
        for method_sig in cpp_methods:
            # Check if method is declared in header
            m = re.search(r'(\w+)::(\w+)\s*\(', method_sig)
            if not m: continue
            class_name = m.group(1)
            method_name = m.group(2)
            
            # Skip constructor/destructor/instance/initialize/shutdown/isEnabled
            if method_name in ['instance', 'initialize', 'shutdown', 'isEnabled', class_name, f'~{class_name}']:
                continue
            
            # Check if method is declared in header
            if method_name not in h:
                print(f'  WARNING: {cat}/{name} - .cpp has {method_name}() but not in header')
                # We can't auto-fix this easily, just report it

print(f"Fixed {fixed} files")
