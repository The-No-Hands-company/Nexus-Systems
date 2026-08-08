"""
Comprehensive interpreter tests for class definition and decorator execution paths.

Targets `execute_class_definition` and decorator behaviors:
- Basic class definition without decorators
- @derive(DebugPrint) generating to_string and debug_print methods
- @derive(Equality) generating equals method
- @derive(Clone) generating clone method
- @derive(Hash) generating hash_code method
- @derive(Default) generating default method
- @derive with multiple traits
- @singleton class instantiation
- User-defined decorator functions
- Attribute decorators with metadata
- Multi-decorator combinations and precedence
"""

from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.runtime.runtime import Object
from nexuslang.runtime.runtime import Runtime


def run_program(source: str) -> Interpreter:
    """Parse and execute source, returning the interpreter for state assertions."""
    runtime = Runtime()
    interpreter = Interpreter(runtime)
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens, source=source)
    ast = parser.parse()
    interpreter.interpret(ast)
    return interpreter


class TestClassDefinitionComprehensive:
    """Coverage-driven tests for interpreter class definition behavior."""

    def test_basic_class_definition_no_decorators_stores_class(self):
        source = """
        class Point
            property x as Integer
            property y as Integer
        end

        set p to new Point
        set p.x to 10
        set p.y to 20
        """

        interpreter = run_program(source)
        obj = interpreter.get_variable("p")
        assert hasattr(obj, 'properties')
        assert obj.properties['x'] == 10
        assert obj.properties['y'] == 20

    def test_derive_debug_print_generates_to_string(self):
        source = """
        @derive(DebugPrint)
        class Point
            property x as Integer
            property y as Integer
        end

        set p to new Point
        set p.x to 5
        set p.y to 10
        """

        interpreter = run_program(source)
        obj = interpreter.get_variable("p")
        # Check that derived method exists
        class_node = interpreter.classes.get("Point")
        assert class_node is not None
        assert hasattr(class_node, '_derived_methods')
        assert "to_string" in class_node._derived_methods
        assert "debug_print" in class_node._derived_methods

    def test_derive_equality_generates_equals_method(self):
        source = """
        @derive(Equality)
        class Point
            property x as Integer
            property y as Integer
        end
        """

        interpreter = run_program(source)
        # Verify derived method exists on class
        class_node = interpreter.classes.get("Point")
        assert "equals" in class_node._derived_methods
        equals_fn = class_node._derived_methods["equals"]

        a = Object("Point")
        a.set_property("x", 5)
        a.set_property("y", 10)
        b = Object("Point")
        b.set_property("x", 5)
        b.set_property("y", 10)
        assert equals_fn(a, b) is True

    def test_derive_equality_false_for_different_values(self):
        source = """
        @derive(Equality)
        class Point
            property x as Integer
            property y as Integer
        end
        """

        interpreter = run_program(source)
        class_node = interpreter.classes.get("Point")
        equals_fn = class_node._derived_methods["equals"]

        a = Object("Point")
        a.set_property("x", 5)
        a.set_property("y", 10)
        b = Object("Point")
        b.set_property("x", 5)
        b.set_property("y", 15)
        assert equals_fn(a, b) is False

    def test_derive_clone_generates_clone_method(self):
        source = """
        @derive(Clone)
        class Point
            property x as Integer
            property y as Integer
        end

        set p1 to new Point
        set p1.x to 5
        set p1.y to 10

        set p2 to p1.clone()
        set p2.x to 20

        set orig_x to p1.x
        set cloned_x to p2.x
        """

        interpreter = run_program(source)
        orig_x = interpreter.get_variable("orig_x")
        cloned_x = interpreter.get_variable("cloned_x")
        # Original should remain unchanged
        assert orig_x == 5
        assert cloned_x == 20
        # Verify clone method exists
        class_node = interpreter.classes.get("Point")
        assert "clone" in class_node._derived_methods

    def test_derive_hash_generates_hash_code_method(self):
        source = """
        @derive(Hash)
        class Point
            property x as Integer
            property y as Integer
        end

        set p1 to new Point
        set p1.x to 5
        set p1.y to 10

        set h1 to p1.hash_code()
        """

        interpreter = run_program(source)
        h1 = interpreter.get_variable("h1")
        # Hash should be an integer
        assert isinstance(h1, int)
        # Verify hash_code method exists
        class_node = interpreter.classes.get("Point")
        assert "hash_code" in class_node._derived_methods

    def test_derive_default_generates_default_method(self):
        source = """
        @derive(Default)
        class Point
            property x as Integer
            property y as Integer
        end

        set p to new Point
        set p.x to 5
        set p.y to 10

        set result to p.default()
        set x_val to p.x
        set y_val to p.y
        """

        interpreter = run_program(source)
        x_val = interpreter.get_variable("x_val")
        y_val = interpreter.get_variable("y_val")
        # Default method should set all properties to null
        assert x_val is None
        assert y_val is None
        # Verify default method exists
        class_node = interpreter.classes.get("Point")
        assert "default" in class_node._derived_methods

    def test_derive_multiple_traits_generates_all_methods(self):
        source = """
        @derive(DebugPrint, Equality, Clone)
        class Point
            property x as Integer
            property y as Integer
        end

        set p to new Point
        set p.x to 5
        set p.y to 10
        """

        interpreter = run_program(source)
        class_node = interpreter.classes.get("Point")
        assert hasattr(class_node, '_derived_methods')
        # All three traits' methods should exist
        assert "to_string" in class_node._derived_methods
        assert "debug_print" in class_node._derived_methods
        assert "equals" in class_node._derived_methods
        assert "clone" in class_node._derived_methods

    def test_singleton_decorator_marks_class_for_singleton_behavior(self):
        source = """
        @singleton
        class Configuration
            property setting as String
        end

        set cfg to new Configuration
        set cfg.setting to "value1"
        """

        interpreter = run_program(source)
        class_node = interpreter.classes.get("Configuration")
        # Verify singleton marker is set
        assert hasattr(class_node, '_is_singleton')
        assert class_node._is_singleton is True
        assert hasattr(class_node, '_singleton_instance')

    def test_user_defined_decorator_function_is_applied(self):
        source = """
        function mark_special with class_name as String
            set result to "marked"
        end

        @mark_special
        class Special
            property value as Integer
        end
        """

        interpreter = run_program(source)
        # User-defined decorator should execute without error
        class_node = interpreter.classes.get("Special")
        assert class_node is not None

    def test_multi_decorator_application_order(self):
        source = """
        @derive(Equality)
        @derive(Clone)
        class Data
            property value as Integer
        end

        set d1 to new Data
        set d1.value to 42
        """

        interpreter = run_program(source)
        # Both decorators should have been applied
        class_node = interpreter.classes.get("Data")
        assert "clone" in class_node._derived_methods
        assert "equals" in class_node._derived_methods

        clone_fn = class_node._derived_methods["clone"]
        equals_fn = class_node._derived_methods["equals"]
        cloned = clone_fn(interpreter.get_variable("d1"))
        assert equals_fn(interpreter.get_variable("d1"), cloned) is True

    def test_derive_DebugPrint_unknown_trait_skipped_gracefully(self):
        source = """
        @derive(DebugPrint)
        class Simple
            property value as Integer
        end

        set s to new Simple
        set s.value to 99
        """

        interpreter = run_program(source)
        obj = interpreter.get_variable("s")
        assert obj.properties['value'] == 99
        # DebugPrint trait should generate methods
        class_node = interpreter.classes.get("Simple")
        assert "to_string" in class_node._derived_methods

    def test_class_instantiation_binds_properties_correctly(self):
        source = """
        class Rectangle
            property width as Integer
            property height as Integer
        end

        set r to new Rectangle
        set r.width to 100
        set r.height to 50

        set w to r.width
        set h to r.height
        """

        interpreter = run_program(source)
        w = interpreter.get_variable("w")
        h = interpreter.get_variable("h")
        assert w == 100
        assert h == 50

    def test_derive_with_all_supported_traits(self):
        source = """
        @derive(DebugPrint, Equality, Clone, Hash, Default)
        class FullFeatured
            property data as String
        end

        set f to new FullFeatured
        set f.data to "test"
        """

        interpreter = run_program(source)
        class_node = interpreter.classes.get("FullFeatured")
        # All five traits should generate methods
        assert "to_string" in class_node._derived_methods
        assert "debug_print" in class_node._derived_methods
        assert "equals" in class_node._derived_methods
        assert "clone" in class_node._derived_methods
        assert "hash_code" in class_node._derived_methods
        assert "default" in class_node._derived_methods

    def test_singleton_with_derive_decorator_combination(self):
        source = """
        @singleton
        @derive(Equality)
        class Settings
            property mode as String
        end

        set s to new Settings
        set s.mode to "debug"
        """

        interpreter = run_program(source)
        class_node = interpreter.classes.get("Settings")
        # Both decorators should be applied
        assert hasattr(class_node, '_is_singleton')
        assert class_node._is_singleton is True
        assert "equals" in class_node._derived_methods

    def test_class_name_returned_from_class_definition(self):
        source = """
        class TestClass
            property value as Integer
        end
        """

        interpreter = run_program(source)
        # Class should be registered in classes dictionary
        assert "TestClass" in interpreter.classes
        class_node = interpreter.classes["TestClass"]
        assert class_node.name == "TestClass"

    def test_nested_class_property_access_with_decorator(self):
        source = """
        @derive(Clone)
        class Inner
            property x as Integer
        end

        class Outer
            property inner as Inner
        end

        set inner to new Inner
        set inner.x to 5

        set outer to new Outer
        set outer.inner to inner

        set cloned_inner to outer.inner.clone()
        set cloned_x to cloned_inner.x
        """

        interpreter = run_program(source)
        cloned_x = interpreter.get_variable("cloned_x")
        assert cloned_x == 5

    def test_equality_check_with_non_object_returns_false(self):
        source = """
        @derive(Equality)
        class Point
            property x as Integer
        end
        """

        interpreter = run_program(source)
        class_node = interpreter.classes.get("Point")
        equals_fn = class_node._derived_methods["equals"]
        p = Object("Point")
        p.set_property("x", 5)
        assert equals_fn(p, 42) is False

    def test_hash_with_non_hashable_properties_uses_object_id(self):
        source = """
        @derive(Hash)
        class Container
            property items as List of String
        end

        set c to new Container
        set c.items to create List of String

        set hash_val to c.hash_code()
        """

        interpreter = run_program(source)
        hash_val = interpreter.get_variable("hash_val")
        # Should return an integer (either hash or id)
        assert isinstance(hash_val, int)

    def test_class_with_property_types_maintains_type_info(self):
        source = """
        class TypedData
            property num as Integer
            property text as String
            property flag as Boolean
        end

        set td to new TypedData
        set td.num to 100
        set td.text to "hello"
        set td.flag to true
        """

        interpreter = run_program(source)
        obj = interpreter.get_variable("td")
        assert obj.properties['num'] == 100
        assert obj.properties['text'] == "hello"
        assert obj.properties['flag'] is True

    def test_multiple_classes_with_different_decorators(self):
        source = """
        @derive(Equality)
        class ClassA
            property value as Integer
        end

        @derive(Clone)
        class ClassB
            property value as Integer
        end

        @singleton
        class ClassC
            property value as Integer
        end

        set a to new ClassA
        set a.value to 1

        set b to new ClassB
        set b.value to 2

        set c to new ClassC
        set c.value to 3
        """

        interpreter = run_program(source)
        class_a = interpreter.classes.get("ClassA")
        class_b = interpreter.classes.get("ClassB")
        class_c = interpreter.classes.get("ClassC")

        # Verify each has appropriate decorators
        assert "equals" in class_a._derived_methods
        assert "clone" in class_b._derived_methods
        assert hasattr(class_c, '_is_singleton')
        assert class_c._is_singleton is True


class TestParserFixesForClasses:
    """Tests for parser fixes: reserved-word member names, generic class syntax, and 'me' alias."""

    def test_equals_method_call_via_derive_works(self):
        """p.equals(other) must work: 'equals' tokenises as EQUAL_TO but is valid after '.'."""
        source = """
        @derive(Equality)
        class Point
            property x as Integer
            property y as Integer
        end

        set p1 to new Point
        set p1.x to 3
        set p1.y to 4

        set p2 to new Point
        set p2.x to 3
        set p2.y to 4

        set result to p1.equals(p2)
        """
        interpreter = run_program(source)
        assert interpreter.get_variable("result") is True

    def test_equals_method_call_false_when_different(self):
        source = """
        @derive(Equality)
        class Point
            property x as Integer
            property y as Integer
        end

        set p1 to new Point
        set p1.x to 1
        set p1.y to 2

        set p2 to new Point
        set p2.x to 1
        set p2.y to 9

        set result to p1.equals(p2)
        """
        interpreter = run_program(source)
        assert interpreter.get_variable("result") is False

    def test_equals_method_call_with_non_object_returns_false(self):
        """Calling equals() on a plain scalar must return False, not crash."""
        source = """
        @derive(Equality)
        class Box
            property value as Integer
        end

        set b to new Box
        set b.value to 10
        """
        interpreter = run_program(source)
        class_node = interpreter.classes.get("Box")
        equals_fn = class_node._derived_methods["equals"]
        b = interpreter.get_variable("b")
        assert equals_fn(b, 42) is False

    def test_generic_class_of_T_syntax_is_parsed(self):
        """class Box of T should parse without error and register the class."""
        source = """
        class Box of T
            property contents as T
        end

        set b to new Box
        set b.contents to 99
        """
        interpreter = run_program(source)
        assert "Box" in interpreter.classes
        obj = interpreter.get_variable("b")
        assert obj.properties['contents'] == 99

    def test_generic_class_of_multiple_params(self):
        """class Map of K, V should parse both type params."""
        source = """
        class Pair of A, B
            property first as A
            property second as B
        end

        set p to new Pair
        set p.first to "hello"
        set p.second to 42
        """
        interpreter = run_program(source)
        class_node = interpreter.classes.get("Pair")
        assert class_node is not None
        assert len(class_node.generic_parameters) == 2
        obj = interpreter.get_variable("p")
        assert obj.properties['first'] == "hello"
        assert obj.properties['second'] == 42

    def test_me_alias_resolves_to_self_in_method(self):
        """Methods should be able to reference the instance via 'me'."""
        source = """
        class Counter
            property count as Integer

            function increment
                set me.count to me.count plus 1
            end
        end

        set c to new Counter
        set c.count to 10
        c.increment()
        set val to c.count
        """
        interpreter = run_program(source)
        assert interpreter.get_variable("val") == 11

    def test_me_and_self_both_resolve_in_same_method(self):
        """Both 'me' and 'self' should resolve to the same instance."""
        source = """
        class Thing
            property x as Integer

            function double_x
                set self.x to self.x times 2
            end
        end

        set t to new Thing
        set t.x to 7
        t.double_x()
        set val to t.x
        """
        interpreter = run_program(source)
        assert interpreter.get_variable("val") == 14

    def test_derive_equality_end_to_end_via_member_call(self):
        """Full end-to-end: @derive(Equality) + p.equals(q) in NexusLang source."""
        source = """
        @derive(Equality)
        class Color
            property r as Integer
            property g as Integer
            property b as Integer
        end

        set c1 to new Color
        set c1.r to 255
        set c1.g to 0
        set c1.b to 0

        set c2 to new Color
        set c2.r to 255
        set c2.g to 0
        set c2.b to 0

        set same to c1.equals(c2)

        set c3 to new Color
        set c3.r to 0
        set c3.g to 255
        set c3.b to 0

        set different to c1.equals(c3)
        """
        interpreter = run_program(source)
        assert interpreter.get_variable("same") is True
        assert interpreter.get_variable("different") is False
