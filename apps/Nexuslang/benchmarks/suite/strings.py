"""
String processing benchmark suite.

Covers: splitting, joining, formatting, searching, regex,
encoding/decoding, template rendering.
"""
from __future__ import annotations

import re
import string
import random
from typing import List

from benchmarks.benchmark_ci import BenchmarkCase

CATEGORY = "strings"


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

_RNG = random.Random(42)
_WORDS = [
    "the", "quick", "brown", "fox", "jumps", "over", "lazy", "dog",
    "programming", "language", "natural", "benchmark", "performance",
    "compiler", "interpreter", "runtime", "memory", "allocation",
    "function", "class", "object", "method", "variable", "constant",
]

def _make_sentence(n_words: int, rng: random.Random = _RNG) -> str:
    return " ".join(rng.choice(_WORDS) for _ in range(n_words))

def _make_text(n_lines: int, words_per_line: int = 15) -> str:
    rng = random.Random(99)
    return "\n".join(_make_sentence(words_per_line, rng) for _ in range(n_lines))

_TEXT_100L  = _make_text(100)
_TEXT_1000L = _make_text(1000)
_TEXT_10KL  = _make_text(10_000)


# ---------------------------------------------------------------------------
# Split / join
# ---------------------------------------------------------------------------

def _bench_split_100l() -> None:
    for line in _TEXT_100L.splitlines():
        line.split()


def _bench_split_1kl() -> None:
    for line in _TEXT_1000L.splitlines():
        line.split()


def _bench_join_1k() -> None:
    words = _WORDS * 40  # ~1000 items
    " ".join(words)


def _bench_join_100k() -> None:
    words = _WORDS * 4000  # ~100k items
    " ".join(words)


# ---------------------------------------------------------------------------
# Search / replace
# ---------------------------------------------------------------------------

def _bench_str_find_100l() -> None:
    for _ in range(100):
        _TEXT_1000L.find("performance")


def _bench_str_count_1kl() -> None:
    _TEXT_1000L.count("the")


def _bench_str_replace_100l() -> None:
    _TEXT_1000L.replace("the", "THE")


def _bench_str_startswith_10k() -> None:
    lines = _TEXT_1000L.splitlines()
    for _ in range(10):
        for line in lines:
            line.startswith("the")


# ---------------------------------------------------------------------------
# Regular expressions
# ---------------------------------------------------------------------------

_RE_WORD  = re.compile(r"\b\w+\b")
_RE_EMAIL = re.compile(r"[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}")
_RE_URL   = re.compile(r"https?://[^\s]+")

_EMAIL_TEXT = " ".join(
    f"user{i}@example{i % 50}.com and some other text" for i in range(500)
)


def _bench_re_findall_words_1kl() -> None:
    _RE_WORD.findall(_TEXT_1000L)


def _bench_re_sub_1kl() -> None:
    _RE_WORD.sub("X", _TEXT_100L)


def _bench_re_findall_email_500() -> None:
    _RE_EMAIL.findall(_EMAIL_TEXT)


def _bench_re_compile_10() -> None:
    for _ in range(10):
        re.compile(r"(\w+)\s+(\w+)\s+(\w+)")


def _bench_re_match_vs_search_1k() -> None:
    pat = re.compile(r"\w{5,}")
    for word in _WORDS * 40:
        pat.match(word)
        pat.search(word)


# ---------------------------------------------------------------------------
# String formatting
# ---------------------------------------------------------------------------

def _bench_format_1k() -> None:
    for i in range(1_000):
        _ = f"item {i}: value={i * 3.14:.4f}"


def _bench_percent_format_1k() -> None:
    for i in range(1_000):
        _ = "item %d: value=%.4f" % (i, i * 3.14)


def _bench_str_format_1k() -> None:
    for i in range(1_000):
        _ = "item {}: value={:.4f}".format(i, i * 3.14)


def _bench_sprintf_table_100() -> None:
    rows = [(i, f"name_{i}", i * 0.01) for i in range(100)]
    lines = [f"{r[0]:>6d}  {r[1]:<20s}  {r[2]:>10.4f}" for r in rows]
    "\n".join(lines)


# ---------------------------------------------------------------------------
# Encoding / decoding
# ---------------------------------------------------------------------------

def _bench_encode_utf8_1kl() -> None:
    _TEXT_1000L.encode("utf-8")


def _bench_decode_utf8_1kl() -> None:
    b = _TEXT_1000L.encode("utf-8")
    b.decode("utf-8")


def _bench_encode_ascii_1kl() -> None:
    # Only ASCII chars here
    _TEXT_1000L.encode("ascii")


# ---------------------------------------------------------------------------
# Case / strip operations
# ---------------------------------------------------------------------------

def _bench_upper_lower_1kl() -> None:
    for line in _TEXT_1000L.splitlines():
        line.upper()
        line.lower()


def _bench_strip_1kl() -> None:
    lines = ["  " + l + "  " for l in _TEXT_1000L.splitlines()]
    for line in lines:
        line.strip()


def _bench_title_case_1kl() -> None:
    for line in _TEXT_1000L.splitlines():
        line.title()


# ---------------------------------------------------------------------------
# String building
# ---------------------------------------------------------------------------

def _bench_concat_loop_1k() -> None:
    result = ""
    for word in _WORDS * 40:
        result += word + " "


def _bench_list_append_join_1k() -> None:
    parts: List[str] = []
    for word in _WORDS * 40:
        parts.append(word)
    " ".join(parts)


def _bench_string_translate_1kl() -> None:
    table = str.maketrans("aeiou", "AEIOU")
    for line in _TEXT_1000L.splitlines():
        line.translate(table)


# ---------------------------------------------------------------------------
# Case list
# ---------------------------------------------------------------------------

STRING_CASES = [
    # Split / join
    BenchmarkCase("split_100l",         CATEGORY, _bench_split_100l),
    BenchmarkCase("split_1kl",          CATEGORY, _bench_split_1kl),
    BenchmarkCase("join_1k",            CATEGORY, _bench_join_1k),
    BenchmarkCase("join_100k",          CATEGORY, _bench_join_100k),
    # Search / replace
    BenchmarkCase("str_find_100l",      CATEGORY, _bench_str_find_100l),
    BenchmarkCase("str_count_1kl",      CATEGORY, _bench_str_count_1kl),
    BenchmarkCase("str_replace_100l",   CATEGORY, _bench_str_replace_100l),
    BenchmarkCase("str_startswith_10k", CATEGORY, _bench_str_startswith_10k),
    # Regex
    BenchmarkCase("re_findall_1kl",     CATEGORY, _bench_re_findall_words_1kl),
    BenchmarkCase("re_sub_1kl",         CATEGORY, _bench_re_sub_1kl),
    BenchmarkCase("re_findall_email",   CATEGORY, _bench_re_findall_email_500),
    BenchmarkCase("re_compile_10",      CATEGORY, _bench_re_compile_10),
    BenchmarkCase("re_match_vs_search", CATEGORY, _bench_re_match_vs_search_1k),
    # Formatting
    BenchmarkCase("fstring_1k",         CATEGORY, _bench_format_1k),
    BenchmarkCase("percent_format_1k",  CATEGORY, _bench_percent_format_1k),
    BenchmarkCase("str_format_1k",      CATEGORY, _bench_str_format_1k),
    BenchmarkCase("sprintf_table_100",  CATEGORY, _bench_sprintf_table_100),
    # Encoding
    BenchmarkCase("encode_utf8_1kl",    CATEGORY, _bench_encode_utf8_1kl),
    BenchmarkCase("decode_utf8_1kl",    CATEGORY, _bench_decode_utf8_1kl),
    BenchmarkCase("encode_ascii_1kl",   CATEGORY, _bench_encode_ascii_1kl),
    # Case / strip
    BenchmarkCase("upper_lower_1kl",    CATEGORY, _bench_upper_lower_1kl),
    BenchmarkCase("strip_1kl",          CATEGORY, _bench_strip_1kl),
    BenchmarkCase("title_case_1kl",     CATEGORY, _bench_title_case_1kl),
    # Building
    BenchmarkCase("concat_loop_1k",     CATEGORY, _bench_concat_loop_1k),
    BenchmarkCase("list_join_1k",       CATEGORY, _bench_list_append_join_1k),
    BenchmarkCase("translate_1kl",      CATEGORY, _bench_string_translate_1kl),
]
