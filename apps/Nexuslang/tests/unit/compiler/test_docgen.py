"""Tests for the NexusLang documentation generator (section 1.4).

Coverage:
- Lexer: DOC_COMMENT token emission
- Parser: doc comment attachment to AST nodes
- extractor._parse_doc_tags: tag parsing
- extractor.extract_from_source: token-based extraction
- extractor.extract_from_directory: multi-file extraction
- html_writer.generate_html: HTML / JSON output
- doc_tester.run_doc_tests: example execution
"""

import json
import sys
import types
from pathlib import Path

import pytest

# ── bootstrap path so we can import the NexusLang package directly ──────────────
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

from nexuslang.parser.lexer import Lexer, TokenType
from nexuslang.tooling.docgen.extractor import (
    DocItem,
    ParamDoc,
    _parse_doc_tags,
    extract_from_directory,
    extract_from_source,
)
from nexuslang.tooling.docgen.html_writer import (
    _build_search_index,
    _highlight_nxl,
    _page_filename,
    _slug,
    generate_html,
)
from nexuslang.tooling.docgen.doc_tester import (
    DocTestFailure,
    DocTestResult,
    run_doc_tests,
)


# ============================================================================
# 1. Lexer — DOC_COMMENT token
# ============================================================================

class TestDocCommentLexer:
    def _tokens(self, source: str):
        return [(t.type, t.literal, t.lexeme) for t in Lexer(source).tokenize()
                if t.type == TokenType.DOC_COMMENT]

    def test_doc_comment_emitted(self):
        tokens = self._tokens("## Hello world\n")
        assert len(tokens) == 1
        tt, lit, lex = tokens[0]
        assert tt == TokenType.DOC_COMMENT
        assert "Hello world" in (lit or lex)

    def test_single_hash_not_emitted(self):
        tokens = self._tokens("# regular comment\n")
        assert tokens == []

    def test_multiple_doc_comments(self):
        src = "## Line one\n## Line two\n"
        tokens = self._tokens(src)
        assert len(tokens) == 2

    def test_doc_comment_followed_by_code(self):
        src = "## A function\nfunction foo\n    return 1\nend\n"
        all_tokens = Lexer(src).tokenize()
        types_list = [t.type.name for t in all_tokens]
        assert "DOC_COMMENT" in types_list
        assert "FUNCTION" in types_list
        doc_idx = next(i for i, t in enumerate(all_tokens) if t.type.name == "DOC_COMMENT")
        fn_idx = next(i for i, t in enumerate(all_tokens) if t.type.name == "FUNCTION")
        assert doc_idx < fn_idx

    def test_empty_doc_comment(self):
        src = "##\n"
        tokens = self._tokens(src)
        assert len(tokens) == 1

    def test_doc_comment_literal_no_hashes(self):
        src = "## clean text here\n"
        tokens = self._tokens(src)
        assert tokens
        lit = tokens[0][1] or ""
        assert "#" not in lit
        assert "clean text here" in lit


# ============================================================================
# 2. Parser — doc attachment
# ============================================================================

class TestParserDocAttachment:
    def _parse(self, source: str):
        from nexuslang.parser.parser import Parser
        tokens = Lexer(source).tokenize()
        parser = Parser(tokens)
        return parser.parse()

    def test_doc_attached_to_function(self):
        src = "## My function doc\nfunction greet\n    print text \"hi\"\nend\n"
        prog = self._parse(src)
        assert prog.statements
        stmt = prog.statements[0]
        assert hasattr(stmt, "doc") and stmt.doc
        assert "My function doc" in stmt.doc

    def test_doc_attached_to_class(self):
        src = "## A class.\nclass Animal\n    property name as String\nend\n"
        prog = self._parse(src)
        stmt = prog.statements[0]
        assert hasattr(stmt, "doc") and stmt.doc
        assert "A class" in stmt.doc

    def test_no_doc_without_double_hash(self):
        src = "# regular\nfunction greet\n    print text \"hi\"\nend\n"
        prog = self._parse(src)
        stmt = prog.statements[0]
        # Should either have no doc or an empty one
        assert not getattr(stmt, "doc", None)

    def test_multiple_doc_lines_joined(self):
        src = (
            "## First line\n"
            "## Second line\n"
            "function foo\n"
            "    return 1\n"
            "end\n"
        )
        prog = self._parse(src)
        doc = getattr(prog.statements[0], "doc", "")
        assert "First line" in doc
        assert "Second line" in doc

    def test_doc_cleared_between_definitions(self):
        src = (
            "## Doc for foo\n"
            "function foo\n    return 1\nend\n"
            "function bar\n    return 2\nend\n"
        )
        prog = self._parse(src)
        assert len(prog.statements) >= 2
        doc_bar = getattr(prog.statements[1], "doc", "") or ""
        assert "Doc for foo" not in doc_bar


# ============================================================================
# 3. _parse_doc_tags
# ============================================================================

class TestParseDocTags:
    def test_description_only(self):
        lines = ["Compute the factorial of n.", "Returns n! as an integer."]
        desc, params, returns, examples, see_also, deprecated = _parse_doc_tags(lines)
        assert "factorial" in desc
        assert params == []
        assert returns is None
        assert examples == []
        assert see_also == []
        assert deprecated is None

    def test_param_tag(self):
        lines = ["@param n the integer to compute"]
        _, params, *_ = _parse_doc_tags(lines)
        assert len(params) == 1
        assert params[0].name == "n"
        assert "integer" in params[0].description

    def test_multiple_params(self):
        lines = ["@param a first", "@param b second"]
        _, params, *_ = _parse_doc_tags(lines)
        assert len(params) == 2
        names = [p.name for p in params]
        assert "a" in names
        assert "b" in names

    def test_returns_tag(self):
        lines = ["@returns the sum as Integer"]
        _, _, returns, *_ = _parse_doc_tags(lines)
        assert returns is not None
        assert "sum" in returns

    def test_example_block(self):
        lines = [
            "@example",
            "set x to 5",
            "print text x",
            "@end",
        ]
        _, _, _, examples, *_ = _parse_doc_tags(lines)
        assert len(examples) == 1
        assert "set x to 5" in examples[0]
        assert "print text x" in examples[0]

    def test_multiple_examples(self):
        lines = [
            "@example",
            "set a to 1",
            "@end",
            "@example",
            "set b to 2",
            "@end",
        ]
        _, _, _, examples, *_ = _parse_doc_tags(lines)
        assert len(examples) == 2
        assert "set a to 1" in examples[0]
        assert "set b to 2" in examples[1]

    def test_see_also_tag(self):
        lines = ["@see other_function"]
        _, _, _, _, see_also, _ = _parse_doc_tags(lines)
        assert "other_function" in see_also

    def test_deprecated_tag_no_message(self):
        lines = ["@deprecated"]
        _, _, _, _, _, deprecated = _parse_doc_tags(lines)
        assert deprecated is not None

    def test_deprecated_tag_with_message(self):
        lines = ["@deprecated use new_function instead"]
        _, _, _, _, _, deprecated = _parse_doc_tags(lines)
        assert deprecated is not None
        assert "new_function" in deprecated

    def test_description_before_tags(self):
        lines = [
            "Main description here.",
            "@param x the x value",
            "@returns the result",
        ]
        desc, params, returns, *_ = _parse_doc_tags(lines)
        assert "Main description" in desc
        assert len(params) == 1
        assert returns is not None

    def test_empty_lines(self):
        lines = ["A description.", "", "@param x something"]
        desc, params, *_ = _parse_doc_tags(lines)
        assert "A description" in desc
        assert len(params) == 1

    def test_unterminated_example_block(self):
        lines = ["@example", "set x to 1"]
        _, _, _, examples, *_ = _parse_doc_tags(lines)
        assert len(examples) == 1
        assert "set x to 1" in examples[0]


# ============================================================================
# 4. extract_from_source
# ============================================================================

class TestExtractFromSource:
    def test_function_extracted(self):
        src = (
            "## Compute the sum of two numbers.\n"
            "## @param a first operand\n"
            "## @param b second operand\n"
            "## @returns sum as Integer\n"
            "function add with a as Integer and b as Integer returns Integer\n"
            "    return a plus b\n"
            "end\n"
        )
        items = extract_from_source(src)
        assert len(items) == 1
        item = items[0]
        assert item.name == "add"
        assert item.kind == "function"
        assert "sum" in item.description
        assert len(item.params) == 2
        assert item.returns is not None
        assert "sum" in item.returns.lower() or "Integer" in item.returns

    def test_class_extracted(self):
        src = (
            "## Represents a 2D point.\n"
            "class Point\n"
            "end\n"
        )
        items = extract_from_source(src)
        assert len(items) == 1
        assert items[0].name == "Point"
        assert items[0].kind == "class"
        assert "2D point" in items[0].description

    def test_struct_extracted(self):
        src = (
            "## Low-level coordinate pair.\n"
            "struct Vector2\n"
            "    x as Float\n"
            "    y as Float\n"
            "end\n"
        )
        items = extract_from_source(src)
        assert len(items) == 1
        assert items[0].kind == "struct"

    def test_undocumented_function_not_included(self):
        src = (
            "function helper\n"
            "    return 42\n"
            "end\n"
        )
        items = extract_from_source(src)
        assert items == []

    def test_multiple_definitions(self):
        src = (
            "## First.\n"
            "function foo\n    return 1\nend\n"
            "## Second.\n"
            "function bar\n    return 2\nend\n"
        )
        items = extract_from_source(src)
        assert len(items) == 2
        names = {i.name for i in items}
        assert "foo" in names
        assert "bar" in names

    def test_doc_only_attaches_to_next_definition(self):
        src = (
            "## Docs for one.\n"
            "function one\n    return 1\nend\n"
            "function two\n    return 2\nend\n"
        )
        items = extract_from_source(src)
        assert len(items) == 1
        assert items[0].name == "one"

    def test_source_file_metadata(self):
        src = "## A thing.\nclass Thing\nend\n"
        items = extract_from_source(src, source_file="mylib/thing.nxl")
        assert items[0].source_file == "mylib/thing.nxl"

    def test_with_example_block(self):
        src = (
            "## A function.\n"
            "## @example\n"
            "## set result to my_function\n"
            "## @end\n"
            "function my_function\n    return 0\nend\n"
        )
        items = extract_from_source(src)
        assert len(items) == 1
        assert len(items[0].examples) == 1
        assert "my_function" in items[0].examples[0]

    def test_deprecated_item(self):
        src = (
            "## Old approach.\n"
            "## @deprecated use new_function instead\n"
            "function old_function\n    return 0\nend\n"
        )
        items = extract_from_source(src)
        assert items[0].is_deprecated
        assert "new_function" in (items[0].deprecated or "")

    def test_empty_source(self):
        assert extract_from_source("") == []

    def test_source_without_doc_comments(self):
        src = "function foo\n    return 1\nend\n"
        assert extract_from_source(src) == []

    def test_doc_item_to_dict(self):
        src = "## A simple item.\nfunction simple\n    return 0\nend\n"
        items = extract_from_source(src)
        d = items[0].to_dict()
        assert d["name"] == "simple"
        assert d["kind"] == "function"
        assert isinstance(d["params"], list)


# ============================================================================
# 5. extract_from_directory
# ============================================================================

class TestExtractFromDirectory:
    def test_extracts_from_multiple_files(self, tmp_path):
        (tmp_path / "a.nxl").write_text(
            "## Item A.\nfunction a_func\n    return 0\nend\n", encoding="utf-8"
        )
        (tmp_path / "b.nxl").write_text(
            "## Item B.\nfunction b_func\n    return 0\nend\n", encoding="utf-8"
        )
        result = extract_from_directory(str(tmp_path))
        all_names = {i.name for lst in result.values() for i in lst}
        assert "a_func" in all_names
        assert "b_func" in all_names

    def test_ignores_non_nxl_files(self, tmp_path):
        (tmp_path / "readme.md").write_text("# not NLPL\n", encoding="utf-8")
        (tmp_path / "code.nxl").write_text(
            "## A function.\nfunction f\n    return 0\nend\n", encoding="utf-8"
        )
        result = extract_from_directory(str(tmp_path))
        assert all(k.endswith(".nxl") for k in result.keys())

    def test_empty_directory_returns_empty(self, tmp_path):
        result = extract_from_directory(str(tmp_path))
        assert result == {}

    def test_undocumented_file_not_in_result(self, tmp_path):
        (tmp_path / "no_docs.nxl").write_text(
            "function no_doc\n    return 1\nend\n", encoding="utf-8"
        )
        result = extract_from_directory(str(tmp_path))
        assert result == {}

    def test_recursive_subdirectory(self, tmp_path):
        sub = tmp_path / "sub"
        sub.mkdir()
        (sub / "deep.nxl").write_text(
            "## Deep item.\nfunction deep_fn\n    return 0\nend\n", encoding="utf-8"
        )
        result = extract_from_directory(str(tmp_path))
        all_names = {i.name for lst in result.values() for i in lst}
        assert "deep_fn" in all_names


# ============================================================================
# 6. HTML writer
# ============================================================================

class TestHtmlWriter:
    def _make_items(self):
        return [
            DocItem(
                name="compute",
                kind="function",
                description="Compute something useful.",
                params=[ParamDoc("x", "the input value"), ParamDoc("y", "the other value")],
                returns="the result as Float",
                examples=["set result to compute with x: 1 and y: 2\nprint text result"],
                see_also=["other_compute"],
                source_file="lib/math.nxl",
                line=10,
            ),
            DocItem(
                name="Vector",
                kind="struct",
                description="A 2D vector.",
                source_file="lib/math.nxl",
                line=50,
            ),
        ]

    def test_output_files_created(self, tmp_path):
        items = self._make_items()
        generate_html({"lib/math.nxl": items}, str(tmp_path), "TestPkg", "1.0")
        assert (tmp_path / "index.html").exists()
        assert (tmp_path / "search_index.json").exists()
        # At least one module page
        html_files = list(tmp_path.glob("*.html"))
        assert len(html_files) >= 2   # index + module

    def test_index_html_contains_module(self, tmp_path):
        items = self._make_items()
        generate_html({"lib/math.nxl": items}, str(tmp_path))
        index = (tmp_path / "index.html").read_text()
        assert "math" in index.lower()

    def test_module_page_contains_item_names(self, tmp_path):
        items = self._make_items()
        written = generate_html({"lib/math.nxl": items}, str(tmp_path))
        module_pages = [p for p in written if p.name != "index.html" and p.suffix == ".html"]
        assert module_pages
        content = module_pages[0].read_text()
        assert "compute" in content
        assert "Vector" in content

    def test_module_page_contains_params(self, tmp_path):
        items = self._make_items()
        written = generate_html({"lib/math.nxl": items}, str(tmp_path))
        module_pages = [p for p in written if p.name != "index.html" and p.suffix == ".html"]
        content = module_pages[0].read_text()
        assert "x" in content
        assert "the input value" in content

    def test_module_page_contains_returns(self, tmp_path):
        items = self._make_items()
        written = generate_html({"lib/math.nxl": items}, str(tmp_path))
        module_pages = [p for p in written if p.name != "index.html" and p.suffix == ".html"]
        content = module_pages[0].read_text()
        assert "result as Float" in content

    def test_module_page_contains_example(self, tmp_path):
        items = self._make_items()
        written = generate_html({"lib/math.nxl": items}, str(tmp_path))
        module_pages = [p for p in written if p.name != "index.html" and p.suffix == ".html"]
        content = module_pages[0].read_text()
        assert "example" in content.lower() or "compute" in content

    def test_search_index_valid_json(self, tmp_path):
        items = self._make_items()
        generate_html({"lib/math.nxl": items}, str(tmp_path))
        raw = (tmp_path / "search_index.json").read_text()
        data = json.loads(raw)
        assert isinstance(data, list)
        assert all({"name", "kind", "file", "anchor", "description"} <= set(d.keys()) for d in data)

    def test_search_index_contains_items(self, tmp_path):
        items = self._make_items()
        generate_html({"lib/math.nxl": items}, str(tmp_path))
        data = json.loads((tmp_path / "search_index.json").read_text())
        names = {d["name"] for d in data}
        assert "compute" in names
        assert "Vector" in names

    def test_empty_items_by_file(self, tmp_path):
        written = generate_html({}, str(tmp_path), "Pkg", "0.1")
        assert (tmp_path / "index.html").exists()
        assert len(written) >= 2   # index.html + search_index.json

    def test_package_name_in_index(self, tmp_path):
        generate_html({}, str(tmp_path), pkg_name="MyAwesomeLib", pkg_version="2.0")
        index = (tmp_path / "index.html").read_text()
        assert "MyAwesomeLib" in index

    def test_deprecated_badge_in_page(self, tmp_path):
        items = [
            DocItem(
                name="old_fn",
                kind="function",
                description="Deprecated.",
                deprecated="use new_fn",
                source_file="lib/old.nxl",
                line=1,
            )
        ]
        generate_html({"lib/old.nxl": items}, str(tmp_path))
        pages = list(tmp_path.glob("*.html"))
        module_page = next((p for p in pages if p.name != "index.html"), None)
        assert module_page is not None
        content = module_page.read_text()
        assert "deprecated" in content.lower()
        assert "new_fn" in content


class TestSlugAndHelpers:
    def test_slug_basic(self):
        assert _slug("my function") == "my-function"

    def test_slug_special_chars(self):
        assert _slug("foo::bar") == "foo--bar"

    def test_slug_lowercase(self):
        assert _slug("FooBar") == "foobar"

    def test_page_filename_strips_nxl(self):
        name = _page_filename("lib/math.nxl")
        assert name.endswith(".html")
        assert ".nxl" not in name

    def test_highlight_keywords(self):
        result = _highlight_nxl("function foo returns Integer")
        assert '<span class="kw">' in result
        assert "function" in result

    def test_highlight_string(self):
        result = _highlight_nxl('set x to "hello"')
        assert '<span class="str">' in result

    def test_highlight_comment(self):
        result = _highlight_nxl("## This is a doc comment")
        assert '<span class="cmt">' in result

    def test_highlight_escapes_html(self):
        result = _highlight_nxl('<script>alert(1)</script>')
        assert "<script>" not in result


# ============================================================================
# 7. Doc tester
# ============================================================================

class TestDocTester:
    def _item(self, name: str, examples: list[str], kind: str = "function") -> DocItem:
        return DocItem(
            name=name,
            kind=kind,
            description="",
            examples=examples,
            source_file="test.nxl",
            line=1,
        )

    def test_passing_example(self):
        item = self._item("greet", ['print text "hello"'])
        result = run_doc_tests([item])
        assert result.passed == 1
        assert result.failed == 0
        assert result.all_passed

    def test_failing_example(self):
        item = self._item("bad", ["this is definitely invalid {{{{;!!! nlpl code"])
        result = run_doc_tests([item])
        assert result.failed == 1
        assert not result.all_passed

    def test_no_examples_skipped(self):
        item = self._item("no_examples", [])
        result = run_doc_tests([item])
        assert result.total == 0

    def test_multiple_items_mixed(self):
        items = [
            self._item("good", ['set x to 1\nprint text x']),
            self._item("bad", ["this {{{{ is invalid"]),
        ]
        result = run_doc_tests(items)
        assert result.passed == 1
        assert result.failed == 1

    def test_failure_details(self):
        item = self._item("broken", ["totally broken @@@###"])
        result = run_doc_tests([item])
        assert result.failures
        f = result.failures[0]
        assert f.item_name == "broken"
        assert f.example_index == 0
        assert "broken" in f.code or "@@@" in f.code

    def test_stop_on_first_failure(self):
        items = [
            self._item("bad1", ["invalid @@@"]),
            self._item("bad2", ["also invalid @@@"]),
        ]
        result = run_doc_tests(items, stop_on_first_failure=True)
        assert result.failed == 1   # stopped after first

    def test_empty_example_skipped(self):
        item = self._item("empty_ex", ["  \n  \n"])
        result = run_doc_tests([item])
        assert result.skipped == 1
        assert result.passed == 0

    def test_print_summary_no_error(self, capsys):
        item = self._item("ok", ['print text "hello"'])
        result = run_doc_tests([item])
        result.print_summary()
        captured = capsys.readouterr()
        assert "passed" in captured.out

    def test_print_summary_failures_shown(self, capsys):
        item = self._item("fail", ["invalid ###"])
        result = run_doc_tests([item])
        result.print_summary()
        captured = capsys.readouterr()
        assert "FAIL" in captured.out or "failed" in captured.out

    def test_result_total(self):
        items = [
            self._item("a", ['set x to 1']),
            self._item("b", ['set y to 2']),
            self._item("c", ['invalid @@@']),
        ]
        result = run_doc_tests(items)
        assert result.total == result.passed + result.failed + result.skipped
        assert result.total >= 3

    def test_doc_test_failure_label(self):
        f = DocTestFailure(
            item_name="my_func",
            source_file="lib/foo.nxl",
            example_index=0,
            code="bad code",
            error="SyntaxError: nope",
        )
        assert "my_func" in f.label
        assert "example 1" in f.label
