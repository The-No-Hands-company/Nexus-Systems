"""Build script support for NexusLang projects.

When a project contains a ``build.nlpl`` file (or has ``build_script = "..."``
set in its ``[build]`` toml section) this module executes that script before
the main compilation begins — identical in spirit to Cargo's ``build.rs``.

Environment variables available inside the build script
--------------------------------------------------------
NLPL_MANIFEST_DIR   Absolute path to the directory containing nlpl.toml.
OUT_DIR             Absolute path to the build output directory.
PROFILE             Active build profile name ("dev", "release", or custom).
NLPL_PKG_NAME       Package name from nexuslang.toml.
NLPL_PKG_VERSION    Package version from nexuslang.toml.
OPT_LEVEL           Optimisation level as a string ("0" through "3").
DEBUG               "true" if debug_info is enabled, "false" otherwise.
NUM_JOBS            Max number of parallel compilation jobs (as a string).

Directives emitted by the build script
---------------------------------------
The build script communicates back to the build system by writing directive
lines to *stdout*.  Every line that does NOT start with ``nlpl:`` is treated
as plain output and printed with a ``[build script]`` prefix.

  nlpl:rerun-if-changed=PATH
      Re-run the build script if PATH (relative to NLPL_MANIFEST_DIR or
      absolute) changes between builds.  May appear multiple times.

  nlpl:rerun-if-env-changed=VAR
      Re-run the build script if the environment variable VAR changes.
      May appear multiple times.

  nlpl:cfg=FLAG
      Inject FLAG into the active feature-flag set for the *main* compilation
      step.  May appear multiple times.

  nlpl:warning=TEXT
      Emit a build warning attached to the current compilation.

  nlpl:error=TEXT
      Immediately fail the build with TEXT as the error message.  The main
      compilation step is skipped.

Caching
-------
To avoid redundant executions the runner persists a small JSON state file at
``{out_dir}/.build_script_state.json``.  The script is re-run only when:

  1. The script file itself has changed (SHA-256 content hash).
  2. Any path listed via ``nlpl:rerun-if-changed`` has changed.
  3. Any environment variable listed via ``nlpl:rerun-if-env-changed`` has
     changed.
  4. ``--clean`` was requested (``force=True`` is passed by the builder).
"""

import hashlib
import json
import os
import subprocess
import sys
import textwrap
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple


# ---------------------------------------------------------------------------
# Data-transfer objects
# ---------------------------------------------------------------------------

@dataclass
class BuildScriptDirectives:
    """Directives emitted by a build script via stdout."""

    rerun_if_changed: List[str] = field(default_factory=list)
    rerun_if_env_changed: List[str] = field(default_factory=list)
    cfg_flags: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    errors: List[str] = field(default_factory=list)


@dataclass
class BuildScriptResult:
    """Result of executing a build script."""

    success: bool
    directives: BuildScriptDirectives = field(default_factory=BuildScriptDirectives)
    output_lines: List[str] = field(default_factory=list)
    stderr: str = ""
    elapsed: float = 0.0


# ---------------------------------------------------------------------------
# Persistent run-state cache
# ---------------------------------------------------------------------------

class BuildScriptCache:
    """Persists build-script run-state in ``{out_dir}/.build_script_state.json``.

    Prevents redundant re-runs when nothing relevant has changed.
    """

    VERSION = 1

    def __init__(self, cache_path: Path) -> None:
        self._path = cache_path
        self._data: Dict = {}
        self._load()

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _load(self) -> None:
        if not self._path.exists():
            return
        try:
            with open(self._path, "r", encoding="utf-8") as f:
                data = json.load(f)
            if data.get("version") == self.VERSION:
                self._data = data
        except Exception:
            self._data = {}

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def needs_rerun(self, script_path: str) -> bool:
        """Return ``True`` if the build script must be executed again."""
        if not self._data:
            return True

        # 1. Script file content changed
        try:
            current_hash = _file_sha256(script_path)
        except OSError:
            return True
        if current_hash != self._data.get("script_hash", ""):
            return True

        # 2. rerun-if-changed paths changed or disappeared
        for path, saved_hash in self._data.get("rerun_if_changed", {}).items():
            try:
                if _file_sha256(path) != saved_hash:
                    return True
            except OSError:
                return True

        # 3. rerun-if-env-changed variables changed
        for var, saved_val in self._data.get("rerun_if_env_changed", {}).items():
            if os.environ.get(var, "") != saved_val:
                return True

        return False

    def save(
        self,
        script_hash: str,
        directives: BuildScriptDirectives,
    ) -> None:
        """Persist the state from a successful run so future calls can skip re-runs."""
        changed_hashes: Dict[str, str] = {}
        for p in directives.rerun_if_changed:
            try:
                changed_hashes[p] = _file_sha256(p)
            except OSError:
                changed_hashes[p] = ""

        env_values: Dict[str, str] = {}
        for var in directives.rerun_if_env_changed:
            env_values[var] = os.environ.get(var, "")

        self._data = {
            "version": self.VERSION,
            "script_hash": script_hash,
            "rerun_if_changed": changed_hashes,
            "rerun_if_env_changed": env_values,
        }
        self._path.parent.mkdir(parents=True, exist_ok=True)
        with open(self._path, "w", encoding="utf-8") as f:
            json.dump(self._data, f, indent=2)

    def clear(self) -> None:
        """Discard all cached state (triggered by ``--clean``)."""
        self._data = {}
        if self._path.exists():
            self._path.unlink(missing_ok=True)


# ---------------------------------------------------------------------------
# Pure-function helpers
# ---------------------------------------------------------------------------

def _file_sha256(path: str) -> str:
    """Return the SHA-256 hex digest of a file's contents."""
    sha = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            sha.update(chunk)
    return sha.hexdigest()


def _parse_directives(stdout: str) -> Tuple[BuildScriptDirectives, List[str]]:
    """Split *stdout* into directives (prefixed ``nlpl:``) and plain output lines.

    Returns:
        (directives, plain_lines) where *plain_lines* contains every line that
        is NOT a build-system directive.
    """
    directives = BuildScriptDirectives()
    plain_lines: List[str] = []

    for line in stdout.splitlines():
        if line.startswith("nlpl:rerun-if-changed="):
            directives.rerun_if_changed.append(
                line[len("nlpl:rerun-if-changed="):]
            )
        elif line.startswith("nlpl:rerun-if-env-changed="):
            directives.rerun_if_env_changed.append(
                line[len("nlpl:rerun-if-env-changed="):]
            )
        elif line.startswith("nlpl:cfg="):
            flag = line[len("nlpl:cfg="):].strip()
            if flag:
                directives.cfg_flags.append(flag)
        elif line.startswith("nlpl:warning="):
            directives.warnings.append(line[len("nlpl:warning="):])
        elif line.startswith("nlpl:error="):
            directives.errors.append(line[len("nlpl:error="):])
        else:
            plain_lines.append(line)

    return directives, plain_lines


# ---------------------------------------------------------------------------
# Main entry point
# ---------------------------------------------------------------------------

def run_build_script(
    script_path: str,
    manifest_dir: str,
    out_dir: str,
    pkg_name: str,
    pkg_version: str,
    profile_name: str,
    opt_level: int,
    debug_info: bool,
    jobs: int,
    force: bool = False,
) -> BuildScriptResult:
    """Execute *script_path* as a pre-build hook.

    The script is run via a subprocess so its side-effects (global state,
    ``sys.exit()`` calls, etc.) cannot corrupt the builder's own process.

    Args:
        script_path:   Absolute path to the build script (e.g. ``build.nlpl``).
        manifest_dir:  Project root — the directory containing ``nlpl.toml``.
        out_dir:       Build output directory (absolute path).
        pkg_name:      Package name from ``nlpl.toml``.
        pkg_version:   Package version from ``nlpl.toml``.
        profile_name:  Active build profile ("dev", "release", or custom).
        opt_level:     Optimisation level 0-3.
        debug_info:    Whether debug information is enabled.
        jobs:          Maximum parallel compilation jobs.
        force:         Always run the script, ignoring cached state (``--clean``).

    Returns:
        :class:`BuildScriptResult` describing success, directives, and output.
    """
    cache_path = Path(out_dir) / ".build_script_state.json"
    cache = BuildScriptCache(cache_path)

    if not force and not cache.needs_rerun(script_path):
        return BuildScriptResult(
            success=True,
            output_lines=["(build script cached — nothing changed)"],
        )

    t0 = time.monotonic()

    # ------------------------------------------------------------------
    # Environment for the child process
    # ------------------------------------------------------------------
    env = os.environ.copy()
    env["NLPL_MANIFEST_DIR"] = str(manifest_dir)
    env["OUT_DIR"] = str(out_dir)
    env["PROFILE"] = profile_name
    env["NLPL_PKG_NAME"] = pkg_name
    env["NLPL_PKG_VERSION"] = pkg_version
    env["OPT_LEVEL"] = str(opt_level)
    env["DEBUG"] = "true" if debug_info else "false"
    env["NUM_JOBS"] = str(jobs)

    # ------------------------------------------------------------------
    # Locate the NexusLang interpreter source path
    # ------------------------------------------------------------------
    try:
        import nexuslang.main as _nxl_entry
        nxl_src_path = str(Path(_nxl_entry.__file__).parent.parent)
    except Exception:
        nxl_src_path = os.path.join(manifest_dir, "src")

    # ------------------------------------------------------------------
    # Inline Python bootstrap that drives the NexusLang interpreter
    # ------------------------------------------------------------------
    bootstrap = textwrap.dedent(f"""\
        import sys, os
        sys.path.insert(0, {nxl_src_path!r})
        from nexuslang.main import run_program
        script = sys.argv[1]
        with open(script, "r", encoding="utf-8") as _f:
            _src = _f.read()
        run_program(_src, file_path=script)
    """)

    # ------------------------------------------------------------------
    # Execute
    # ------------------------------------------------------------------
    try:
        proc = subprocess.run(
            [sys.executable, "-c", bootstrap, script_path],
            env=env,
            cwd=manifest_dir,
            capture_output=True,
            text=True,
            timeout=300,
        )
    except subprocess.TimeoutExpired:
        return BuildScriptResult(
            success=False,
            directives=BuildScriptDirectives(
                errors=["Build script timed out after 300 seconds"]
            ),
            elapsed=time.monotonic() - t0,
        )
    except OSError as exc:
        return BuildScriptResult(
            success=False,
            directives=BuildScriptDirectives(
                errors=[f"Failed to start build script: {exc}"]
            ),
            elapsed=time.monotonic() - t0,
        )

    # ------------------------------------------------------------------
    # Parse directives
    # ------------------------------------------------------------------
    directives, plain_lines = _parse_directives(proc.stdout)

    if proc.returncode != 0 and not directives.errors:
        msg = proc.stderr.strip() or f"Build script exited with code {proc.returncode}"
        directives.errors.append(msg)

    success = proc.returncode == 0 and not directives.errors

    if success:
        try:
            script_hash = _file_sha256(script_path)
            cache.save(script_hash, directives)
        except OSError:
            pass

    return BuildScriptResult(
        success=success,
        directives=directives,
        output_lines=plain_lines,
        stderr=proc.stderr,
        elapsed=time.monotonic() - t0,
    )
