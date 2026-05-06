"""
Runtime package for NexusLang.
"""

from nexuslang.runtime.runtime import Runtime


def _import_gc():
    """Lazy import helper for GC names — avoids heavy imports at package load time."""
    from nexuslang.runtime.gc import (  # noqa: F401
        GCColor, GCStats, GCError, NLPLObject,
        TricolorMarkSweepGC, Generation, GenerationalGC,
        IncrementalGC, GCConfig, GarbageCollector,
    )
    return {
        'GCColor': GCColor,
        'GCStats': GCStats,
        'GCError': GCError,
        'NLPLObject': NLPLObject,
        'TricolorMarkSweepGC': TricolorMarkSweepGC,
        'Generation': Generation,
        'GenerationalGC': GenerationalGC,
        'IncrementalGC': IncrementalGC,
        'GCConfig': GCConfig,
        'GarbageCollector': GarbageCollector,
    }


_GC_NAMES = frozenset({
    'GCColor', 'GCStats', 'GCError', 'NLPLObject',
    'TricolorMarkSweepGC', 'Generation', 'GenerationalGC',
    'IncrementalGC', 'GCConfig', 'GarbageCollector',
})


def __getattr__(name: str):
    """Module-level __getattr__ for lazy GC imports."""
    if name in _GC_NAMES:
        return _import_gc()[name]
    raise AttributeError(f"module 'nexuslang.runtime' has no attribute {name!r}")


__all__ = [
    'Runtime',
    # Garbage Collection
    'GCColor',
    'GCStats',
    'GCError',
    'NLPLObject',
    'TricolorMarkSweepGC',
    'Generation',
    'GenerationalGC',
    'IncrementalGC',
    'GCConfig',
    'GarbageCollector',
]
