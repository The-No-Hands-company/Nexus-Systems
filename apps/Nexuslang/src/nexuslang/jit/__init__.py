"""
NLPL JIT Compiler
=================

Just-In-Time compilation for hot functions using LLVM.

Key Features:
- Hot function detection based on call count thresholds
- LLVM IR generation for NexusLang functions
- JIT compilation to native machine code
- Hybrid interpreter/JIT execution mode
- Transparent fallback to interpreter on JIT failure
- Tiered compilation (interpreter → baseline → optimizing JIT)
- Runtime type feedback for speculative optimization

Usage:
    from nexuslang.jit import JITCompiler, enable_jit

    # Enable JIT for an interpreter
    jit = enable_jit(interpreter, hot_threshold=100)

    # Enable tiered compilation (recommended for long-running programs)
    from nexuslang.jit import TieredCompiler, TypeFeedbackCollector

    feedback = TypeFeedbackCollector()
    feedback.attach(interpreter)

    tiered = TieredCompiler(tier1_threshold=50, tier2_threshold=500)
    tiered.attach_to_interpreter(interpreter)

    # Or create manually
    jit = JITCompiler(hot_threshold=100)
    jit.attach_to_interpreter(interpreter)
"""

from .jit_compiler import JITCompiler, JITStats
from .hot_function_detector import HotFunctionDetector
from .tiered_compiler import TieredCompiler, ExecutionTier, FunctionTierState
from .type_feedback import TypeFeedbackCollector, FunctionFeedback, Polymorphism
from .code_gen import NLPLCodeGenerator, JITGuardFailed, CodeGenError
from .native_jit import NativeFunctionJIT, NativeCompileError

def enable_jit(interpreter, hot_threshold=100, optimization_level=2):
    """
    Enable JIT compilation for an interpreter instance.

    Args:
        interpreter: The Interpreter instance to attach JIT to
        hot_threshold: Number of calls before function is considered "hot"
        optimization_level: LLVM optimization level (0-3)

    Returns:
        JITCompiler instance
    """
    jit = JITCompiler(
        hot_threshold=hot_threshold,
        optimization_level=optimization_level
    )
    jit.attach_to_interpreter(interpreter)
    return jit


def enable_tiered_jit(
    interpreter,
    tier1_threshold=50,
    tier2_threshold=500,
    with_type_feedback=True,
):
    """
    Enable tiered JIT compilation with optional type feedback.

    This is the recommended way to enable JIT for long-running programs.
    It starts functions in the interpreter and progressively promotes them
    to faster tiers based on call counts.

    Args:
        interpreter:        The Interpreter instance to attach.
        tier1_threshold:    Calls before Tier-1 (baseline JIT).  Default 50.
        tier2_threshold:    Calls before Tier-2 (optimizing JIT). Default 500.
        with_type_feedback: Also attach a TypeFeedbackCollector.

    Returns:
        (TieredCompiler, TypeFeedbackCollector | None)
    """
    feedback = None
    if with_type_feedback:
        feedback = TypeFeedbackCollector()
        feedback.attach(interpreter)

    tiered = TieredCompiler(
        tier1_threshold=tier1_threshold,
        tier2_threshold=tier2_threshold,
    )
    tiered.attach_to_interpreter(interpreter)
    return tiered, feedback


__all__ = [
    'JITCompiler',
    'JITStats',
    'HotFunctionDetector',
    'enable_jit',
    'TieredCompiler',
    'ExecutionTier',
    'FunctionTierState',
    'TypeFeedbackCollector',
    'FunctionFeedback',
    'Polymorphism',
    'enable_tiered_jit',
    'NLPLCodeGenerator',
    'JITGuardFailed',
    'CodeGenError',
    'NativeFunctionJIT',
    'NativeCompileError',
]
