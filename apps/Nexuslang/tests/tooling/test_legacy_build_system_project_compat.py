"""Regression tests for legacy build_system project and resolver compatibility."""

from pathlib import Path
from types import SimpleNamespace

import pytest

from nexuslang.build_system.dependency_resolver import DependencyGraph, DependencyResolver, ResolvedDependency
from nexuslang.build_system.project import Dependency, Project, ProjectConfig
import nexuslang.build_system.project as legacy_project_module


def test_legacy_project_config_loads_targets_from_maintained_manifest_shapes(tmp_path):
    (tmp_path / "src").mkdir()
    (tmp_path / "src" / "main.nxl").write_text('print text "hi"\n')
    (tmp_path / "src" / "lib.nxl").write_text('print text "lib"\n')
    (tmp_path / "nexuslang.toml").write_text(
        """
[package]
name = "compat-app"
version = "0.1.0"

[build]
source_dir = "src"
build_dir = "legacy-build"
output_dir = "dist"

[dependencies]
local-lib = { path = "../local-lib" }

[[bin]]
name = "compat-app"
path = "src/main.nxl"

[lib]
name = "compat-lib"
path = "src/lib.nxl"
""".strip()
    )

    config = ProjectConfig.from_toml(tmp_path / "nexuslang.toml")

    assert config.name == "compat-app"
    assert config.build_dir == "legacy-build"
    assert config.output_dir == "dist"
    assert config.dependencies["local-lib"].source == "path"
    assert config.targets["compat-app"].source == "src/main.nxl"
    assert config.targets["compat-lib"].target_type == "library"


def test_legacy_project_dependency_commands_delegate_to_maintained_manager(monkeypatch, tmp_path):
    project = Project.init(tmp_path, "compat-app")
    calls = []

    monkeypatch.setattr(
        "nexuslang.build_system.project.maintained_dependency_manager.add_dependency",
        lambda root, package_spec, **kwargs: calls.append(("add", root, package_spec, kwargs)),
    )
    monkeypatch.setattr(
        "nexuslang.build_system.project.maintained_dependency_manager.remove_dependency",
        lambda root, package_name, dev=False: calls.append(("remove", root, package_name, {"dev": dev})),
    )
    monkeypatch.setattr(
        "nexuslang.build_system.project.maintained_dependency_manager.update_lockfile",
        lambda root, offline=False: calls.append(("lock", root, None, {"offline": offline})),
    )
    monkeypatch.setattr(
        "nexuslang.build_system.project.maintained_dependency_manager.list_dependencies",
        lambda root: calls.append(("list", root, None, {})),
    )
    monkeypatch.setattr(
        "nexuslang.build_system.project.ProjectConfig.from_toml",
        lambda path: project.config,
    )

    project.add_dependency("demo@^1.0")
    project.remove_dependency("demo", dev=True)
    project.update_lockfile(offline=True)
    project.list_dependencies()

    assert calls == [
        ("add", project.root_path, "demo@^1.0", {}),
        ("remove", project.root_path, "demo", {"dev": True}),
        ("lock", project.root_path, None, {"offline": True}),
        ("list", project.root_path, None, {}),
    ]


def test_legacy_project_config_propagates_malformed_build_overlay_errors(tmp_path):
    (tmp_path / "nexuslang.toml").write_text(
        """
[package]
name = "compat-app"
version = "0.1.0"

[build]
optimization = "3"
""".strip()
    )

    with pytest.raises(ValueError, match="build.optimization"):
        ProjectConfig.from_toml(tmp_path / "nexuslang.toml")


def test_legacy_project_config_propagates_malformed_profile_errors(tmp_path):
    (tmp_path / "nexuslang.toml").write_text(
        """
[package]
name = "compat-app"
version = "0.1.0"

[profile.release]
panic = "ignore"
""".strip()
    )

    with pytest.raises(ValueError, match="panic"):
        ProjectConfig.from_toml(tmp_path / "nexuslang.toml")


def test_legacy_project_config_delegates_to_maintained_loader_once(monkeypatch, tmp_path):
    manifest_path = tmp_path / "nexuslang.toml"
    manifest_path.write_text(
        """
[package]
name = "compat-app"
version = "0.1.0"
""".strip()
    )

    calls = []
    original_loader = legacy_project_module.ConfigLoader.load

    def traced_loader(path):
        calls.append(path)
        return original_loader(path)

    monkeypatch.setattr(legacy_project_module.ConfigLoader, "load", traced_loader)

    config = ProjectConfig.from_toml(manifest_path)
    assert config.name == "compat-app"
    assert len(calls) == 1


def test_legacy_dependency_resolver_delegates_path_resolution_to_maintained_layer(monkeypatch, tmp_path):
    resolver = DependencyResolver(project_root=tmp_path)
    captured = {}

    def fake_resolve_path(name, spec, project_root):
        captured["name"] = name
        captured["spec"] = spec
        captured["project_root"] = project_root
        return SimpleNamespace(
            name=name,
            version="0.4.0",
            source="path",
            resolved_path=str((tmp_path / "dep").resolve()),
            git_url=None,
            git_commit=None,
        )

    monkeypatch.setattr(
        "nexuslang.build_system.dependency_resolver.maintained_resolve_path_dependency",
        fake_resolve_path,
    )

    graph = resolver.resolve({"dep": Dependency(name="dep", version="*", source="path", path="../dep")})

    assert captured == {
        "name": "dep",
        "spec": {"version": "*", "path": "../dep"},
        "project_root": tmp_path,
    }
    assert graph.nodes["dep"].version == "0.4.0"
    assert graph.nodes["dep"].path == (tmp_path / "dep").resolve()


def test_legacy_dependency_resolver_delegates_registry_resolution_to_maintained_layer(monkeypatch, tmp_path):
    resolver = DependencyResolver(project_root=tmp_path)
    captured = {}

    def fake_resolve_registry(name, version_req, project_root):
        captured["args"] = (name, version_req, project_root)
        return SimpleNamespace(
            name=name,
            version="1.2.3",
            source="registry",
            resolved_path=None,
            git_url=None,
            git_commit=None,
        )

    monkeypatch.setattr(
        "nexuslang.build_system.dependency_resolver.maintained_resolve_registry_dependency",
        fake_resolve_registry,
    )

    graph = resolver.resolve({"demo": Dependency(name="demo", version="^1.0")})

    assert captured["args"] == ("demo", "^1.0", tmp_path)
    assert graph.nodes["demo"].version == "1.2.3"


def test_legacy_dependency_download_non_path_fails_fast(tmp_path):
    resolver = DependencyResolver(project_root=tmp_path)
    graph = DependencyGraph()
    graph.add_dependency(ResolvedDependency(name="dep", version="1.0.0", source="registry"))

    with pytest.raises(NotImplementedError, match="download_dependencies"):
        resolver.download_dependencies(graph, tmp_path / "install")