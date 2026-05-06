"""
Build System Core
==================

Handles compilation, incremental builds, and caching.
"""

import os
import sys
import hashlib
import json
import pickle
import subprocess
from pathlib import Path
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Set, Any
from datetime import datetime

# Add src to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator, LLVM_AVAILABLE
from nexuslang.optimizer import OptimizationLevel, create_optimization_pipeline
from .project import Project, Target


@dataclass
class BuildConfig:
    """Build configuration."""
    optimization: int = 0
    debug: bool = False
    profile: str = "dev"
    incremental: bool = True
    verbose: bool = False
    jobs: int = 1  # Parallel build jobs
    
    def __str__(self):
        flags = []
        if self.debug:
            flags.append("debug")
        if self.optimization > 0:
            flags.append(f"O{self.optimization}")
        if self.incremental:
            flags.append("incremental")
        return f"BuildConfig({', '.join(flags)})"


@dataclass
class BuildArtifact:
    """Represents a compiled artifact."""
    source_file: Path
    output_file: Path
    object_file: Optional[Path] = None
    ir_file: Optional[Path] = None
    source_hash: str = ""
    build_time: datetime = field(default_factory=datetime.now)
    dependencies: List[Path] = field(default_factory=list)
    
    def is_stale(self) -> bool:
        """Check if artifact needs rebuilding."""
        if not self.output_file.exists():
            return True
        
        # Check source file modification time
        if self.source_file.stat().st_mtime > self.output_file.stat().st_mtime:
            return True
        
        # Check dependencies
        for dep in self.dependencies:
            if dep.exists() and dep.stat().st_mtime > self.output_file.stat().st_mtime:
                return True
        
        # Check hash
        current_hash = self._compute_hash(self.source_file)
        if current_hash != self.source_hash:
            return True
        
        return False
    
    @staticmethod
    def _compute_hash(file_path: Path) -> str:
        """Compute SHA-256 hash of file."""
        sha256 = hashlib.sha256()
        with open(file_path, 'rb') as f:
            for chunk in iter(lambda: f.read(4096), b''):
                sha256.update(chunk)
        return sha256.hexdigest()


class BuildCache:
    """Manages build cache for incremental compilation."""
    
    def __init__(self, cache_dir: Path):
        self.cache_dir = cache_dir
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        self.cache_file = cache_dir / "build_cache.pkl"
        self.artifacts: Dict[str, BuildArtifact] = {}
        self._load()
    
    def _load(self):
        """Load cache from disk."""
        if self.cache_file.exists():
            try:
                with open(self.cache_file, 'rb') as f:
                    self.artifacts = pickle.load(f)
            except Exception as e:
                print(f"Warning: Failed to load build cache: {e}")
                self.artifacts = {}
    
    def _save(self):
        """Save cache to disk."""
        try:
            with open(self.cache_file, 'wb') as f:
                pickle.dump(self.artifacts, f)
        except Exception as e:
            print(f"Warning: Failed to save build cache: {e}")
    
    def get_artifact(self, source_file: Path) -> Optional[BuildArtifact]:
        """Get cached artifact for source file."""
        key = str(source_file.absolute())
        return self.artifacts.get(key)
    
    def add_artifact(self, artifact: BuildArtifact):
        """Add artifact to cache."""
        key = str(artifact.source_file.absolute())
        artifact.source_hash = BuildArtifact._compute_hash(artifact.source_file)
        self.artifacts[key] = artifact
        self._save()
    
    def invalidate(self, source_file: Path):
        """Invalidate cache entry for source file."""
        key = str(source_file.absolute())
        if key in self.artifacts:
            del self.artifacts[key]
            self._save()
    
    def clear(self):
        """Clear entire cache."""
        self.artifacts = {}
        self._save()


class Builder:
    """Builds NexusLang projects."""
    
    def __init__(self, project: Project, config: BuildConfig):
        self.project = project
        self.config = config
        self.cache = BuildCache(project.get_build_dir() / ".cache")
        self.built_targets: Set[str] = set()
    
    def build_target(self, target_name: str) -> bool:
        """Build a specific target."""
        target = self.project.get_target(target_name)
        if not target:
            print(f"Error: Target '{target_name}' not found")
            return False
        
        if target_name in self.built_targets:
            if self.config.verbose:
                print(f"Target '{target_name}' already built")
            return True
        
        # Build dependencies first
        for dep in target.dependencies:
            if not self.build_target(dep):
                print(f"Error: Failed to build dependency '{dep}'")
                return False
        
        # Check if rebuild needed
        source_path = Path(target.source)
        if not source_path.is_absolute():
            source_path = self.project.root_path / target.source
        if not source_path.exists():
            print(f"Error: Source file not found: {source_path}")
            return False
        
        # Check cache
        artifact = self.cache.get_artifact(source_path)
        if self.config.incremental and artifact and not artifact.is_stale():
            if self.config.verbose:
                print(f"Using cached build for {target_name}")
            self.built_targets.add(target_name)
            return True
        
        # Build target
        print(f"Building {target_name}...")
        
        output_path = self.project.get_output_dir() / target.name
        if target.target_type == "library":
            output_path = output_path.with_suffix(".a")
        
        success = self._compile_file(
            source_path,
            output_path,
            optimization=target.optimization or self.config.optimization,
            debug=target.debug or self.config.debug,
            target_type=target.target_type,
        )
        
        if success:
            # Update cache
            artifact = BuildArtifact(
                source_file=source_path,
                output_file=output_path,
            )
            self.cache.add_artifact(artifact)
            self.built_targets.add(target_name)
            print(f" Built {target_name}")
        else:
            print(f" Failed to build {target_name}")
        
        return success
    
    def _compile_file(
        self,
        source_path: Path,
        output_path: Path,
        optimization: int = 0,
        debug: bool = False,
        target_type: str = "executable",
    ) -> bool:
        """Compile a single NexusLang file."""
        try:
            # Read source
            with open(source_path, 'r') as f:
                source_code = f.read()
            
            if self.config.verbose:
                print(f"  Lexing {source_path.name}...")
            
            # Lex
            lexer = Lexer(source_code)
            lexer.filename = str(source_path)
            tokens = lexer.tokenize()
            
            if self.config.verbose:
                print(f"  Parsing {source_path.name}...")
            
            # Parse
            parser = Parser(tokens)
            parser.filename = str(source_path)
            ast = parser.parse()
            
            if self.config.verbose:
                print(f"  Generating LLVM IR...")
            
            # Generate LLVM IR
            generator = LLVMIRGenerator()
            llvm_ir = generator.generate(ast, source_file=str(source_path), debug_info=debug)
            
            # Apply optimizations
            if optimization > 0:
                if self.config.verbose:
                    print(f"  Optimizing (O{optimization})...")
                # Optimizations are applied at compile time via opt_level parameter
            
            # Add debug info if requested
            # Debug info is generated in generate() call above
            
            # Ensure output directory exists
            output_path.parent.mkdir(parents=True, exist_ok=True)
            
            if target_type == "module":
                # Just save LLVM IR
                ir_path = output_path.with_suffix('.ll')
                if self.config.verbose:
                    print(f"  Writing IR to {ir_path}...")
                with open(ir_path, 'w') as f:
                    f.write(llvm_ir)
                return True
            
            # Compile to executable/library
            if self.config.verbose:
                print(f"  Compiling to {target_type}...")
            
            # Save IR to temporary file
            import tempfile
            with tempfile.NamedTemporaryFile(mode='w', suffix='.ll', delete=False) as f:
                temp_ll = f.name
                f.write(llvm_ir)
            
            try:
                # Compile to executable
                success = generator.compile_to_executable(str(output_path), opt_level=optimization)
                return success
            finally:
                # Clean up temporary file
                if os.path.exists(temp_ll):
                    os.remove(temp_ll)
            
            return True
            
        except Exception as e:
            print(f"Error compiling {source_path}: {e}")
            if self.config.verbose:
                import traceback
                traceback.print_exc()
            return False
    
    def build_all(self) -> bool:
        """Build all targets in the project."""
        success = True
        for target_name in self.project.config.targets:
            if not self.build_target(target_name):
                success = False
        return success
    
    def clean(self):
        """Clean build artifacts."""
        print("Cleaning build artifacts...")
        
        # Remove build directory
        import shutil
        build_dir = self.project.get_build_dir()
        if build_dir.exists():
            shutil.rmtree(build_dir)
            build_dir.mkdir()
        
        # Remove output directory
        output_dir = self.project.get_output_dir()
        if output_dir.exists():
            shutil.rmtree(output_dir)
            output_dir.mkdir()
        
        print(" Cleaned")
    
    def run_target(self, target_name: str, args: List[str] = None) -> int:
        """Build and run a target."""
        if not self.build_target(target_name):
            return 1
        
        target = self.project.get_target(target_name)
        if target.target_type != "executable":
            print(f"Error: Target '{target_name}' is not executable")
            return 1
        
        output_path = self.project.get_output_dir() / target.name
        if not output_path.exists():
            print(f"Error: Executable not found: {output_path}")
            return 1
        
        # Run the executable
        cmd = [str(output_path)]
        if args:
            cmd.extend(args)
        
        if self.config.verbose:
            print(f"Running: {' '.join(cmd)}")
        
        try:
            result = subprocess.run(cmd)
            return result.returncode
        except KeyboardInterrupt:
            print("\nInterrupted")
            return 130
        except Exception as e:
            print(f"Error running executable: {e}")
            return 1
