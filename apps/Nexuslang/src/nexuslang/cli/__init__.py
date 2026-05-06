"""NexusLang command-line toolchain.

Subcommands
-----------
new       Create a new project in a new directory
init      Initialise a project in the current directory
build     Compile the project
run       Build and execute the project
check     Parse/type-check without producing output
clean     Remove the build output directory
test      Discover and run tests from tests/
coverage  Run a file with coverage collection and report
profile   Run a file with CPU/memory profiling and report
lint      Run the static analyser on the project
pgo       Profile-guided optimisation workflow (instrument/collect/build)
add       Add a dependency to nlpl.toml
remove    Remove a dependency from nexuslang.toml
lock      Regenerate nlpl.lock from the current nlpl.toml
deps      List all dependencies and their lock status
search    Search the package registry for packages
publish   Publish this package to the registry
workspace Manage multi-package workspaces (init/list/build/clean/test)
"""

from typing import Set
import os
import sys
import argparse
from pathlib import Path

from ..tooling.config import ConfigLoader
from ..tooling.builder import BuildSystem
from ..optimizer import int_to_opt_level

__all__ = ["main", "nlpllint"]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _load_project(manifest: str = "nexuslang.toml"):
    """Load project config or exit with a helpful message."""
    path = Path(manifest)
    if not path.exists():
        print(f"error: {manifest} not found. Are you in an NexusLang project root?")
        sys.exit(1)
    try:
        return ConfigLoader.load(manifest)
    except Exception as exc:
        print(f"error: failed to parse {manifest}: {exc}")
        sys.exit(1)


def _scaffold_project(root: Path, name: str) -> None:
    """Write initial nlpl.toml, src/main.nlpl, tests/.gitkeep, and .gitignore."""
    (root / "src").mkdir(parents=True, exist_ok=True)
    (root / "tests").mkdir(parents=True, exist_ok=True)

    toml_content = (
        f'[package]\n'
        f'name = "{name}"\n'
        f'version = "0.1.0"\n'
        f'authors = []\n'
        f'description = ""\n'
        f'\n'
        f'[build]\n'
        f'source_dir = "src"\n'
        f'output_dir = "build"\n'
        f'target = "c"\n'
        f'optimization = 0\n'
        f'headers = false\n'
        f'\n'
        f'[dependencies]\n'
        f'\n'
        f'[dev-dependencies]\n'
        f'\n'
        f'[features]\n'
        f'default = []\n'
    )
    (root / "nexuslang.toml").write_text(toml_content)

    main_content = (
        f'function main\n'
        f'    print text "Hello from {name}!"\n'
        f'end\n'
    )
    (root / "src" / "main.nxl").write_text(main_content)

    (root / "tests" / ".gitkeep").write_text("")

    gitignore_content = "/build/\nnlpl.lock\n*.o\n*.ll\n"
    gi = root / ".gitignore"
    if not gi.exists():
        gi.write_text(gitignore_content)


# ---------------------------------------------------------------------------
# Subcommand handlers
# ---------------------------------------------------------------------------

def cmd_new(args):
    """Create a new NexusLang project in a new directory."""
    name = args.name
    root = Path(name)
    if root.exists():
        print(f"error: directory '{name}' already exists.")
        sys.exit(1)
    root.mkdir(parents=True)
    _scaffold_project(root, name)
    print(f"     Created NexusLang project `{name}`")
    print(f"       Enter the project:  cd {name}")
    print(f"       Build:              nlpl build")
    print(f"       Run:                nlpl run")


def cmd_init(args):
    """Initialise a new NexusLang project in the current directory."""
    cwd = Path.cwd()
    if (cwd / "nexuslang.toml").exists():
        print("error: nlpl.toml already exists in the current directory.")
        sys.exit(1)
    name = args.name or cwd.name
    _scaffold_project(cwd, name)
    print(f"     Initialised NexusLang project `{name}` in {cwd}")


def cmd_build(args):
    """Compile the project."""
    config = _load_project()

    # --opt-level / -O overrides profile
    if hasattr(args, 'opt_level') and args.opt_level is not None:
        try:
            opt = int_to_opt_level(args.opt_level)
            config.profile.optimization = opt.value
        except (ValueError, KeyError):
            print(f"error: invalid optimisation level '{args.opt_level}'. Use 0, 1, 2, 3, or s.")
            sys.exit(1)

    if hasattr(args, 'lto') and args.lto:
        config.profile.lto = True

    if hasattr(args, 'pgo_use') and args.pgo_use:
        config.profile.pgo_profile = args.pgo_use

    builder = BuildSystem(config)
    features = args.features or []
    ok = builder.build(
        release=args.release,
        profile=args.profile,
        features=features,
        jobs=args.jobs,
        clean=args.clean,
        optimize_bounds_checks=args.optimize_bounds_checks,
        lint=args.lint,
        lint_strict=args.lint_strict,
        lint_errors_only=args.lint_errors_only,
        lint_fail_on_warnings=args.lint_fail_on_warnings,
    )
    sys.exit(0 if ok else 1)


def cmd_run(args):
    """Build and run the project executable."""
    config = _load_project()

    if hasattr(args, 'opt_level') and args.opt_level is not None:
        try:
            opt = int_to_opt_level(args.opt_level)
            config.profile.optimization = opt.value
        except (ValueError, KeyError):
            print(f"error: invalid optimisation level '{args.opt_level}'.")
            sys.exit(1)

    builder = BuildSystem(config)
    features = args.features or []
    run_args = args.run_args or []
    code = builder.run(
        release=args.release,
        features=features,
        run_args=run_args,
    )
    sys.exit(code)


def cmd_lint(args):
    """Run the static analyser on the project."""
    from ..tooling.analyzer.analyzer import StaticAnalyzer

    config = _load_project()
    src_dir = getattr(args, 'path', None) or 'src'
    analyzer = StaticAnalyzer(
        enable_all=True,
        enable_style=getattr(args, 'style', False),
        enable_security=not getattr(args, 'no_security', False),
        enable_performance=not getattr(args, 'no_performance', False),
        enable_data_flow=not getattr(args, 'no_data_flow', False),
    )
    reports = analyzer.analyze_directory(src_dir, recursive=True)
    total_errors = 0
    total_warnings = 0
    for report in reports:
        printed = report.format()
        if printed:
            print(printed)
        total_errors += sum(
            1 for i in report.issues
            if i.severity.value == 'error'
        )
        total_warnings += sum(
            1 for i in report.issues
            if i.severity.value == 'warning'
        )
    print(f"\nLint complete: {total_errors} errors, {total_warnings} warnings.")
    sys.exit(1 if total_errors > 0 else 0)


def cmd_pgo(args):
    """Profile-guided optimisation workflow."""
    from ..tooling.pgo import PGODriver, PGOConfig

    subcmd = getattr(args, 'pgo_command', None)
    if subcmd is None:
        print("error: specify a pgo subcommand: instrument, collect, or build")
        sys.exit(1)

    config = _load_project()
    profile_dir = getattr(args, 'profile_dir', 'build/profiles')
    pgo_cfg = PGOConfig(
        profile_dir=profile_dir,
        min_hot_count=getattr(args, 'min_hot_count', 100),
        merge_profiles=True,
        verbose=getattr(args, 'verbose', False),
    )
    driver = PGODriver(pgo_cfg)

    if subcmd == 'instrument':
        source = getattr(args, 'source', None)
        if not source:
            print("error: specify source file to instrument")
            sys.exit(1)
        print(f"Instrumenting {source} for PGO data collection...")
        print(f"Run your workload, then use: nlpl pgo collect --profile-dir {profile_dir}")

    elif subcmd == 'collect':
        print(f"Collecting PGO profiles from {profile_dir}...")
        merged_path = driver.save_merged_profile()
        driver.print_profile_summary()
        print(f"Merged profile written to: {merged_path}")

    elif subcmd == 'build':
        pgo_profile = getattr(args, 'pgo_profile', None)
        if not pgo_profile:
            # Auto-detect merged profile
            import os as _os
            candidate = _os.path.join(profile_dir, 'merged.json')
            if _os.path.exists(candidate):
                pgo_profile = candidate
            else:
                print(f"error: no PGO profile found. Run 'nlpl pgo collect' first, or pass --pgo-profile.")
                sys.exit(1)
        builder = BuildSystem(config)
        print(f"Building with PGO data from {pgo_profile}...")
        ok = builder.build(
            release=True,
            profile=None,
            features=[],
            jobs=None,
            clean=False,
            optimize_bounds_checks=False,
        )
        sys.exit(0 if ok else 1)

    else:
        print(f"error: unknown pgo subcommand '{subcmd}'")
        sys.exit(1)


def cmd_check(args):
    """Parse and type-check without producing output."""
    config = _load_project()
    builder = BuildSystem(config)
    features = args.features or []
    ok = builder.check(features=features)
    sys.exit(0 if ok else 1)


def cmd_clean(args):
    """Remove the build output directory and clear build cache."""
    config = _load_project()
    builder = BuildSystem(config)
    builder.clean()


def cmd_test(args):
    """Discover and run tests from the tests/ directory."""
    config = _load_project()
    builder = BuildSystem(config)
    features = args.features or []
    filter_names = args.filter or []
    code = builder.test(
        filter_names=filter_names if filter_names else None,
        release=args.release,
        features=features,
        coverage=getattr(args, 'coverage', False),
        coverage_output=getattr(args, 'coverage_output', None),
        jobs=getattr(args, 'jobs', None),
    )
    sys.exit(code)


def cmd_coverage(args):
    """Run a NexusLang file (or test suite) and generate a coverage report."""
    from ..tooling.coverage import run_with_coverage
    source = args.source
    output_dir = args.output or "coverage"
    report = run_with_coverage(
        source_path=source,
        output_dir=output_dir,
        report_json=True,
        report_html=True,
    )
    if report.total_pct() < (args.fail_under or 0):
        print(f"error: coverage {report.total_pct():.1f}% is below required {args.fail_under}%")
        sys.exit(1)


def cmd_profile(args):
    """Run a NexusLang file with CPU and memory profiling."""
    from ..tooling.profiler import run_with_profiling
    source = args.source
    output_dir = args.output or "profile"
    run_with_profiling(
        source_path=source,
        output_dir=output_dir,
        cpu=not args.no_cpu,
        memory=not args.no_memory,
    )


def cmd_add(args):
    """Add a dependency to nlpl.toml and update nlpl.lock."""
    from ..tooling.dependency_manager import add_dependency
    try:
        add_dependency(
            Path.cwd(),
            args.spec,
            dev=args.dev,
            path=args.path,
            git=args.git,
            branch=args.branch,
            tag=args.tag,
            rev=args.rev,
        )
    except Exception as exc:
        print(f"error: {exc}")
        sys.exit(1)


def cmd_remove(args):
    """Remove a dependency from nexuslang.toml and update nlpl.lock."""
    from ..tooling.dependency_manager import remove_dependency
    try:
        remove_dependency(Path.cwd(), args.name, dev=args.dev)
    except Exception as exc:
        print(f"error: {exc}")
        sys.exit(1)


def cmd_lock(args):
    """Regenerate nlpl.lock from the current nlpl.toml."""
    from ..tooling.dependency_manager import update_lockfile
    offline = getattr(args, "offline", False)
    try:
        update_lockfile(Path.cwd(), offline=offline)
    except Exception as exc:
        print(f"error: {exc}")
        sys.exit(1)


def cmd_deps(args):
    """List all dependencies and their lock status."""
    from ..tooling.dependency_manager import list_dependencies
    try:
        list_dependencies(Path.cwd())
    except Exception as exc:
        print(f"error: {exc}")
        sys.exit(1)


def cmd_search(args):
    """Search the package registry for packages."""
    from ..tooling.registry import RegistryConfig, RegistryClient, RegistryError

    config = RegistryConfig.from_project(Path.cwd())
    client = RegistryClient(config)

    try:
        results = client.search(args.query, limit=args.limit or 20)
    except RegistryError as exc:
        print(f"error: {exc}")
        sys.exit(1)

    if not results:
        print(f"No packages found matching '{args.query}'.")
        return

    print(f"Registry: {config.url}")
    print(f"Results for '{args.query}':\n")
    for r in results:
        dl = f"  ({r.downloads:,} downloads)" if r.downloads else ""
        desc = f"  {r.description}" if r.description else ""
        print(f"  {r.name} {r.version}{dl}")
        if desc:
            print(f"  {r.description}")
        print()


def cmd_publish(args):
    """Publish this package to the registry."""
    from ..tooling.registry import (
        RegistryConfig,
        RegistryClient,
        RegistryError,
        AuthError,
    )

    config = RegistryConfig.from_project(Path.cwd())
    client = RegistryClient(config)
    dry_run = getattr(args, "dry_run", False)

    if dry_run:
        print("  Dry run mode: package will not be uploaded.")

    try:
        client.publish(Path.cwd(), dry_run=dry_run)
    except AuthError as exc:
        print(f"error: {exc}")
        print("Hint: Set NLPL_REGISTRY_TOKEN or add token to nlpl.toml [registry].")
        sys.exit(1)
    except RegistryError as exc:
        print(f"error: {exc}")
        sys.exit(1)
    except (FileNotFoundError, ValueError) as exc:
        print(f"error: {exc}")
        sys.exit(1)


def cmd_workspace(args):
    """Dispatch workspace sub-subcommands."""
    from ..tooling.workspace import (
        WorkspaceManifest,
        WorkspaceResolver,
        WorkspaceBuilder,
        WorkspaceError,
        init_workspace,
        load_workspace,
    )

    subcmd = getattr(args, "ws_command", None)

    if subcmd is None or subcmd == "init":
        _ws_init(args, init_workspace)
    elif subcmd == "list":
        _ws_list(args, load_workspace)
    elif subcmd == "build":
        _ws_build(args, load_workspace)
    elif subcmd == "clean":
        _ws_clean(args, load_workspace)
    elif subcmd == "test":
        _ws_test(args, load_workspace)
    elif subcmd == "lock":
        _ws_lock(args, load_workspace)
    else:
        print(f"error: unknown workspace subcommand '{subcmd}'")
        print("Available: init, list, build, clean, test, lock")
        sys.exit(1)


def _ws_init(args, init_workspace):
    from ..tooling.workspace import WorkspaceError
    members = getattr(args, "members", None)
    description = getattr(args, "description", "") or ""
    try:
        init_workspace(
            Path.cwd(),
            members=members,
            description=description,
        )
    except FileExistsError as exc:
        print(f"error: {exc}")
        sys.exit(1)
    except WorkspaceError as exc:
        print(f"error: {exc}")
        sys.exit(1)


def _ws_list(args, load_workspace):
    from ..tooling.workspace import WorkspaceError
    try:
        manifest, resolver = load_workspace(Path.cwd())
    except (FileNotFoundError, WorkspaceError) as exc:
        print(f"error: {exc}")
        sys.exit(1)

    print(f"Workspace: {manifest.root}")
    if manifest.description:
        print(f"  {manifest.description}")
    print(f"\nMembers ({len(resolver.members)}):")
    for info in resolver.status():
        deps_str = ""
        if info["intra_deps"]:
            deps_str = f"  -> {', '.join(info['intra_deps'])}"
        print(f"  {info['name']} {info['version']}{deps_str}")
        if info["description"]:
            print(f"    {info['description']}")
    print(f"\nBuild order: {' -> '.join(resolver.build_order)}")


def _ws_build(args, load_workspace):
    from ..tooling.workspace import WorkspaceError
    try:
        manifest, resolver = load_workspace(Path.cwd())
    except (FileNotFoundError, WorkspaceError) as exc:
        print(f"error: {exc}")
        sys.exit(1)

    builder = WorkspaceBuilder(resolver)
    member_name = getattr(args, "member", None)
    features = getattr(args, "features", None) or []
    release = getattr(args, "release", False)
    jobs = getattr(args, "jobs", None)

    if member_name:
        try:
            ok = builder.build_member(
                member_name, release=release, features=features, jobs=jobs
            )
        except KeyError as exc:
            print(f"error: {exc}")
            sys.exit(1)
    else:
        ok = builder.build_all(release=release, features=features, jobs=jobs)
    sys.exit(0 if ok else 1)


def _ws_clean(args, load_workspace):
    from ..tooling.workspace import WorkspaceError
    try:
        manifest, resolver = load_workspace(Path.cwd())
    except (FileNotFoundError, WorkspaceError) as exc:
        print(f"error: {exc}")
        sys.exit(1)
    WorkspaceBuilder(resolver).clean_all()


def _ws_test(args, load_workspace):
    from ..tooling.workspace import WorkspaceError
    try:
        manifest, resolver = load_workspace(Path.cwd())
    except (FileNotFoundError, WorkspaceError) as exc:
        print(f"error: {exc}")
        sys.exit(1)

    filter_names = getattr(args, "filter", None) or []
    release = getattr(args, "release", False)
    features = getattr(args, "features", None) or []

    code = WorkspaceBuilder(resolver).test_all(
        filter_names=filter_names if filter_names else None,
        release=release,
        features=features,
    )
    sys.exit(code)


def _ws_lock(args, load_workspace):
    from ..tooling.workspace import WorkspaceError
    try:
        manifest, resolver = load_workspace(Path.cwd())
    except (FileNotFoundError, WorkspaceError) as exc:
        print(f"error: {exc}")
        sys.exit(1)
    try:
        resolver.regenerate_shared_lock()
    except WorkspaceError as exc:
        print(f"error: {exc}")
        sys.exit(1)


# ---------------------------------------------------------------------------
# Argument parser
# ---------------------------------------------------------------------------

def _add_new_subcommand(sub: argparse._SubParsersAction) -> None:
    p = sub.add_parser("new", help="Create a new project")
    p.add_argument("name", help="Project name (also the new directory name)")


def _add_init_subcommand(sub: argparse._SubParsersAction) -> None:
    p = sub.add_parser("init", help="Initialise a project in the current directory")
    p.add_argument("name", nargs="?", default=None,
                   help="Package name (defaults to current directory name)")


def _add_build_subcommand(sub: argparse._SubParsersAction) -> None:
    p = sub.add_parser("build", help="Compile the project")
    p.add_argument("--release", action="store_true",
                   help="Build with release profile (optimised, no debug info)")
    p.add_argument("--profile", metavar="PROFILE",
                   help="Use a named build profile (overrides --release)")
    p.add_argument("--features", nargs="+", metavar="FEATURE",
                   help="Enable additional named features")
    p.add_argument("--jobs", "-j", type=int, metavar="N",
                   help="Number of parallel compilation workers")
    p.add_argument("--clean", action="store_true",
                   help="Remove cached artefacts before building")
    p.add_argument("--optimize-bounds-checks", action="store_true",
                   help="Enable compile-time bounds check elimination")
    p.add_argument("--verbose", "-v", action="store_true",
                   help="Show verbose compiler output")
    p.add_argument("--quiet", "-q", action="store_true",
                   help="Suppress informational output")
    p.add_argument("--target", metavar="TRIPLE",
                   help="Target triple for cross-compilation (e.g. x86_64-linux-gnu)")
    p.add_argument("--opt-level", "-O", metavar="LEVEL",
                   help="Optimisation level: 0 (none), 1 (basic), 2 (standard), 3 (aggressive), s (size). Overrides profile.")
    p.add_argument("--lto", action="store_true",
                   help="Enable link-time optimisation (LTO)")
    p.add_argument("--pgo-use", metavar="PROFILE",
                   help="Path to a merged PGO profile to guide optimisation")
    p.add_argument("--lint", action=argparse.BooleanOptionalAction, default=None,
                   help="Run static analysis during build (overrides [build].lint_on_build)")
    p.add_argument("--lint-strict", action="store_true",
                   help="Use strict lint profile (includes style checks)")
    p.add_argument("--lint-errors-only", action="store_true",
                   help="Only report lint errors (suppress lint warnings)")
    p.add_argument("--lint-fail-on-warnings", action="store_true",
                   help="Treat lint warnings as build-breaking")


def _add_run_subcommand(sub: argparse._SubParsersAction) -> None:
    p = sub.add_parser("run", help="Build and run the project")
    p.add_argument("--release", action="store_true",
                   help="Build with release profile before running")
    p.add_argument("--features", nargs="+", metavar="FEATURE",
                   help="Enable additional named features")
    p.add_argument("run_args", nargs=argparse.REMAINDER, metavar="-- [ARGS...]",
                   help="Arguments to pass to the compiled program (after --)")
    p.add_argument("--opt-level", "-O", metavar="LEVEL",
                   help="Optimisation level: 0, 1, 2, 3, or s (size).")


def _add_lint_subcommand(sub: argparse._SubParsersAction) -> None:
    p = sub.add_parser("lint", help="Run static analysis on the project")
    p.add_argument("path", nargs="?", default="src", metavar="PATH",
                   help="Source directory to analyse (default: src/)")
    p.add_argument("--style", action="store_true",
                   help="Enable style checks")
    p.add_argument("--no-security", action="store_true",
                   help="Disable security checks")
    p.add_argument("--no-performance", action="store_true",
                   help="Disable performance checks")
    p.add_argument("--no-data-flow", action="store_true",
                   help="Disable data-flow checks")


def _add_pgo_subcommand(sub: argparse._SubParsersAction) -> None:
    p = sub.add_parser("pgo", help="Profile-guided optimisation workflow")
    pgo_sub = p.add_subparsers(dest="pgo_command", metavar="<subcommand>")

    p_inst = pgo_sub.add_parser("instrument",
                                help="Build an instrumented binary that emits profile data")
    p_inst.add_argument("source", nargs="?", metavar="FILE",
                        help="Source file to instrument (default: project entry-point)")
    p_inst.add_argument("--profile-dir", metavar="DIR", default="build/profiles",
                        help="Directory to write raw profile data (default: build/profiles)")

    p_coll = pgo_sub.add_parser("collect",
                                help="Merge raw profile files into a single optimisation profile")
    p_coll.add_argument("--profile-dir", metavar="DIR", default="build/profiles",
                        help="Directory containing raw profiles (default: build/profiles)")
    p_coll.add_argument("--min-hot-count", type=int, default=100, metavar="N",
                        help="Call-count threshold for 'hot' functions (default: 100)")
    p_coll.add_argument("--verbose", "-v", action="store_true")

    p_build = pgo_sub.add_parser("build", help="Build with PGO optimisation data")
    p_build.add_argument("--pgo-profile", metavar="FILE",
                         help="Path to merged profile (default: build/profiles/merged.json)")
    p_build.add_argument("--profile-dir", metavar="DIR", default="build/profiles")


def _add_check_subcommand(sub: argparse._SubParsersAction) -> None:
    p = sub.add_parser("check", help="Type-check without producing output")
    p.add_argument("--features", nargs="+", metavar="FEATURE",
                   help="Enable additional named features")


def _add_test_subcommand(sub: argparse._SubParsersAction) -> None:
    p = sub.add_parser("test", help="Run tests from the tests/ directory")
    p.add_argument("filter", nargs="*", metavar="NAME",
                   help="Only run tests whose filename contains this string")
    p.add_argument("--release", action="store_true",
                   help="Build tests with release profile")
    p.add_argument("--features", nargs="+", metavar="FEATURE",
                   help="Enable additional named features")
    p.add_argument("--coverage", action="store_true",
                   help="Collect line coverage and write report to coverage/")
    p.add_argument("--coverage-output", metavar="DIR",
                   help="Directory to write coverage report (default: coverage/)")
    p.add_argument("--jobs", "-j", type=int, metavar="N",
                   help="Number of parallel test jobs (default: CPU count)")


def _add_coverage_subcommand(sub: argparse._SubParsersAction) -> None:
    p = sub.add_parser("coverage", help="Run a file with coverage collection")
    p.add_argument("source", metavar="FILE", help="NLPL source file to run")
    p.add_argument("--output", "-o", metavar="DIR",
                   help="Output directory for coverage report (default: coverage/)")
    p.add_argument("--fail-under", type=float, metavar="PCT",
                   help="Exit 1 if coverage is below this percentage")


def _add_profile_subcommand(sub: argparse._SubParsersAction) -> None:
    p = sub.add_parser("profile", help="Run a file with CPU/memory profiling")
    p.add_argument("source", metavar="FILE", help="NLPL source file to profile")
    p.add_argument("--output", "-o", metavar="DIR",
                   help="Output directory for profile report (default: profile/)")
    p.add_argument("--no-cpu", action="store_true",
                   help="Disable CPU profiling")
    p.add_argument("--no-memory", action="store_true",
                   help="Disable memory profiling")


def _add_add_subcommand(sub: argparse._SubParsersAction) -> None:
    p = sub.add_parser("add", help="Add a dependency")
    p.add_argument("spec", metavar="PACKAGE[@VERSION]",
                   help="Package name with optional version constraint (e.g. mylib@^1.2)")
    p.add_argument("--dev", action="store_true",
                   help="Add as a dev-dependency")
    p.add_argument("--path", metavar="PATH",
                   help="Local filesystem path to an NexusLang project")
    p.add_argument("--git", metavar="URL",
                   help="Git repository URL")
    p.add_argument("--branch", metavar="BRANCH",
                   help="Git branch (used with --git)")
    p.add_argument("--tag", metavar="TAG",
                   help="Git tag (used with --git)")
    p.add_argument("--rev", metavar="SHA",
                   help="Exact git commit SHA (used with --git)")


def _add_remove_subcommand(sub: argparse._SubParsersAction) -> None:
    p = sub.add_parser("remove", help="Remove a dependency")
    p.add_argument("name", help="Package name to remove")
    p.add_argument("--dev", action="store_true",
                   help="Remove from dev-dependencies")


def _add_lock_subcommand(sub: argparse._SubParsersAction) -> None:
    p = sub.add_parser("lock", help="Regenerate nlpl.lock from nexuslang.toml")
    p.add_argument("--offline", action="store_true",
                   help="Skip registry network calls (use cached/fallback data)")


def _add_search_subcommand(sub: argparse._SubParsersAction) -> None:
    p = sub.add_parser("search", help="Search the package registry")
    p.add_argument("query", help="Search term")
    p.add_argument("--limit", "-n", type=int, default=20, metavar="N",
                   help="Maximum number of results (default: 20)")


def _add_publish_subcommand(sub: argparse._SubParsersAction) -> None:
    p = sub.add_parser("publish", help="Publish this package to the registry")
    p.add_argument("--dry-run", action="store_true",
                   help="Create archive and validate metadata but do not upload")


def _add_workspace_subcommand(sub: argparse._SubParsersAction) -> None:
    p = sub.add_parser("workspace", help="Manage multi-package workspaces",
                       aliases=["ws"])
    ws_sub = p.add_subparsers(dest="ws_command", metavar="<subcommand>")

    p_init = ws_sub.add_parser("init", help="Create a workspace manifest in the current directory")
    p_init.add_argument("--members", nargs="+", metavar="GLOB",
                        help="Member glob patterns (default: packages/*)")
    p_init.add_argument("--description", metavar="TEXT",
                        help="Optional workspace description")

    ws_sub.add_parser("list", help="List all workspace members and build order")

    p_build = ws_sub.add_parser("build", help="Build all (or a single) workspace member")
    p_build.add_argument("member", nargs="?", metavar="NAME",
                         help="Build only this member (default: build all)")
    p_build.add_argument("--release", action="store_true",
                         help="Build with release profile")
    p_build.add_argument("--features", nargs="+", metavar="FEATURE",
                         help="Enable named features")
    p_build.add_argument("--jobs", "-j", type=int, metavar="N",
                         help="Parallel compilation workers")

    ws_sub.add_parser("clean", help="Clean all workspace members")

    p_test = ws_sub.add_parser("test", help="Run tests across all workspace members")
    p_test.add_argument("filter", nargs="*", metavar="NAME",
                        help="Only run tests whose filename contains this string")
    p_test.add_argument("--release", action="store_true",
                        help="Run tests with release profile")
    p_test.add_argument("--features", nargs="+", metavar="FEATURE",
                        help="Enable named features")

    ws_sub.add_parser("lock", help="Regenerate the shared workspace nlpl.lock")


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="nexuslang",
        description="NLPL Compiler & Toolchain",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = parser.add_subparsers(dest="command", metavar="<command>")

    _add_new_subcommand(sub)
    _add_init_subcommand(sub)
    _add_build_subcommand(sub)
    _add_run_subcommand(sub)
    _add_lint_subcommand(sub)
    _add_pgo_subcommand(sub)
    _add_check_subcommand(sub)
    sub.add_parser("clean", help="Remove build output directory and cache")
    _add_test_subcommand(sub)
    _add_coverage_subcommand(sub)
    _add_profile_subcommand(sub)
    _add_add_subcommand(sub)
    _add_remove_subcommand(sub)
    _add_lock_subcommand(sub)
    sub.add_parser("deps", help="List dependencies and lock status")
    _add_search_subcommand(sub)
    _add_publish_subcommand(sub)
    _add_workspace_subcommand(sub)

    return parser


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = _build_parser()
    # Strip the '--' separator before REMAINDER args so argparse handles them cleanly
    argv = sys.argv[1:]
    if "run" in argv:
        run_idx = argv.index("run")
        if "--" in argv[run_idx:]:
            sep = argv.index("--", run_idx)
            argv = argv[:sep] + argv[sep + 1:]

    args = parser.parse_args(argv)

    dispatch = {
        "new":       cmd_new,
        "init":      cmd_init,
        "build":     cmd_build,
        "run":       cmd_run,
        "check":     cmd_check,
        "clean":     cmd_clean,
        "test":      cmd_test,
        "add":       cmd_add,
        "remove":    cmd_remove,
        "lock":      cmd_lock,
        "deps":      cmd_deps,
        "search":    cmd_search,
        "publish":   cmd_publish,
        "workspace": cmd_workspace,
        "ws":        cmd_workspace,  # Short alias
        "coverage":  cmd_coverage,
        "profile":   cmd_profile,
        "lint":      cmd_lint,
        "pgo":       cmd_pgo,
    }

    handler = dispatch.get(args.command)
    if handler is None:
        parser.print_help()
        sys.exit(0)

    handler(args)


if __name__ == "__main__":
    main()
