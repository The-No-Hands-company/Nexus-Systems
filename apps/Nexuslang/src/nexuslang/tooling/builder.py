
import os
import shutil
import glob
import time
import hashlib
import json
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import List, Optional, Dict, Tuple
from dataclasses import dataclass, field

from .config import ProjectConfig, ProfileConfig
from .build_script import BuildScriptResult, run_build_script
from ..compiler import Compiler, CompilerOptions, CompilationTarget
from ..build.lto import LTOConfig, LTOLinker, LTOMode, LTOResult
from ..parser.lexer import Lexer
from ..parser.parser import Parser


# ---------------------------------------------------------------------------
# Build cache (file-level incremental compilation)
# ---------------------------------------------------------------------------

@dataclass
class _CacheEntry:
    path: str
    mtime: float
    size: int
    content_hash: str


class _BuildCache:
    """Lightweight build cache backed by a JSON file in the build output dir."""

    VERSION = 1

    def __init__(self, cache_path: Path) -> None:
        self._path = cache_path
        self._entries: Dict[str, _CacheEntry] = {}
        self._load()

    def _load(self) -> None:
        if not self._path.exists():
            return
        try:
            with open(self._path, "r", encoding="utf-8") as f:
                data = json.load(f)
            if data.get("version") != self.VERSION:
                return
            for e in data.get("entries", []):
                self._entries[e["path"]] = _CacheEntry(**e)
        except Exception:
            self._entries = {}

    def save(self) -> None:
        self._path.parent.mkdir(parents=True, exist_ok=True)
        data = {
            "version": self.VERSION,
            "entries": [vars(e) for e in self._entries.values()],
        }
        with open(self._path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)

    @staticmethod
    def _file_hash(path: str) -> str:
        sha = hashlib.sha256()
        with open(path, "rb") as f:
            while chunk := f.read(65536):
                sha.update(chunk)
        return sha.hexdigest()

    def needs_rebuild(self, source_path: str) -> bool:
        """Return True if *source_path* has changed since it was last cached."""
        if source_path not in self._entries:
            return True
        try:
            st = os.stat(source_path)
        except OSError:
            return True
        e = self._entries[source_path]
        if st.st_mtime != e.mtime or st.st_size != e.size:
            current_hash = self._file_hash(source_path)
            if current_hash != e.content_hash:
                return True
        return False

    def mark_built(self, source_path: str) -> None:
        """Record that *source_path* was successfully compiled."""
        try:
            st = os.stat(source_path)
            self._entries[source_path] = _CacheEntry(
                path=source_path,
                mtime=st.st_mtime,
                size=st.st_size,
                content_hash=self._file_hash(source_path),
            )
        except OSError:
            pass

    def clear(self) -> None:
        self._entries = {}
        if self._path.exists():
            self._path.unlink()


# ---------------------------------------------------------------------------
# Build result
# ---------------------------------------------------------------------------

@dataclass
class BuildResult:
    """Outcome of a build invocation."""
    success: bool
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    files_compiled: int = 0
    files_cached: int = 0
    elapsed: float = 0.0
    # LTO fields (populated when prof.lto=True and target=llvm_ir)
    lto_output: Optional[Path] = None
    lto_skipped_reason: Optional[str] = None

    def print_summary(self, project_name: str, profile: str) -> None:
        status = "Finished" if self.success else "Failed"
        cached_note = f" ({self.files_cached} cached)" if self.files_cached else ""
        lto_note = ""
        if self.lto_output:
            lto_note = f", LTO -> {self.lto_output.name}"
        elif self.lto_skipped_reason:
            lto_note = f" [LTO skipped: {self.lto_skipped_reason}]"
        print(
            f"  {status} [{profile}] {project_name!r} in {self.elapsed:.2f}s"
            f" — {self.files_compiled} compiled{cached_note}{lto_note}"
        )
        for err in self.errors:
            print(f"  error: {err}")
        for warn in self.warnings:
            print(f"  warning: {warn}")


# ---------------------------------------------------------------------------
# Main builder
# ---------------------------------------------------------------------------

class BuildSystem:
    """Orchestrates the build process based on project configuration.

    Supports:
    - Incremental builds (file-level caching via SHA-256 hashes)
    - Parallel compilation (one thread per source file)
    - Release / dev profiles
    - Feature flags
    """

    def __init__(self, config: ProjectConfig) -> None:
        self.config = config

    # ------------------------------------------------------------------
    # Public commands
    # ------------------------------------------------------------------

    def build(
        self,
        *,
        release: bool = False,
        profile: Optional[str] = None,
        features: Optional[List[str]] = None,
        jobs: Optional[int] = None,
        clean: bool = False,
        check_only: bool = False,
        optimize_bounds_checks: bool = False,
        lint: Optional[bool] = None,
        lint_strict: bool = False,
        lint_errors_only: bool = False,
        lint_fail_on_warnings: bool = False,
    ) -> bool:
        """Build the project.

        Args:
            release:                Build with ``release`` profile (O3, no debug).
            profile:                Explicit profile name (overrides *release*).
            features:               Extra feature flags to enable.
            jobs:                   Max parallel workers. None = CPU count.
            clean:                  Discard cache and rebuild everything.
            check_only:             Parse and type-check without emitting output.
            optimize_bounds_checks: Enable compile-time bounds check elimination.

        Returns:
            True if all files compiled successfully.
        """
        result = self._build_internal(
            release=release,
            profile=profile,
            features=features,
            jobs=jobs,
            clean=clean,
            check_only=check_only,
            optimize_bounds_checks=optimize_bounds_checks,
            lint=lint,
            lint_strict=lint_strict,
            lint_errors_only=lint_errors_only,
            lint_fail_on_warnings=lint_fail_on_warnings,
        )
        result.print_summary(
            self.config.package.name,
            profile or ("release" if release else self.config.build.profile),
        )
        return result.success

    def run(
        self,
        *,
        release: bool = False,
        features: Optional[List[str]] = None,
        run_args: Optional[List[str]] = None,
    ) -> int:
        """Build then run the project executable.

        Returns:
            Exit code of the executed program (1 if build failed).
        """
        if not self.build(release=release, features=features):
            return 1

        executable = self._find_executable(self.config.build.output_dir)
        if not executable:
            print(f"  error: no executable found in {self.config.build.output_dir}")
            return 1

        import subprocess
        cmd = [executable] + (run_args or [])
        print(f"\nRunning `{' '.join(cmd)}`")
        print("-" * 40)
        try:
            return subprocess.run(cmd).returncode
        except Exception as exc:
            print(f"  error running executable: {exc}")
            return 1

    def check(
        self,
        *,
        features: Optional[List[str]] = None,
    ) -> bool:
        """Parse and type-check without producing compiled output (fast path).

        Returns:
            True if no errors were found.
        """
        return self.build(check_only=True, features=features)

    def clean(self) -> None:
        """Remove build output directory and clear the build cache."""
        out_dir = self.config.build.output_dir
        if os.path.exists(out_dir):
            shutil.rmtree(out_dir)
            print(f"  Removed {out_dir}/")
        cache_path = self._cache_path()
        if cache_path.exists():
            cache_path.unlink()
        print("  Done")

    def test(
        self,
        *,
        filter_names: Optional[List[str]] = None,
        release: bool = False,
        features: Optional[List[str]] = None,
        coverage: bool = False,
        coverage_output: Optional[str] = None,
        parallel: bool = True,
        jobs: Optional[int] = None,
    ) -> int:
        """Discover and run test files from the ``tests/`` directory.

        Args:
            filter_names:     If provided, only run tests whose filename
                              contains one of these strings.
            release:          Build tests with release profile.
            features:         Extra feature flags for test compilation.
            coverage:         Collect and report line coverage.
            coverage_output:  Directory to write coverage HTML/JSON.
                              Defaults to ``coverage/``.
            parallel:         Run tests in parallel (default True).
            jobs:             Max parallel test jobs (default: CPU count).

        Returns:
            0 if all tests passed, 1 otherwise.
        """
        import time as _time

        test_dir_path = Path(self.config.build.source_dir).parent / "tests"
        if not test_dir_path.exists():
            print("  No tests/ directory found.")
            return 0

        test_files = sorted(test_dir_path.rglob("*.nxl"))
        if filter_names:
            test_files = [
                f for f in test_files
                if any(name in f.name for name in filter_names)
            ]

        if not test_files:
            print("  No test files found.")
            return 0

        # Coverage setup
        collector = None
        if coverage:
            from .coverage import CoverageCollector
            collector = CoverageCollector()
            collector.start()

        print(f"\nRunning {len(test_files)} test file(s)...\n")
        t_start = _time.perf_counter()

        results: List[Tuple[str, bool, float]] = []  # (label, ok, duration)

        max_workers = jobs or min(len(test_files), os.cpu_count() or 4)

        if parallel and max_workers > 1:
            with ThreadPoolExecutor(max_workers=max_workers) as exe:
                future_to_file = {
                    exe.submit(
                        self._run_single_test,
                        test_file,
                        release=release,
                        features=features,
                    ): test_file
                    for test_file in test_files
                }
                for future in as_completed(future_to_file):
                    tf = future_to_file[future]
                    t0 = _time.perf_counter()
                    try:
                        ok = future.result()
                    except Exception:
                        ok = False
                    dur = _time.perf_counter() - t0
                    results.append((tf.stem, ok, dur))
        else:
            for test_file in test_files:
                t0 = _time.perf_counter()
                ok = self._run_single_test(
                    test_file, release=release, features=features
                )
                dur = _time.perf_counter() - t0
                results.append((test_file.stem, ok, dur))

        passed = 0
        failed = 0
        failed_names: List[str] = []
        for label, ok, dur in sorted(results, key=lambda r: r[0]):
            if ok:
                passed += 1
                print(f"  test {label:<50s} ... ok      ({dur:.2f}s)")
            else:
                failed += 1
                failed_names.append(label)
                print(f"  test {label:<50s} ... FAILED  ({dur:.2f}s)")

        total_time = _time.perf_counter() - t_start
        status_word = "ok" if failed == 0 else "FAILED"
        print(
            f"\ntest result: {status_word}. "
            f"{passed} passed; {failed} failed; "
            f"total time: {total_time:.2f}s"
        )
        if failed_names:
            print("\nFailed tests:")
            for name in failed_names:
                print(f"  - {name}")
            print()

        # Coverage report
        if collector:
            collector.stop()
            out_dir = coverage_output or "coverage"
            from .coverage import CoverageReport
            source_paths = [str(f) for f in test_files]
            report = collector.build_report(source_paths=source_paths)
            print(report.summary())
            report.write_json(os.path.join(out_dir, "coverage.json"))
            report.write_html(out_dir)
            print(f"Coverage report written to: {out_dir}/index.html")

        return 0 if failed == 0 else 1

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _cache_path(self) -> Path:
        out_dir = Path(self.config.build.output_dir)
        return out_dir / ".build_cache.json"

    def _resolve_profile(
        self, release: bool, profile: Optional[str]
    ) -> ProfileConfig:
        if profile:
            return self.config.get_profile(profile)
        if release:
            return self.config.get_profile("release")
        return self.config.get_profile(self.config.build.profile)

    def _configure_compiler(
        self,
        prof: ProfileConfig,
        active_features: List[str],
        optimize_bounds_checks: bool = False,
    ) -> Compiler:
        """Create and configure a fresh Compiler instance."""
        from ..compiler import create_compiler
        compiler = create_compiler(
            optimization_level=prof.optimization,
            debug=prof.debug_info,
        )
        compiler.options.optimization_level = prof.optimization
        compiler.options.generate_header = self.config.build.headers
        if optimize_bounds_checks:
            # Signal to backends that bounds-check elimination is requested
            compiler.options.optimization_level = max(compiler.options.optimization_level, 1)

        # Add dependency library search paths
        all_deps = {**self.config.dependencies, **self.config.build_dependencies}
        for dep_name, dep_spec in all_deps.items():
            if isinstance(dep_spec, dict) and "path" in dep_spec:
                lib_path = os.path.join(dep_spec["path"], "build", "lib")
                if os.path.exists(lib_path):
                    compiler.options.library_search_paths.append(lib_path)

        return compiler

    def _compile_one(
        self, source_path: str, out_dir: str, prof: ProfileConfig,
        active_features: List[str], check_only: bool, optimize_bounds_checks: bool
    ) -> Tuple[str, bool, List[str]]:
        """Compile a single source file.  Designed to run in a thread.

        Returns:
            (source_path, success, list_of_error_lines)
        """
        errors: List[str] = []
        try:
            with open(source_path, "r", encoding="utf-8") as f:
                code = f.read()

            lexer = Lexer(code)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            ast = parser.parse()

            if check_only:
                # Parse-only: no backend emission
                return source_path, True, []

            compiler = self._configure_compiler(
                prof, active_features, optimize_bounds_checks
            )
            rel = os.path.relpath(source_path, self.config.build.source_dir)
            base = os.path.splitext(rel)[0]
            output_file = os.path.join(out_dir, base)

            ok, _ = compiler.compile(ast, self.config.build.target, output_file)
            if not ok:
                errors.append(f"Compiler reported failure for {source_path}")
            return source_path, ok, errors

        except Exception as exc:
            return source_path, False, [f"{source_path}: {exc}"]

    def _compile_files(
        self,
        to_compile: List[str],
        out_dir: str,
        prof,
        active_features: List[str],
        check_only: bool,
        optimize_bounds_checks: bool,
        max_workers: int,
        cache,
    ):
        """Compile *to_compile* files serially or in parallel.

        Updates *cache* with successfully compiled files.

        Returns:
            (compiled_ok, all_errors, all_warnings)
        """
        all_errors: List[str] = []
        all_warnings: List[str] = []
        compiled_ok = 0

        if max_workers == 1:
            for src in to_compile:
                _, ok, errs = self._compile_one(
                    src, out_dir, prof, active_features, check_only, optimize_bounds_checks
                )
                if ok:
                    compiled_ok += 1
                    cache.mark_built(src)
                else:
                    all_errors.extend(errs)
        else:
            with ThreadPoolExecutor(max_workers=max_workers) as executor:
                futures = {
                    executor.submit(
                        self._compile_one,
                        src, out_dir, prof, active_features, check_only, optimize_bounds_checks,
                    ): src
                    for src in to_compile
                }
                for future in as_completed(futures):
                    src_path, ok, errs = future.result()
                    if ok:
                        compiled_ok += 1
                        cache.mark_built(src_path)
                    else:
                        all_errors.extend(errs)

        return compiled_ok, all_errors, all_warnings

    def _build_internal(
        self,
        release: bool,
        profile: Optional[str],
        features: Optional[List[str]],
        jobs: Optional[int],
        clean: bool,
        check_only: bool,
        optimize_bounds_checks: bool,
        lint: Optional[bool] = None,
        lint_strict: bool = False,
        lint_errors_only: bool = False,
        lint_fail_on_warnings: bool = False,
    ) -> BuildResult:
        t0 = time.monotonic()

        prof = self._resolve_profile(release, profile)
        profile_name = profile or ("release" if release else self.config.build.profile)

        # Merge features: manifest defaults + cli-supplied
        cfg_features = self.config.build.features[:]
        if features:
            cfg_features.extend(features)
        active_features = list(dict.fromkeys(cfg_features))  # deduplicate, preserve order

        src_dir = self.config.build.source_dir
        out_dir = self.config.build.output_dir
        os.makedirs(out_dir, exist_ok=True)

        # ------------------------------------------------------------------
        # Pre-build hook (build.nxl)
        # ------------------------------------------------------------------
        _resolved_jobs = jobs or self.config.build.jobs or os.cpu_count() or 1
        script_result = self._run_build_script_if_present(
            prof, profile_name, out_dir, _resolved_jobs, clean
        )
        script_warnings: List[str] = []
        if script_result is not None:
            for line in script_result.output_lines:
                if line.strip():
                    print(f"  [build script] {line}")
            for w in script_result.directives.warnings:
                msg = f"[build script] {w}"
                script_warnings.append(msg)
                print(f"  warning: {msg}")
            if not script_result.success:
                return BuildResult(
                    success=False,
                    errors=[f"[build script] {e}" for e in script_result.directives.errors],
                    warnings=script_warnings,
                    elapsed=time.monotonic() - t0,
                )
            for flag in script_result.directives.cfg_flags:
                if flag not in active_features:
                    active_features.append(flag)

        # Build cache
        cache = _BuildCache(self._cache_path())
        if clean:
            cache.clear()

        # Discover source files
        sources = glob.glob(os.path.join(src_dir, "**/*.nxl"), recursive=True)
        if not sources:
            return BuildResult(
                success=False,
                errors=[f"No source files found in {src_dir}"],
                elapsed=time.monotonic() - t0,
            )

        # ------------------------------------------------------------------
        # Optional static-analysis (nlpllint) integration
        # ------------------------------------------------------------------
        lint_enabled = self.config.build.lint_on_build if lint is None else lint
        effective_lint_strict = self.config.build.lint_strict or lint_strict
        effective_lint_errors_only = self.config.build.lint_errors_only or lint_errors_only
        effective_lint_fail_on_warnings = (
            self.config.build.lint_fail_on_warnings or lint_fail_on_warnings
        )

        if lint_enabled:
            print("  Running static analysis (nlpllint)...")
            lint_errors, lint_warnings = self._run_static_analysis(
                src_dir,
                strict=effective_lint_strict,
                errors_only=effective_lint_errors_only,
            )
            if lint_errors:
                return BuildResult(
                    success=False,
                    errors=lint_errors,
                    warnings=script_warnings + lint_warnings,
                    elapsed=time.monotonic() - t0,
                )
            if effective_lint_fail_on_warnings and lint_warnings:
                promoted = [f"[lint] warning treated as error: {w}" for w in lint_warnings]
                return BuildResult(
                    success=False,
                    errors=promoted,
                    warnings=script_warnings + lint_warnings,
                    elapsed=time.monotonic() - t0,
                )
            script_warnings.extend(lint_warnings)

        # Determine which files need (re)compilation
        to_compile = [s for s in sources if cache.needs_rebuild(s)] if not clean else sources
        cached_count = len(sources) - len(to_compile)

        if not to_compile:
            print(f"  Nothing to compile (all {len(sources)} file(s) up-to-date).")
            return BuildResult(
                success=True,
                files_compiled=0,
                files_cached=cached_count,
                elapsed=time.monotonic() - t0,
            )

        # Determine parallelism
        max_workers = jobs or self.config.build.jobs or os.cpu_count() or 1
        max_workers = min(max_workers, len(to_compile))

        print(
            f"  Compiling {self.config.package.name} v{self.config.package.version} "
            f"[{profile_name}] "
            f"({len(to_compile)} file{'s' if len(to_compile) != 1 else ''}"
            f"{', ' + str(cached_count) + ' cached' if cached_count else ''})"
        )

        all_errors: List[str] = []
        all_warnings: List[str] = []

        compiled_ok, compile_errors, _compile_warnings = self._compile_files(
            to_compile, out_dir, prof, active_features, check_only, optimize_bounds_checks,
            max_workers, cache,
        )
        all_errors.extend(compile_errors)

        # Persist updated cache entries
        cache.save()

        # LTO: after successful compilation, run whole-program optimization
        lto_output: Optional[Path] = None
        lto_skip_reason: Optional[str] = None
        if not check_only and not all_errors:
            ran_lto, lto_output, lto_skip_reason, lto_errors, lto_warnings = \
                self._run_lto_if_enabled(prof, out_dir, self.config.package.name)
            if ran_lto:
                all_errors.extend(lto_errors)
                all_warnings.extend(lto_warnings)
                if lto_errors:
                    lto_output = None

        # Link multi-file C/C++ projects when more than one file compiled
        if (
            not check_only
            and not all_errors
            and self.config.build.target in ("c", "cpp", CompilationTarget.C, CompilationTarget.CPP)
            and len(sources) > 1
        ):
            self._maybe_link(out_dir, all_errors)

        return BuildResult(
            success=len(all_errors) == 0,
            errors=all_errors,
            warnings=script_warnings + all_warnings,
            files_compiled=compiled_ok,
            files_cached=cached_count,
            elapsed=time.monotonic() - t0,
            lto_output=lto_output,
            lto_skipped_reason=lto_skip_reason,
        )

    def _run_static_analysis(
        self,
        src_dir: str,
        *,
        strict: bool,
        errors_only: bool,
    ) -> Tuple[List[str], List[str]]:
        """Run static analyzer over source tree and return (errors, warnings)."""
        from ..tooling.analyzer.analyzer import create_default_analyzer, create_strict_analyzer

        if not os.path.isdir(src_dir):
            return [f"[lint] Source directory not found: {src_dir}"], []

        analyzer = create_strict_analyzer() if strict else create_default_analyzer()
        reports = analyzer.analyze_directory(src_dir, recursive=True)

        lint_errors: List[str] = []
        lint_warnings: List[str] = []

        for report in reports:
            for issue in report.issues:
                severity = getattr(issue.severity, "value", str(issue.severity)).lower()
                if errors_only and severity != "error":
                    continue

                file_path = getattr(issue.location, "file", report.file_path)
                line = getattr(issue.location, "line", 1)
                column = getattr(issue.location, "column", 1)
                rendered = (
                    f"[lint] {file_path}:{line}:{column} "
                    f"{issue.code} {issue.message}"
                )

                if severity == "error":
                    lint_errors.append(rendered)
                elif severity == "warning":
                    lint_warnings.append(rendered)

        return lint_errors, lint_warnings

    def _run_build_script_if_present(
        self,
        prof: ProfileConfig,
        profile_name: str,
        out_dir: str,
        jobs: int,
        clean: bool,
    ) -> Optional[BuildScriptResult]:
        """Locate and run the build script for this project, if any.

        Returns:
            :class:`BuildScriptResult` if a script was found and executed,
            ``None`` if there is no build script to run.
        """
        cfg_script = self.config.build.build_script

        # Explicitly disabled
        if cfg_script == "":
            return None

        # Resolve the manifest directory (project root)
        manifest_dir = self.config.manifest_dir or os.getcwd()

        if cfg_script is None:
            # Auto-detect build.nlpl in the project root
            candidate = os.path.join(manifest_dir, "build.nxl")
            if not os.path.isfile(candidate):
                return None
            script_path = candidate
        else:
            # Explicit path — may be relative to manifest_dir or absolute
            script_path = (
                cfg_script if os.path.isabs(cfg_script)
                else os.path.join(manifest_dir, cfg_script)
            )
            if not os.path.isfile(script_path):
                # Configured script missing — hard error
                from dataclasses import field as _field
                from .build_script import BuildScriptDirectives
                return BuildScriptResult(
                    success=False,
                    directives=BuildScriptDirectives(
                        errors=[f"Build script not found: {script_path}"]
                    ),
                )

        print(f"  Running build script {os.path.relpath(script_path, manifest_dir)}")
        result = run_build_script(
            script_path=script_path,
            manifest_dir=manifest_dir,
            out_dir=os.path.abspath(out_dir),
            pkg_name=self.config.package.name,
            pkg_version=self.config.package.version,
            profile_name=profile_name,
            opt_level=prof.optimization,
            debug_info=prof.debug_info,
            jobs=jobs,
            force=clean,
        )
        return result

    # ------------------------------------------------------------------
    # LTO integration
    # ------------------------------------------------------------------

    def _run_lto_if_enabled(
        self,
        prof: ProfileConfig,
        out_dir: str,
        pkg_name: str,
    ) -> Tuple[bool, Optional[Path], Optional[str], List[str], List[str]]:
        """Run LTO pipeline when the profile has lto=True and target is llvm_ir.

        Returns:
            (ran_lto, lto_output_path, skip_reason, extra_errors, extra_warnings)
        """
        if not prof.lto:
            return False, None, None, [], []

        if self.config.build.target not in ("llvm_ir", CompilationTarget.LLVM_IR):
            reason = (
                f"LTO requested but target is '{self.config.build.target}'; "
                "LTO requires target='llvm_ir'"
            )
            return False, None, reason, [], []

        # Collect all emitted .ll bitcode files
        ll_files = [
            Path(p)
            for p in glob.glob(os.path.join(out_dir, "**/*.ll"), recursive=True)
        ]
        if not ll_files:
            return False, None, "no .ll bitcode files found after compilation", [], []

        # Choose ThinLTO for opt<3, Full LTO for O3
        mode = LTOMode.FULL if prof.optimization >= 3 else LTOMode.THIN
        lto_cfg = LTOConfig(
            mode=mode,
            opt_level=prof.optimization,
            strip_debug=prof.strip,
            internalize=True,
        )
        linker = LTOLinker(lto_cfg)
        lto_out = Path(out_dir) / pkg_name
        result: LTOResult = linker.link_with_lto(
            bitcode_files=ll_files,
            output=lto_out,
            work_dir=Path(out_dir),
        )
        if result.success:
            print(
                f"  LTO ({mode.value}): linked {len(ll_files)} module(s) "
                f"-> {lto_out.name}"
            )
        return True, result.output_file, None, result.errors, result.warnings

    def _maybe_link(self, out_dir: str, errors: List[str]) -> None:
        """Attempt to link compiled C/C++ objects into a single executable."""
        import subprocess

        target = self.config.build.target
        compilers = (
            ["g++", "clang++"] if target in ("cpp", CompilationTarget.CPP)
            else ["gcc", "clang"]
        )
        cc = next((shutil.which(c) for c in compilers if shutil.which(c)), None)
        if not cc:
            return  # No host C compiler available

        object_files = glob.glob(os.path.join(out_dir, "**/*.c"), recursive=True)
        object_files += glob.glob(os.path.join(out_dir, "**/*.cpp"), recursive=True)
        if not object_files:
            return

        final = os.path.join(out_dir, self.config.package.name)
        cmd = [cc] + object_files + ["-o", final]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            errors.append(f"Linking failed:\n{result.stderr.strip()}")

    def _run_single_test(
        self,
        test_file: Path,
        release: bool,
        features: Optional[List[str]],
    ) -> bool:
        """Compile and run a single test file.  Returns True if it exits 0."""
        import subprocess
        import tempfile

        prof = self._resolve_profile(release, None)
        active_features = self.config.build.features[:] + (features or [])
        out_dir = Path(self.config.build.output_dir) / "test_bins"
        out_dir.mkdir(parents=True, exist_ok=True)
        out_file = str(out_dir / test_file.stem)

        _, ok, _ = self._compile_one(
            str(test_file), str(out_dir), prof, active_features,
            check_only=False, optimize_bounds_checks=False
        )
        if not ok:
            return False

        try:
            result = subprocess.run(
                [out_file], capture_output=True, timeout=30
            )
            return result.returncode == 0
        except (FileNotFoundError, subprocess.TimeoutExpired):
            return False

    def _find_executable(self, out_dir: str) -> Optional[str]:
        """Find the project executable in the build output directory."""
        # Strategy 1: package name
        exe = os.path.join(out_dir, self.config.package.name)
        if os.path.isfile(exe) and os.access(exe, os.X_OK):
            return exe

        # Strategy 2: 'main'
        exe = os.path.join(out_dir, "main")
        if os.path.isfile(exe) and os.access(exe, os.X_OK):
            return exe

        # Strategy 3: any executable
        for f in glob.glob(os.path.join(out_dir, "*")):
            if (os.path.isfile(f) and os.access(f, os.X_OK)
                    and not f.endswith((".o", ".c", ".cpp", ".h", ".nxl", ".ll"))):
                return f

        return None


