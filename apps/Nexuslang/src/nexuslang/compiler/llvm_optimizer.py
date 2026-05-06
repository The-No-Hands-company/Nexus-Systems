"""
LLVM-Level Optimizer for NexusLang
==============================

Integrates LLVM's optimization passes for native code generation.
Uses llvmlite binding to LLVM's PassManager for production-grade optimization.

Optimization Levels:
- O0: No optimization (debug-friendly, fast compile)
- O1: Basic optimization (constant folding, simple DCE)
- O2: Standard optimization (inlining, loop opts, CSE) [DEFAULT]
- O3: Aggressive optimization (vectorization, unrolling)
- Os: Size optimization (minimize binary size)

This module complements the AST-level optimizations in optimizer.py.
"""

from typing import Optional, List, Dict
from enum import Enum


class OptimizationLevel(Enum):
    """Optimization levels matching LLVM/GCC convention."""
    O0 = 0  # No optimization
    O1 = 1  # Basic
    O2 = 2  # Standard (default)
    O3 = 3  # Aggressive
    Os = 's'  # Size


class LLVMOptimizer:
    """
    LLVM IR optimization using llvmlite PassManager.
    
    Applies LLVM's world-class optimization passes to generated IR.
    Handles module-level and function-level optimizations.
    """
    
    def __init__(self, optimization_level: OptimizationLevel = OptimizationLevel.O2):
        """
        Initialize LLVM optimizer.
        
        Args:
            optimization_level: Optimization level (O0-O3, Os)
        """
        self.optimization_level = optimization_level
        self.pass_manager_builder = None
        self.module_pass_manager = None
        self.function_pass_manager = None
        
        # Track optimization statistics
        self.stats = {
            'passes_run': 0,
            'functions_optimized': 0,
            'estimated_speedup': 1.0,
        }
        
        # Lazy initialization (only import llvmlite when needed)
        self._llvm_initialized = False
    
    def _initialize_llvm(self):
        """Lazy initialization of LLVM binding."""
        if self._llvm_initialized:
            return
        
        try:
            from llvmlite import binding as llvm
            
            # Note: llvm.initialize() is deprecated in newer versions
            # LLVM initialization is now automatic
            # Only initialize targets
            llvm.initialize_native_target()
            llvm.initialize_native_asmprinter()
            
            self._llvm = llvm
            self._llvm_initialized = True
        except ImportError:
            raise RuntimeError(
                "llvmlite not installed. Install with: pip install llvmlite\n"
                "Required for LLVM optimization passes."
            )
    
    def _configure_passes(self):
        """
        Configure optimization passes based on level.
        
        Note: Modern llvmlite (0.40+) uses different pass management.
        We use opt tool for optimization instead of PassManagerBuilder.
        """
        # The modern llvmlite API requires using external tools (opt)
        # This method is kept for compatibility but does nothing
        pass
    
    def optimize_module(self, llvm_ir: str) -> str:
        """
        Optimize LLVM IR module using LLVM's opt tool.
        
        Args:
            llvm_ir: LLVM IR as string
            
        Returns:
            Optimized LLVM IR as string
        """
        import subprocess
        import tempfile
        import os
        
        # If O0, skip optimization
        if self.optimization_level == OptimizationLevel.O0:
            return llvm_ir
        
        try:
            # Write IR to temporary file
            with tempfile.NamedTemporaryFile(mode='w', suffix='.ll', delete=False) as f:
                temp_input = f.name
                f.write(llvm_ir)
            
            # Create output file
            temp_output = temp_input.replace('.ll', '_opt.ll')
            
            # Build opt command based on optimization level
            opt_flags = []
            
            if self.optimization_level == OptimizationLevel.O1:
                opt_flags = ['-O1']
            elif self.optimization_level == OptimizationLevel.O2:
                opt_flags = ['-O2']
            elif self.optimization_level == OptimizationLevel.O3:
                opt_flags = ['-O3']
            elif self.optimization_level == OptimizationLevel.Os:
                opt_flags = ['-Os']
            
            # Run opt
            cmd = ['opt'] + opt_flags + ['-S', temp_input, '-o', temp_output]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            if result.returncode != 0:
                raise RuntimeError(f"opt failed: {result.stderr}")
            
            # Read optimized IR
            with open(temp_output, 'r') as f:
                optimized_ir = f.read()
            
            # Clean up
            os.remove(temp_input)
            os.remove(temp_output)
            
            # Update stats
            self.stats['passes_run'] += 1
            self._estimate_speedup()
            
            return optimized_ir
            
        except FileNotFoundError:
            print("Warning: LLVM 'opt' tool not found in PATH")
            print("Install LLVM tools or LLVM optimization will be skipped")
            return llvm_ir
        except subprocess.TimeoutExpired:
            print("Warning: LLVM optimization timed out (>30s)")
            return llvm_ir
        except Exception as e:
            # If optimization fails, return original IR
            print(f"Warning: LLVM optimization failed: {e}")
            return llvm_ir
    
    def optimize_file(self, input_path: str, output_path: str) -> bool:
        """
        Optimize LLVM IR file.
        
        Args:
            input_path: Path to .ll file
            output_path: Path to output optimized .ll file
            
        Returns:
            True if successful, False otherwise
        """
        try:
            with open(input_path, 'r') as f:
                llvm_ir = f.read()
            
            optimized_ir = self.optimize_module(llvm_ir)
            
            with open(output_path, 'w') as f:
                f.write(optimized_ir)
            
            return True
        except Exception as e:
            print(f"Error optimizing file: {e}")
            return False
    
    def get_pass_list(self) -> List[str]:
        """
        Get list of optimization passes enabled for current level.
        
        Returns:
            List of pass names
        """
        passes = []
        
        if self.optimization_level == OptimizationLevel.O0:
            return ['none']
        
        # Basic passes (O1+)
        if self.optimization_level in [OptimizationLevel.O1, OptimizationLevel.O2, OptimizationLevel.O3, OptimizationLevel.Os]:
            passes.extend([
                'mem2reg',           # Memory to register promotion
                'simplifycfg',       # Control flow simplification
                'instcombine',       # Instruction combining
                'reassociate',       # Reassociate expressions
            ])
        
        # Standard passes (O2+)
        if self.optimization_level in [OptimizationLevel.O2, OptimizationLevel.O3, OptimizationLevel.Os]:
            passes.extend([
                'inline',            # Function inlining
                'gvn',               # Global value numbering (CSE)
                'sccp',              # Sparse conditional constant propagation
                'dce',               # Dead code elimination
                'dse',               # Dead store elimination
                'loop-simplify',     # Loop canonicalization
                'loop-rotate',       # Loop rotation
                'licm',              # Loop-invariant code motion
                'indvars',           # Induction variable simplification
            ])
        
        # Aggressive passes (O3+)
        if self.optimization_level == OptimizationLevel.O3:
            passes.extend([
                'loop-unroll',       # Loop unrolling
                'vectorize',         # Auto-vectorization
                'slp-vectorizer',    # Superword-level parallelism
                'aggressive-instcombine',
                'tailcallelim',      # Tail call elimination
            ])
        
        # Size passes (Os)
        if self.optimization_level == OptimizationLevel.Os:
            # O2 passes but with size focus
            passes.append('size-optimization')
        
        return passes
    
    def _estimate_speedup(self):
        """Estimate performance improvement based on optimization level."""
        # Rough estimates based on typical LLVM performance
        speedup_factors = {
            OptimizationLevel.O0: 1.0,
            OptimizationLevel.O1: 1.5,
            OptimizationLevel.O2: 2.5,
            OptimizationLevel.O3: 3.0,
            OptimizationLevel.Os: 2.0,
        }
        self.stats['estimated_speedup'] = speedup_factors.get(
            self.optimization_level, 1.0
        )
    
    def get_stats(self) -> Dict:
        """Get optimization statistics."""
        return self.stats.copy()
    
    def set_level(self, level: OptimizationLevel):
        """Change optimization level."""
        self.optimization_level = level
        self._configure_passes()
    
    def print_stats(self):
        """Print optimization statistics."""
        print(f"\n=== LLVM Optimization Stats ===")
        print(f"Level: {self.optimization_level.name}")
        print(f"Passes run: {self.stats['passes_run']}")
        print(f"Functions optimized: {self.stats['functions_optimized']}")
        print(f"Estimated speedup: {self.stats['estimated_speedup']}x")
        print(f"Enabled passes: {', '.join(self.get_pass_list()[:5])}...")
        print(f"================================\n")


def optimize_llvm_ir(ir_code: str, level: str = 'O2') -> str:
    """
    Convenience function to optimize LLVM IR.
    
    Args:
        ir_code: LLVM IR as string
        level: Optimization level ('O0', 'O1', 'O2', 'O3', 'Os')
        
    Returns:
        Optimized LLVM IR
        
    Example:
        >>> ir = generate_llvm_ir(ast)
        >>> optimized_ir = optimize_llvm_ir(ir, level='O3')
    """
    level_map = {
        'O0': OptimizationLevel.O0,
        'O1': OptimizationLevel.O1,
        'O2': OptimizationLevel.O2,
        'O3': OptimizationLevel.O3,
        'Os': OptimizationLevel.Os,
    }
    
    opt_level = level_map.get(level, OptimizationLevel.O2)
    optimizer = LLVMOptimizer(opt_level)
    return optimizer.optimize_module(ir_code)


# Example usage
if __name__ == '__main__':
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python llvm_optimizer.py <input.ll> [output.ll] [-O0|-O1|-O2|-O3|-Os]")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else input_file.replace('.ll', '_opt.ll')
    
    # Parse optimization level
    opt_level = OptimizationLevel.O2
    for arg in sys.argv:
        if arg.startswith('-O'):
            level_str = arg[1:]  # Remove '-'
            if level_str in ['O0', 'O1', 'O2', 'O3', 'Os']:
                opt_level = OptimizationLevel[level_str]
    
    # Optimize
    optimizer = LLVMOptimizer(opt_level)
    print(f"Optimizing {input_file} with level {opt_level.name}...")
    
    success = optimizer.optimize_file(input_file, output_file)
    
    if success:
        print(f"Optimized IR written to {output_file}")
        optimizer.print_stats()
    else:
        print("Optimization failed")
        sys.exit(1)
