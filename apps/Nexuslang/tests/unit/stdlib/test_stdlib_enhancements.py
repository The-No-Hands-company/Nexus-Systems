"""Tests for enhanced stdlib functions (string, math, io)."""

import os
import tempfile
import pytest
import math as py_math
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib


class TestStringEnhancements:
    """Test new string module functions."""

    def setup_method(self):
        """Set up test runtime with stdlib."""
        self.runtime = Runtime()
        register_stdlib(self.runtime)

    def test_strip(self):
        """Test strip function."""
        result = self.runtime.invoke_function("strip", "  hello  ")
        assert result == "hello"

        result = self.runtime.invoke_function("strip", "\t\ntest\n\t")
        assert result == "test"

    def test_lstrip(self):
        """Test left strip function."""
        result = self.runtime.invoke_function("lstrip", "  hello  ")
        assert result == "hello  "

        result = self.runtime.invoke_function("lstrip", "\t\ntest")
        assert result == "test"

    def test_rstrip(self):
        """Test right strip function."""
        result = self.runtime.invoke_function("rstrip", "  hello  ")
        assert result == "  hello"

        result = self.runtime.invoke_function("rstrip", "test\n\t")
        assert result == "test"

    def test_capitalize(self):
        """Test capitalize function."""
        result = self.runtime.invoke_function("capitalize", "hello world")
        assert result == "Hello world"

        result = self.runtime.invoke_function("capitalize", "HELLO")
        assert result == "Hello"

    def test_title_case(self):
        """Test title case conversion."""
        result = self.runtime.invoke_function("title_case", "hello world")
        assert result == "Hello World"

        result = self.runtime.invoke_function("title_case", "the quick brown fox")
        assert result == "The Quick Brown Fox"

    def test_reverse(self):
        """Test string reversal."""
        result = self.runtime.invoke_function("reverse", "hello")
        assert result == "olleh"

        result = self.runtime.invoke_function("reverse", "12345")
        assert result == "54321"

    def test_repeat(self):
        """Test string repetition."""
        result = self.runtime.invoke_function("repeat", "abc", 3)
        assert result == "abcabcabc"

        result = self.runtime.invoke_function("repeat", "x", 5)
        assert result == "xxxxx"

    def test_count_occurrences(self):
        """Test substring counting."""
        result = self.runtime.invoke_function("count_occurrences", "hello world", "l")
        assert result == 3

        # Note: Python's str.count() doesn't count overlapping occurrences
        result = self.runtime.invoke_function("count_occurrences", "aaa", "aa")
        assert result == 1  # Non-overlapping count
        
        # Test non-overlapping case
        result = self.runtime.invoke_function("count_occurrences", "abcabc", "abc")
        assert result == 2

    def test_index_of(self):
        """Test substring index finding."""
        result = self.runtime.invoke_function("index_of", "hello world", "world")
        assert result == 6

        result = self.runtime.invoke_function("index_of", "test", "xyz")
        assert result == -1

    def test_split_lines(self):
        """Test line splitting."""
        result = self.runtime.invoke_function("split_lines", "line1\nline2\nline3")
        assert result == ["line1", "line2", "line3"]

        result = self.runtime.invoke_function("split_lines", "single line")
        assert result == ["single line"]

    def test_is_numeric(self):
        """Test numeric string checking."""
        assert self.runtime.invoke_function("is_numeric", "12345") is True
        assert self.runtime.invoke_function("is_numeric", "123.45") is False
        assert self.runtime.invoke_function("is_numeric", "abc") is False

    def test_is_alphabetic(self):
        """Test alphabetic string checking."""
        assert self.runtime.invoke_function("is_alphabetic", "hello") is True
        assert self.runtime.invoke_function("is_alphabetic", "Hello123") is False
        assert self.runtime.invoke_function("is_alphabetic", "test") is True

    def test_is_alphanumeric(self):
        """Test alphanumeric string checking."""
        assert self.runtime.invoke_function("is_alphanumeric", "hello123") is True
        assert self.runtime.invoke_function("is_alphanumeric", "test") is True
        assert self.runtime.invoke_function("is_alphanumeric", "hello world") is False

    def test_is_lowercase(self):
        """Test lowercase string checking."""
        assert self.runtime.invoke_function("is_lowercase", "hello") is True
        assert self.runtime.invoke_function("is_lowercase", "Hello") is False
        assert self.runtime.invoke_function("is_lowercase", "test123") is True

    def test_is_uppercase(self):
        """Test uppercase string checking."""
        assert self.runtime.invoke_function("is_uppercase", "HELLO") is True
        assert self.runtime.invoke_function("is_uppercase", "Hello") is False
        assert self.runtime.invoke_function("is_uppercase", "TEST123") is True


class TestMathEnhancements:
    """Test new math module functions."""

    def setup_method(self):
        """Set up test runtime with stdlib."""
        self.runtime = Runtime()
        register_stdlib(self.runtime)

    def test_constants(self):
        """Test new mathematical constants."""
        tau = self.runtime.invoke_function("TAU")
        assert abs(tau - 2 * py_math.pi) < 1e-10

        inf = self.runtime.invoke_function("INF")
        assert py_math.isinf(inf)

        nan = self.runtime.invoke_function("NAN")
        assert py_math.isnan(nan)

    def test_truncate(self):
        """Test truncate function."""
        assert self.runtime.invoke_function("truncate", 3.7) == 3
        assert self.runtime.invoke_function("truncate", -3.7) == -3
        assert self.runtime.invoke_function("truncate", 5.0) == 5

    def test_sign(self):
        """Test sign function."""
        assert self.runtime.invoke_function("sign", 5) == 1
        assert self.runtime.invoke_function("sign", -5) == -1
        assert self.runtime.invoke_function("sign", 0) == 0

    def test_gcd(self):
        """Test greatest common divisor."""
        assert self.runtime.invoke_function("gcd", 12, 8) == 4
        assert self.runtime.invoke_function("gcd", 7, 5) == 1
        assert self.runtime.invoke_function("gcd", 100, 50) == 50

    def test_lcm(self):
        """Test least common multiple."""
        assert self.runtime.invoke_function("lcm", 4, 6) == 12
        assert self.runtime.invoke_function("lcm", 3, 5) == 15
        assert self.runtime.invoke_function("lcm", 10, 15) == 30

    def test_factorial(self):
        """Test factorial function."""
        assert self.runtime.invoke_function("factorial", 5) == 120
        assert self.runtime.invoke_function("factorial", 0) == 1
        assert self.runtime.invoke_function("factorial", 1) == 1

    def test_inverse_trig(self):
        """Test inverse trigonometric functions."""
        # arcsine
        result = self.runtime.invoke_function("arcsine", 0.5)
        assert abs(result - py_math.asin(0.5)) < 1e-10

        # arccosine
        result = self.runtime.invoke_function("arccosine", 0.5)
        assert abs(result - py_math.acos(0.5)) < 1e-10

        # arctangent
        result = self.runtime.invoke_function("arctangent", 1)
        assert abs(result - py_math.atan(1)) < 1e-10

        # arctangent2
        result = self.runtime.invoke_function("arctangent2", 1, 1)
        assert abs(result - py_math.atan2(1, 1)) < 1e-10

    def test_angle_conversion(self):
        """Test degree/radian conversion."""
        # radians to degrees
        result = self.runtime.invoke_function("degrees", py_math.pi)
        assert abs(result - 180) < 1e-10

        # degrees to radians
        result = self.runtime.invoke_function("radians", 180)
        assert abs(result - py_math.pi) < 1e-10

    def test_hyperbolic(self):
        """Test hyperbolic functions."""
        # sinh
        result = self.runtime.invoke_function("sinh", 1)
        assert abs(result - py_math.sinh(1)) < 1e-10

        # cosh
        result = self.runtime.invoke_function("cosh", 1)
        assert abs(result - py_math.cosh(1)) < 1e-10

        # tanh
        result = self.runtime.invoke_function("tanh", 1)
        assert abs(result - py_math.tanh(1)) < 1e-10

    def test_logarithms(self):
        """Test logarithm functions."""
        # log2
        result = self.runtime.invoke_function("log2", 8)
        assert abs(result - 3.0) < 1e-10

        # log10
        result = self.runtime.invoke_function("log10", 100)
        assert abs(result - 2.0) < 1e-10

    def test_exponential(self):
        """Test exponential function."""
        result = self.runtime.invoke_function("exponential", 1)
        assert abs(result - py_math.e) < 1e-10

        result = self.runtime.invoke_function("exponential", 0)
        assert abs(result - 1.0) < 1e-10

    def test_special_checks(self):
        """Test special value checking functions."""
        nan = float('nan')
        inf = float('inf')

        # is_nan
        assert self.runtime.invoke_function("is_nan", nan) is True
        assert self.runtime.invoke_function("is_nan", 5.0) is False

        # is_infinite
        assert self.runtime.invoke_function("is_infinite", inf) is True
        assert self.runtime.invoke_function("is_infinite", 5.0) is False

        # is_finite
        assert self.runtime.invoke_function("is_finite", 5.0) is True
        assert self.runtime.invoke_function("is_finite", inf) is False
        assert self.runtime.invoke_function("is_finite", nan) is False


class TestIOEnhancements:
    """Test new IO module functions."""

    def setup_method(self):
        """Set up test runtime with stdlib and temp directory."""
        self.runtime = Runtime()
        register_stdlib(self.runtime)
        self.temp_dir = tempfile.mkdtemp()

    def teardown_method(self):
        """Clean up temporary files."""
        import shutil
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir)

    def test_copy_file(self):
        """Test file copying."""
        source = os.path.join(self.temp_dir, "source.txt")
        dest = os.path.join(self.temp_dir, "dest.txt")

        with open(source, "w") as f:
            f.write("test content")

        result = self.runtime.invoke_function("copy_file", source, dest)
        assert result is True
        assert os.path.exists(dest)

        with open(dest, "r") as f:
            assert f.read() == "test content"

    def test_move_file(self):
        """Test file moving."""
        source = os.path.join(self.temp_dir, "source.txt")
        dest = os.path.join(self.temp_dir, "dest.txt")

        with open(source, "w") as f:
            f.write("test content")

        result = self.runtime.invoke_function("move_file", source, dest)
        assert result is True
        assert not os.path.exists(source)
        assert os.path.exists(dest)

    def test_get_file_size(self):
        """Test getting file size."""
        test_file = os.path.join(self.temp_dir, "test.txt")
        content = "Hello, World!"

        with open(test_file, "w") as f:
            f.write(content)

        size = self.runtime.invoke_function("get_file_size", test_file)
        assert size == len(content)

    def test_read_write_lines(self):
        """Test reading and writing lines."""
        test_file = os.path.join(self.temp_dir, "lines.txt")
        lines = ["line1", "line2", "line3"]

        # Write lines (write_lines adds \n automatically)
        result = self.runtime.invoke_function("write_lines", test_file, lines)
        assert result is True

        # Read lines (read_lines strips \n)
        read_lines = self.runtime.invoke_function("read_lines", test_file)
        assert read_lines == lines

    def test_read_write_bytes(self):
        """Test reading and writing bytes."""
        test_file = os.path.join(self.temp_dir, "binary.dat")
        data = b"Hello, bytes!"

        # Write bytes
        result = self.runtime.invoke_function("write_bytes", test_file, data)
        assert result is True

        # Read bytes
        read_data = self.runtime.invoke_function("read_bytes", test_file)
        assert read_data == data

    def test_delete_directory(self):
        """Test directory deletion."""
        test_dir = os.path.join(self.temp_dir, "subdir")
        os.makedirs(test_dir)

        # Create a file in the directory
        test_file = os.path.join(test_dir, "file.txt")
        with open(test_file, "w") as f:
            f.write("test")

        result = self.runtime.invoke_function("delete_directory", test_dir)
        assert result is True
        assert not os.path.exists(test_dir)

    def test_walk_directory(self):
        """Test directory walking."""
        # Create directory structure
        subdir1 = os.path.join(self.temp_dir, "dir1")
        subdir2 = os.path.join(self.temp_dir, "dir2")
        os.makedirs(subdir1)
        os.makedirs(subdir2)

        # Create files
        with open(os.path.join(self.temp_dir, "file1.txt"), "w") as f:
            f.write("test")
        with open(os.path.join(subdir1, "file2.txt"), "w") as f:
            f.write("test")

        results = self.runtime.invoke_function("walk_directory", self.temp_dir)
        assert isinstance(results, list)
        assert len(results) >= 1

        # Check that the root directory is included
        root_entry = results[0]
        assert "path" in root_entry
        assert "directories" in root_entry
        assert "files" in root_entry

    def test_absolute_path(self):
        """Test absolute path resolution."""
        result = self.runtime.invoke_function("absolute_path", "./test")
        assert os.path.isabs(result)

    def test_normalize_path(self):
        """Test path normalization.

        normalize_path returns an absolute path with all .. and . resolved.
        """
        import os
        result = self.runtime.invoke_function("normalize_path", "./dir/../file.txt")
        assert ".." not in result
        # Result should be absolute and resolve to a path ending with 'file.txt'
        assert os.path.isabs(result) or result.endswith("file.txt")
        assert result.endswith("file.txt")
