"""
Unit tests for NexusLang Build System manifest parser.
"""

import pytest
import tempfile
import os
from pathlib import Path
from textwrap import dedent

# Import manifest module
import sys
sys.path.insert(0, str(Path(__file__).parent.parent / 'src'))

from nexuslang.build.manifest import (
    Manifest, load_manifest, Dependency, BuildProfile, 
    PanicStrategy, CrateType, PackageMetadata
)


@pytest.fixture
def temp_project():
    """Create temporary project directory."""
    with tempfile.TemporaryDirectory() as tmpdir:
        project_dir = Path(tmpdir)
        yield project_dir


def write_manifest(project_dir: Path, content: str):
    """Write nlpl.toml to project directory."""
    manifest_path = project_dir / 'nexuslang.toml'
    manifest_path.write_text(dedent(content))
    return manifest_path


class TestBasicManifest:
    """Test basic manifest parsing."""
    
    def test_minimal_manifest(self, temp_project):
        """Test minimal valid manifest."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test-project"
            version = "0.1.0"
        """)
        
        manifest = Manifest(manifest_path)
        assert manifest.package.name == "test-project"
        assert manifest.package.version == "0.1.0"
        assert manifest.package.edition == "2026"
    
    def test_full_package_metadata(self, temp_project):
        """Test complete package metadata."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "awesome-lib"
            version = "1.2.3"
            authors = ["Alice <alice@example.com>", "Bob <bob@example.com>"]
            license = "MIT OR Apache-2.0"
            description = "An awesome library"
            repository = "https://github.com/user/awesome-lib"
            homepage = "https://awesome-lib.dev"
            documentation = "https://docs.awesome-lib.dev"
            readme = "README.md"
            keywords = ["graphics", "3d", "rendering"]
            categories = ["game-dev", "graphics"]
            edition = "2026"
        """)
        
        manifest = Manifest(manifest_path)
        pkg = manifest.package
        assert pkg.name == "awesome-lib"
        assert pkg.version == "1.2.3"
        assert len(pkg.authors) == 2
        assert pkg.license == "MIT OR Apache-2.0"
        assert pkg.description == "An awesome library"
        assert pkg.repository == "https://github.com/user/awesome-lib"
        assert len(pkg.keywords) == 3
        assert "graphics" in pkg.keywords
    
    def test_missing_required_fields(self, temp_project):
        """Test error on missing required fields."""
        # Missing version
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test"
        """)
        
        with pytest.raises(ValueError, match="Missing required field 'version'"):
            Manifest(manifest_path)
    
    def test_invalid_package_name(self, temp_project):
        """Test error on invalid package name."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "Invalid-Name-With-Caps"
            version = "1.0.0"
        """)
        
        with pytest.raises(ValueError, match="Invalid package name"):
            Manifest(manifest_path)
    
    def test_invalid_version(self, temp_project):
        """Test error on invalid version."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test"
            version = "not-a-version"
        """)
        
        with pytest.raises(ValueError, match="Invalid version"):
            Manifest(manifest_path)


class TestDependencies:
    """Test dependency parsing."""
    
    def test_simple_dependencies(self, temp_project):
        """Test simple version dependencies."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test"
            version = "0.1.0"
            
            [dependencies]
            nlpl-math = "1.0"
            nlpl-string = "2.3.4"
        """)
        
        manifest = Manifest(manifest_path)
        assert len(manifest.dependencies) == 2
        assert "nlpl-math" in manifest.dependencies
        assert manifest.dependencies["nlpl-math"].version_req == "1.0"
        assert manifest.dependencies["nlpl-string"].version_req == "2.3.4"
    
    def test_path_dependencies(self, temp_project):
        """Test local path dependencies."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test"
            version = "0.1.0"
            
            [dependencies]
            local-lib = { path = "../local-lib" }
        """)
        
        manifest = Manifest(manifest_path)
        dep = manifest.dependencies["local-lib"]
        assert dep.path == "../local-lib"
    
    def test_git_dependencies(self, temp_project):
        """Test git dependencies."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test"
            version = "0.1.0"
            
            [dependencies]
            experimental = { git = "https://github.com/user/repo", branch = "main" }
            stable = { git = "https://github.com/user/stable", tag = "v1.0.0" }
        """)
        
        manifest = Manifest(manifest_path)
        exp = manifest.dependencies["experimental"]
        assert exp.git == "https://github.com/user/repo"
        assert exp.branch == "main"
        
        stable = manifest.dependencies["stable"]
        assert stable.tag == "v1.0.0"
    
    def test_features_in_dependencies(self, temp_project):
        """Test dependency features."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test"
            version = "0.1.0"
            
            [dependencies]
            graphics = { version = "1.0", features = ["opengl", "vulkan"] }
            minimal = { version = "2.0", default-features = false }
        """)
        
        manifest = Manifest(manifest_path)
        graphics = manifest.dependencies["graphics"]
        assert graphics.features == ["opengl", "vulkan"]
        assert graphics.default_features == True
        
        minimal = manifest.dependencies["minimal"]
        assert minimal.default_features == False
    
    def test_dev_dependencies(self, temp_project):
        """Test dev-dependencies section."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test"
            version = "0.1.0"
            
            [dev-dependencies]
            test-utils = "1.0"
        """)
        
        manifest = Manifest(manifest_path)
        assert len(manifest.dev_dependencies) == 1
        assert "test-utils" in manifest.dev_dependencies


class TestBinaryTargets:
    """Test binary target parsing."""
    
    def test_single_binary(self, temp_project):
        """Test single binary target."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test"
            version = "0.1.0"
            
            [[bin]]
            name = "my-app"
            path = "src/main.nxl"
        """)
        
        manifest = Manifest(manifest_path)
        assert len(manifest.binary_targets) == 1
        bin_target = manifest.binary_targets[0]
        assert bin_target.name == "my-app"
        assert bin_target.path == "src/main.nxl"
    
    def test_multiple_binaries(self, temp_project):
        """Test multiple binary targets."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test"
            version = "0.1.0"
            
            [[bin]]
            name = "server"
            path = "src/bin/server.nxl"
            
            [[bin]]
            name = "client"
            path = "src/bin/client.nxl"
        """)
        
        manifest = Manifest(manifest_path)
        assert len(manifest.binary_targets) == 2
        names = [t.name for t in manifest.binary_targets]
        assert "server" in names
        assert "client" in names


class TestLibraryTarget:
    """Test library target parsing."""
    
    def test_library_target(self, temp_project):
        """Test library configuration."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "my-lib"
            version = "0.1.0"
            
            [lib]
            name = "my_lib"
            path = "src/lib.nxl"
            crate-type = ["lib", "staticlib"]
        """)
        
        manifest = Manifest(manifest_path)
        lib = manifest.library_target
        assert lib is not None
        assert lib.name == "my_lib"
        assert lib.path == "src/lib.nxl"
        assert len(lib.crate_type) == 2
        assert CrateType.LIB in lib.crate_type
        assert CrateType.STATICLIB in lib.crate_type


class TestProfiles:
    """Test build profile parsing."""
    
    def test_default_profiles(self, temp_project):
        """Test default dev and release profiles."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test"
            version = "0.1.0"
        """)
        
        manifest = Manifest(manifest_path)
        
        # Dev profile
        dev = manifest.profiles['dev']
        assert dev.opt_level == 0
        assert dev.debug == True
        assert dev.incremental == True
        
        # Release profile
        release = manifest.profiles['release']
        assert release.opt_level == 3
        assert release.debug == False
        assert release.lto == True
        assert release.strip == True
    
    def test_custom_profile(self, temp_project):
        """Test custom build profile."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test"
            version = "0.1.0"
            
            [profile.production]
            opt-level = 3
            lto = "thin"
            strip = "symbols"
            panic = "abort"
        """)
        
        manifest = Manifest(manifest_path)
        prod = manifest.profiles['production']
        assert prod.opt_level == 3
        assert prod.lto == "thin"
        assert prod.strip == "symbols"
        assert prod.panic == PanicStrategy.ABORT
    
    def test_profile_inheritance(self, temp_project):
        """Test profile inheriting from another."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test"
            version = "0.1.0"
            
            [profile.test]
            inherits = "dev"
            opt-level = 1
        """)
        
        manifest = Manifest(manifest_path)
        test_profile = manifest.profiles['test']
        assert test_profile.opt_level == 1  # Overridden
        assert test_profile.debug == True  # Inherited from dev
        assert test_profile.incremental == True  # Inherited from dev


class TestFeatures:
    """Test feature parsing."""
    
    def test_features_section(self, temp_project):
        """Test feature flags."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test"
            version = "0.1.0"
            
            [features]
            default = ["std"]
            simd = []
            experimental = ["dep:advanced-lib"]
        """)
        
        manifest = Manifest(manifest_path)
        assert len(manifest.features) == 3
        assert manifest.has_feature("simd")
        assert manifest.get_feature_dependencies("default") == ["std"]


class TestWorkspace:
    """Test workspace configuration."""
    
    def test_workspace_members(self, temp_project):
        """Test workspace with multiple members."""
        manifest_path = write_manifest(temp_project, """
            [workspace]
            members = ["crates/core", "crates/graphics", "crates/network"]
            exclude = ["examples", "benches"]
        """)
        
        manifest = Manifest(manifest_path)
        ws = manifest.workspace
        assert ws is not None
        assert len(ws.members) == 3
        assert "crates/core" in ws.members
        assert len(ws.exclude) == 2


class TestTargetSpecificDeps:
    """Test platform-specific dependencies."""
    
    def test_unix_dependencies(self, temp_project):
        """Test Unix-specific dependencies."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test"
            version = "0.1.0"
            
            [target.'cfg(unix)'.dependencies]
            unix-sockets = "0.2"
        """)
        
        manifest = Manifest(manifest_path)
        target_deps = manifest.target_specific_deps
        assert 'cfg(unix)' in target_deps
        assert 'unix-sockets' in target_deps['cfg(unix)']


class TestManifestUtilities:
    """Test utility methods."""
    
    def test_get_all_dependencies(self, temp_project):
        """Test getting all dependencies."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test"
            version = "0.1.0"
            
            [dependencies]
            runtime = "1.0"
            
            [dev-dependencies]
            testing = "1.0"
        """)
        
        manifest = Manifest(manifest_path)
        
        # Only runtime deps
        runtime_deps = manifest.get_all_dependencies()
        assert len(runtime_deps) == 1
        assert "runtime" in runtime_deps
        
        # Include dev deps
        all_deps = manifest.get_all_dependencies(include_dev=True)
        assert len(all_deps) == 2
        assert "testing" in all_deps
    
    def test_resolve_path(self, temp_project):
        """Test path resolution."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test"
            version = "0.1.0"
        """)
        
        manifest = Manifest(manifest_path)
        resolved = manifest.resolve_path("src/main.nxl")
        assert resolved == temp_project / "src" / "main.nxl"


class TestLoadManifest:
    """Test load_manifest function."""
    
    def test_load_by_path(self, temp_project):
        """Test loading manifest by explicit path."""
        manifest_path = write_manifest(temp_project, """
            [package]
            name = "test"
            version = "0.1.0"
        """)
        
        manifest = load_manifest(manifest_path)
        assert manifest.package.name == "test"
    
    def test_load_from_current_dir(self, temp_project):
        """Test loading manifest from current directory."""
        write_manifest(temp_project, """
            [package]
            name = "test"
            version = "0.1.0"
        """)
        
        # Change to project directory
        old_cwd = os.getcwd()
        try:
            os.chdir(temp_project)
            manifest = load_manifest()
            assert manifest.package.name == "test"
        finally:
            os.chdir(old_cwd)
    
    def test_file_not_found(self, temp_project):
        """Test error when manifest not found."""
        old_cwd = os.getcwd()
        try:
            os.chdir(temp_project)
            with pytest.raises(FileNotFoundError, match="Could not find nlpl.toml"):
                load_manifest()
        finally:
            os.chdir(old_cwd)


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
