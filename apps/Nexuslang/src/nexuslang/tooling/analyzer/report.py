"""
Analysis Report System
======================

Structured reporting for static analysis issues.
"""

from dataclasses import dataclass, field
from typing import List, Optional, Dict
from enum import Enum


class Severity(Enum):
    """Issue severity levels."""
    ERROR = "error"        # Must fix - will cause runtime error
    WARNING = "warning"    # Should fix - potential bug
    INFO = "info"          # Consider fixing - style/performance
    HINT = "hint"          # Optional improvement


class Category(Enum):
    """Issue categories."""
    MEMORY = "memory"              # Memory safety
    NULL_SAFETY = "null-safety"    # Null pointer issues
    TYPE_SAFETY = "type-safety"    # Type mismatches
    RESOURCE_LEAK = "resource-leak"  # Resource management
    CONCURRENCY = "concurrency"    # Race conditions, deadlocks
    SECURITY = "security"          # Security vulnerabilities
    PERFORMANCE = "performance"    # Performance issues
    STYLE = "style"                # Code style
    BEST_PRACTICE = "best-practice"  # Best practices
    DEAD_CODE = "dead-code"        # Unreachable code
    CONTROL_FLOW = "control-flow"  # Control flow issues


@dataclass
class SourceLocation:
    """Source code location."""
    file: str
    line: int
    column: int
    end_line: Optional[int] = None
    end_column: Optional[int] = None
    
    def __str__(self) -> str:
        result = f"{self.file}:{self.line}:{self.column}"
        if self.end_line and self.end_line != self.line:
            result += f"-{self.end_line}:{self.end_column}"
        return result


@dataclass
class Issue:
    """A single static analysis issue."""
    
    # Core info
    code: str                    # e.g., "E001", "W042"
    severity: Severity
    category: Category
    message: str
    location: SourceLocation
    
    # Context
    source_line: Optional[str] = None
    suggestion: Optional[str] = None
    fix: Optional[str] = None    # Auto-fix code
    
    # Additional info
    related_locations: List[SourceLocation] = field(default_factory=list)
    help_url: Optional[str] = None
    
    def format(self, show_source: bool = True, use_colors: bool = True) -> str:
        """Format issue for display."""
        # Colors
        if use_colors:
            colors = {
                Severity.ERROR: '\033[91m',    # Red
                Severity.WARNING: '\033[93m',  # Yellow
                Severity.INFO: '\033[94m',     # Blue
                Severity.HINT: '\033[92m',     # Green
            }
            color = colors.get(self.severity, '')
            reset = '\033[0m'
            bold = '\033[1m'
        else:
            color = reset = bold = ''
        
        lines = []
        
        # Header: severity + code + location
        lines.append(
            f"{color}{bold}{self.severity.value.upper()}: {self.code}{reset} "
            f"at {self.location}"
        )
        
        # Message
        lines.append(f"  {self.message}")
        
        # Source code with caret
        if show_source and self.source_line:
            lines.append(f"")
            line_num_str = str(self.location.line)
            lines.append(f"  {line_num_str} | {self.source_line}")
            
            # Caret pointer
            padding = len(line_num_str) + 3 + self.location.column - 1
            lines.append(f"  {' ' * padding}{color}^{reset}")
        
        # Suggestion
        if self.suggestion:
            lines.append(f"")
            lines.append(f"   Suggestion: {self.suggestion}")
        
        # Auto-fix
        if self.fix:
            lines.append(f"")
            lines.append(f"   Auto-fix available:")
            for line in self.fix.split('\n'):
                lines.append(f"     {line}")
        
        # Help URL
        if self.help_url:
            lines.append(f"")
            lines.append(f"   Learn more: {self.help_url}")
        
        return '\n'.join(lines)


@dataclass
class AnalysisReport:
    """Complete analysis report for a file or project."""
    
    file_path: str
    issues: List[Issue] = field(default_factory=list)
    
    # Statistics
    total_lines: int = 0
    lines_analyzed: int = 0
    analysis_time_ms: float = 0.0
    
    def add_issue(self, issue: Issue):
        """Add an issue to the report."""
        self.issues.append(issue)
    
    def filter(self, 
              severity: Optional[Severity] = None,
              category: Optional[Category] = None) -> List[Issue]:
        """Filter issues by severity or category."""
        filtered = self.issues
        
        if severity:
            filtered = [i for i in filtered if i.severity == severity]
        if category:
            filtered = [i for i in filtered if i.category == category]
        
        return filtered
    
    def count_by_severity(self) -> Dict[Severity, int]:
        """Count issues by severity."""
        counts = {s: 0 for s in Severity}
        for issue in self.issues:
            counts[issue.severity] += 1
        return counts
    
    def count_by_category(self) -> Dict[Category, int]:
        """Count issues by category."""
        counts = {c: 0 for c in Category}
        for issue in self.issues:
            counts[issue.category] += 1
        return counts
    
    def has_errors(self) -> bool:
        """Check if report contains any errors."""
        return any(i.severity == Severity.ERROR for i in self.issues)
    
    def summary(self, use_colors: bool = True) -> str:
        """Generate summary string."""
        if use_colors:
            red = '\033[91m'
            yellow = '\033[93m'
            blue = '\033[94m'
            green = '\033[92m'
            bold = '\033[1m'
            reset = '\033[0m'
        else:
            red = yellow = blue = green = bold = reset = ''
        
        counts = self.count_by_severity()
        
        lines = []
        lines.append(f"\n{bold}Analysis Summary for {self.file_path}{reset}")
        lines.append(f"{'=' * 60}")
        
        # Issue counts
        lines.append(f"  {red}Errors:   {counts[Severity.ERROR]:3d}{reset}")
        lines.append(f"  {yellow}Warnings: {counts[Severity.WARNING]:3d}{reset}")
        lines.append(f"  {blue}Info:     {counts[Severity.INFO]:3d}{reset}")
        lines.append(f"  {green}Hints:    {counts[Severity.HINT]:3d}{reset}")
        lines.append(f"  {'' * 60}")
        lines.append(f"  Total:    {len(self.issues):3d}")
        
        # Category breakdown
        if self.issues:
            lines.append(f"\n{bold}Issues by Category:{reset}")
            cat_counts = self.count_by_category()
            for category, count in sorted(cat_counts.items(), key=lambda x: -x[1]):
                if count > 0:
                    lines.append(f"  {category.value}: {count}")
        
        # Performance info
        lines.append(f"\n{bold}Analysis Stats:{reset}")
        lines.append(f"  Lines analyzed: {self.lines_analyzed}/{self.total_lines}")
        lines.append(f"  Time: {self.analysis_time_ms:.2f}ms")
        
        lines.append(f"{'=' * 60}\n")
        
        return '\n'.join(lines)
    
    def format_all(self, 
                   show_source: bool = True, 
                   use_colors: bool = True,
                   max_issues: Optional[int] = None) -> str:
        """Format all issues for display."""
        lines = []
        
        issues_to_show = self.issues[:max_issues] if max_issues else self.issues
        
        for i, issue in enumerate(issues_to_show, 1):
            lines.append(issue.format(show_source, use_colors))
            if i < len(issues_to_show):
                lines.append("")  # Blank line between issues
        
        if max_issues and len(self.issues) > max_issues:
            remaining = len(self.issues) - max_issues
            lines.append(f"\n... and {remaining} more issues")
        
        return '\n'.join(lines)
