"""
Static Analyzer Core
====================

Main analyzer that coordinates all checks.
"""

import time
from pathlib import Path
from typing import List, Optional, Set, Type
import sys
import os

# Add src to path for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../..'))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import ASTNode
from .report import AnalysisReport, Issue, SourceLocation, Severity, Category
from .checks.memory_safety import MemorySafetyChecker
from .checks.null_safety import NullSafetyAnalyzer
from .checks.resource_leak import ResourceLeakChecker
from .checks.initialization import InitializationChecker
from .checks.type_safety import TypeSafetyChecker
from .checks.dead_code import DeadCodeChecker
from .checks.style import StyleChecker
from .checks.performance import PerformanceChecker
from .checks.security import SecurityChecker
from .checks.data_flow import DataFlowChecker
from .checks.control_flow import ControlFlowChecker


class StaticAnalyzer:
    """
    Production-grade static analyzer for NexusLang.
    
    Runs multiple analysis passes to detect:
    - Memory safety violations
    - Null pointer dereferences
    - Resource leaks
    - Uninitialized variables
    - Type errors
    - Dead code
    - Style issues
    """
    
    def __init__(self,
                 enable_all: bool = True,
                 enable_memory: bool = True,
                 enable_null: bool = True,
                 enable_resources: bool = True,
                 enable_init: bool = True,
                 enable_types: bool = True,
                 enable_dead_code: bool = True,
                 enable_style: bool = False,
                 enable_performance: bool = True,
                 enable_security: bool = True,
                 enable_data_flow: bool = True,
                 enable_control_flow: bool = True):
        """
        Initialize analyzer with configuration.
        
        Args:
            enable_all: Enable all checks (default: True)
            enable_memory: Memory safety checks
            enable_null: Null safety checks
            enable_resources: Resource leak checks
            enable_init: Uninitialized variable checks
            enable_types: Type safety checks
            enable_dead_code: Dead code detection
            enable_style: Style checks (disabled by default)
        """
        self.config = {
            'memory': enable_all and enable_memory,
            'null': enable_all and enable_null,
            'resources': enable_all and enable_resources,
            'init': enable_all and enable_init,
            'types': enable_all and enable_types,
            'dead_code': enable_all and enable_dead_code,
            'style': enable_style,  # Not affected by enable_all
            'performance': enable_all and enable_performance,
            'security': enable_all and enable_security,
            'data_flow': enable_all and enable_data_flow,
            'control_flow': enable_all and enable_control_flow,
        }
        
        # Initialize checkers
        self.checkers = []
        
        if self.config['memory']:
            self.checkers.append(MemorySafetyChecker())
        if self.config['null']:
            self.checkers.append(NullSafetyAnalyzer())
        if self.config['resources']:
            self.checkers.append(ResourceLeakChecker())
        if self.config['init']:
            self.checkers.append(InitializationChecker())
        if self.config['types']:
            self.checkers.append(TypeSafetyChecker())
        if self.config['dead_code']:
            self.checkers.append(DeadCodeChecker())
        if self.config['style']:
            self.checkers.append(StyleChecker())
        if self.config['performance']:
            self.checkers.append(PerformanceChecker())
        if self.config['security']:
            self.checkers.append(SecurityChecker())
        if self.config['data_flow']:
            self.checkers.append(DataFlowChecker())
        if self.config['control_flow']:
            self.checkers.append(ControlFlowChecker())
    
    def analyze_file(self, file_path: str) -> AnalysisReport:
        """
        Analyze a single NexusLang file.
        
        Args:
            file_path: Path to .nlpl file
            
        Returns:
            AnalysisReport with all issues found
        """
        start_time = time.time()
        report = AnalysisReport(file_path=file_path)
        
        try:
            # Read source
            with open(file_path, 'r') as f:
                source = f.read()
                lines = source.split('\n')
            
            report.total_lines = len(lines)
            report.lines_analyzed = len([l for l in lines if l.strip()])
            
            # Parse
            lexer = Lexer(source)
            tokens = lexer.tokenize()
            
            parser = Parser(tokens)
            ast = parser.parse()
            
            # Run all checkers
            for checker in self.checkers:
                issues = checker.check(ast, source, lines)
                for issue in issues:
                    report.add_issue(issue)
        
        except Exception as e:
            # Analysis failed - add error
            report.add_issue(Issue(
                code="E000",
                severity=Severity.ERROR,
                category=Category.BEST_PRACTICE,
                message=f"Analysis failed: {str(e)}",
                location=SourceLocation(file_path, 1, 1)
            ))
        
        finally:
            end_time = time.time()
            report.analysis_time_ms = (end_time - start_time) * 1000
        
        return report
    
    def analyze_directory(self, 
                         dir_path: str, 
                         recursive: bool = True) -> List[AnalysisReport]:
        """
        Analyze all NexusLang files in a directory.
        
        Args:
            dir_path: Directory path
            recursive: Recursively search subdirectories
            
        Returns:
            List of AnalysisReports
        """
        reports = []
        path = Path(dir_path)
        
        if not path.exists() or not path.is_dir():
            raise ValueError(f"Invalid directory: {dir_path}")
        
        # Find all .nlpl files
        pattern = "**/*.nxl" if recursive else "*.nxl"
        for file_path in path.glob(pattern):
            report = self.analyze_file(str(file_path))
            reports.append(report)
        
        return reports
    
    def analyze_project(self, 
                       project_root: str) -> List[AnalysisReport]:
        """
        Analyze entire project (src/ and examples/).
        
        Args:
            project_root: Project root directory
            
        Returns:
            List of AnalysisReports
        """
        reports = []
        root = Path(project_root)
        
        # Common source directories
        source_dirs = [
            root / "src",
            root / "examples",
            root / "test_programs",
            root / "tests",
        ]
        
        for source_dir in source_dirs:
            if source_dir.exists():
                reports.extend(self.analyze_directory(str(source_dir), recursive=True))
        
        return reports


def create_default_analyzer() -> StaticAnalyzer:
    """Create analyzer with default configuration."""
    return StaticAnalyzer(enable_all=True, enable_style=False)


def create_strict_analyzer() -> StaticAnalyzer:
    """Create analyzer with all checks enabled (including style)."""
    return StaticAnalyzer(enable_all=True, enable_style=True)


def create_minimal_analyzer() -> StaticAnalyzer:
    """Create analyzer with only critical checks."""
    return StaticAnalyzer(
        enable_all=False,
        enable_memory=True,
        enable_null=True,
        enable_init=True
    )
