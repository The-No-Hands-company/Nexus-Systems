"""Simple project model used by build-system tests."""

from __future__ import annotations

import tomllib
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

import tomli_w


@dataclass
class BuildConfig:
    source_dir: str = "src"
    output_dir: str = "build"
    target: str = "c"
    parallel_jobs: int = 0
    lto: str = "disabled"
    sysroot: Optional[str] = None


@dataclass
class Project:
    root: Path
    name: str
    version: str = "0.1.0"
    build: BuildConfig = field(default_factory=BuildConfig)

    @classmethod
    def init(cls, root: Path, name: str) -> "Project":
        root = Path(root)
        root.mkdir(parents=True, exist_ok=True)
        (root / "src").mkdir(parents=True, exist_ok=True)
        (root / "build").mkdir(parents=True, exist_ok=True)
        main_file = root / "src" / "main.nxl"
        if not main_file.exists():
            main_file.write_text(f'print text "Hello from {name}"\n', encoding="utf-8")
        return cls(root=root, name=name)

    @classmethod
    def from_toml(cls, path: Path) -> "Project":
        path = Path(path)
        with open(path, "rb") as handle:
            data = tomllib.load(handle)

        package = data.get("package", {})
        build_data = data.get("build", {})

        project = cls(
            root=path.parent,
            name=package.get("name", "unnamed"),
            version=package.get("version", "0.1.0"),
            build=BuildConfig(
                source_dir=build_data.get("source_dir", "src"),
                output_dir=build_data.get("output_dir", "build"),
                target=build_data.get("target", "c"),
                parallel_jobs=build_data.get("parallel_jobs", 0),
                lto=build_data.get("lto", "disabled"),
                sysroot=build_data.get("sysroot"),
            ),
        )
        return project

    def to_toml(self, path: Path) -> None:
        path = Path(path)
        build_section = {
            "source_dir": self.build.source_dir,
            "output_dir": self.build.output_dir,
            "target": self.build.target,
            "parallel_jobs": self.build.parallel_jobs,
            "lto": self.build.lto,
        }
        if self.build.sysroot is not None:
            build_section["sysroot"] = self.build.sysroot

        payload = {
            "package": {
                "name": self.name,
                "version": self.version,
            },
            "build": build_section,
        }
        with open(path, "wb") as handle:
            tomli_w.dump(payload, handle)


__all__ = ["BuildConfig", "Project"]
