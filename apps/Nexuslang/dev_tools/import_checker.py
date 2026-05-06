#!/usr/bin/env python3
"""
Import Consistency Checker
===========================

Detects when the same module is imported via different paths,
creating duplicate classes/enums that break === comparisons.

This would have caught the TokenType dual-enum bug immediately!

Usage:
    python import_checker.py
    python import_checker.py --verbose
"""

import sys
import os
import ast
from pathlib import Path
from collections import defaultdict
from colorama import Fore, Style, init

init(autoreset=True)


class ImportCollector(ast.NodeVisitor):
    """Collect all imports from a Python file."""
    
    def __init__(self, filepath):
        self.filepath = filepath
        self.imports = []
        
    def visit_Import(self, node):
        """Handle: import module"""
        for alias in node.names:
            self.imports.append({
                'type': 'import',
                'module': alias.name,
                'name': alias.asname or alias.name,
                'line': node.lineno
            })
        self.generic_visit(node)
    
    def visit_ImportFrom(self, node):
        """Handle: from module import name"""
        if node.module:
            for alias in node.names:
                full_module = f"{node.module}.{alias.name}"
                self.imports.append({
                    'type': 'from',
                    'module': node.module,
                    'name': alias.name,
                    'full': full_module,
                    'as': alias.asname,
                    'line': node.lineno
                })
        self.generic_visit(node)


def check_file(filepath):
    """Check a single file for imports."""
    try:
        with open(filepath, 'r') as f:
            tree = ast.parse(f.read(), filename=str(filepath))
        
        collector = ImportCollector(filepath)
        collector.visit(tree)
        return collector.imports
    
    except SyntaxError:
        # Skip files with syntax errors
        return []
    except Exception as e:
        print(f"{Fore.RED}Error reading {filepath}: {e}")
        return []


def normalize_module_path(module_name):
    """Normalize module names to canonical form."""
    # Remove leading dots
    module_name = module_name.lstrip('.')
    
    # Normalize common patterns
    if module_name.startswith('src.'):
        return module_name
    
    # If it's a relative import from project, prefix with src
    if module_name.startswith('parser') or module_name.startswith('compiler'):
        return f'src.{module_name}'
    
    return module_name


def find_duplicate_imports(project_root):
    """Find files that import the same modules via different paths."""
    
    # Collect all imports from all Python files
    all_imports = defaultdict(list)
    
    print(f"{Fore.CYAN}Scanning Python files in {project_root}...\n")
    
    for py_file in Path(project_root).rglob('*.py'):
        # Skip __pycache__
        if '__pycache__' in str(py_file):
            continue
        
        imports = check_file(py_file)
        
        for imp in imports:
            # Track which file imported this module
            key = imp.get('full', imp.get('module'))
            all_imports[key].append({
                'file': str(py_file.relative_to(project_root)),
                'import': imp
            })
    
    # Find inconsistencies
    issues = []
    
    # Check for same module imported via different paths
    module_groups = defaultdict(set)
    
    for module, locations in all_imports.items():
        # Group by normalized form
        normalized = normalize_module_path(module)
        module_groups[normalized].add(module)
    
    # Find groups with multiple import paths
    for normalized, variants in module_groups.items():
        if len(variants) > 1:
            issues.append({
                'module': normalized,
                'variants': list(variants),
                'locations': {}
            })
            
            # Collect locations for each variant
            for variant in variants:
                issues[-1]['locations'][variant] = all_imports[variant]
    
    return issues


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description="Check for import inconsistencies")
    parser.add_argument('--verbose', '-v', action='store_true', help="Show all imports")
    args = parser.parse_args()
    
    project_root = Path(__file__).parent.parent
    
    print(f"{Fore.CYAN}{'='*80}")
    print(f"IMPORT CONSISTENCY CHECKER")
    print(f"{'='*80}\n")
    
    issues = find_duplicate_imports(project_root)
    
    if not issues:
        print(f"{Fore.GREEN} No import inconsistencies found!\n")
        return 0
    
    # Report issues
    print(f"{Fore.RED}Found {len(issues)} import inconsistency issues:\n")
    
    for i, issue in enumerate(issues, 1):
        print(f"{Fore.YELLOW}Issue #{i}: {issue['module']}")
        print(f"{Fore.YELLOW}{''*80}")
        
        print(f"\n{Fore.RED}  This module is imported via {len(issue['variants'])} DIFFERENT paths:")
        
        for variant in issue['variants']:
            print(f"\n  {Fore.CYAN}Path: {Fore.WHITE}{variant}")
            locations = issue['locations'][variant]
            
            for loc in locations[:5]:  # Show first 5
                file = loc['file']
                imp = loc['import']
                line = imp['line']
                
                if imp['type'] == 'from':
                    print(f"    {Fore.GREEN}{file}:{line} {Fore.WHITE}from {imp['module']} import {imp['name']}")
                else:
                    print(f"    {Fore.GREEN}{file}:{line} {Fore.WHITE}import {imp['module']}")
            
            if len(locations) > 5:
                print(f"    {Fore.YELLOW}... and {len(locations) - 5} more")
        
        print(f"\n  {Fore.RED} WARNING:")
        print(f"    These different import paths create SEPARATE module instances!")
        print(f"    This breaks === comparison, isinstance() checks, and enum comparisons.")
        
        print(f"\n  {Fore.GREEN} FIX:")
        canonical = f"src.{issue['module']}" if not issue['module'].startswith('src.') else issue['module']
        print(f"    Use ONE consistent import path: {Fore.CYAN}from {canonical.rsplit('.', 1)[0]} import {canonical.rsplit('.', 1)[1]}")
        
        print(f"\n{Fore.YELLOW}{''*80}\n")
    
    # Show summary
    print(f"\n{Fore.RED}{'='*80}")
    print(f"SUMMARY: {len(issues)} issues found")
    print(f"{'='*80}\n")
    
    print(f"{Fore.YELLOW}Why this matters:")
    print(f"  When you import the same module via different paths, Python creates")
    print(f"  SEPARATE instances. This means:")
    print(f"    • token.type == TokenType.SET can be FALSE even when it should be TRUE")
    print(f"    • isinstance(obj, MyClass) fails")
    print(f"    • Enum comparisons break")
    print(f"\n{Fore.GREEN}Solution:")
    print(f"  Always use the SAME import path across your entire project!")
    print(f"  Example: Always use 'from src.parser.lexer' NOT 'from parser.lexer'\n")
    
    return 1


if __name__ == "__main__":
    sys.exit(main())
