"""
Test cases for the NexusLang standard library.
Tests all standard library modules and functions using valid NexusLang syntax.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

import math
import unittest
import tempfile
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib


class TestStandardLibrary(unittest.TestCase):
    """Test cases for the NexusLang standard library."""

    def setUp(self):
        """Set up test fixtures before each test method."""
        self.runtime = Runtime()
        register_stdlib(self.runtime)
        self.interpreter = Interpreter(self.runtime)
        self.temp_dir = tempfile.mkdtemp()
        self.original_cwd = os.getcwd()
        os.chdir(self.temp_dir)

    def tearDown(self):
        """Clean up after each test method."""
        os.chdir(self.original_cwd)
        import shutil
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def _run(self, source):
        self.interpreter.interpret(source)

    def _get(self, name):
        return self.interpreter.get_variable(name)

    def test_math_module(self):
        """Test math module functions."""
        source = """
set pi_val to PI
set e_val to E
set abs_val to absolute(42)
set sqrt_val to square_root(16)
set pow_val to pow(2, 3)
set sin_val to sine(1.5707963267948966)
set cos_val to cosine(0)
set log_val to log(8, 2)
set ln_val to ln(2.718281828459045)
set numbers to [1, 2, 3, 4, 5]
set mean_val to mean(numbers)
"""
        self._run(source)
        self.assertAlmostEqual(self._get("pi_val"), math.pi)
        self.assertAlmostEqual(self._get("e_val"), math.e)
        self.assertEqual(self._get("abs_val"), 42)
        self.assertEqual(self._get("sqrt_val"), 4.0)
        self.assertAlmostEqual(self._get("pow_val"), 8.0)
        self.assertAlmostEqual(self._get("sin_val"), 1.0, places=6)
        self.assertEqual(self._get("cos_val"), 1.0)
        self.assertAlmostEqual(self._get("log_val"), 3.0, places=6)
        self.assertAlmostEqual(self._get("ln_val"), 1.0, places=6)
        self.assertEqual(self._get("mean_val"), 3.0)

    def test_string_module(self):
        """Test string module functions."""
        source = """
set str1 to "hello"
set str2 to "world"
set upper to uppercase(str1)
set lower to lowercase("HELLO")
set ttl to title_case("hello world")
set str_len to length(str1)
set concat to str_concat(str1, " ", str2)
set substr to substring(str1, 0, 2)
set cnts to str_contains("hello world", "world")
set idx to index_of("hello world", "world")
set sw to str_starts_with("hello world", "hello")
set ew to str_ends_with("hello world", "world")
set repl to replace("hello world", "world", "there")
set wrds to str_split("hello world", " ")
set jnd to str_join("-", wrds)
"""
        self._run(source)
        self.assertEqual(self._get("upper"), "HELLO")
        self.assertEqual(self._get("lower"), "hello")
        self.assertEqual(self._get("ttl"), "Hello World")
        self.assertEqual(self._get("str_len"), 5)
        self.assertEqual(self._get("concat"), "hello world")
        self.assertEqual(self._get("substr"), "he")
        self.assertTrue(self._get("cnts"))
        self.assertEqual(self._get("idx"), 6)
        self.assertTrue(self._get("sw"))
        self.assertTrue(self._get("ew"))
        self.assertEqual(self._get("repl"), "hello there")
        self.assertEqual(self._get("wrds"), ["hello", "world"])
        self.assertEqual(self._get("jnd"), "hello-world")

    def test_io_module(self):
        """Test IO module functions."""
        source = """
set content to "Hello World"
set file_path to "nxl_test_io.txt"
set r1 to write_file(file_path, content)
set read_content to read_file(file_path)
set exists to file_exists(file_path)
set r2 to delete_file(file_path)
set deleted_exists to file_exists(file_path)
set dir_path to "nxl_test_dir_xyz"
set r3 to create_directory(dir_path)
set dir_exists to directory_exists(dir_path)
set files to list_directory(dir_path)
set r4 to delete_directory(dir_path)
set dir_deleted to directory_exists(dir_path)
"""
        self._run(source)
        self.assertEqual(self._get("read_content"), "Hello World")
        self.assertTrue(self._get("exists"))
        self.assertFalse(self._get("deleted_exists"))
        self.assertTrue(self._get("dir_exists"))
        self.assertEqual(self._get("files"), [])
        self.assertFalse(self._get("dir_deleted"))

    def test_system_module(self):
        """Test system module functions."""
        source = """
set os_name to get_os_name()
set os_ver to get_os_version()
set plat to get_platform()
set py_ver to get_python_version()
set hname to get_hostname()
set pid to get_process_id()
set cur_time to get_current_time()
set cur_date to get_current_date()
set r1 to set_env("NLPL_TEST_VAR_42", "test_value")
set env_val to get_env("NLPL_TEST_VAR_42")
"""
        self._run(source)
        self.assertIsInstance(self._get("os_name"), str)
        self.assertIsInstance(self._get("os_ver"), str)
        self.assertIsInstance(self._get("plat"), str)
        self.assertIsInstance(self._get("py_ver"), str)
        self.assertIsInstance(self._get("hname"), str)
        self.assertIsInstance(self._get("pid"), int)
        self.assertIsInstance(self._get("cur_time"), str)
        self.assertIsInstance(self._get("cur_date"), str)
        self.assertEqual(self._get("env_val"), "test_value")

    def test_collections_module(self):
        """Test collections module functions."""
        source = """
set numbers to [1, 2, 3, 4, 5]
set r1 to append(numbers, 6)
set lst_len to length(numbers)
set first_val to numbers[0]
set last_val to numbers[5]
set d to {"a": 1, "b": 2}
set val_a to dict_get(d, "a")
set r2 to dict_set(d, "c", 3)
set val_c to dict_get(d, "c")
"""
        self._run(source)
        self.assertEqual(self._get("lst_len"), 6)
        self.assertEqual(self._get("first_val"), 1)
        self.assertEqual(self._get("last_val"), 6)
        self.assertEqual(self._get("val_a"), 1)
        self.assertEqual(self._get("val_c"), 3)

    def test_network_module(self):
        """Test network module functions."""
        source = """
set url to "https://example.com/path?param=value"
set parsed_url to url_parse(url)
set encoded to url_encode("Hello World")
set decoded to url_decode(encoded)
"""
        self._run(source)
        parsed = self._get("parsed_url")
        self.assertEqual(parsed["scheme"], "https")
        self.assertEqual(parsed["netloc"], "example.com")
        self.assertEqual(parsed["path"], "/path")
        self.assertEqual(parsed["query"], "param=value")
        self.assertEqual(self._get("decoded"), "Hello World")


if __name__ == '__main__':
    unittest.main()
