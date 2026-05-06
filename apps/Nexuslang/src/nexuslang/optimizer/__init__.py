"""
NLPL Compiler Optimization Framework
=====================================

Provides infrastructure for compiler optimizations including:
- Dead code elimination (DCE)
- Constant folding and propagation
- Function inlining
- Peephole optimizations
- Control flow optimizations
"""

from typing import Dict, List, Set, Optional, Any, Tuple
from dataclasses import dataclass
from enum import Enum


class OptimizationLevel(Enum):
    """Optimization levels similar to GCC/Clang."""
    O0 = 0  # No optimization
    O1 = 1  # Basic optimizations
    O2 = 2  # More optimizations
    O3 = 3  # Aggressive optimizations
    Os = 4  # Optimize for size


@dataclass
class OptimizationStats:
    """Statistics about optimizations performed."""
    dead_functions_removed: int = 0
    dead_variables_removed: int = 0
    unreachable_blocks_removed: int = 0
    constants_folded: int = 0
    functions_inlined: int = 0
    total_passes: int = 0
    
    def __str__(self):
        return f"""Optimization Statistics:
  Dead Functions Removed: {self.dead_functions_removed}
  Dead Variables Removed: {self.dead_variables_removed}
  Unreachable Blocks Removed: {self.unreachable_blocks_removed}
  Constants Folded: {self.constants_folded}
  Functions Inlined: {self.functions_inlined}
  Total Optimization Passes: {self.total_passes}"""


class OptimizationPass:
    """Base class for optimization passes."""
    
    def __init__(self, name: str):
        self.name = name
        self.enabled = True
        self.stats = OptimizationStats()
    
    def run(self, ast: Any) -> Any:
        """Run the optimization pass on the AST."""
        raise NotImplementedError(f"Pass {self.name} must implement run()")
    
    def should_run(self, level: OptimizationLevel) -> bool:
        """Check if this pass should run at the given optimization level."""
        return self.enabled


class OptimizationPipeline:
    """Manages the sequence of optimization passes."""
    
    def __init__(self, level: OptimizationLevel = OptimizationLevel.O0):
        self.level = level
        self.passes: List[OptimizationPass] = []
        self.stats = OptimizationStats()
        self.verbose = False
    
    def add_pass(self, pass_: OptimizationPass):
        """Add an optimization pass to the pipeline."""
        self.passes.append(pass_)
    
    def run(self, ast: Any) -> Any:
        """Run all optimization passes on the AST."""
        current_ast = ast
        
        for pass_ in self.passes:
            if not pass_.should_run(self.level):
                continue
            
            if self.verbose:
                print(f"Running optimization pass: {pass_.name}")
            
            current_ast = pass_.run(current_ast)
            
            # Aggregate statistics
            self.stats.total_passes += 1
            self.stats.dead_functions_removed += pass_.stats.dead_functions_removed
            self.stats.dead_variables_removed += pass_.stats.dead_variables_removed
            self.stats.unreachable_blocks_removed += pass_.stats.unreachable_blocks_removed
            self.stats.constants_folded += pass_.stats.constants_folded
            self.stats.functions_inlined += pass_.stats.functions_inlined
        
        return current_ast
    
    def print_stats(self):
        """Print optimization statistics."""
        print(self.stats)


def create_optimization_pipeline(
    level: OptimizationLevel,
    verbose: bool = False,
    interpreter_mode: bool = False,
) -> OptimizationPipeline:
    """
    Create a standard optimization pipeline for the given level.

    O0: No optimizations
    O1: Basic DCE, constant folding, dispatch hints, string interning
    O2: O1 + inlining, strength reduction, loop unrolling, type specialization
    O3: O2 + aggressive inlining, CSE, tail call optimization, all NexusLang opts
    Os: Optimize for code size (small inline threshold, aggressive DCE)

    NLPL-specific passes added at each level (compiled mode only):
    - O1+: DispatchOptimizationPass (inline-cache hints, builtin lowering)
    - O1+: StringInterningPass (intern repeated string literals)
    - O2+: TypeSpecializationPass (specialize generics for known types)

    interpreter_mode:
        When True, restricts the pipeline to passes that actually reduce
        work for the AST-walking interpreter.  Passes that inflate the AST
        (FunctionInlining, LoopUnrolling, TypeSpecialization) or annotate
        nodes the interpreter never reads (DispatchOptimization,
        StringInterning) are skipped because their pre-processing cost
        outweighs any execution benefit in a single-pass interpreter.
    """
    from ..optimizer.dead_code_elimination import DeadCodeEliminationPass
    from ..optimizer.constant_folding import ConstantFoldingPass
    from ..optimizer.function_inlining import FunctionInliningPass
    from ..optimizer.strength_reduction import StrengthReductionPass
    from ..optimizer.loop_unrolling import LoopUnrollingPass
    from ..optimizer.common_subexpression_elimination import CommonSubexpressionEliminationPass
    from ..optimizer.tail_call_optimization import TailCallOptimizationPass
    from ..optimizer.string_interning import StringInterningPass
    from ..optimizer.type_specialization import TypeSpecializationPass
    from ..optimizer.dispatch_optimization import DispatchOptimizationPass

    pipeline = OptimizationPipeline(level)
    pipeline.verbose = verbose

    if level == OptimizationLevel.O0:
        # No optimizations – fastest compile, easiest debugging
        return pipeline

    if interpreter_mode:
        # --- Interpreter-safe pipeline ---
        # Only include passes that reduce the number of AST nodes the
        # interpreter must evaluate.  Passes that:
        #   - deepcopy/duplicate nodes (inlining, unrolling, specialization)
        #   - attach metadata the interpreter never reads (dispatch hints,
        #     string interning)
        #   - insert extra variable declarations (CSE temp vars)
        # are excluded because their transformation overhead exceeds any
        # runtime benefit when the AST is walked exactly once per execution.
        #
        # NOTE: CommonSubexpressionEliminationPass is deliberately excluded
        # at all levels in interpreter mode because it inserts temporary
        # variable declarations that *increase* the AST node count.  In an
        # AST-walking interpreter the extra variable lookups are not cheaper
        # than re-evaluating the expression.
        #
        # TailCallOptimizationPass is also excluded because its
        # transformation only benefits compiled code paths (the interpreter
        # uses Python's own call stack regardless).
        if level.value >= OptimizationLevel.O1.value:
            pipeline.add_pass(ConstantFoldingPass())
            pipeline.add_pass(DeadCodeEliminationPass(aggressive=False))

        if level.value >= OptimizationLevel.O2.value:
            pipeline.add_pass(StrengthReductionPass())
            # Second fold after strength reduction can collapse new constants.
            pipeline.add_pass(ConstantFoldingPass())
            pipeline.add_pass(DeadCodeEliminationPass(aggressive=True))

        if level == OptimizationLevel.O3:
            # At O3 in interpreter mode, apply another round of strength
            # reduction and constant folding to squeeze out remaining
            # evaluations, plus aggressive DCE for any dead code exposed
            # by the previous rounds.
            pipeline.add_pass(StrengthReductionPass())
            pipeline.add_pass(ConstantFoldingPass())
            pipeline.add_pass(DeadCodeEliminationPass(aggressive=True))

        return pipeline

    # --- Compiled-output pipeline (default) ---
    # O1 and above: basic optimizations
    if level.value >= OptimizationLevel.O1.value:
        pipeline.add_pass(ConstantFoldingPass())
        pipeline.add_pass(DeadCodeEliminationPass(aggressive=False))
        # NLPL-specific: annotate call sites with dispatch hints
        pipeline.add_pass(DispatchOptimizationPass(enable_builtin_lowering=True))
        # NLPL-specific: intern duplicate string literals
        pipeline.add_pass(StringInterningPass(min_occurrences=2))

    # O2 and above: more optimizations
    if level.value >= OptimizationLevel.O2.value:
        pipeline.add_pass(StrengthReductionPass())
        pipeline.add_pass(LoopUnrollingPass(max_unroll_count=8))
        pipeline.add_pass(FunctionInliningPass(max_size=50))
        pipeline.add_pass(DeadCodeEliminationPass(aggressive=True))
        # NLPL-specific: create type-specialized function variants
        pipeline.add_pass(TypeSpecializationPass(min_calls=3))
        # Run constant folding again after transformations
        pipeline.add_pass(ConstantFoldingPass())

    # O3: aggressive optimizations
    if level == OptimizationLevel.O3:
        pipeline.add_pass(CommonSubexpressionEliminationPass())
        pipeline.add_pass(TailCallOptimizationPass())
        pipeline.add_pass(FunctionInliningPass(max_size=100, aggressive=True))
        # Multiple passes for maximum optimization
        pipeline.add_pass(StrengthReductionPass())
        pipeline.add_pass(DeadCodeEliminationPass(aggressive=True))
        pipeline.add_pass(ConstantFoldingPass())
        # Additional NLPL-specific: more aggressive specialization at O3
        pipeline.add_pass(TypeSpecializationPass(min_calls=2, max_specs=8))
        pipeline.add_pass(StringInterningPass(min_occurrences=1))

    # Os: optimize for code size
    if level == OptimizationLevel.Os:
        pipeline.add_pass(ConstantFoldingPass())
        pipeline.add_pass(DeadCodeEliminationPass(aggressive=True))
        # Small inline threshold for size optimization
        pipeline.add_pass(FunctionInliningPass(max_size=20))
        # String interning reduces binary size
        pipeline.add_pass(StringInterningPass(min_occurrences=2))

    return pipeline


def int_to_opt_level(n: int) -> OptimizationLevel:
    """Convert an integer (0-3) or special string to OptimizationLevel."""
    mapping = {
        0: OptimizationLevel.O0,
        1: OptimizationLevel.O1,
        2: OptimizationLevel.O2,
        3: OptimizationLevel.O3,
        4: OptimizationLevel.Os,
        's': OptimizationLevel.Os,
    }
    if n not in mapping:
        raise ValueError(f"Unknown optimization level: {n!r}. Use 0-3 or 's'.")
    return mapping[n]


# Export main classes
__all__ = [
    'OptimizationLevel',
    'OptimizationStats',
    'OptimizationPass',
    'OptimizationPipeline',
    'create_optimization_pipeline',
    'int_to_opt_level',
    # Link-Time Optimization
    'LTOStats',
    'LTOUnit',
    'LTOContext',
    'LTOPipeline',
    'lto_optimize',
    'lto_stats_report',
    # Loop Optimizations
    'LoopOptimizationStats',
    'LoopInfo',
    'LoopAnalysisPass',
    'LoopInvariantCodeMotionPass',
    'LoopFusionPass',
    'InductionVariableSimplificationPass',
    'LoopStrengthReductionPass',
    'LoopOptimizationPipeline',
    'loop_optimize',
]


# ---------------------------------------------------------------------------
# Link-Time Optimization convenience re-exports
# ---------------------------------------------------------------------------
def _import_lto():
    """Lazy import helper — avoids circular imports during pipeline setup."""
    from ..optimizer.lto import (  # noqa: F401
        LTOStats, LTOUnit, LTOContext, LTOPipeline,
        lto_optimize, lto_stats_report,
        SymbolReferenceAnalysisPass, CrossModuleDCEPass,
        CrossModuleInliningPass, ConstantPropagationPass,
        DeadImportEliminationPass, RedundantExportPass,
    )
    return {
        'LTOStats': LTOStats,
        'LTOUnit': LTOUnit,
        'LTOContext': LTOContext,
        'LTOPipeline': LTOPipeline,
        'lto_optimize': lto_optimize,
        'lto_stats_report': lto_stats_report,
        'SymbolReferenceAnalysisPass': SymbolReferenceAnalysisPass,
        'CrossModuleDCEPass': CrossModuleDCEPass,
        'CrossModuleInliningPass': CrossModuleInliningPass,
        'ConstantPropagationPass': ConstantPropagationPass,
        'DeadImportEliminationPass': DeadImportEliminationPass,
        'RedundantExportPass': RedundantExportPass,
    }


def _import_loop_opts():
    """Lazy import helper for loop optimization names."""
    from ..optimizer.loop_optimizations import (  # noqa: F401
        LoopOptimizationStats, LoopInfo,
        LoopAnalysisPass, LoopInvariantCodeMotionPass,
        LoopFusionPass, InductionVariableSimplificationPass,
        LoopStrengthReductionPass, LoopOptimizationPipeline,
        loop_optimize,
    )
    return {
        'LoopOptimizationStats': LoopOptimizationStats,
        'LoopInfo': LoopInfo,
        'LoopAnalysisPass': LoopAnalysisPass,
        'LoopInvariantCodeMotionPass': LoopInvariantCodeMotionPass,
        'LoopFusionPass': LoopFusionPass,
        'InductionVariableSimplificationPass': InductionVariableSimplificationPass,
        'LoopStrengthReductionPass': LoopStrengthReductionPass,
        'LoopOptimizationPipeline': LoopOptimizationPipeline,
        'loop_optimize': loop_optimize,
    }


def __getattr__(name: str):
    """Module-level __getattr__ for lazy LTO and loop optimization imports."""
    lto_names = {
        'LTOStats', 'LTOUnit', 'LTOContext', 'LTOPipeline',
        'lto_optimize', 'lto_stats_report',
        'SymbolReferenceAnalysisPass', 'CrossModuleDCEPass',
        'CrossModuleInliningPass', 'ConstantPropagationPass',
        'DeadImportEliminationPass', 'RedundantExportPass',
    }
    loop_names = {
        'LoopOptimizationStats', 'LoopInfo',
        'LoopAnalysisPass', 'LoopInvariantCodeMotionPass',
        'LoopFusionPass', 'InductionVariableSimplificationPass',
        'LoopStrengthReductionPass', 'LoopOptimizationPipeline',
        'loop_optimize',
    }
    if name in lto_names:
        return _import_lto()[name]
    if name in loop_names:
        return _import_loop_opts()[name]
    raise AttributeError(f"module 'nexuslang.optimizer' has no attribute {name!r}")
