"""
NLPL Compiler Performance Profiling System

Provides detailed metrics for inline assembly compilation:
- Validation overhead
- Code generation time
- Optimization statistics
- Register allocation efficiency
- Memory usage tracking

Usage:
    with CompilerProfiler() as profiler:
        # ... compile code ...
        pass
    
    profiler.print_report()
"""

import time
import os
import psutil
from dataclasses import dataclass, field
from typing import Dict, List, Optional
from contextlib import contextmanager


@dataclass
class ProfileMetrics:
    """Container for profiling metrics."""
    
    # Timing metrics (seconds)
    total_time: float = 0.0
    lexer_time: float = 0.0
    parser_time: float = 0.0
    type_check_time: float = 0.0
    codegen_time: float = 0.0
    optimization_time: float = 0.0
    
    # Inline assembly specific
    asm_validation_time: float = 0.0
    asm_generation_time: float = 0.0
    asm_instruction_count: int = 0
    asm_block_count: int = 0
    
    # Safety validation metrics
    dangerous_instruction_checks: int = 0
    memory_safety_checks: int = 0
    register_usage_checks: int = 0
    syntax_validation_checks: int = 0
    
    # Error/Warning counts
    syntax_errors: int = 0
    safety_warnings: int = 0
    optimization_hints: int = 0
    
    # Memory metrics (bytes)
    peak_memory: int = 0
    memory_delta: int = 0
    
    # Code size metrics
    source_size: int = 0
    ir_size: int = 0
    binary_size: int = 0
    
    # Register allocation metrics
    registers_used: List[str] = field(default_factory=list)
    register_spills: int = 0
    
    # LLVM optimization metrics
    optimization_passes: int = 0
    instructions_eliminated: int = 0
    functions_inlined: int = 0


class CompilerProfiler:
    """
    Compiler profiling context manager.
    
    Tracks compilation metrics and provides detailed reports.
    """
    
    def __init__(self, enabled: bool = True):
        self.enabled = enabled
        self.metrics = ProfileMetrics()
        self.process = psutil.Process(os.getpid())
        self.start_memory = 0
        self.start_time = 0
        self._phase_stack: List[tuple] = []  # Stack of (phase_name, start_time)
    
    def __enter__(self):
        """Start profiling."""
        if self.enabled:
            self.start_time = time.perf_counter()
            self.start_memory = self.process.memory_info().rss
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """End profiling."""
        if self.enabled:
            self.metrics.total_time = time.perf_counter() - self.start_time
            current_memory = self.process.memory_info().rss
            self.metrics.memory_delta = current_memory - self.start_memory
            self.metrics.peak_memory = self.process.memory_info().rss
    
    @contextmanager
    def phase(self, phase_name: str):
        """
        Context manager for timing individual compilation phases.
        
        Usage:
            with profiler.phase("lexer"):
                # ... lexer code ...
                pass
        """
        if not self.enabled:
            yield
            return
        
        start = time.perf_counter()
        self._phase_stack.append((phase_name, start))
        
        try:
            yield
        finally:
            elapsed = time.perf_counter() - start
            self._phase_stack.pop()
            
            # Store timing in appropriate metric
            if phase_name == "lexer":
                self.metrics.lexer_time = elapsed
            elif phase_name == "parser":
                self.metrics.parser_time = elapsed
            elif phase_name == "type_check":
                self.metrics.type_check_time = elapsed
            elif phase_name == "codegen":
                self.metrics.codegen_time = elapsed
            elif phase_name == "optimization":
                self.metrics.optimization_time = elapsed
            elif phase_name == "asm_validation":
                self.metrics.asm_validation_time = elapsed
            elif phase_name == "asm_generation":
                self.metrics.asm_generation_time = elapsed
    
    def record_asm_block(self, instruction_count: int):
        """Record an inline assembly block."""
        if self.enabled:
            self.metrics.asm_block_count += 1
            self.metrics.asm_instruction_count += instruction_count
    
    def record_validation(self, validation_type: str):
        """Record a validation check."""
        if not self.enabled:
            return
        
        if validation_type == "dangerous_instruction":
            self.metrics.dangerous_instruction_checks += 1
        elif validation_type == "memory_safety":
            self.metrics.memory_safety_checks += 1
        elif validation_type == "register_usage":
            self.metrics.register_usage_checks += 1
        elif validation_type == "syntax":
            self.metrics.syntax_validation_checks += 1
    
    def record_error(self, error_type: str):
        """Record an error or warning."""
        if not self.enabled:
            return
        
        if error_type == "syntax":
            self.metrics.syntax_errors += 1
        elif error_type == "warning":
            self.metrics.safety_warnings += 1
        elif error_type == "hint":
            self.metrics.optimization_hints += 1
    
    def record_register_usage(self, registers: List[str]):
        """Record register usage."""
        if self.enabled:
            self.metrics.registers_used.extend(registers)
    
    def record_register_spill(self):
        """Record a register spill."""
        if self.enabled:
            self.metrics.register_spills += 1
    
    def record_code_size(self, source: int = 0, ir: int = 0, binary: int = 0):
        """Record code size metrics."""
        if not self.enabled:
            return
        
        if source > 0:
            self.metrics.source_size = source
        if ir > 0:
            self.metrics.ir_size = ir
        if binary > 0:
            self.metrics.binary_size = binary
    
    def record_optimization(self, optimization_type: str, count: int = 1):
        """Record optimization statistics."""
        if not self.enabled:
            return
        
        if optimization_type == "pass":
            self.metrics.optimization_passes += count
        elif optimization_type == "eliminated":
            self.metrics.instructions_eliminated += count
        elif optimization_type == "inlined":
            self.metrics.functions_inlined += count
    
    def print_report(self, verbose: bool = False):
        """Print profiling report."""
        if not self.enabled:
            return
        
        print("\n" + "=" * 70)
        print("NLPL COMPILER PROFILING REPORT")
        print("=" * 70)
        
        # Timing breakdown
        print("\n📊 Timing Breakdown:")
        print(f"  Total compilation time: {self.metrics.total_time*1000:.2f} ms")
        
        if self.metrics.lexer_time > 0:
            pct = (self.metrics.lexer_time / self.metrics.total_time) * 100
            print(f"    - Lexer:        {self.metrics.lexer_time*1000:>8.2f} ms ({pct:>5.1f}%)")
        
        if self.metrics.parser_time > 0:
            pct = (self.metrics.parser_time / self.metrics.total_time) * 100
            print(f"    - Parser:       {self.metrics.parser_time*1000:>8.2f} ms ({pct:>5.1f}%)")
        
        if self.metrics.type_check_time > 0:
            pct = (self.metrics.type_check_time / self.metrics.total_time) * 100
            print(f"    - Type check:   {self.metrics.type_check_time*1000:>8.2f} ms ({pct:>5.1f}%)")
        
        if self.metrics.codegen_time > 0:
            pct = (self.metrics.codegen_time / self.metrics.total_time) * 100
            print(f"    - Code gen:     {self.metrics.codegen_time*1000:>8.2f} ms ({pct:>5.1f}%)")
        
        if self.metrics.optimization_time > 0:
            pct = (self.metrics.optimization_time / self.metrics.total_time) * 100
            print(f"    - Optimization: {self.metrics.optimization_time*1000:>8.2f} ms ({pct:>5.1f}%)")
        
        # Inline assembly metrics
        if self.metrics.asm_block_count > 0:
            print("\n⚙️  Inline Assembly Metrics:")
            print(f"  Assembly blocks:     {self.metrics.asm_block_count}")
            print(f"  Total instructions:  {self.metrics.asm_instruction_count}")
            
            if self.metrics.asm_validation_time > 0:
                print(f"  Validation time:     {self.metrics.asm_validation_time*1000:.2f} ms")
                avg_per_block = (self.metrics.asm_validation_time*1000) / self.metrics.asm_block_count
                print(f"  Avg per block:       {avg_per_block:.3f} ms")
            
            if self.metrics.asm_generation_time > 0:
                print(f"  Generation time:     {self.metrics.asm_generation_time*1000:.2f} ms")
        
        # Safety validation metrics
        total_checks = (
            self.metrics.dangerous_instruction_checks +
            self.metrics.memory_safety_checks +
            self.metrics.register_usage_checks +
            self.metrics.syntax_validation_checks
        )
        
        if total_checks > 0:
            print("\n🛡️  Safety Validation:")
            print(f"  Total checks:        {total_checks}")
            if self.metrics.dangerous_instruction_checks > 0:
                print(f"    - Dangerous instr: {self.metrics.dangerous_instruction_checks}")
            if self.metrics.memory_safety_checks > 0:
                print(f"    - Memory safety:   {self.metrics.memory_safety_checks}")
            if self.metrics.register_usage_checks > 0:
                print(f"    - Register usage:  {self.metrics.register_usage_checks}")
            if self.metrics.syntax_validation_checks > 0:
                print(f"    - Syntax checks:   {self.metrics.syntax_validation_checks}")
            
            # Error/warning summary
            if self.metrics.syntax_errors > 0 or self.metrics.safety_warnings > 0:
                print(f"\n  Issues found:")
                if self.metrics.syntax_errors > 0:
                    print(f"    - Syntax errors:   {self.metrics.syntax_errors}")
                if self.metrics.safety_warnings > 0:
                    print(f"    - Safety warnings: {self.metrics.safety_warnings}")
                if self.metrics.optimization_hints > 0:
                    print(f"    - Optimization hints: {self.metrics.optimization_hints}")
        
        # Memory metrics
        if self.metrics.memory_delta != 0:
            print("\n💾 Memory Usage:")
            print(f"  Peak memory:         {self.metrics.peak_memory / 1024 / 1024:.2f} MB")
            print(f"  Memory delta:        {self.metrics.memory_delta / 1024:.2f} KB")
        
        # Code size metrics
        if self.metrics.source_size > 0:
            print("\n📦 Code Size:")
            print(f"  Source size:         {self.metrics.source_size:,} bytes")
            if self.metrics.ir_size > 0:
                print(f"  LLVM IR size:        {self.metrics.ir_size:,} bytes")
                ratio = self.metrics.ir_size / self.metrics.source_size
                print(f"  IR/source ratio:     {ratio:.2f}x")
            if self.metrics.binary_size > 0:
                print(f"  Binary size:         {self.metrics.binary_size:,} bytes")
                ratio = self.metrics.binary_size / self.metrics.source_size
                print(f"  Binary/source ratio: {ratio:.2f}x")
        
        # Register allocation metrics
        if verbose and self.metrics.registers_used:
            print("\n🎯 Register Allocation:")
            unique_regs = set(self.metrics.registers_used)
            print(f"  Unique registers:    {len(unique_regs)}")
            print(f"  Total uses:          {len(self.metrics.registers_used)}")
            if self.metrics.register_spills > 0:
                print(f"  Register spills:     {self.metrics.register_spills}")
            
            # Show register usage distribution
            from collections import Counter
            reg_counts = Counter(self.metrics.registers_used)
            print(f"\n  Most used registers:")
            for reg, count in reg_counts.most_common(5):
                print(f"    {reg}: {count}")
        
        # Optimization metrics
        if self.metrics.optimization_passes > 0:
            print("\n🚀 Optimization Statistics:")
            print(f"  Optimization passes: {self.metrics.optimization_passes}")
            if self.metrics.instructions_eliminated > 0:
                print(f"  Instructions elim:   {self.metrics.instructions_eliminated}")
            if self.metrics.functions_inlined > 0:
                print(f"  Functions inlined:   {self.metrics.functions_inlined}")
        
        print("\n" + "=" * 70 + "\n")
    
    def get_metrics(self) -> ProfileMetrics:
        """Get profiling metrics."""
        return self.metrics


# Global profiler instance
_global_profiler: Optional[CompilerProfiler] = None


def get_profiler() -> Optional[CompilerProfiler]:
    """Get the global profiler instance."""
    return _global_profiler


def set_profiler(profiler: Optional[CompilerProfiler]):
    """Set the global profiler instance."""
    global _global_profiler
    _global_profiler = profiler


def enable_profiling() -> CompilerProfiler:
    """Enable global profiling."""
    profiler = CompilerProfiler(enabled=True)
    set_profiler(profiler)
    return profiler


def disable_profiling():
    """Disable global profiling."""
    set_profiler(None)
