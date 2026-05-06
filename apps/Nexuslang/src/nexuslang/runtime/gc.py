"""
Garbage Collection for NexusLang
============================

Provides optional automatic memory management as a complement to NLPL's
manual malloc/free and Rc<T> reference counting:

- ``NLPLObject``             - Base class for GC-managed objects (tri-color state)
- ``TricolorMarkSweepGC``   - Precise tri-color mark-and-sweep GC
- ``GenerationalGC``         - Two-generation GC (young / old promotion)
- ``IncrementalGC``          - Incremental mark-and-sweep (bounded pause times)
- ``GCConfig``               - Configuration dataclass
- ``GarbageCollector``       - Facade that selects backend from GCConfig or CLI flag

Enabling GC
-----------
Pass ``--enable-gc`` (mark-sweep) or ``--enable-gc=<mode>`` where *mode* is one
of ``mark-sweep``, ``generational``, or ``incremental``::

    gc = GarbageCollector.from_flag("--enable-gc=generational")
    obj = gc.allocate(value=42, type_name="Integer")
    gc.add_root(obj)
    stats = gc.collect()

Without the flag (or with ``mode="none"``) the facade is a no-op and
``allocate()`` returns plain ``NLPLObject`` instances without tracking them.
"""
from __future__ import annotations

import sys
import time
from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Dict, List, Optional, Set


# ---------------------------------------------------------------------------
# GC colour (tri-colour marking)
# ---------------------------------------------------------------------------

class GCColor(Enum):
    """Tri-colour invariant used during the mark phase."""
    WHITE = "white"   # Not yet reached; candidate for collection
    GRAY  = "gray"    # Reached, but outgoing references not yet scanned
    BLACK = "black"   # Reached and all references fully scanned


# ---------------------------------------------------------------------------
# Statistics
# ---------------------------------------------------------------------------

@dataclass
class GCStats:
    """Cumulative statistics produced by a GC collection cycle."""
    collections: int = 0
    objects_collected: int = 0
    bytes_freed: int = 0
    max_heap_bytes: int = 0
    total_pause_ms: float = 0.0
    pause_times_ms: list = field(default_factory=list)
    young_gen_collections: int = 0
    old_gen_collections: int = 0

    def __add__(self, other: "GCStats") -> "GCStats":
        return GCStats(
            collections=self.collections + other.collections,
            objects_collected=self.objects_collected + other.objects_collected,
            bytes_freed=self.bytes_freed + other.bytes_freed,
            max_heap_bytes=max(self.max_heap_bytes, other.max_heap_bytes),
            total_pause_ms=self.total_pause_ms + other.total_pause_ms,
            pause_times_ms=self.pause_times_ms + other.pause_times_ms,
            young_gen_collections=self.young_gen_collections + other.young_gen_collections,
            old_gen_collections=self.old_gen_collections + other.old_gen_collections,
        )

    @property
    def avg_pause_ms(self) -> float:
        """Average pause duration in milliseconds, or 0.0 if no pauses recorded."""
        if not self.pause_times_ms:
            return 0.0
        return sum(self.pause_times_ms) / len(self.pause_times_ms)

    def __str__(self) -> str:
        return (
            "GC Statistics:\n"
            f"  Collections:          {self.collections}\n"
            f"  Objects Collected:    {self.objects_collected}\n"
            f"  Bytes Freed:          {self.bytes_freed}\n"
            f"  Max Heap Bytes:       {self.max_heap_bytes}\n"
            f"  Total Pause (ms):     {self.total_pause_ms:.3f}\n"
            f"  Avg Pause (ms):       {self.avg_pause_ms:.3f}\n"
            f"  Young Gen Collections: {self.young_gen_collections}\n"
            f"  Old Gen Collections:  {self.old_gen_collections}"
        )


# ---------------------------------------------------------------------------
# GCError
# ---------------------------------------------------------------------------

class GCError(Exception):
    """Raised for invalid GC operations (e.g. freeing a non-heap object)."""


# ---------------------------------------------------------------------------
# Managed object
# ---------------------------------------------------------------------------

def _estimate_size(value: Any) -> int:
    """Return a rough byte-size estimate for a Python object."""
    try:
        return max(16, sys.getsizeof(value))
    except TypeError:
        return 64


class NLPLObject:
    """
    Base class for GC-managed objects.

    Each instance carries GC metadata (colour, age, pinned flag, outgoing
    references) alongside an arbitrary ``value`` payload.

    Attributes:
        value:        The payload stored in this GC-managed cell.
        type_name:    Human-readable type name used for diagnostics.
        _gc_id:       Unique monotonically increasing object identifier.
        _gc_color:    Tri-colour marking state (WHITE / GRAY / BLACK).
        _gc_refs:     List of directly referenced ``NLPLObject`` instances.
        _gc_size:     Estimated byte size used for heap accounting.
        _gc_age:      Number of collection cycles survived (for generational GC).
        _gc_pinned:   If True the object is never collected (root anchor).
    """

    _id_counter: int = 0

    def __init__(
        self,
        value: Any = None,
        type_name: str = "object",
        size: int = 0,
        refs: Optional[List["NLPLObject"]] = None,
    ) -> None:
        NLPLObject._id_counter += 1
        self._gc_id: int = NLPLObject._id_counter
        self._gc_color: GCColor = GCColor.WHITE
        self._gc_refs: List["NLPLObject"] = list(refs) if refs else []
        self._gc_size: int = size if size > 0 else _estimate_size(value)
        self._gc_age: int = 0
        self._gc_pinned: bool = False
        self.value: Any = value
        self.type_name: str = type_name

    # -- Reference management -----------------------------------------------

    def add_ref(self, obj: "NLPLObject") -> None:
        """Record that this object holds a reference to *obj*."""
        if obj not in self._gc_refs:
            self._gc_refs.append(obj)

    def remove_ref(self, obj: "NLPLObject") -> None:
        """Remove a previously recorded reference to *obj*."""
        if obj in self._gc_refs:
            self._gc_refs.remove(obj)

    # -- Pinning -------------------------------------------------------------

    def pin(self) -> None:
        """Pin the object so the GC never collects it."""
        self._gc_pinned = True

    def unpin(self) -> None:
        """Allow the GC to collect the object again."""
        self._gc_pinned = False

    def __repr__(self) -> str:
        return (
            f"<NLPLObject id={self._gc_id} type={self.type_name}"
            f" color={self._gc_color.value} age={self._gc_age}>"
        )


# ---------------------------------------------------------------------------
# Tri-colour mark-and-sweep GC
# ---------------------------------------------------------------------------

class TricolorMarkSweepGC:
    """
    Precise tri-colour mark-and-sweep garbage collector.

    Algorithm
    ---------
    Mark phase (``_mark_phase``):
    1. All heap objects are coloured WHITE.
    2. All root objects are coloured GRAY and placed on the work-list.
    3. While the work-list is non-empty, pop a GRAY object, colour it BLACK,
       then colour every WHITE reference GRAY and push it onto the work-list.

    Sweep phase (``_sweep_phase``):
    4. Iterate over the heap; every WHITE (unreachable) and non-pinned object
       is removed and its size added to the freed-bytes counter.

    The collector is stop-the-world: ``collect()`` runs both phases atomically.
    """

    def __init__(self, heap_limit_bytes: int = 256 * 1024 * 1024) -> None:
        self._heap: Dict[int, NLPLObject] = {}      # gc_id -> object
        self._roots: Set[int] = set()               # gc_ids of root objects
        self.heap_limit_bytes = heap_limit_bytes
        self._cumulative_stats = GCStats()

    # -- Public interface ----------------------------------------------------

    def allocate(
        self,
        value: Any = None,
        type_name: str = "object",
        size: int = 0,
        refs: Optional[List[NLPLObject]] = None,
    ) -> NLPLObject:
        """Allocate a new GC-managed object and register it on the heap."""
        obj = NLPLObject(value=value, type_name=type_name, size=size, refs=refs)
        self._heap[obj._gc_id] = obj
        return obj

    def add_root(self, obj: NLPLObject) -> None:
        """Mark *obj* as a GC root (reachable from the stack / global scope)."""
        if obj._gc_id not in self._heap:
            raise GCError(f"Object {obj!r} is not registered on this heap.")
        self._roots.add(obj._gc_id)

    def remove_root(self, obj: NLPLObject) -> None:
        """Remove a GC root (object may now be collected if unreachable)."""
        self._roots.discard(obj._gc_id)

    def collect(self) -> GCStats:
        """Run a full mark-and-sweep collection cycle. Returns cycle stats."""
        t0 = time.perf_counter()
        self._mark_phase()
        freed_count, freed_bytes = self._sweep_phase()
        pause_ms = (time.perf_counter() - t0) * 1000.0

        heap_bytes = sum(o._gc_size for o in self._heap.values())
        stats = GCStats(
            collections=1,
            objects_collected=freed_count,
            bytes_freed=freed_bytes,
            max_heap_bytes=heap_bytes,
            total_pause_ms=pause_ms,
            pause_times_ms=[pause_ms],
        )
        self._cumulative_stats = self._cumulative_stats + stats
        return stats

    @property
    def cumulative_stats(self) -> GCStats:
        return self._cumulative_stats

    def heap_size(self) -> int:
        """Total bytes occupied by live objects on the heap."""
        return sum(o._gc_size for o in self._heap.values())

    def live_objects(self) -> int:
        """Number of objects currently tracked on the heap."""
        return len(self._heap)

    def is_heap_full(self) -> bool:
        """Return True when the heap byte limit is reached."""
        return self.heap_size() >= self.heap_limit_bytes

    # -- Mark phase ----------------------------------------------------------

    def _mark_phase(self) -> None:
        # 1. Colour all heap objects WHITE
        for obj in self._heap.values():
            obj._gc_color = GCColor.WHITE

        # 2. Colour roots GRAY and push onto work-list
        work_list: list[NLPLObject] = []
        for root_id in self._roots:
            obj = self._heap.get(root_id)
            if obj is not None:
                obj._gc_color = GCColor.GRAY
                work_list.append(obj)
        # Pinned objects are always reachable
        for obj in self._heap.values():
            if obj._gc_pinned and obj._gc_color == GCColor.WHITE:
                obj._gc_color = GCColor.GRAY
                work_list.append(obj)

        # 3. Tri-colour traversal
        while work_list:
            current = work_list.pop()
            current._gc_color = GCColor.BLACK
            for ref in current._gc_refs:
                if ref._gc_id in self._heap and ref._gc_color == GCColor.WHITE:
                    ref._gc_color = GCColor.GRAY
                    work_list.append(ref)

    # -- Sweep phase ---------------------------------------------------------

    def _sweep_phase(self) -> tuple:
        dead_ids = [
            oid for oid, obj in self._heap.items()
            if obj._gc_color == GCColor.WHITE and not obj._gc_pinned
        ]
        freed_bytes = sum(self._heap[oid]._gc_size for oid in dead_ids)
        for oid in dead_ids:
            del self._heap[oid]
        return len(dead_ids), freed_bytes


# ---------------------------------------------------------------------------
# Generational GC
# ---------------------------------------------------------------------------

@dataclass
class Generation:
    """A single generation in a generational garbage collector."""
    objects: list = field(default_factory=list)
    threshold: int = 100    # Collection triggered when len >= threshold
    max_age: int = 3        # Survive this many young collections → promote

    @property
    def size(self) -> int:
        return len(self.objects)

    @property
    def is_full(self) -> bool:
        return len(self.objects) >= self.threshold

    def byte_size(self) -> int:
        return sum(o._gc_size for o in self.objects)


class GenerationalGC:
    """
    Two-generation garbage collector (young / old).

    Young generation (minor GC):
    - New objects are allocated into the young generation.
    - When the young generation is full, a minor collection runs.
    - Objects that survive ``young.max_age`` collections are promoted to old.

    Old generation (major GC):
    - Long-lived objects live here.
    - ``collect_full()`` evacuates young → old and then sweeps the entire heap.

    Cross-generation references are handled conservatively: during a minor
    collection, all old-generation objects are treated as additional roots.
    """

    def __init__(
        self,
        young_threshold: int = 100,
        old_threshold: int = 500,
        max_age: int = 3,
    ) -> None:
        self.young = Generation(threshold=young_threshold, max_age=max_age)
        self.old = Generation(threshold=old_threshold, max_age=max_age + 2)
        self._roots: Set[int] = set()
        self._all_objects: Dict[int, NLPLObject] = {}
        self._cumulative_stats = GCStats()

    # -- Public interface ----------------------------------------------------

    def allocate(
        self,
        value: Any = None,
        type_name: str = "object",
        size: int = 0,
        refs: Optional[List[NLPLObject]] = None,
    ) -> NLPLObject:
        obj = NLPLObject(value=value, type_name=type_name, size=size, refs=refs)
        self._all_objects[obj._gc_id] = obj
        self.young.objects.append(obj)
        return obj

    def add_root(self, obj: NLPLObject) -> None:
        if obj._gc_id not in self._all_objects:
            raise GCError(f"Object {obj!r} is not tracked by this GC.")
        self._roots.add(obj._gc_id)

    def remove_root(self, obj: NLPLObject) -> None:
        self._roots.discard(obj._gc_id)

    def collect_young(self) -> GCStats:
        """Run a minor (young-generation only) collection."""
        t0 = time.perf_counter()

        # Roots include explicit roots + all old-generation objects (conservative)
        root_ids: Set[int] = set(self._roots)
        for obj in self.old.objects:
            root_ids.add(obj._gc_id)

        reachable = self._trace_reachable(root_ids, self.young.objects)

        freed_count = 0
        freed_bytes = 0
        survivors: list[NLPLObject] = []
        promoted: list[NLPLObject] = []

        for obj in self.young.objects:
            if obj._gc_id in reachable or obj._gc_pinned:
                obj._gc_age += 1
                if obj._gc_age >= self.young.max_age:
                    promoted.append(obj)
                else:
                    survivors.append(obj)
            else:
                freed_count += 1
                freed_bytes += obj._gc_size
                del self._all_objects[obj._gc_id]

        self.young.objects = survivors
        for obj in promoted:
            self.old.objects.append(obj)

        pause_ms = (time.perf_counter() - t0) * 1000.0
        stats = GCStats(
            collections=1,
            objects_collected=freed_count,
            bytes_freed=freed_bytes,
            max_heap_bytes=sum(o._gc_size for o in self._all_objects.values()),
            total_pause_ms=pause_ms,
            pause_times_ms=[pause_ms],
            young_gen_collections=1,
        )
        self._cumulative_stats = self._cumulative_stats + stats
        return stats

    def collect_full(self) -> GCStats:
        """Run a full (major) collection across both generations."""
        t0 = time.perf_counter()
        all_objs = list(self._all_objects.values())
        reachable = self._trace_reachable(self._roots, all_objs)

        freed_count = 0
        freed_bytes = 0
        live_young: list[NLPLObject] = []
        live_old: list[NLPLObject] = []

        for obj in all_objs:
            if obj._gc_id in reachable or obj._gc_pinned:
                if obj in self.young.objects:
                    live_young.append(obj)
                else:
                    live_old.append(obj)
            else:
                freed_count += 1
                freed_bytes += obj._gc_size
                del self._all_objects[obj._gc_id]

        self.young.objects = live_young
        self.old.objects = live_old

        pause_ms = (time.perf_counter() - t0) * 1000.0
        stats = GCStats(
            collections=1,
            objects_collected=freed_count,
            bytes_freed=freed_bytes,
            max_heap_bytes=sum(o._gc_size for o in self._all_objects.values()),
            total_pause_ms=pause_ms,
            pause_times_ms=[pause_ms],
            old_gen_collections=1,
        )
        self._cumulative_stats = self._cumulative_stats + stats
        return stats

    @property
    def cumulative_stats(self) -> GCStats:
        return self._cumulative_stats

    def live_objects(self) -> int:
        return len(self._all_objects)

    # -- Internal tracer -----------------------------------------------------

    def _trace_reachable(
        self, root_ids: Set[int], candidate_pool: list
    ) -> Set[int]:
        """BFS from root_ids across all objects; return the set of reachable ids."""
        candidate_map = {obj._gc_id: obj for obj in self._all_objects.values()}
        reachable: Set[int] = set()
        work: list[int] = []
        for rid in root_ids:
            if rid in candidate_map and rid not in reachable:
                reachable.add(rid)
                work.append(rid)
        while work:
            oid = work.pop()
            obj = candidate_map.get(oid)
            if obj is None:
                continue
            for ref in obj._gc_refs:
                if ref._gc_id in candidate_map and ref._gc_id not in reachable:
                    reachable.add(ref._gc_id)
                    work.append(ref._gc_id)
        return reachable


# ---------------------------------------------------------------------------
# Incremental GC
# ---------------------------------------------------------------------------

class _IncrementalState(Enum):
    IDLE      = "idle"
    MARKING   = "marking"
    SWEEPING  = "sweeping"


class IncrementalGC:
    """
    Incremental tri-colour mark-and-sweep GC.

    Instead of stopping the world for a full collection, ``step()`` processes
    at most ``step_budget`` objects per call. The mutator can interleave work
    between steps, keeping individual pause times short.

    A collection cycle proceeds through three states:
    - IDLE:     No collection in progress. ``step()`` is a no-op (returns False).
    - MARKING:  Work-list driven tri-colour traversal, bounded by ``step_budget``.
    - SWEEPING: Linear sweep of dead objects, bounded by ``step_budget``.

    Call ``start_collection()`` to begin a cycle, then call ``step()``
    repeatedly until it returns ``True`` (cycle complete). Alternatively,
    ``full_collect()`` runs steps until completion.
    """

    def __init__(
        self,
        step_budget: int = 1000,
        heap_limit_bytes: int = 256 * 1024 * 1024,
    ) -> None:
        self.step_budget = step_budget
        self.heap_limit_bytes = heap_limit_bytes
        self._heap: Dict[int, NLPLObject] = {}
        self._roots: Set[int] = set()
        self._state = _IncrementalState.IDLE
        self._work_list: list[NLPLObject] = []
        self._sweep_candidates: list[int] = []
        self._cycle_freed_count = 0
        self._cycle_freed_bytes = 0
        self._cycle_t0: float = 0.0
        self._cumulative_stats = GCStats()

    # -- Public interface ----------------------------------------------------

    def allocate(
        self,
        value: Any = None,
        type_name: str = "object",
        size: int = 0,
        refs: Optional[List[NLPLObject]] = None,
    ) -> NLPLObject:
        obj = NLPLObject(value=value, type_name=type_name, size=size, refs=refs)
        self._heap[obj._gc_id] = obj
        return obj

    def add_root(self, obj: NLPLObject) -> None:
        if obj._gc_id not in self._heap:
            raise GCError(f"Object {obj!r} is not on this heap.")
        self._roots.add(obj._gc_id)

    def remove_root(self, obj: NLPLObject) -> None:
        self._roots.discard(obj._gc_id)

    def is_collecting(self) -> bool:
        """Return True while a collection cycle is in progress."""
        return self._state != _IncrementalState.IDLE

    def start_collection(self) -> None:
        """Begin a new incremental collection cycle."""
        if self._state != _IncrementalState.IDLE:
            return  # Already in progress
        self._cycle_t0 = time.perf_counter()
        self._cycle_freed_count = 0
        self._cycle_freed_bytes = 0

        # Colour all objects WHITE; seed work-list from roots
        for obj in self._heap.values():
            obj._gc_color = GCColor.WHITE
        self._work_list = []
        for root_id in self._roots:
            obj = self._heap.get(root_id)
            if obj is not None:
                obj._gc_color = GCColor.GRAY
                self._work_list.append(obj)
        for obj in self._heap.values():
            if obj._gc_pinned and obj._gc_color == GCColor.WHITE:
                obj._gc_color = GCColor.GRAY
                self._work_list.append(obj)
        self._state = _IncrementalState.MARKING

    def step(self) -> bool:
        """
        Process up to ``step_budget`` units of GC work.

        Returns True when the current collection cycle has completed (the
        caller can read ``cumulative_stats`` for the latest cycle result).
        Returns False if work remains or no cycle is active.
        """
        if self._state == _IncrementalState.IDLE:
            return False

        budget = self.step_budget

        if self._state == _IncrementalState.MARKING:
            while self._work_list and budget > 0:
                obj = self._work_list.pop()
                obj._gc_color = GCColor.BLACK
                for ref in obj._gc_refs:
                    if ref._gc_id in self._heap and ref._gc_color == GCColor.WHITE:
                        ref._gc_color = GCColor.GRAY
                        self._work_list.append(ref)
                budget -= 1
            if not self._work_list:
                # Marking complete; collect sweep candidates
                self._sweep_candidates = [
                    oid for oid, obj in self._heap.items()
                    if obj._gc_color == GCColor.WHITE and not obj._gc_pinned
                ]
                self._state = _IncrementalState.SWEEPING

        if self._state == _IncrementalState.SWEEPING:
            while self._sweep_candidates and budget > 0:
                oid = self._sweep_candidates.pop()
                obj = self._heap.pop(oid, None)
                if obj is not None:
                    self._cycle_freed_count += 1
                    self._cycle_freed_bytes += obj._gc_size
                budget -= 1
            if not self._sweep_candidates:
                self._finish_cycle()
                return True

        return False

    def full_collect(self) -> GCStats:
        """Run a complete incremental collection cycle synchronously."""
        self.start_collection()
        while not self.step():
            pass
        return self._last_cycle_stats

    def live_objects(self) -> int:
        return len(self._heap)

    @property
    def cumulative_stats(self) -> GCStats:
        return self._cumulative_stats

    # -- Internal helpers ----------------------------------------------------

    def _finish_cycle(self) -> None:
        pause_ms = (time.perf_counter() - self._cycle_t0) * 1000.0
        heap_bytes = sum(o._gc_size for o in self._heap.values())
        cycle_stats = GCStats(
            collections=1,
            objects_collected=self._cycle_freed_count,
            bytes_freed=self._cycle_freed_bytes,
            max_heap_bytes=heap_bytes,
            total_pause_ms=pause_ms,
            pause_times_ms=[pause_ms],
        )
        self._last_cycle_stats = cycle_stats
        self._cumulative_stats = self._cumulative_stats + cycle_stats
        self._state = _IncrementalState.IDLE
        self._work_list = []
        self._sweep_candidates = []


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

@dataclass
class GCConfig:
    """
    Configuration for the NexusLang garbage collector.

    Attributes:
        mode:                  Backend: ``"none"`` | ``"mark-sweep"`` |
                               ``"generational"`` | ``"incremental"``.
        heap_limit_mb:         Maximum heap size in mebibytes.
        young_gen_limit_mb:    Young generation threshold for generational GC.
        gc_trigger_threshold:  Fraction of heap_limit at which GC is triggered
                               (0 < threshold <= 1).
        incremental_step_budget: Objects processed per incremental step.
        enable_statistics:     Accumulate and expose GC statistics.
        concurrent:            Reserved for future concurrent GC backend.
    """
    mode: str = "mark-sweep"
    heap_limit_mb: int = 256
    young_gen_limit_mb: int = 16
    gc_trigger_threshold: float = 0.75
    incremental_step_budget: int = 1000
    enable_statistics: bool = True
    concurrent: bool = False

    _VALID_MODES = frozenset({"none", "mark-sweep", "generational", "incremental"})

    def __post_init__(self) -> None:
        if self.mode not in self._VALID_MODES:
            raise ValueError(
                f"Invalid GC mode {self.mode!r}. "
                f"Must be one of {sorted(self._VALID_MODES)}."
            )
        if not (0.0 < self.gc_trigger_threshold <= 1.0):
            raise ValueError(
                "gc_trigger_threshold must satisfy 0 < threshold <= 1, "
                f"got {self.gc_trigger_threshold!r}."
            )
        if self.heap_limit_mb <= 0:
            raise ValueError(f"heap_limit_mb must be positive, got {self.heap_limit_mb}.")
        if self.young_gen_limit_mb <= 0:
            raise ValueError(f"young_gen_limit_mb must be positive, got {self.young_gen_limit_mb}.")
        if self.incremental_step_budget <= 0:
            raise ValueError(
                f"incremental_step_budget must be positive, got {self.incremental_step_budget}."
            )


# ---------------------------------------------------------------------------
# GarbageCollector facade
# ---------------------------------------------------------------------------

class GarbageCollector:
    """
    Main GC facade.

    Selects the appropriate backend (TricolorMarkSweepGC, GenerationalGC, or
    IncrementalGC) based on ``GCConfig.mode``. When ``mode="none"`` the facade
    is a no-op: ``allocate()`` returns untracked ``NLPLObject`` instances and
    ``collect()`` returns an empty ``GCStats``.

    Typical usage::

        gc = GarbageCollector(GCConfig(mode="generational"))
        obj = gc.allocate(value=[1, 2, 3], type_name="List")
        gc.add_root(obj)
        stats = gc.collect()

    CLI flag parsing::

        gc = GarbageCollector.from_flag("--enable-gc=incremental")
    """

    def __init__(self, config: Optional[GCConfig] = None) -> None:
        self.config = config or GCConfig()
        self._enabled = self.config.mode != "none"
        self._backend = self._create_backend(self.config)

    # -- Factory -------------------------------------------------------------

    @classmethod
    def from_flag(cls, gc_flag: str) -> "GarbageCollector":
        """
        Parse a ``--enable-gc`` or ``--enable-gc=<mode>`` CLI flag string and
        return a configured ``GarbageCollector`` instance.

        Accepted forms:
        - ``--enable-gc``              → mark-sweep
        - ``--enable-gc=mark-sweep``   → mark-sweep
        - ``--enable-gc=generational`` → generational
        - ``--enable-gc=incremental``  → incremental
        - ``--disable-gc``             → no GC (mode="none")
        """
        flag = gc_flag.strip()
        if flag == "--disable-gc":
            return cls(GCConfig(mode="none"))
        if flag == "--enable-gc":
            return cls(GCConfig(mode="mark-sweep"))
        if flag.startswith("--enable-gc="):
            mode = flag[len("--enable-gc="):]
            return cls(GCConfig(mode=mode))
        raise ValueError(
            f"Unrecognised GC flag {gc_flag!r}. "
            "Expected '--enable-gc', '--enable-gc=<mode>', or '--disable-gc'."
        )

    # -- Core operations -----------------------------------------------------

    def allocate(
        self,
        value: Any = None,
        type_name: str = "object",
        size: int = 0,
        refs: Optional[List[NLPLObject]] = None,
    ) -> NLPLObject:
        """
        Allocate a new GC-managed object.

        When GC is disabled the object is created but not tracked; manual
        lifecycle management applies.
        """
        if self._backend is None:
            return NLPLObject(value=value, type_name=type_name, size=size, refs=refs)
        return self._backend.allocate(value=value, type_name=type_name, size=size, refs=refs)

    def add_root(self, obj: NLPLObject) -> None:
        """Mark *obj* as a GC root. Raises ``GCError`` if GC is disabled."""
        if self._backend is None:
            raise GCError("Cannot add GC roots when GC is disabled (mode='none').")
        self._backend.add_root(obj)

    def remove_root(self, obj: NLPLObject) -> None:
        """Remove a GC root. No-op when GC is disabled."""
        if self._backend is not None:
            self._backend.remove_root(obj)

    def collect(self) -> GCStats:
        """
        Trigger a collection cycle and return the cycle statistics.
        Returns an empty ``GCStats`` when GC is disabled.
        """
        if self._backend is None:
            return GCStats()
        if isinstance(self._backend, IncrementalGC):
            return self._backend.full_collect()
        return self._backend.collect()  # type: ignore[union-attr]

    def stats(self) -> GCStats:
        """Return cumulative statistics across all collection cycles."""
        if self._backend is None:
            return GCStats()
        return self._backend.cumulative_stats

    def live_objects(self) -> int:
        """Return the number of live objects on the managed heap. 0 when disabled."""
        if self._backend is None:
            return 0
        return self._backend.live_objects()

    # -- Dynamic reconfiguration ---------------------------------------------

    def enable_gc(self, mode: str = "mark-sweep") -> None:
        """Switch the GC on (or change mode). Existing allocation tracking is reset."""
        self.config = GCConfig(mode=mode)
        self._backend = self._create_backend(self.config)
        self._enabled = True

    def disable_gc(self) -> None:
        """Turn the GC off. Existing heap is abandoned; objects become untracked."""
        self.config = GCConfig(mode="none")
        self._backend = None
        self._enabled = False

    def configure(self, config: GCConfig) -> None:
        """Apply a new ``GCConfig``. Resets the backend and existing heap data."""
        self.config = config
        self._enabled = config.mode != "none"
        self._backend = self._create_backend(config)

    @property
    def is_enabled(self) -> bool:
        return self._enabled

    @property
    def mode(self) -> str:
        return self.config.mode

    # -- Internal ------------------------------------------------------------

    def _create_backend(self, config: GCConfig):
        heap_bytes = config.heap_limit_mb * 1024 * 1024
        if config.mode == "none":
            return None
        if config.mode == "mark-sweep":
            return TricolorMarkSweepGC(heap_limit_bytes=heap_bytes)
        if config.mode == "generational":
            young_threshold = max(
                10,
                int(config.young_gen_limit_mb * 1024 * 1024 / 256),  # ~4 MB / 256 B avg
            )
            old_threshold = max(50, young_threshold * 5)
            return GenerationalGC(
                young_threshold=young_threshold,
                old_threshold=old_threshold,
                max_age=3,
            )
        if config.mode == "incremental":
            return IncrementalGC(
                step_budget=config.incremental_step_budget,
                heap_limit_bytes=heap_bytes,
            )
        raise ValueError(f"Unknown GC mode: {config.mode!r}")


__all__ = [
    "GCColor",
    "GCStats",
    "GCError",
    "NLPLObject",
    "TricolorMarkSweepGC",
    "Generation",
    "GenerationalGC",
    "IncrementalGC",
    "GCConfig",
    "GarbageCollector",
    "_estimate_size",
]
