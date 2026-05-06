"""Minimal Builder used by build module integration tests."""

from __future__ import annotations

import shutil
from pathlib import Path
from typing import Optional

from .project import Project


class Builder:
    """Project builder facade.

    Current test coverage validates job-selection logic and compiler discovery
    plumbing only.
    """

    def __init__(self, project: Project, jobs: Optional[int] = None):
        self.project = project
        project_jobs = project.build.parallel_jobs
        self._jobs = jobs if jobs is not None else project_jobs
        self.compiler = self._find_compiler()

    def _find_compiler(self) -> Path:
        for candidate in ("nexuslangc", "nxlc", "nlplc"):
            resolved = shutil.which(candidate)
            if resolved:
                return Path(resolved)
        raise FileNotFoundError("Could not find NexusLang compiler in PATH")


__all__ = ["Builder"]
