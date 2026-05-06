"""
I/O benchmark suite.

Covers: file read/write, JSON parse/serialize, CSV, binary I/O.
"""
from __future__ import annotations

import csv
import io
import json
import os
import random
import tempfile
from pathlib import Path
from typing import Any, Dict, List

from benchmarks.benchmark_ci import BenchmarkCase

CATEGORY = "io"


# ---------------------------------------------------------------------------
# Fixtures (in-memory and on-disk)
# ---------------------------------------------------------------------------

def _make_json_data(n: int, seed: int = 42) -> List[Dict[str, Any]]:
    rng = random.Random(seed)
    return [
        {
            "id": i,
            "name": f"item_{i}",
            "value": rng.random() * 1000,
            "active": bool(i % 2),
            "tags": [f"tag_{j}" for j in range(rng.randint(1, 4))],
        }
        for i in range(n)
    ]


_JSON_100   = json.dumps(_make_json_data(100))
_JSON_1K    = json.dumps(_make_json_data(1_000))
_JSON_10K   = json.dumps(_make_json_data(10_000))
_DATA_100   = _make_json_data(100)
_DATA_1K    = _make_json_data(1_000)

_CSV_HEADER = ["id", "name", "value", "active"]

def _make_csv_data(n: int) -> str:
    buf = io.StringIO()
    w = csv.writer(buf)
    w.writerow(_CSV_HEADER)
    rng = random.Random(7)
    for i in range(n):
        w.writerow([i, f"item_{i}", round(rng.random() * 100, 4), i % 2 == 0])
    return buf.getvalue()

_CSV_1K  = _make_csv_data(1_000)
_CSV_10K = _make_csv_data(10_000)


# ---------------------------------------------------------------------------
# JSON
# ---------------------------------------------------------------------------

def _bench_json_parse_100() -> None:
    json.loads(_JSON_100)


def _bench_json_parse_1k() -> None:
    json.loads(_JSON_1K)


def _bench_json_parse_10k() -> None:
    json.loads(_JSON_10K)


def _bench_json_dumps_100() -> None:
    json.dumps(_DATA_100)


def _bench_json_dumps_1k() -> None:
    json.dumps(_DATA_1K)


def _bench_json_roundtrip_1k() -> None:
    json.loads(json.dumps(_DATA_1K))


# ---------------------------------------------------------------------------
# CSV
# ---------------------------------------------------------------------------

def _bench_csv_read_1k() -> None:
    reader = csv.DictReader(io.StringIO(_CSV_1K))
    list(reader)


def _bench_csv_read_10k() -> None:
    reader = csv.DictReader(io.StringIO(_CSV_10K))
    list(reader)


def _bench_csv_write_1k() -> None:
    buf = io.StringIO()
    w = csv.writer(buf)
    w.writerow(_CSV_HEADER)
    rng = random.Random(5)
    for i in range(1_000):
        w.writerow([i, f"row_{i}", rng.random(), i % 2 == 0])


def _bench_csv_roundtrip_1k() -> None:
    buf = io.StringIO()
    w = csv.writer(buf)
    w.writerow(_CSV_HEADER)
    reader_rows = list(csv.DictReader(io.StringIO(_CSV_1K)))
    for row in reader_rows:
        w.writerow([row["id"], row["name"], row["value"], row["active"]])


# ---------------------------------------------------------------------------
# File I/O (tmpfs / real disk)
# ---------------------------------------------------------------------------

def _bench_file_write_1mb() -> None:
    data = b"x" * (1024 * 1024)
    with tempfile.NamedTemporaryFile(delete=False) as f:
        fname = f.name
    try:
        Path(fname).write_bytes(data)
    finally:
        os.unlink(fname)


def _bench_file_read_1mb() -> None:
    data = b"y" * (1024 * 1024)
    with tempfile.NamedTemporaryFile(delete=False) as f:
        fname = f.name
        f.write(data)
    try:
        Path(fname).read_bytes()
    finally:
        os.unlink(fname)


def _bench_file_write_text_100l() -> None:
    lines = [f"line {i}: some text content here\n" for i in range(100)]
    with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
        fname = f.name
        f.writelines(lines)
    os.unlink(fname)


def _bench_file_readline_1kl() -> None:
    """Write and then read back 1000 lines."""
    content = "\n".join(f"line {i} content" for i in range(1_000))
    with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
        fname = f.name
        f.write(content)
    try:
        with open(fname, "r") as f:
            lines = f.readlines()
        assert len(lines) >= 1
    finally:
        os.unlink(fname)


# ---------------------------------------------------------------------------
# In-memory streams
# ---------------------------------------------------------------------------

def _bench_stringio_write_1k() -> None:
    buf = io.StringIO()
    for i in range(1_000):
        buf.write(f"line {i}\n")
    buf.getvalue()


def _bench_bytesio_write_1mb() -> None:
    buf = io.BytesIO()
    chunk = b"A" * 1024
    for _ in range(1_000):
        buf.write(chunk)
    buf.getvalue()


def _bench_bytesio_read_1mb() -> None:
    data = b"B" * (1024 * 1024)
    buf = io.BytesIO(data)
    buf.read()


# ---------------------------------------------------------------------------
# Serialization (JSON as proxy for parse/write performance)
# ---------------------------------------------------------------------------

def _bench_json_indent_1k() -> None:
    json.dumps(_DATA_1K, indent=2)


def _bench_json_sort_keys_1k() -> None:
    json.dumps(_DATA_1K, sort_keys=True)


# ---------------------------------------------------------------------------
# Case list
# ---------------------------------------------------------------------------

IO_CASES = [
    # JSON
    BenchmarkCase("json_parse_100",     CATEGORY, _bench_json_parse_100),
    BenchmarkCase("json_parse_1k",      CATEGORY, _bench_json_parse_1k),
    BenchmarkCase("json_parse_10k",     CATEGORY, _bench_json_parse_10k, bench_iters=10),
    BenchmarkCase("json_dumps_100",     CATEGORY, _bench_json_dumps_100),
    BenchmarkCase("json_dumps_1k",      CATEGORY, _bench_json_dumps_1k),
    BenchmarkCase("json_roundtrip_1k",  CATEGORY, _bench_json_roundtrip_1k),
    BenchmarkCase("json_indent_1k",     CATEGORY, _bench_json_indent_1k),
    BenchmarkCase("json_sort_keys_1k",  CATEGORY, _bench_json_sort_keys_1k),
    # CSV
    BenchmarkCase("csv_read_1k",        CATEGORY, _bench_csv_read_1k),
    BenchmarkCase("csv_read_10k",       CATEGORY, _bench_csv_read_10k),
    BenchmarkCase("csv_write_1k",       CATEGORY, _bench_csv_write_1k),
    BenchmarkCase("csv_roundtrip_1k",   CATEGORY, _bench_csv_roundtrip_1k),
    # File I/O
    BenchmarkCase("file_write_1mb",     CATEGORY, _bench_file_write_1mb, bench_iters=10),
    BenchmarkCase("file_read_1mb",      CATEGORY, _bench_file_read_1mb, bench_iters=10),
    BenchmarkCase("file_write_text",    CATEGORY, _bench_file_write_text_100l),
    BenchmarkCase("file_readline_1kl",  CATEGORY, _bench_file_readline_1kl),
    # In-memory streams
    BenchmarkCase("stringio_write_1k",  CATEGORY, _bench_stringio_write_1k),
    BenchmarkCase("bytesio_write_1mb",  CATEGORY, _bench_bytesio_write_1mb),
    BenchmarkCase("bytesio_read_1mb",   CATEGORY, _bench_bytesio_read_1mb),
]
