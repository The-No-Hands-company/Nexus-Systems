import pytest

from nexuslang.tooling.workspace import (
    WorkspaceBuilder,
    WorkspaceError,
    WorkspaceManifest,
    WorkspaceMember,
    WorkspaceResolver,
)


def _resolved_resolver(tmp_path):
    manifest = WorkspaceManifest(root=tmp_path)
    resolver = WorkspaceResolver(manifest)
    member = WorkspaceMember(root=tmp_path, name="app", version="0.1.0")
    resolver.members = {"app": member}
    resolver.build_order = ["app"]
    resolver._resolved = True
    return resolver


def test_regenerate_shared_lock_wraps_recoverable_member_lock_errors(monkeypatch, tmp_path):
    resolver = _resolved_resolver(tmp_path)

    def _boom(_root):
        raise ValueError("bad lock generation")

    monkeypatch.setattr("nexuslang.tooling.lockfile.generate_lockfile", _boom)

    with pytest.raises(WorkspaceError, match="Failed to generate lockfile for member 'app'"):
        resolver.regenerate_shared_lock(quiet=True)


def test_build_all_handles_recoverable_builder_setup_errors(monkeypatch, tmp_path):
    resolver = _resolved_resolver(tmp_path)
    builder = WorkspaceBuilder(resolver)

    def _boom(_member):
        raise ValueError("invalid member config")

    monkeypatch.setattr(builder, "_build_system_for", _boom)

    assert builder.build_all(quiet=True) is False


def test_build_all_propagates_non_recoverable_interrupt(monkeypatch, tmp_path):
    resolver = _resolved_resolver(tmp_path)
    builder = WorkspaceBuilder(resolver)

    def _interrupt(_member):
        raise KeyboardInterrupt()

    monkeypatch.setattr(builder, "_build_system_for", _interrupt)

    with pytest.raises(KeyboardInterrupt):
        builder.build_all(quiet=True)


def test_clean_all_handles_recoverable_member_clean_errors(monkeypatch, tmp_path):
    resolver = _resolved_resolver(tmp_path)
    builder = WorkspaceBuilder(resolver)

    class _BadBuildSystem:
        def clean(self):
            raise RuntimeError("clean failed")

    monkeypatch.setattr(builder, "_build_system_for", lambda _member: _BadBuildSystem())

    # Should not raise for recoverable clean errors.
    builder.clean_all(quiet=True)


def test_test_all_handles_recoverable_member_test_setup_errors(monkeypatch, tmp_path):
    resolver = _resolved_resolver(tmp_path)
    builder = WorkspaceBuilder(resolver)

    def _boom(_member):
        raise TypeError("bad test setup")

    monkeypatch.setattr(builder, "_build_system_for", _boom)

    assert builder.test_all(quiet=True) == 1
