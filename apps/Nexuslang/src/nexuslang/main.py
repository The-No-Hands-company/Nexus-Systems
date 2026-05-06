"""
Main module for the NexusLang interpreter.
This is the entry point for the NexusLang interpreter.
"""

import sys
import os
import argparse
import traceback

from .parser.lexer import Lexer
from .parser.parser import Parser
from .interpreter.interpreter import Interpreter
from .runtime.runtime import Runtime
from .stdlib import register_stdlib
from .tools import get_profiler, enable_profiling, disable_profiling
from .errors import NxlError, NxlTypeError, NxlRuntimeError

def run_program(source_code, debug=False, type_check=True, profiler=None, optimize=0,
               file_path=None, freestanding_config=None, target=None,
               coverage_collector=None):
    """
    Run an NexusLang program from source code.

    Args:
        source_code (str): The source code of the NexusLang program
        debug (bool): Whether to enable debug mode
        type_check (bool): Whether to enable type checking
        profiler: Optional profiler instance for performance tracking
        optimize (int): AST optimization level (0=none, 1=basic, 2=standard, 3=aggressive)
        file_path (str): Absolute or relative path of the source file (enables relative imports)
        freestanding_config: Optional FreestandingConfig for bare-metal builds
        target: Optional CompileTarget for conditional compilation; defaults to host_target()
        coverage_collector: Optional CoverageCollector instance; when provided the interpreter
                            will record every executed line into it.

    Returns:
        The result of the program execution
    """
    # Initialize the runtime
    runtime = Runtime()
    
    # Record the source file path so the module loader can resolve relative imports
    if file_path:
        runtime.module_path = os.path.abspath(file_path)
    
    # Register standard library functions
    register_stdlib(runtime)

    # Apply freestanding (bare-metal) configuration if provided
    if freestanding_config is not None:
        from .compiler.freestanding import apply_freestanding_config
        apply_freestanding_config(runtime, freestanding_config)
    
    # Initialize components
    lexer = Lexer(source_code)
    tokens = lexer.tokenize()
    
    if debug:
        print("Tokens:")
        for token in tokens:
            print(f"  {token}")
    
    parser = Parser(tokens, source=source_code)
    ast = parser.parse()

    # Static conditional-compilation pruning: strip dead 'when target ...' blocks
    # BEFORE any static analysis so checkers never see dead-branch code.
    from .compiler.preprocessor import preprocess_ast, host_target
    preprocess_ast(ast, target=target if target is not None else host_target())

    if debug:
        print("\nAST:")
        print_ast(ast)

    # Compile-time borrow checker (ownership / borrow safety)
    from .typesystem.borrow_checker import BorrowChecker
    from .typesystem.lifetime_checker import LifetimeChecker

    borrow_errors = BorrowChecker().check(ast)
    if borrow_errors:
        messages = "\n".join(str(e) for e in borrow_errors)
        raise NxlRuntimeError(
            f"Borrow checker failed:\n{messages}",
            error_type_key="runtime_error",
            full_source=source_code,
        )

    lifetime_errors = LifetimeChecker().check(ast)
    # Separate errors from warnings
    hard_lt_errors = [e for e in lifetime_errors if not e.is_warning]
    lt_warnings = [e for e in lifetime_errors if e.is_warning]
    if lt_warnings and debug:
        for w in lt_warnings:
            print(f"[lifetime warning] {w}")
    if hard_lt_errors:
        messages = "\n".join(str(e) for e in hard_lt_errors)
        raise NxlRuntimeError(
            f"Lifetime checker failed:\n{messages}",
            error_type_key="runtime_error",
            full_source=source_code,
        )

    interpreter = Interpreter(runtime, enable_type_checking=type_check, source=source_code)
    
    # Attach profiler if provided
    if profiler:
        interpreter.profiler = profiler

    # Attach coverage collector if provided
    if coverage_collector is not None:
        interpreter._coverage_collector = coverage_collector
    
    try:
        result = interpreter.interpret(ast, optimization_level=optimize)
        return result
    except NxlError:
        # Re-raise NexusLang errors to preserve rich formatting
        raise
    except TypeError as e:
        raise NxlTypeError(
            str(e),
            error_type_key="type_mismatch",
            full_source=source_code,
        )
    except Exception as e:
        if debug:
            traceback.print_exc()
        raise NxlRuntimeError(
            str(e),
            error_type_key="runtime_error",
            full_source=source_code,
        )

def print_ast(node, indent=0):
    """Print the AST in a readable format for debugging."""
    indent_str = "  " * indent
    if hasattr(node, 'statements'):
        print(f"{indent_str}{node}")
        for stmt in node.statements:
            print_ast(stmt, indent + 1)
    elif hasattr(node, 'body') and isinstance(node.body, list):
        print(f"{indent_str}{node}")
        for stmt in node.body:
            print_ast(stmt, indent + 1)
    else:
        print(f"{indent_str}{node}")

def _apply_freestanding(args: argparse.Namespace, runtime: "Runtime"):
    """Apply freestanding mode configuration to runtime if --freestanding was given.

    When runtime is None, only builds and returns the config (no application).
    Returns the FreestandingConfig if freestanding mode is active, else None.
    """
    if not getattr(args, 'freestanding', False):
        return None
    from .compiler.freestanding import parse_freestanding_args, apply_freestanding_config
    config = parse_freestanding_args(args)
    if config is not None and runtime is not None:
        apply_freestanding_config(runtime, config)
    return config


def _build_argument_parser() -> argparse.ArgumentParser:
    """Build and return the argument parser for the NexusLang interpreter."""
    parser = argparse.ArgumentParser(description='NexusLang Interpreter')
    parser.add_argument('file', nargs='?', help='The NexusLang file to execute (omit for interactive REPL)')
    parser.add_argument('--debug', action='store_true', help='Enable debug mode')
    parser.add_argument('--no-type-check', action='store_true', help='Disable type checking')
    parser.add_argument('--repl', action='store_true', help='Start interactive REPL (default if no file)')
    parser.add_argument(
        '--target',
        metavar='TRIPLE',
        default=None,
        help=(
            'Cross-compilation target triple, e.g. x86_64-unknown-linux-gnu, '
            'aarch64-unknown-linux-gnu, riscv64-unknown-none-elf. '
            'Controls conditional compilation blocks (when target os/arch is ...)'
        ),
    )
    parser.add_argument('--debugger', action='store_true', help='Enable interactive debugger')
    parser.add_argument('--break', '-b', dest='breakpoints', action='append',
                       help='Set breakpoint at line (can be used multiple times)')
    parser.add_argument('--profile', action='store_true', help='Enable runtime profiling')
    parser.add_argument('--profile-output', help='Save profile results to JSON file')
    parser.add_argument('--profile-flamegraph', help='Export flamegraph format to file')
    parser.add_argument('--explain', metavar='CODE', help='Explain error code (e.g., E001)')
    parser.add_argument('--list-errors', action='store_true', help='List all error codes')
    parser.add_argument(
        '--optimize', '-O',
        type=int, choices=[0, 1, 2, 3], default=0,
        metavar='LEVEL',
        help='AST optimization level: 0=none (default), 1=basic, 2=standard, 3=aggressive'
    )

    # Freestanding / bare-metal build options
    parser.add_argument(
        '--freestanding',
        action='store_true',
        help='Enable freestanding (bare-metal) mode: strips OS-dependent stdlib modules'
    )
    parser.add_argument(
        '--linker-script',
        metavar='FILE',
        help='Path to a custom linker script (.ld) for bare-metal builds'
    )
    parser.add_argument(
        '--arch',
        default='x86_64',
        metavar='ARCH',
        help='Target architecture for freestanding mode (x86_64, arm, cortex-m, riscv, riscv32, riscv64)'
    )
    parser.add_argument(
        '--entry-symbol',
        default='nxl_main',
        metavar='SYMBOL',
        help='Entry point symbol name for freestanding builds (default: nxl_main)'
    )
    parser.add_argument(
        '--stack-size',
        type=int,
        default=65536,
        metavar='BYTES',
        help='Stack size in bytes for freestanding builds (default: 65536)'
    )
    parser.add_argument(
        '--heap-size',
        type=int,
        default=131072,
        metavar='BYTES',
        help='Heap size in bytes for freestanding builds (default: 131072)'
    )
    parser.add_argument(
        '--emit-entry-stub',
        metavar='FILE',
        help='Write the assembly entry stub for --arch to FILE and exit'
    )
    parser.add_argument(
        '--emit-linker-script',
        metavar='FILE',
        help='Write a template linker script for --arch to FILE and exit'
    )
    parser.add_argument(
        '--no-float',
        action='store_true',
        help='Disable floating-point support in freestanding mode (soft-float targets)'
    )
    parser.add_argument(
        '--no-exceptions',
        action='store_true',
        help='Disable exception-handling runtime in freestanding mode'
    )
    parser.add_argument(
        '--bare-metal-threads',
        action='store_true',
        help='Allow threading primitives in freestanding mode (requires RTOS)'
    )
    return parser


def _handle_emit_commands(args: argparse.Namespace) -> bool:
    """Handle --emit-entry-stub and --emit-linker-script early-exit commands.

    Returns True if a command was handled (caller should return immediately).
    """
    if args.emit_entry_stub:
        from .compiler.freestanding import generate_entry_stub
        arch = getattr(args, 'arch', 'x86_64') or 'x86_64'
        stub = generate_entry_stub(arch)
        with open(args.emit_entry_stub, 'w') as _f:
            _f.write(stub)
        print(f"Entry stub for '{arch}' written to: {args.emit_entry_stub}")
        return True

    if args.emit_linker_script:
        from .compiler.linker import get_linker_script_for_arch
        arch = getattr(args, 'arch', 'x86_64') or 'x86_64'
        script = get_linker_script_for_arch(arch)
        with open(args.emit_linker_script, 'w') as _f:
            _f.write(script)
        print(f"Linker script for '{arch}' written to: {args.emit_linker_script}")
        return True

    return False


def _handle_error_code_commands(args: argparse.Namespace) -> bool:
    """Handle --explain and --list-errors early-exit commands.

    Returns True if a command was handled (caller should return immediately).
    """
    if args.explain:
        from .error_codes import get_error_info
        error_info = get_error_info(args.explain.upper())
        if error_info:
            print(error_info.format_help())
        else:
            print(f"Error code '{args.explain}' not found.")
            print("Use --list-errors to see all available error codes.")
        return True

    if args.list_errors:
        from .error_codes import ERROR_CODES
        print("NexusLang Error Codes")
        print("=" * 60)
        categories: dict = {}
        for code, info in sorted(ERROR_CODES.items()):
            if info.category not in categories:
                categories[info.category] = []
            categories[info.category].append((code, info.title))
        for category, errors in sorted(categories.items()):
            print(f"\n{category.upper()} ERRORS:")
            for code, title in errors:
                print(f"  {code}: {title}")
        print(f"\nTotal: {len(ERROR_CODES)} error codes")
        print("Use 'nxl --explain CODE' for details on a specific error.")
        return True

    return False


def _run_debugger_mode(args: argparse.Namespace, source_code: str) -> None:
    """Run the program under the interactive debugger."""
    from .debugger.debugger import Debugger
    from .parser.lexer import Lexer as _Lexer
    from .parser.parser import Parser as _Parser

    runtime = Runtime()
    runtime.module_path = os.path.abspath(args.file)
    register_stdlib(runtime)
    _fs_config = _apply_freestanding(args, runtime)
    interpreter = Interpreter(runtime, enable_type_checking=not args.no_type_check)

    debugger = Debugger(interpreter, interactive=True)
    interpreter.debugger = debugger
    interpreter.current_file = args.file

    if args.breakpoints:
        for bp in args.breakpoints:
            try:
                line = int(bp)
                debugger.add_breakpoint(args.file, line)
            except ValueError:
                print(f"Warning: Invalid breakpoint: {bp}")

    print(f"Debugging: {args.file}")
    if args.breakpoints:
        print(f"Breakpoints: {len(debugger.list_breakpoints())}")
    print()

    try:
        lexer = _Lexer(source_code)
        tokens = lexer.tokenize()

        if args.debug:
            print("Tokens:")
            for token in tokens:
                print(f"  {token}")

        parser = _Parser(tokens)
        ast = parser.parse()

        if args.debug:
            print("\nAST:")
            print_ast(ast)

        result = interpreter.interpret(ast, optimization_level=getattr(args, 'optimize', 0))

        if result is not None:
            print(f"\nProgram result: {result}")

        print("\n" + "=" * 60)
        debugger.print_statistics()

    except Exception as e:
        if hasattr(e, 'format_error'):
            print(f"\n{e.format_error()}")
        else:
            print(f"\nError: {e}")
        if args.debug:
            import traceback
            traceback.print_exc()
        sys.exit(1)


def main():
    """Main entry point for the NexusLang interpreter."""
    args = _build_argument_parser().parse_args()

    if _handle_emit_commands(args):
        return

    if _handle_error_code_commands(args):
        return

    # Initialize profiler if requested
    profiler = None
    if args.profile:
        profiler = enable_profiling()
        profiler.start()

    # Warn when type checking is disabled
    if args.no_type_check:
        print(
            "warning: type checking disabled via --no-type-check; "
            "type errors will only be caught at runtime",
            file=sys.stderr,
        )

    # Start REPL if no file specified or --repl flag
    if args.file is None or (args.repl and not args.file):
        from .repl.repl import REPL
        repl = REPL(debug=args.debug, type_check=not args.no_type_check)
        try:
            repl.run()
        except Exception as e:
            print(f"REPL Error: {e}")
            if args.debug:
                import traceback
                traceback.print_exc()
            sys.exit(1)
        return

    # Check if the file exists
    if not os.path.isfile(args.file):
        print(f"Error: File '{args.file}' not found")
        sys.exit(1)

    # Read the file
    with open(args.file, 'r') as f:
        source_code = f.read()

    # Run with debugger if requested
    if args.debugger or args.breakpoints:
        _run_debugger_mode(args, source_code)
        return

    # Resolve --target triple into a CompileTarget (for conditional compilation)
    _compile_target = None
    if getattr(args, 'target', None):
        from .compiler.preprocessor import CompileTarget
        try:
            _compile_target = CompileTarget.from_triple(args.target)
        except ValueError as e:
            print(f"Error: Invalid target triple: {e}")
            sys.exit(1)

    # Run normally without debugger
    try:
        _fs_cfg = _apply_freestanding(args, None)
        result = run_program(source_code, args.debug, not args.no_type_check, profiler,
                             optimize=args.optimize, file_path=args.file,
                             freestanding_config=_fs_cfg, target=_compile_target)
        if result is not None:
            print(f"Program result: {result}")

        # Print and export profiling results if enabled
        if profiler:
            profiler.stop()
            print("\n" + "=" * 70)
            profiler.print_report()

            if args.profile_output:
                profiler.export_json(args.profile_output)
                print(f"\nProfile results saved to: {args.profile_output}")

            if args.profile_flamegraph:
                profiler.export_flamegraph(args.profile_flamegraph)
                print(f"Flamegraph data saved to: {args.profile_flamegraph}")
    except Exception as e:
        if hasattr(e, 'format_error'):
            print(e.format_error())
        else:
            print(f"Error: {e}")
        if args.debug:
            import traceback
            traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    main() 