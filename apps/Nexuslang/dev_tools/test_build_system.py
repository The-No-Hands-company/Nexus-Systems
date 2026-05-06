"""
Test Build System Improvements
===============================

Tests for the enhanced build system with:
1. Dependency path resolution
2. Multi-file module handling
3. Better main entry point detection
"""

import sys
import os
import tempfile
import shutil
from pathlib import Path

# Add project root to path
sys.path.insert(0, str(Path(__file__).parent.parent / 'src'))

from nexuslang.tooling.config import ProjectConfig, PackageConfig, BuildConfig
from nexuslang.tooling.builder import BuildSystem


def test_detect_main_entry_point():
    """Test main entry point detection strategies."""
    print("=" * 80)
    print("Test 1: Main Entry Point Detection")
    print("=" * 80)
    
    # Create a temporary directory with test files
    with tempfile.TemporaryDirectory() as tmpdir:
        src_dir = os.path.join(tmpdir, 'src')
        os.makedirs(src_dir)
        
        # Create test files
        files = [
            'utils.nxl',
            'helper.nxl',
            'main.nxl'  # This should be detected as main
        ]
        
        for filename in files:
            filepath = os.path.join(src_dir, filename)
            with open(filepath, 'w') as f:
                f.write('# Test file\n')
        
        # Create config
        config = ProjectConfig(
            package=PackageConfig(name='test_project', version='0.1.0'),
            build=BuildConfig(source_dir=src_dir, output_dir=os.path.join(tmpdir, 'build'))
        )
        
        builder = BuildSystem(config)
        
        # Get all sources
        import glob
        sources = glob.glob(os.path.join(src_dir, '*.nxl'))
        
        # Test detection
        main_file = builder._detect_main_entry_point(sources)
        
        print(f"\nSource files: {[os.path.basename(f) for f in sources]}")
        print(f"Detected main: {os.path.basename(main_file) if main_file else 'None'}")
        
        assert main_file is not None, "Should detect a main file"
        assert os.path.basename(main_file) == 'main.nxl', "Should detect main.nxl"
        
        print(" Test passed: main.nlpl correctly detected")
    
    print()


def test_detect_main_by_package_name():
    """Test detection by package name."""
    print("=" * 80)
    print("Test 2: Main Entry Point Detection by Package Name")
    print("=" * 80)
    
    with tempfile.TemporaryDirectory() as tmpdir:
        src_dir = os.path.join(tmpdir, 'src')
        os.makedirs(src_dir)
        
        # Create test files
        files = [
            'utils.nxl',
            'myapp.nxl'  # Matches package name
        ]
        
        for filename in files:
            filepath = os.path.join(src_dir, filename)
            with open(filepath, 'w') as f:
                f.write('# Test file\n')
        
        # Create config with matching package name
        config = ProjectConfig(
            package=PackageConfig(name='myapp', version='0.1.0'),
            build=BuildConfig(source_dir=src_dir, output_dir=os.path.join(tmpdir, 'build'))
        )
        
        builder = BuildSystem(config)
        
        import glob
        sources = glob.glob(os.path.join(src_dir, '*.nxl'))
        main_file = builder._detect_main_entry_point(sources)
        
        print(f"\nPackage name: {config.package.name}")
        print(f"Source files: {[os.path.basename(f) for f in sources]}")
        print(f"Detected main: {os.path.basename(main_file) if main_file else 'None'}")
        
        assert main_file is not None
        assert os.path.basename(main_file) == 'myapp.nxl'
        
        print(" Test passed: Package name file correctly detected")
    
    print()


def test_detect_main_by_function():
    """Test detection by main() function presence."""
    print("=" * 80)
    print("Test 3: Main Entry Point Detection by main() Function")
    print("=" * 80)
    
    with tempfile.TemporaryDirectory() as tmpdir:
        src_dir = os.path.join(tmpdir, 'src')
        os.makedirs(src_dir)
        
        # Create test files
        utils_file = os.path.join(src_dir, 'utils.nxl')
        with open(utils_file, 'w') as f:
            f.write('# Utility functions\nfunction helper\n    print text "helper"\nend\n')
        
        app_file = os.path.join(src_dir, 'app.nxl')
        with open(app_file, 'w') as f:
            f.write('# Main application\nfunction main\n    print text "Hello"\nend\n')
        
        config = ProjectConfig(
            package=PackageConfig(name='test', version='0.1.0'),
            build=BuildConfig(source_dir=src_dir, output_dir=os.path.join(tmpdir, 'build'))
        )
        
        builder = BuildSystem(config)
        
        import glob
        sources = glob.glob(os.path.join(src_dir, '*.nxl'))
        main_file = builder._detect_main_entry_point(sources)
        
        print(f"\nSource files: {[os.path.basename(f) for f in sources]}")
        print(f"Detected main: {os.path.basename(main_file) if main_file else 'None'}")
        
        assert main_file is not None
        assert os.path.basename(main_file) == 'app.nxl'
        
        print(" Test passed: File with main() function correctly detected")
    
    print()


def test_group_into_modules():
    """Test module grouping by directory structure."""
    print("=" * 80)
    print("Test 4: Module Grouping")
    print("=" * 80)
    
    with tempfile.TemporaryDirectory() as tmpdir:
        src_dir = os.path.join(tmpdir, 'src')
        
        # Create directory structure
        os.makedirs(os.path.join(src_dir, 'utils'))
        os.makedirs(os.path.join(src_dir, 'models'))
        
        # Create test files
        files = [
            'main.nxl',
            'utils/string.nxl',
            'utils/math.nxl',
            'models/user.nxl'
        ]
        
        for filename in files:
            filepath = os.path.join(src_dir, filename)
            with open(filepath, 'w') as f:
                f.write('# Test file\n')
        
        config = ProjectConfig(
            package=PackageConfig(name='test', version='0.1.0'),
            build=BuildConfig(source_dir=src_dir, output_dir=os.path.join(tmpdir, 'build'))
        )
        
        builder = BuildSystem(config)
        
        import glob
        sources = glob.glob(os.path.join(src_dir, '**/*.nxl'), recursive=True)
        modules = builder._group_into_modules(sources)
        
        print(f"\nSource files: {[os.path.relpath(f, src_dir) for f in sources]}")
        print(f"\nModules detected:")
        for module_name, module_files in modules.items():
            print(f"  {module_name}: {[os.path.basename(f) for f in module_files]}")
        
        assert 'main' in modules
        assert 'utils' in modules
        assert 'models' in modules
        assert len(modules['utils']) == 2
        
        print("\n Test passed: Modules correctly grouped by directory")
    
    print()


def test_dependency_path_configuration():
    """Test dependency path configuration."""
    print("=" * 80)
    print("Test 5: Dependency Path Configuration")
    print("=" * 80)
    
    with tempfile.TemporaryDirectory() as tmpdir:
        # Create a fake dependency directory
        dep_dir = os.path.join(tmpdir, 'my_dep')
        dep_lib_dir = os.path.join(dep_dir, 'build', 'lib')
        os.makedirs(dep_lib_dir)
        
        # Create config with local dependency
        config = ProjectConfig(
            package=PackageConfig(name='test', version='0.1.0'),
            build=BuildConfig(source_dir='src', output_dir='build'),
            dependencies={'my_dep': {'path': dep_dir}}
        )
        
        builder = BuildSystem(config)
        
        print(f"\nDependency: my_dep at {dep_dir}")
        print(f"Library search paths:")
        for path in builder.compiler.options.library_search_paths:
            print(f"  {path}")
        
        assert dep_lib_dir in builder.compiler.options.library_search_paths
        
        print("\n Test passed: Dependency paths correctly added to library search paths")
    
    print()


def test_find_executable():
    """Test executable finding strategies."""
    print("=" * 80)
    print("Test 6: Find Executable")
    print("=" * 80)
    
    with tempfile.TemporaryDirectory() as tmpdir:
        build_dir = os.path.join(tmpdir, 'build')
        os.makedirs(build_dir)
        
        # Create a fake executable with package name
        executable_path = os.path.join(build_dir, 'myapp')
        with open(executable_path, 'w') as f:
            f.write('#!/bin/bash\necho "Hello"\n')
        os.chmod(executable_path, 0o755)  # Make executable
        
        # Create some other files
        with open(os.path.join(build_dir, 'myapp.c'), 'w') as f:
            f.write('// Source file\n')
        
        config = ProjectConfig(
            package=PackageConfig(name='myapp', version='0.1.0'),
            build=BuildConfig(source_dir='src', output_dir=build_dir)
        )
        
        builder = BuildSystem(config)
        
        found_exe = builder._find_executable(build_dir)
        
        print(f"\nPackage name: {config.package.name}")
        print(f"Build directory: {build_dir}")
        print(f"Files in build dir: {os.listdir(build_dir)}")
        print(f"Found executable: {os.path.basename(found_exe) if found_exe else 'None'}")
        
        assert found_exe is not None
        assert os.path.basename(found_exe) == 'myapp'
        
        print("\n Test passed: Executable correctly found by package name")
    
    print()


if __name__ == '__main__':
    print("\n" + "=" * 80)
    print("NLPL Build System Tests")
    print("=" * 80 + "\n")
    
    try:
        test_detect_main_entry_point()
        test_detect_main_by_package_name()
        test_detect_main_by_function()
        test_group_into_modules()
        test_dependency_path_configuration()
        test_find_executable()
        
        print("=" * 80)
        print(" All tests passed!")
        print("=" * 80)
        
    except AssertionError as e:
        print(f"\n Test failed: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
    except Exception as e:
        print(f"\n Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
