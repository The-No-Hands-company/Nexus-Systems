#!/usr/bin/env python3
"""
nlpllint - NexusLang Static Analyzer
================================

Command-line interface for NexusLang static analysis.

Usage:
    nlpllint file.nlpl                  # Analyze single file
    nlpllint dir/                       # Analyze directory
    nlpllint --strict file.nlpl         # Strict mode (all checks)
    nlpllint --json file.nlpl           # JSON output
    nlpllint --fix file.nlpl            # Auto-fix issues
    nlpllint --help                     # Show help
"""

import sys
import os
import argparse
import json
import tomllib
from pathlib import Path
from typing import Any, Dict, List, Optional

# Add parent directories to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../..'))

from nexuslang.tooling.analyzer import StaticAnalyzer, AnalysisReport
from nexuslang.tooling.analyzer.analyzer import (
    create_default_analyzer,
    create_strict_analyzer,
    create_minimal_analyzer
)
from nexuslang.tooling.analyzer.report import Severity
from nexuslang.tooling.analyzer.autofix import AutoFixer
from nexuslang.tooling.analyzer.ide_hooks import IDEHooks, LspFormatter


_ALLOWED_ROOT_KEYS = {
    'mode',
    'strict',
    'minimal',
    'json',
    'fix',
    'dry_run',
    'no_color',
    'errors_only',
    'max_issues',
    'analyzer',
}

_ALLOWED_ANALYZER_KEYS = {
    'memory',
    'null',
    'resources',
    'init',
    'types',
    'dead_code',
    'style',
    'performance',
    'security',
    'data_flow',
    'control_flow',
}


def _validate_bool(config: Dict[str, Any], key: str) -> None:
    if key in config and not isinstance(config[key], bool):
        raise ValueError(f"Config key '{key}' must be a boolean")


def _validate_config(config: Dict[str, Any]) -> None:
    """Validate linter configuration schema and value types."""
    unknown_root = sorted(set(config.keys()) - _ALLOWED_ROOT_KEYS)
    if unknown_root:
        raise ValueError(f"Unknown config key(s): {', '.join(unknown_root)}")

    if 'mode' in config:
        mode = config['mode']
        if not isinstance(mode, str):
            raise ValueError("Config key 'mode' must be a string")
        if mode.strip().lower() not in {'default', 'strict', 'minimal'}:
            raise ValueError("Config key 'mode' must be one of: default, strict, minimal")

    for key in ('strict', 'minimal', 'json', 'fix', 'dry_run', 'no_color', 'errors_only'):
        _validate_bool(config, key)

    if 'max_issues' in config:
        max_issues = config['max_issues']
        if not isinstance(max_issues, int):
            raise ValueError("Config key 'max_issues' must be an integer")
        if max_issues <= 0:
            raise ValueError("Config key 'max_issues' must be > 0")

    analyzer_cfg = config.get('analyzer')
    if analyzer_cfg is not None:
        if not isinstance(analyzer_cfg, dict):
            raise ValueError("Config key 'analyzer' must be an object/table")
        unknown_analyzer = sorted(set(analyzer_cfg.keys()) - _ALLOWED_ANALYZER_KEYS)
        if unknown_analyzer:
            raise ValueError(
                f"Unknown analyzer config key(s): {', '.join(unknown_analyzer)}"
            )
        for key, value in analyzer_cfg.items():
            if not isinstance(value, bool):
                raise ValueError(f"Analyzer config key '{key}' must be a boolean")


def _load_config(path: str) -> Dict[str, Any]:
    """Load linter configuration from JSON or TOML."""
    cfg_path = Path(path)
    if not cfg_path.exists():
        raise FileNotFoundError(f"Configuration file not found: {path}")

    raw = cfg_path.read_bytes()
    suffix = cfg_path.suffix.lower()

    if suffix == '.json':
        data = json.loads(raw.decode('utf-8'))
    elif suffix in ('.toml', '.tml'):
        data = tomllib.loads(raw.decode('utf-8'))
    else:
        # Try TOML first, then JSON for extension-less config files.
        try:
            data = tomllib.loads(raw.decode('utf-8'))
        except Exception:
            data = json.loads(raw.decode('utf-8'))

    if not isinstance(data, dict):
        raise ValueError("Configuration root must be an object/table")

    # Allow nesting under [nlpllint] or [linter], while keeping flat files valid.
    if isinstance(data.get('nlpllint'), dict):
        config = data['nlpllint']
        _validate_config(config)
        return config
    if isinstance(data.get('linter'), dict):
        config = data['linter']
        _validate_config(config)
        return config

    _validate_config(data)
    return data


def _create_analyzer_from_config(args, config: Dict[str, Any]) -> StaticAnalyzer:
    """Create analyzer instance using CLI mode flags with config fallbacks."""
    mode = str(config.get('mode', '')).strip().lower()

    strict_mode = args.strict or mode == 'strict' or bool(config.get('strict', False))
    minimal_mode = args.minimal or mode == 'minimal' or bool(config.get('minimal', False))

    if strict_mode and minimal_mode:
        # CLI conflict should keep argparse behavior intuitive: explicit strict wins.
        minimal_mode = False

    if strict_mode:
        return create_strict_analyzer()
    if minimal_mode:
        return create_minimal_analyzer()

    analyzer_cfg = config.get('analyzer', {})
    if not isinstance(analyzer_cfg, dict) or not analyzer_cfg:
        return create_default_analyzer()

    params = {
        'enable_all': True,
        'enable_memory': bool(analyzer_cfg.get('memory', True)),
        'enable_null': bool(analyzer_cfg.get('null', True)),
        'enable_resources': bool(analyzer_cfg.get('resources', True)),
        'enable_init': bool(analyzer_cfg.get('init', True)),
        'enable_types': bool(analyzer_cfg.get('types', True)),
        'enable_dead_code': bool(analyzer_cfg.get('dead_code', True)),
        'enable_style': bool(analyzer_cfg.get('style', False)),
        'enable_performance': bool(analyzer_cfg.get('performance', True)),
        'enable_security': bool(analyzer_cfg.get('security', True)),
        'enable_data_flow': bool(analyzer_cfg.get('data_flow', True)),
        'enable_control_flow': bool(analyzer_cfg.get('control_flow', True)),
    }
    return StaticAnalyzer(**params)


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description='NexusLang Static Analyzer - Catch bugs before runtime',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument(
        'path',
        help='File or directory to analyze'
    )
    
    parser.add_argument(
        '-r', '--recursive',
        action=argparse.BooleanOptionalAction,
        default=True,
        help='Recursively analyze directory (default: True)'
    )
    
    parser.add_argument(
        '--strict',
        action='store_true',
        help='Strict mode (enable all checks including style)'
    )
    
    parser.add_argument(
        '--minimal',
        action='store_true',
        help='Minimal mode (only critical checks: memory, null, init)'
    )
    
    parser.add_argument(
        '--json',
        action='store_true',
        help='Output in JSON format'
    )
    
    parser.add_argument(
        '--fix',
        action='store_true',
        help='Automatically fix issues where possible'
    )

    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Show what --fix would change without writing files'
    )

    parser.add_argument(
        '--no-color',
        action='store_true',
        help='Disable colored output'
    )
    
    parser.add_argument(
        '--errors-only',
        action='store_true',
        help='Show only errors (suppress warnings and info)'
    )
    
    parser.add_argument(
        '--max-issues',
        type=int,
        default=None,
        help='Maximum number of issues to display'
    )
    
    parser.add_argument(
        '--config',
        type=str,
        help='Path to JSON/TOML configuration file'
    )
    
    args = parser.parse_args()
    
    # Check path exists
    path = Path(args.path)
    if not path.exists():
        print(f"Error: Path not found: {args.path}", file=sys.stderr)
        return 1
    
    # Load optional config file.
    config: Dict[str, Any] = {}
    if args.config:
        try:
            config = _load_config(args.config)
        except Exception as exc:
            print(f"Error loading config: {exc}", file=sys.stderr)
            return 1

    analyzer = _create_analyzer_from_config(args, config)

    json_output = args.json or bool(config.get('json', False))
    fix_enabled = args.fix or bool(config.get('fix', False))
    dry_run_enabled = args.dry_run or bool(config.get('dry_run', False))
    no_color = args.no_color or bool(config.get('no_color', False))
    errors_only = args.errors_only or bool(config.get('errors_only', False))
    max_issues = args.max_issues if args.max_issues is not None else config.get('max_issues')
    
    # Analyze
    try:
        if path.is_file():
            reports = [analyzer.analyze_file(str(path))]
        else:
            reports = analyzer.analyze_directory(str(path), recursive=args.recursive)
    except Exception as e:
        print(f"Error during analysis: {e}", file=sys.stderr)
        return 1
    
    # Auto-fix pass (runs before output so the summary reflects remaining issues)
    if fix_enabled or dry_run_enabled:
        _apply_fixes(reports, dry_run=dry_run_enabled, use_colors=not no_color)

    # Output results
    use_colors = not no_color and sys.stdout.isatty()
    
    if json_output:
        output_json(reports)
    else:
        return output_text(reports, use_colors, errors_only, max_issues)


def _apply_fixes(
    reports: List[AnalysisReport],
    dry_run: bool = False,
    use_colors: bool = True,
) -> None:
    """
    Run the AutoFixer over every report and print a summary.
    In dry-run mode the modified source is printed instead of written.
    """
    fixer = AutoFixer()
    bold = '\033[1m' if use_colors else ''
    green = '\033[92m' if use_colors else ''
    yellow = '\033[93m' if use_colors else ''
    reset = '\033[0m' if use_colors else ''

    mode_label = "[dry-run] " if dry_run else ""
    total_applied = 0
    total_skipped = 0

    for report in reports:
        if not report.issues:
            continue
        try:
            result = fixer.apply_to_file(
                report.file_path, report.issues, dry_run=dry_run
            )
        except (OSError, IOError) as exc:
            print(f"{yellow}Warning: could not fix {report.file_path}: {exc}{reset}")
            continue

        if result.applied:
            print(f"{bold}{mode_label}Fixed {result.applied} issue(s) in "
                  f"{report.file_path}{reset}")
            for change in result.changes:
                print(f"  {green}+ {change}{reset}")
            if dry_run:
                print(f"  {yellow}(dry-run: file not written){reset}")
        total_applied += result.applied
        total_skipped += result.skipped

    if total_applied or total_skipped:
        print(f"\n{bold}Fix summary:{reset} applied={total_applied} "
              f"no-structured-fix={total_skipped}\n")


def output_text(reports: List[AnalysisReport], 
                use_colors: bool,
                errors_only: bool,
                max_issues: Optional[int]) -> int:
    """Output results in text format."""
    total_errors = 0
    total_warnings = 0
    total_issues = 0
    
    for report in reports:
        # Filter if errors only
        if errors_only:
            issues = report.filter(severity=Severity.ERROR)
        else:
            issues = report.issues
        
        # Skip files with no issues
        if not issues:
            continue
        
        # Print summary
        print(report.summary(use_colors))
        
        # Print issues
        if issues:
            print(report.format_all(
                show_source=True,
                use_colors=use_colors,
                max_issues=max_issues
            ))
            print()
        
        # Count
        counts = report.count_by_severity()
        total_errors += counts[Severity.ERROR]
        total_warnings += counts[Severity.WARNING]
        total_issues += len(issues)
    
    # Overall summary
    if len(reports) > 1:
        if use_colors:
            red = '\033[91m'
            yellow = '\033[93m'
            bold = '\033[1m'
            reset = '\033[0m'
        else:
            red = yellow = bold = reset = ''
        
        print(f"\n{bold}Overall Summary{reset}")
        print(f"{'=' * 60}")
        print(f"  Files analyzed: {len(reports)}")
        print(f"  {red}Total errors:   {total_errors}{reset}")
        print(f"  {yellow}Total warnings: {total_warnings}{reset}")
        print(f"  Total issues:   {total_issues}")
        print(f"{'=' * 60}\n")
    
    # Return exit code
    return 1 if total_errors > 0 else 0


def output_json(reports: List[AnalysisReport]):
    """Output results in JSON format."""
    output = {
        'files': [],
        'summary': {
            'total_files': len(reports),
            'total_errors': 0,
            'total_warnings': 0,
            'total_info': 0,
            'total_hints': 0,
        }
    }
    
    for report in reports:
        counts = report.count_by_severity()
        
        file_data = {
            'path': report.file_path,
            'lines_analyzed': report.lines_analyzed,
            'total_lines': report.total_lines,
            'analysis_time_ms': report.analysis_time_ms,
            'issues': [],
            'counts': {
                'errors': counts[Severity.ERROR],
                'warnings': counts[Severity.WARNING],
                'info': counts[Severity.INFO],
                'hints': counts[Severity.HINT],
            }
        }
        
        for issue in report.issues:
            issue_data = {
                'code': issue.code,
                'severity': issue.severity.value,
                'category': issue.category.value,
                'message': issue.message,
                'location': {
                    'file': issue.location.file,
                    'line': issue.location.line,
                    'column': issue.location.column,
                },
            }
            
            if issue.suggestion:
                issue_data['suggestion'] = issue.suggestion
            if issue.fix:
                issue_data['fix'] = issue.fix
            if issue.help_url:
                issue_data['help_url'] = issue.help_url
            
            file_data['issues'].append(issue_data)
        
        output['files'].append(file_data)
        
        # Update summary
        output['summary']['total_errors'] += counts[Severity.ERROR]
        output['summary']['total_warnings'] += counts[Severity.WARNING]
        output['summary']['total_info'] += counts[Severity.INFO]
        output['summary']['total_hints'] += counts[Severity.HINT]
    
    print(json.dumps(output, indent=2))


if __name__ == '__main__':
    sys.exit(main())
