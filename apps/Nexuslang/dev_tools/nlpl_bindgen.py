#!/usr/bin/env python3
"""
nlpl-bindgen - NexusLang FFI Binding Generator

Automatically generates NexusLang extern declarations from C header files.
Supports functions, structs, unions, enums, typedefs, and macros.

Usage:
    nlpl-bindgen <header.h> [options]
    
Examples:
    nlpl-bindgen /usr/include/math.h -l m -o math_bindings.nlpl
    nlpl-bindgen sqlite3.h -l sqlite3 -o sqlite3_bindings.nlpl
    nlpl-bindgen opengl.h -l GL -o opengl_bindings.nlpl --config gl.json
"""

import argparse
import sys
import os
import json
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from nexuslang.compiler.header_parser import CHeaderParser, TypeMapper


def load_config(config_path: str) -> dict:
    """Load configuration file with custom type mappings."""
    try:
        with open(config_path, 'r') as f:
            config = json.load(f)
        return config
    except Exception as e:
        print(f"Warning: Could not load config file {config_path}: {e}")
        return {}


def apply_config(parser: CHeaderParser, config: dict):
    """Apply configuration settings to parser."""
    # Add custom type mappings
    if 'type_mappings' in config:
        for c_type, nxl_type in config['type_mappings'].items():
            parser.type_mapper.add_custom_mapping(c_type, nxl_type)
            print(f"  Custom mapping: {c_type} -> {nxl_type}")
    
    # Add opaque types
    if 'opaque_types' in config:
        for opaque_type in config['opaque_types']:
            parser.type_mapper.opaque_types.add(opaque_type)
            print(f"  Opaque type: {opaque_type}")
    
    # Set library name if specified
    if 'library' in config:
        parser.library_name = config['library']


def find_system_headers() -> list:
    """Find common system header directories."""
    common_paths = [
        '/usr/include',
        '/usr/local/include',
        '/opt/local/include',
        'C:\\Program Files\\Microsoft Visual Studio\\*\\include',
        'C:\\MinGW\\include',
    ]
    
    found = []
    for path in common_paths:
        if os.path.exists(path):
            found.append(path)
    
    return found


def main():
    parser = argparse.ArgumentParser(
        description='Generate NexusLang FFI bindings from C header files',
        epilog='Examples:\n'
               '  %(prog)s /usr/include/math.h -l m -o math.nlpl\n'
               '  %(prog)s sqlite3.h -l sqlite3 -o sqlite3.nlpl\n'
               '  %(prog)s opengl.h -l GL --config gl.json\n',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument('header', help='C header file to parse')
    parser.add_argument('-l', '--library', help='Library name for extern declarations (e.g., "c", "m", "pthread")')
    parser.add_argument('-o', '--output', help='Output NexusLang file (default: <header_name>_bindings.nxl)')
    parser.add_argument('-c', '--config', help='JSON configuration file with custom type mappings')
    parser.add_argument('-I', '--include', action='append', help='Additional include directories')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
    parser.add_argument('--list-headers', action='store_true', help='List system header directories and exit')
    parser.add_argument('--print-only', action='store_true', help='Print bindings to stdout instead of file')
    
    args = parser.parse_args()
    
    # List system headers if requested
    if args.list_headers:
        print("System header directories:")
        for path in find_system_headers():
            print(f"  {path}")
        return 0
    
    # Validate header file
    header_path = args.header
    if not os.path.exists(header_path):
        # Try to find in system include paths
        if args.include:
            for include_dir in args.include:
                candidate = os.path.join(include_dir, header_path)
                if os.path.exists(candidate):
                    header_path = candidate
                    break
        
        # Try system paths
        if not os.path.exists(header_path):
            for include_dir in find_system_headers():
                candidate = os.path.join(include_dir, header_path)
                if os.path.exists(candidate):
                    header_path = candidate
                    break
        
        if not os.path.exists(header_path):
            print(f"Error: Header file not found: {args.header}", file=sys.stderr)
            print(f"Try specifying -I <include_dir> or use absolute path", file=sys.stderr)
            return 1
    
    # Determine output path
    if args.output:
        output_path = args.output
    elif args.print_only:
        output_path = None
    else:
        # Default: <header_name>_bindings.nlpl
        header_name = Path(header_path).stem
        output_path = f"{header_name}_bindings.nxl"
    
    if args.verbose:
        print(f"Parsing header: {header_path}")
        if args.library:
            print(f"Library: {args.library}")
        if output_path:
            print(f"Output: {output_path}")
    
    # Create parser
    c_parser = CHeaderParser(library_name=args.library)
    
    # Load and apply configuration
    if args.config:
        if args.verbose:
            print(f"Loading configuration: {args.config}")
        config = load_config(args.config)
        apply_config(c_parser, config)
    
    # Parse header
    if args.verbose:
        print("Parsing...")
    
    success = c_parser.parse_header(header_path)
    
    if not success:
        print(f"Error: Failed to parse {header_path}", file=sys.stderr)
        return 1
    
    # Get summary
    summary = c_parser.get_summary()
    
    if args.verbose or args.print_only:
        print(f"\nParsing complete:")
        print(f"  Functions: {summary['functions']}")
        print(f"  Structs/Unions: {summary['structs']}")
        print(f"  Enums: {summary['enums']}")
        print(f"  Typedefs: {summary['typedefs']}")
        print(f"  Macros: {summary['macros']}")
    
    # Generate bindings
    if args.verbose:
        print("\nGenerating NexusLang bindings...")
    
    if args.print_only:
        # Print to stdout
        output = c_parser.generate_nxl_module(output_path=None)
        print("\n" + "="*70)
        print(output)
    else:
        # Write to file
        c_parser.generate_nxl_module(output_path=output_path)
        print(f"\nSuccess! Generated {output_path}")
        print(f"  {summary['functions']} functions")
        print(f"  {summary['structs']} structs/unions")
        print(f"  {summary['enums']} enums")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
