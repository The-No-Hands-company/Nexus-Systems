"""
pytest tests for src/nlpl/runtime/gc.py

Run with:
    pytest tests/unit/runtime/test_gc.py
"""
import sys
from pathlib import Path

import pytest

_SRC = str(Path(__file__).resolve().parent.parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

from nexuslang.runtime.gc import (
    GCColor,
    GCStats,
    GCError,
    NLPLObject,
    TricolorMarkSweepGC,
    Generation,
    GenerationalGC,
    IncrementalGC,
    GCConfig,
    GarbageCollector,
    _estimate_size,
)


# ---------------------------------------------------------------------------
# TestGCColor
# ---------------------------------------------------------------------------

class TestGCColor:
    def test_has_white(self):
        assert GCColor.WHITE.value == "white"

    def test_has_gray(self):
        assert GCColor.GRAY.value == "gray"

    def test_has_black(self):
        assert GCColor.BLACK.value == "black"

    def test_three_colors_only(self):
        assert len(list(GCColor)) == 3


# ---------------------------------------------------------------------------
# TestGCStats
# ---------------------------------------------------------------------------

class TestGCStats:
    def test_default_values(self):
        s = GCStats()
        assert s.collections == 0
        assert s.objects_collected == 0
        assert s.bytes_freed == 0
        assert s.total_pause_ms == 0.0
        assert s.pause_times_ms == []

    def test_add_two_stats(self):
        a = GCStats(collections=2, objects_collected=10, bytes_freed=1000)
        b = GCStats(collections=3, objects_collected=5, bytes_freed=500)
        c = a + b
        assert c.collections == 5
        assert c.objects_collected == 15
        assert c.bytes_freed == 1500

    def test_add_pause_times_concatenated(self):
        a = GCStats(pause_times_ms=[1.0, 2.0])
        b = GCStats(pause_times_ms=[3.0])
        c = a + b
        assert c.pause_times_ms == [1.0, 2.0, 3.0]

    def test_avg_pause_ms_no_pauses(self):
        s = GCStats()
        assert s.avg_pause_ms == 0.0

    def test_avg_pause_ms_computed(self):
        s = GCStats(pause_times_ms=[2.0, 4.0])
        assert s.avg_pause_ms == 3.0

    def test_str_contains_key_fields(self):
        s = GCStats(collections=5, objects_collected=20)
        text = str(s)
        assert "Collections" in text
        assert "5" in text

    def test_max_heap_bytes_taken_from_largest(self):
        a = GCStats(max_heap_bytes=1000)
        b = GCStats(max_heap_bytes=2000)
        c = a + b
        assert c.max_heap_bytes == 2000


# ---------------------------------------------------------------------------
# TestGCError
# ---------------------------------------------------------------------------

class TestGCError:
    def test_is_exception(self):
        assert issubclass(GCError, Exception)

    def test_can_be_raised(self):
        with pytest.raises(GCError, match="test error"):
            raise GCError("test error")

    def test_can_be_caught_as_exception(self):
        try:
            raise GCError("oops")
        except Exception as e:
            assert str(e) == "oops"


# ---------------------------------------------------------------------------
# TestEstimateSize
# ---------------------------------------------------------------------------

class TestEstimateSize:
    def test_integer_has_positive_size(self):
        assert _estimate_size(42) >= 16

    def test_string_has_positive_size(self):
        assert _estimate_size("hello world") >= 16

    def test_none_returns_minimum(self):
        assert _estimate_size(None) >= 16

    def test_list_size_positive(self):
        assert _estimate_size([1, 2, 3]) >= 16


# ---------------------------------------------------------------------------
# TestNLPLObject
# ---------------------------------------------------------------------------

class TestNLPLObject:
    def test_default_color_is_white(self):
        obj = NLPLObject()
        assert obj._gc_color == GCColor.WHITE

    def test_unique_ids(self):
        a = NLPLObject()
        b = NLPLObject()
        assert a._gc_id != b._gc_id

    def test_value_stored(self):
        obj = NLPLObject(value=42)
        assert obj.value == 42

    def test_type_name_stored(self):
        obj = NLPLObject(value="hello", type_name="String")
        assert obj.type_name == "String"

    def test_default_age_zero(self):
        obj = NLPLObject()
        assert obj._gc_age == 0

    def test_default_not_pinned(self):
        obj = NLPLObject()
        assert obj._gc_pinned is False

    def test_pin_and_unpin(self):
        obj = NLPLObject()
        obj.pin()
        assert obj._gc_pinned is True
        obj.unpin()
        assert obj._gc_pinned is False

    def test_add_ref(self):
        a = NLPLObject()
        b = NLPLObject()
        a.add_ref(b)
        assert b in a._gc_refs

    def test_add_ref_no_duplicates(self):
        a = NLPLObject()
        b = NLPLObject()
        a.add_ref(b)
        a.add_ref(b)
        assert a._gc_refs.count(b) == 1

    def test_remove_ref(self):
        a = NLPLObject()
        b = NLPLObject()
        a.add_ref(b)
        a.remove_ref(b)
        assert b not in a._gc_refs

    def test_repr_contains_id(self):
        obj = NLPLObject(type_name="Integer")
        assert "Integer" in repr(obj)

    def test_custom_size_honored(self):
        obj = NLPLObject(size=512)
        assert obj._gc_size == 512

    def test_refs_initially_empty_by_default(self):
        obj = NLPLObject()
        assert obj._gc_refs == []

    def test_refs_from_constructor(self):
        child = NLPLObject()
        parent = NLPLObject(refs=[child])
        assert child in parent._gc_refs


# ---------------------------------------------------------------------------
# TestTricolorMarkSweepGC
# ---------------------------------------------------------------------------

class TestTricolorMarkSweepGC:
    def _gc(self):
        return TricolorMarkSweepGC()

    def test_empty_collect_returns_stats(self):
        gc = self._gc()
        stats = gc.collect()
        assert isinstance(stats, GCStats)
        assert stats.collections == 1

    def test_unreachable_object_collected(self):
        gc = self._gc()
        obj = gc.allocate(value=1)
        # Not added as root → unreachable
        stats = gc.collect()
        assert stats.objects_collected == 1
        assert gc.live_objects() == 0

    def test_root_object_survives(self):
        gc = self._gc()
        obj = gc.allocate(value=42)
        gc.add_root(obj)
        gc.collect()
        assert gc.live_objects() == 1

    def test_referenced_object_survives(self):
        gc = self._gc()
        child = gc.allocate(value="child")
        parent = gc.allocate(value="parent")
        parent.add_ref(child)
        gc.add_root(parent)
        gc.collect()
        # Both parent (root) and child (reachable via parent) survive
        assert gc.live_objects() == 2

    def test_unreachable_after_root_removed(self):
        gc = self._gc()
        obj = gc.allocate(value=99)
        gc.add_root(obj)
        gc.collect()
        assert gc.live_objects() == 1
        gc.remove_root(obj)
        gc.collect()
        assert gc.live_objects() == 0

    def test_pinned_object_never_collected(self):
        gc = self._gc()
        obj = gc.allocate(value="pinned")
        obj.pin()
        gc.collect()
        assert gc.live_objects() == 1

    def test_bytes_freed_counted(self):
        gc = self._gc()
        obj = gc.allocate(value=1, size=256)
        stats = gc.collect()
        assert stats.bytes_freed == 256

    def test_heap_size_decreases_after_collect(self):
        gc = self._gc()
        for _ in range(10):
            gc.allocate(value=0)
        before = gc.live_objects()
        gc.collect()
        assert gc.live_objects() < before

    def test_allocate_non_heap_object_raises(self):
        gc = self._gc()
        foreign = NLPLObject()
        with pytest.raises(GCError):
            gc.add_root(foreign)

    def test_cycle_stats_accumulate(self):
        gc = self._gc()
        for _ in range(3):
            gc.allocate(value=0)
            gc.collect()
        assert gc.cumulative_stats.collections == 3

    def test_multiple_roots_all_survive(self):
        gc = self._gc()
        objs = [gc.allocate(value=i) for i in range(5)]
        for o in objs:
            gc.add_root(o)
        gc.collect()
        assert gc.live_objects() == 5

    def test_collect_colors_all_white_initially(self):
        gc = self._gc()
        obj = gc.allocate(value=0)
        gc.add_root(obj)
        gc.collect()
        # After collection, live objects are BLACK (they were traced)
        assert obj._gc_color == GCColor.BLACK

    def test_heap_size_property(self):
        gc = self._gc()
        obj = gc.allocate(value=0, size=100)
        gc.add_root(obj)
        assert gc.heap_size() == 100

    def test_is_heap_full_false_by_default(self):
        gc = TricolorMarkSweepGC(heap_limit_bytes=100 * 1024 * 1024)
        gc.allocate(value=0, size=1)
        assert not gc.is_heap_full()

    def test_empty_heap_no_objects_freed(self):
        gc = self._gc()
        stats = gc.collect()
        assert stats.objects_collected == 0

    def test_deep_reference_chain_survives(self):
        gc = self._gc()
        prev = gc.allocate(value="root")
        gc.add_root(prev)
        for i in range(10):
            child = gc.allocate(value=i)
            prev.add_ref(child)
            prev = child
        gc.collect()
        assert gc.live_objects() == 11

    def test_diamond_graph_all_survive(self):
        gc = self._gc()
        root = gc.allocate(value="root")
        left = gc.allocate(value="left")
        right = gc.allocate(value="right")
        bottom = gc.allocate(value="bottom")
        root.add_ref(left)
        root.add_ref(right)
        left.add_ref(bottom)
        right.add_ref(bottom)
        gc.add_root(root)
        gc.collect()
        assert gc.live_objects() == 4

    def test_pause_time_recorded(self):
        gc = self._gc()
        stats = gc.collect()
        assert len(stats.pause_times_ms) == 1
        assert stats.pause_times_ms[0] >= 0.0

    def test_remove_nonexistent_root_does_not_raise(self):
        gc = self._gc()
        obj = gc.allocate(value=0)
        gc.remove_root(obj)  # Never added as root; should not raise

    def test_collecting_twice_frees_newly_unreachable(self):
        gc = self._gc()
        obj = gc.allocate(value=0)
        gc.add_root(obj)
        gc.collect()
        gc.remove_root(obj)
        stats2 = gc.collect()
        assert stats2.objects_collected == 1


# ---------------------------------------------------------------------------
# TestGeneration
# ---------------------------------------------------------------------------

class TestGeneration:
    def test_default_values(self):
        g = Generation()
        assert g.objects == []
        assert g.threshold == 100
        assert g.max_age == 3

    def test_size_reflects_objects(self):
        g = Generation()
        g.objects.append(NLPLObject())
        assert g.size == 1

    def test_is_full_false_when_below_threshold(self):
        g = Generation(threshold=5)
        g.objects = [NLPLObject() for _ in range(3)]
        assert not g.is_full

    def test_is_full_true_at_threshold(self):
        g = Generation(threshold=3)
        g.objects = [NLPLObject() for _ in range(3)]
        assert g.is_full

    def test_byte_size_sum(self):
        g = Generation()
        g.objects = [NLPLObject(size=100), NLPLObject(size=200)]
        assert g.byte_size() == 300

    def test_empty_byte_size_zero(self):
        g = Generation()
        assert g.byte_size() == 0


# ---------------------------------------------------------------------------
# TestGenerationalGC
# ---------------------------------------------------------------------------

class TestGenerationalGC:
    def _gc(self):
        return GenerationalGC(young_threshold=5, old_threshold=20, max_age=2)

    def test_allocate_goes_to_young(self):
        gc = self._gc()
        obj = gc.allocate(value=0)
        assert obj in gc.young.objects

    def test_unreachable_young_collected(self):
        gc = self._gc()
        gc.allocate(value=0)  # No root
        stats = gc.collect_young()
        assert stats.objects_collected == 1
        assert gc.young.size == 0

    def test_rooted_young_survives(self):
        gc = self._gc()
        obj = gc.allocate(value=1)
        gc.add_root(obj)
        gc.collect_young()
        assert gc.young.size + len(gc.old.objects) >= 1

    def test_promotion_after_max_age(self):
        gc = self._gc()
        obj = gc.allocate(value=2)
        gc.add_root(obj)
        # After max_age==2 collections surviving, object should be promoted
        gc.collect_young()
        gc.collect_young()
        assert obj in gc.old.objects

    def test_full_collect_clears_both_gens(self):
        gc = self._gc()
        gc.allocate(value=0)
        gc.allocate(value=0)
        stats = gc.collect_full()
        assert stats.objects_collected == 2

    def test_root_survives_full_collect(self):
        gc = self._gc()
        obj = gc.allocate(value=42)
        gc.add_root(obj)
        gc.collect_full()
        assert gc.live_objects() == 1

    def test_live_objects_count(self):
        gc = self._gc()
        for i in range(4):
            gc.allocate(value=i)
        assert gc.live_objects() == 4

    def test_add_root_nontracked_raises(self):
        gc = self._gc()
        foreign = NLPLObject()
        with pytest.raises(GCError):
            gc.add_root(foreign)

    def test_cross_gen_reference_survives_minor(self):
        gc = self._gc()
        old_obj = gc.allocate(value="old")
        gc.add_root(old_obj)
        # Promote old_obj to old generation
        gc.collect_young()
        gc.collect_young()
        # Now allocate a young object; old_obj references it
        young_child = gc.allocate(value="young_child")
        old_obj.add_ref(young_child)
        # Minor GC: young_child reachable via old_obj (cross-gen root)
        gc.collect_young()
        assert gc.live_objects() >= 2

    def test_collect_young_returns_stats(self):
        gc = self._gc()
        stats = gc.collect_young()
        assert isinstance(stats, GCStats)
        assert stats.young_gen_collections == 1

    def test_collect_full_returns_stats(self):
        gc = self._gc()
        stats = gc.collect_full()
        assert isinstance(stats, GCStats)
        assert stats.old_gen_collections == 1

    def test_cumulative_stats_accumulate(self):
        gc = self._gc()
        gc.collect_young()
        gc.collect_young()
        assert gc.cumulative_stats.collections == 2

    def test_remove_root_allows_collection(self):
        gc = self._gc()
        obj = gc.allocate(value=0)
        gc.add_root(obj)
        gc.collect_young()
        gc.remove_root(obj)
        stats = gc.collect_young()
        assert gc.live_objects() == 0

    def test_pinned_survives_full_collect(self):
        gc = self._gc()
        obj = gc.allocate(value="pinned")
        obj.pin()
        gc.collect_full()
        assert gc.live_objects() == 1

    def test_empty_gc_no_errors(self):
        gc = self._gc()
        gc.collect_young()
        gc.collect_full()
        assert gc.live_objects() == 0


# ---------------------------------------------------------------------------
# TestIncrementalGC
# ---------------------------------------------------------------------------

class TestIncrementalGC:
    def _gc(self):
        return IncrementalGC(step_budget=10)

    def test_not_collecting_initially(self):
        gc = self._gc()
        assert not gc.is_collecting()

    def test_start_collection_transitions_to_marking(self):
        gc = self._gc()
        gc.allocate(value=0)
        gc.start_collection()
        assert gc.is_collecting()

    def test_step_returns_false_when_idle(self):
        gc = self._gc()
        assert gc.step() is False

    def test_full_collect_frees_unreachable(self):
        gc = self._gc()
        gc.allocate(value=0)
        stats = gc.full_collect()
        assert stats.objects_collected == 1
        assert gc.live_objects() == 0

    def test_root_survives_incremental(self):
        gc = self._gc()
        obj = gc.allocate(value=1)
        gc.add_root(obj)
        gc.full_collect()
        assert gc.live_objects() == 1

    def test_multiple_steps_complete_cycle(self):
        gc = IncrementalGC(step_budget=2)
        for _ in range(10):
            gc.allocate(value=0)
        gc.start_collection()
        completed = False
        for _ in range(50):
            if gc.step():
                completed = True
                break
        assert completed

    def test_full_collect_returns_stats(self):
        gc = self._gc()
        stats = gc.full_collect()
        assert isinstance(stats, GCStats)
        assert stats.collections == 1

    def test_add_root_nontracked_raises(self):
        gc = self._gc()
        foreign = NLPLObject()
        with pytest.raises(GCError):
            gc.add_root(foreign)

    def test_removed_root_collected_next_cycle(self):
        gc = self._gc()
        obj = gc.allocate(value=0)
        gc.add_root(obj)
        gc.full_collect()
        gc.remove_root(obj)
        stats = gc.full_collect()
        assert stats.objects_collected == 1

    def test_cumulative_stats_accumulate(self):
        gc = self._gc()
        gc.full_collect()
        gc.full_collect()
        assert gc.cumulative_stats.collections == 2

    def test_pinned_survives_incremental(self):
        gc = self._gc()
        obj = gc.allocate(value="anchor")
        obj.pin()
        gc.full_collect()
        assert gc.live_objects() == 1

    def test_live_objects_count(self):
        gc = self._gc()
        for _ in range(5):
            gc.allocate(value=0)
        assert gc.live_objects() == 5

    def test_not_collecting_after_full_collect(self):
        gc = self._gc()
        gc.full_collect()
        assert not gc.is_collecting()

    def test_start_collection_idempotent(self):
        gc = self._gc()
        gc.allocate(value=0)
        gc.start_collection()
        gc.start_collection()  # Second call has no effect
        assert gc.is_collecting()

    def test_referenced_child_survives(self):
        gc = self._gc()
        child = gc.allocate(value="child")
        parent = gc.allocate(value="parent")
        parent.add_ref(child)
        gc.add_root(parent)
        gc.full_collect()
        assert gc.live_objects() == 2


# ---------------------------------------------------------------------------
# TestGCConfig
# ---------------------------------------------------------------------------

class TestGCConfig:
    def test_default_mode_mark_sweep(self):
        cfg = GCConfig()
        assert cfg.mode == "mark-sweep"

    def test_valid_modes(self):
        for mode in ("none", "mark-sweep", "generational", "incremental"):
            cfg = GCConfig(mode=mode)
            assert cfg.mode == mode

    def test_invalid_mode_raises(self):
        with pytest.raises(ValueError, match="Invalid GC mode"):
            GCConfig(mode="refcount")

    def test_invalid_threshold_raises(self):
        with pytest.raises(ValueError):
            GCConfig(gc_trigger_threshold=0.0)

    def test_threshold_above_one_raises(self):
        with pytest.raises(ValueError):
            GCConfig(gc_trigger_threshold=1.5)

    def test_negative_heap_limit_raises(self):
        with pytest.raises(ValueError):
            GCConfig(heap_limit_mb=-1)

    def test_zero_step_budget_raises(self):
        with pytest.raises(ValueError):
            GCConfig(incremental_step_budget=0)

    def test_defaults_are_sensible(self):
        cfg = GCConfig()
        assert cfg.heap_limit_mb > 0
        assert 0 < cfg.gc_trigger_threshold <= 1.0
        assert cfg.incremental_step_budget > 0


# ---------------------------------------------------------------------------
# TestGarbageCollector
# ---------------------------------------------------------------------------

class TestGarbageCollector:
    def test_default_mode_mark_sweep(self):
        gc = GarbageCollector()
        assert gc.mode == "mark-sweep"

    def test_is_enabled_true_by_default(self):
        gc = GarbageCollector()
        assert gc.is_enabled

    def test_disabled_mode(self):
        gc = GarbageCollector(GCConfig(mode="none"))
        assert not gc.is_enabled

    def test_allocate_returns_nxl_object(self):
        gc = GarbageCollector()
        obj = gc.allocate(value=10)
        assert isinstance(obj, NLPLObject)
        assert obj.value == 10

    def test_allocate_disabled_returns_untracked_object(self):
        gc = GarbageCollector(GCConfig(mode="none"))
        obj = gc.allocate(value=42)
        assert isinstance(obj, NLPLObject)

    def test_collect_returns_stats(self):
        gc = GarbageCollector()
        stats = gc.collect()
        assert isinstance(stats, GCStats)

    def test_collect_disabled_returns_empty_stats(self):
        gc = GarbageCollector(GCConfig(mode="none"))
        stats = gc.collect()
        assert stats.collections == 0

    def test_add_root_disabled_raises(self):
        gc = GarbageCollector(GCConfig(mode="none"))
        obj = gc.allocate(value=0)
        with pytest.raises(GCError):
            gc.add_root(obj)

    def test_remove_root_disabled_noop(self):
        gc = GarbageCollector(GCConfig(mode="none"))
        obj = gc.allocate(value=0)
        gc.remove_root(obj)  # Should not raise

    def test_enable_gc_after_disable(self):
        gc = GarbageCollector(GCConfig(mode="none"))
        gc.enable_gc("mark-sweep")
        assert gc.is_enabled
        assert gc.mode == "mark-sweep"

    def test_disable_gc(self):
        gc = GarbageCollector()
        gc.disable_gc()
        assert not gc.is_enabled

    def test_generational_mode(self):
        gc = GarbageCollector(GCConfig(mode="generational"))
        assert gc.mode == "generational"
        obj = gc.allocate(value=1)
        assert isinstance(obj, NLPLObject)

    def test_incremental_mode(self):
        gc = GarbageCollector(GCConfig(mode="incremental"))
        assert gc.mode == "incremental"
        stats = gc.collect()
        assert isinstance(stats, GCStats)

    def test_stats_returns_gc_stats(self):
        gc = GarbageCollector()
        assert isinstance(gc.stats(), GCStats)

    def test_live_objects_disabled_is_zero(self):
        gc = GarbageCollector(GCConfig(mode="none"))
        gc.allocate(value=0)
        assert gc.live_objects() == 0

    def test_configure_changes_backend(self):
        gc = GarbageCollector(GCConfig(mode="mark-sweep"))
        gc.configure(GCConfig(mode="incremental"))
        assert gc.mode == "incremental"


# ---------------------------------------------------------------------------
# TestGCFacadeFromFlag
# ---------------------------------------------------------------------------

class TestGCFacadeFromFlag:
    def test_enable_gc_flag_gives_mark_sweep(self):
        gc = GarbageCollector.from_flag("--enable-gc")
        assert gc.mode == "mark-sweep"
        assert gc.is_enabled

    def test_enable_gc_mark_sweep_explicit(self):
        gc = GarbageCollector.from_flag("--enable-gc=mark-sweep")
        assert gc.mode == "mark-sweep"

    def test_enable_gc_generational(self):
        gc = GarbageCollector.from_flag("--enable-gc=generational")
        assert gc.mode == "generational"

    def test_enable_gc_incremental(self):
        gc = GarbageCollector.from_flag("--enable-gc=incremental")
        assert gc.mode == "incremental"

    def test_disable_gc_flag(self):
        gc = GarbageCollector.from_flag("--disable-gc")
        assert gc.mode == "none"
        assert not gc.is_enabled

    def test_unknown_flag_raises(self):
        with pytest.raises(ValueError, match="Unrecognised"):
            GarbageCollector.from_flag("--gc-on")

    def test_from_flag_allocate_works(self):
        gc = GarbageCollector.from_flag("--enable-gc")
        obj = gc.allocate(value="test")
        assert obj.value == "test"

    def test_from_flag_collect_works(self):
        gc = GarbageCollector.from_flag("--enable-gc")
        gc.allocate(value=0)
        stats = gc.collect()
        assert stats.collections == 1


# ---------------------------------------------------------------------------
# TestGCLazyImportViaRuntime
# ---------------------------------------------------------------------------

class TestGCLazyImportViaRuntime:
    def test_garbage_collector_importable_via_runtime(self):
        from nexuslang.runtime import GarbageCollector as GC
        assert GC is GarbageCollector

    def test_gc_config_importable_via_runtime(self):
        from nexuslang.runtime import GCConfig as Cfg
        assert Cfg is GCConfig

    def test_gc_stats_importable_via_runtime(self):
        from nexuslang.runtime import GCStats as Stats
        assert Stats is GCStats

    def test_nxl_object_importable_via_runtime(self):
        from nexuslang.runtime import NLPLObject as Obj
        assert Obj is NLPLObject

    def test_gc_error_importable_via_runtime(self):
        from nexuslang.runtime import GCError as Err
        assert Err is GCError
