"""
Tests for compiler-level allocator syntax.

Grammar: set <name> to <value> as <Type> with allocator <allocator_name>

Covers
------
- Parser: ``as Type with allocator <name>`` recognised in variable declarations
- AST:    VariableDeclaration.type_annotation is an AllocatorHint node
- Interpreter:
    - Empty list/dict wrapped in AllocatorTrackedList/AllocatorTrackedDict
    - Pre-populated collection uses initial element count for first alloc
    - list_append / index assignment tracked through allocator stats
    - Undefined allocator raises a runtime error
- Type system:
    - AllocatorHint stripped to base type for compatibility/inference
    - Undefined allocator name reported as type error
- AllocatorTrackedList / AllocatorTrackedDict (unit-level)
- All four allocator types (system, arena, pool, slab) as backing allocators
"""

import sys
import os
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import VariableDeclaration, AllocatorHint
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib
from nexuslang.stdlib.allocators import (
    ArenaAllocator, PoolAllocator, SystemAllocator, SlabAllocator,
    AllocatorTrackedList, AllocatorTrackedDict,
    wrap_collection_with_allocator,
)
from nexuslang.errors import NxlRuntimeError


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_interpreter() -> Interpreter:
    rt = Runtime()
    register_stdlib(rt)
    return Interpreter(rt)


def _run(source: str) -> Interpreter:
    interp = _make_interpreter()
    interp.interpret(source)
    return interp


def _parse_first_stmt(source: str):
    tokens = Lexer(source).tokenize()
    parser = Parser(tokens, source)
    program = parser.parse()
    return program.statements[0]


# ---------------------------------------------------------------------------
# Parser tests
# ---------------------------------------------------------------------------

class TestParserAllocatorHint:
    """The parser attaches AllocatorHint to VariableDeclaration.type_annotation."""

    def test_list_type_annotation_is_allocator_hint(self):
        stmt = _parse_first_stmt(
            'set items to [] as List of Integer with allocator arena'
        )
        assert isinstance(stmt, VariableDeclaration)
        assert isinstance(stmt.type_annotation, AllocatorHint)

    def test_allocator_hint_base_type(self):
        stmt = _parse_first_stmt(
            'set items to [] as List of Integer with allocator arena'
        )
        assert stmt.type_annotation.base_type == 'List of Integer'

    def test_allocator_hint_allocator_name(self):
        stmt = _parse_first_stmt(
            'set items to [] as List of Integer with allocator my_pool'
        )
        assert stmt.type_annotation.allocator_name == 'my_pool'

    def test_dict_type_annotation(self):
        stmt = _parse_first_stmt(
            'set mapping to {} as Dictionary of String, Integer with allocator slab'
        )
        hint = stmt.type_annotation
        assert isinstance(hint, AllocatorHint)
        assert 'Dictionary' in hint.base_type
        assert hint.allocator_name == 'slab'

    def test_plain_as_type_still_works(self):
        """Plain ``as List of Integer`` (no allocator) leaves a string annotation."""
        stmt = _parse_first_stmt('set x to [] as List of Integer')
        assert isinstance(stmt, VariableDeclaration)
        # Without allocator hint, type_annotation is a plain string
        assert isinstance(stmt.type_annotation, str)
        assert 'List of Integer' in stmt.type_annotation

    def test_no_as_clause_keeps_none(self):
        """Without an ``as`` clause, type_annotation is None."""
        stmt = _parse_first_stmt('set x to 42')
        assert isinstance(stmt, VariableDeclaration)
        assert stmt.type_annotation is None

    def test_collection_variable_name_preserved(self):
        stmt = _parse_first_stmt(
            'set my_items to [] as List of String with allocator buf'
        )
        assert stmt.name == 'my_items'

    def test_nested_element_type(self):
        stmt = _parse_first_stmt(
            'set matrix to [] as List of Float with allocator arena'
        )
        assert stmt.type_annotation.base_type == 'List of Float'


# ---------------------------------------------------------------------------
# AllocatorHint AST node tests
# ---------------------------------------------------------------------------

class TestAllocatorHintNode:

    def test_repr(self):
        hint = AllocatorHint('List of Integer', 'arena')
        r = repr(hint)
        assert 'List of Integer' in r
        assert 'arena' in r

    def test_attributes(self):
        hint = AllocatorHint('Dictionary of String, Float', 'my_alloc', line_number=7)
        assert hint.base_type == 'Dictionary of String, Float'
        assert hint.allocator_name == 'my_alloc'
        assert hint.line_number == 7


# ---------------------------------------------------------------------------
# AllocatorTrackedList unit tests
# ---------------------------------------------------------------------------

class TestAllocatorTrackedList:

    def _arena(self, cap=65536):
        return ArenaAllocator(cap)

    def test_isinstance_list(self):
        tl = AllocatorTrackedList([], self._arena())
        assert isinstance(tl, list)

    def test_empty_no_alloc(self):
        a = self._arena()
        AllocatorTrackedList([], a)
        assert a.stats.allocation_count == 0

    def test_append_increments_alloc_count(self):
        a = self._arena()
        tl = AllocatorTrackedList([], a)
        tl.append(1)
        tl.append(2)
        assert a.stats.allocation_count == 2

    def test_insert_increments_alloc_count(self):
        a = self._arena()
        tl = AllocatorTrackedList([1, 2], a)
        tl.insert(1, 99)
        assert a.stats.allocation_count == 1

    def test_extend_increments_alloc_count(self):
        a = self._arena()
        tl = AllocatorTrackedList([], a)
        tl.extend([10, 20, 30])
        assert a.stats.allocation_count == 3

    def test_pop_updates_dealloc_stats(self):
        a = self._arena()
        tl = AllocatorTrackedList([1, 2, 3], a)
        tl.append(4)  # alloc_count = 1
        tl.pop()
        # record_dealloc increments deallocation_count by 1, total_deallocated by 8
        assert a.stats.deallocation_count >= 1
        assert a.stats.total_deallocated == 8

    def test_remove_updates_dealloc_stats(self):
        a = self._arena()
        tl = AllocatorTrackedList([1, 2, 3], a)
        tl.append(1)  # triggers alloc
        tl.remove(1)  # removes first match, triggers dealloc
        assert a.stats.deallocation_count >= 1
        assert a.stats.total_deallocated == 8

    def test_clear_accounts_for_all_elements(self):
        a = self._arena()
        tl = AllocatorTrackedList([], a)
        tl.extend([1, 2, 3, 4, 5])
        assert a.stats.allocation_count == 5
        tl.clear()
        assert len(tl) == 0
        # record_dealloc(40) → 1 dealloc call, 40 bytes freed
        assert a.stats.total_deallocated == 40
        assert a.stats.deallocation_count >= 1

    def test_setitem_no_extra_alloc(self):
        a = self._arena()
        tl = AllocatorTrackedList([], a)
        tl.append(1)
        before = a.stats.allocation_count
        tl[0] = 99
        assert a.stats.allocation_count == before

    def test_iadd(self):
        a = self._arena()
        tl = AllocatorTrackedList([], a)
        tl += [1, 2, 3]
        assert a.stats.allocation_count == 3

    def test_works_without_allocator(self):
        tl = AllocatorTrackedList([1, 2, 3], None)
        tl.append(4)
        assert len(tl) == 4

    def test_iteration(self):
        tl = AllocatorTrackedList([1, 2, 3], None)
        assert list(tl) == [1, 2, 3]

    def test_contains(self):
        tl = AllocatorTrackedList([1, 2, 3], None)
        assert 2 in tl
        assert 99 not in tl

    def test_indexing(self):
        tl = AllocatorTrackedList([10, 20, 30], None)
        assert tl[0] == 10
        assert tl[-1] == 30

    def test_len(self):
        tl = AllocatorTrackedList([1, 2, 3], None)
        assert len(tl) == 3


# ---------------------------------------------------------------------------
# AllocatorTrackedDict unit tests
# ---------------------------------------------------------------------------

class TestAllocatorTrackedDict:

    def _arena(self, cap=65536):
        return ArenaAllocator(cap)

    def test_isinstance_dict(self):
        td = AllocatorTrackedDict({}, self._arena())
        assert isinstance(td, dict)

    def test_empty_no_alloc(self):
        a = self._arena()
        AllocatorTrackedDict({}, a)
        assert a.stats.allocation_count == 0

    def test_setitem_new_key_allocs(self):
        a = self._arena()
        td = AllocatorTrackedDict({}, a)
        td['x'] = 1
        assert a.stats.allocation_count == 1

    def test_setitem_existing_key_no_extra_alloc(self):
        a = self._arena()
        td = AllocatorTrackedDict({}, a)
        td['x'] = 1
        before = a.stats.allocation_count
        td['x'] = 99
        assert a.stats.allocation_count == before

    def test_update_new_keys(self):
        a = self._arena()
        td = AllocatorTrackedDict({}, a)
        td.update({'a': 1, 'b': 2, 'c': 3})
        assert a.stats.allocation_count == 3

    def test_pop_updates_dealloc(self):
        a = self._arena()
        td = AllocatorTrackedDict({}, a)
        td['x'] = 1
        before_dealloc = a.stats.deallocation_count
        td.pop('x')
        assert a.stats.deallocation_count > before_dealloc

    def test_del_updates_dealloc(self):
        a = self._arena()
        td = AllocatorTrackedDict({}, a)
        td['key'] = 42
        before = a.stats.deallocation_count
        del td['key']
        assert a.stats.deallocation_count > before

    def test_clear_accounts_all_entries(self):
        a = self._arena()
        td = AllocatorTrackedDict({}, a)
        td['a'] = 1
        td['b'] = 2
        td['c'] = 3
        td.clear()
        assert len(td) == 0
        assert a.stats.deallocation_count > 0

    def test_works_without_allocator(self):
        td = AllocatorTrackedDict({'k': 'v'}, None)
        td['k2'] = 'v2'
        assert td['k2'] == 'v2'


# ---------------------------------------------------------------------------
# wrap_collection_with_allocator
# ---------------------------------------------------------------------------

class TestWrapCollection:

    def _arena(self):
        return ArenaAllocator(65536)

    def test_list_becomes_tracked_list(self):
        a = self._arena()
        result = wrap_collection_with_allocator([1, 2, 3], a)
        assert isinstance(result, AllocatorTrackedList)
        assert isinstance(result, list)
        assert list(result) == [1, 2, 3]

    def test_dict_becomes_tracked_dict(self):
        a = self._arena()
        result = wrap_collection_with_allocator({'k': 'v'}, a)
        assert isinstance(result, AllocatorTrackedDict)
        assert isinstance(result, dict)
        assert result['k'] == 'v'

    def test_initial_elements_trigger_alloc(self):
        a = self._arena()
        wrap_collection_with_allocator([1, 2, 3], a)
        assert a.stats.allocation_count == 3

    def test_empty_collection_no_alloc(self):
        a = self._arena()
        wrap_collection_with_allocator([], a)
        assert a.stats.allocation_count == 0

    def test_scalar_returned_unchanged(self):
        a = self._arena()
        result = wrap_collection_with_allocator(42, a)
        assert result == 42
        assert type(result) is int


# ---------------------------------------------------------------------------
# Interpreter integration tests
# ---------------------------------------------------------------------------

class TestInterpreterAllocatorSyntax:
    """End-to-end tests through the interpreter."""

    def test_empty_list_with_arena(self):
        interp = _run('''
set arena to create_arena_allocator with 65536
set items to [] as List of Integer with allocator arena
''')
        items = interp.get_variable('items')
        assert isinstance(items, AllocatorTrackedList)
        assert len(items) == 0

    def test_append_tracked_by_arena(self):
        interp = _run('''
set arena to create_arena_allocator with 65536
set items to [] as List of Integer with allocator arena
call list_append with items and 10
call list_append with items and 20
call list_append with items and 30
set stats to get_allocator_stats with arena
''')
        stats = interp.get_variable('stats')
        assert stats['allocation_count'] == 3
        items = interp.get_variable('items')
        assert list(items) == [10, 20, 30]

    def test_pre_populated_list_initial_alloc(self):
        interp = _run('''
set arena to create_arena_allocator with 65536
set nums to [1, 2, 3, 4, 5] as List of Integer with allocator arena
set stats to get_allocator_stats with arena
''')
        stats = interp.get_variable('stats')
        assert stats['allocation_count'] == 5

    def test_empty_dict_with_pool(self):
        interp = _run('''
set pool to create_pool_allocator with 16 and 64
set mapping to {} as Dictionary of String, Integer with allocator pool
''')
        mapping = interp.get_variable('mapping')
        assert isinstance(mapping, AllocatorTrackedDict)
        assert len(mapping) == 0

    def test_dict_insertions_tracked(self):
        interp = _run('''
set pool to create_pool_allocator with 16 and 64
set mapping to {} as Dictionary of String, Integer with allocator pool
set mapping["a"] to 1
set mapping["b"] to 2
set stats to get_allocator_stats with pool
''')
        stats = interp.get_variable('stats')
        assert stats['allocation_count'] >= 2

    def test_system_allocator_list(self):
        interp = _run('''
set sys to create_system_allocator with 0
set floats to [] as List of Float with allocator sys
call list_append with floats and 3.14
call list_append with floats and 2.72
set stats to get_allocator_stats with sys
''')
        stats = interp.get_variable('stats')
        assert stats['allocation_count'] == 2

    def test_slab_allocator_list(self):
        interp = _run('''
set slab to create_slab_allocator with 8 and 64
set data to [] as List of Integer with allocator slab
call list_append with data and 100
call list_append with data and 200
set stats to get_allocator_stats with slab
''')
        stats = interp.get_variable('stats')
        assert stats['allocation_count'] == 2

    def test_undefined_allocator_raises_error(self):
        with pytest.raises(NxlRuntimeError) as exc_info:
            _run('''
set items to [] as List of Integer with allocator nonexistent_alloc
''')
        assert 'nonexistent_alloc' in str(exc_info.value)

    def test_list_element_access(self):
        interp = _run('''
set arena to create_arena_allocator with 65536
set nums to [] as List of Integer with allocator arena
call list_append with nums and 42
call list_append with nums and 99
set first to nums[0]
set second to nums[1]
''')
        assert interp.get_variable('first') == 42
        assert interp.get_variable('second') == 99

    def test_list_length(self):
        interp = _run('''
set arena to create_arena_allocator with 65536
set items to [] as List of Integer with allocator arena
call list_append with items and 1
call list_append with items and 2
call list_append with items and 3
set n to length of items
''')
        assert interp.get_variable('n') == 3

    def test_arena_reset_clears_stats(self):
        interp = _run('''
set arena to create_arena_allocator with 65536
set items to [] as List of Integer with allocator arena
call list_append with items and 10
call list_append with items and 20
call allocator_reset with arena
set stats to get_allocator_stats with arena
''')
        stats = interp.get_variable('stats')
        assert stats['allocation_count'] == 0

    def test_list_still_works_after_arena_reset(self):
        """Collection is backed by Python memory; arena reset doesn't free it."""
        interp = _run('''
set arena to create_arena_allocator with 65536
set items to [] as List of Integer with allocator arena
call list_append with items and 10
call list_append with items and 20
call allocator_reset with arena
call list_append with items and 30
set n to length of items
''')
        assert interp.get_variable('n') == 3

    def test_multiple_collections_different_allocators(self):
        interp = _run('''
set arena1 to create_arena_allocator with 65536
set arena2 to create_arena_allocator with 65536
set list1 to [] as List of Integer with allocator arena1
set list2 to [] as List of Integer with allocator arena2
call list_append with list1 and 1
call list_append with list1 and 2
call list_append with list2 and 10
set s1 to get_allocator_stats with arena1
set s2 to get_allocator_stats with arena2
''')
        s1 = interp.get_variable('s1')
        s2 = interp.get_variable('s2')
        assert s1['allocation_count'] == 2
        assert s2['allocation_count'] == 1

    def test_plain_as_type_still_works(self):
        """``set x to [] as List of Integer`` without allocator still works."""
        interp = _run('''
set x to [] as List of Integer
call list_append with x and 5
set n to length of x
''')
        assert interp.get_variable('n') == 1

    def test_string_list_with_arena(self):
        interp = _run('''
set arena to create_arena_allocator with 65536
set words to [] as List of String with allocator arena
call list_append with words and "hello"
call list_append with words and "world"
set stats to get_allocator_stats with arena
''')
        stats = interp.get_variable('stats')
        assert stats['allocation_count'] == 2

    def test_nested_list_value(self):
        """Pre-populated list with complex values still wraps correctly."""
        interp = _run('''
set arena to create_arena_allocator with 65536
set scores to [100, 200, 300] as List of Integer with allocator arena
set peak to get_allocator_stats with arena
call list_append with scores and 400
set after to get_allocator_stats with arena
''')
        peak = interp.get_variable('peak')
        after = interp.get_variable('after')
        assert peak['allocation_count'] == 3
        assert after['allocation_count'] == 4


# ---------------------------------------------------------------------------
# Type system integration tests
# ---------------------------------------------------------------------------

class TestTypeSystemAllocatorHint:
    """Type checker and inference handle AllocatorHint correctly."""

    def _check(self, source: str):
        from nexuslang.typesystem.typechecker import TypeChecker
        checker = TypeChecker()
        tokens = Lexer(source).tokenize()
        parser = Parser(tokens, source)
        program = parser.parse()
        checker.check_program(program)
        return checker.errors

    def test_allocator_hint_stripped_for_inference(self):
        from nexuslang.typesystem.type_inference import TypeInferenceEngine
        from nexuslang.parser.ast import VariableDeclaration
        from nexuslang.parser.ast import AllocatorHint

        engine = TypeInferenceEngine()
        hint = AllocatorHint('List of Integer', 'arena')
        decl = VariableDeclaration('items', None, hint)
        from nexuslang.typesystem.types import ListType, INTEGER_TYPE
        result_type = engine.infer_variable_declaration(decl, {})
        # Should produce a list type, not crash
        assert result_type is not None

    def test_undefined_allocator_in_type_checker(self):
        source = '''
set items to [] as List of Integer with allocator no_such_alloc
'''
        errors = self._check(source)
        assert any('no_such_alloc' in e for e in errors)

    def test_known_allocator_no_type_error(self):
        # With known allocator in scope, no type error should be reported
        source = '''
set arena to create_arena_allocator with 65536
set items to [] as List of Integer with allocator arena
'''
        errors = self._check(source)
        # Any errors should not be about the allocator name
        alloc_errors = [e for e in errors if 'arena' in e and 'not defined' in e]
        assert len(alloc_errors) == 0
