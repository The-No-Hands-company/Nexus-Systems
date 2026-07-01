"""Tests for ecosystem-autopilot / autopilot.py"""

import json
import tempfile
from pathlib import Path

import pytest

# Import the module under test
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from autopilot import (
    AppContext,
    active_tasks,
    can_start_task,
    complete_task,
    completed_ids,
    compute_release_readiness_index,
    find_task,
    generate_scorecard,
    is_task_ready,
    latest_gate_health,
    print_board,
    print_brief,
    print_next,
    print_plan,
    run_ceo_command,
    save_json,
    slugify,
    sorted_backlog,
    start_task,
    unresolved_dependencies,
    utc_now,
)

# ── helpers ──────────────────────────────────────────────────────────


def _make_ctx(tasks=None, wip=None, workflows=None, gates=None):
    config = {
        "wip": wip or {"maxActiveGlobal": 3, "maxActivePerLane": 1},
        "workflows": workflows or {},
        "pipelines": {},
        "qualityGates": gates or {},
    }
    state = {"version": 1, "tasks": tasks or [], "history": [], "updatedAt": utc_now()}
    return AppContext(root=Path("/fake"), config=config, state=state)


# ── slugify ──────────────────────────────────────────────────────────


def test_slugify_basic():
    assert slugify("Hello World") == "hello-world"


def test_slugify_special_chars():
    assert slugify("Nexus-Cloud v2.0!") == "nexus-cloud-v2-0"


# ── task helpers ─────────────────────────────────────────────────────


def test_active_tasks():
    tasks = [
        {"id": "1", "status": "in-progress"},
        {"id": "2", "status": "not-started"},
        {"id": "3", "status": "done"},
    ]
    active = active_tasks(tasks)
    assert len(active) == 1
    assert active[0]["id"] == "1"


def test_completed_ids():
    tasks = [
        {"id": "a", "status": "done"},
        {"id": "b", "status": "in-progress"},
        {"id": "c", "status": "done"},
    ]
    ids = completed_ids(tasks)
    assert ids == {"a", "c"}


def test_unresolved_dependencies():
    task = {"dependsOn": ["a", "b", "c"]}
    done = {"a", "c"}
    assert unresolved_dependencies(task, done) == ["b"]


def test_unresolved_dependencies_none():
    task = {"dependsOn": None}
    assert unresolved_dependencies(task, {"a"}) == []


def test_is_task_ready():
    task = {"status": "not-started"}
    assert is_task_ready(task, {"x"}) is True


def test_is_task_ready_blocked():
    task = {"status": "not-started", "dependsOn": ["blocker"]}
    assert is_task_ready(task, set()) is False


def test_is_task_ready_wrong_status():
    task = {"status": "in-progress"}
    assert is_task_ready(task, set()) is False


# ── sorted_backlog ───────────────────────────────────────────────────


def test_sorted_backlog():
    tasks = [
        {"id": "t1", "status": "not-started", "priority": 1, "lane": "frontend"},
        {"id": "t2", "status": "not-started", "priority": 2, "lane": "frontend", "dependsOn": ["t3"]},
        {"id": "t3", "status": "done", "priority": 3, "lane": "frontend"},
    ]
    ready = sorted_backlog(tasks)
    # t3 is done, so t2's dependency is satisfied — both t1 and t2 are ready
    assert len(ready) == 2
    assert ready[0]["id"] == "t1"


# ── WIP limits & can_start ───────────────────────────────────────────


def test_can_start_under_wip():
    ctx = _make_ctx(tasks=[{"id": "t1", "status": "not-started", "lane": "api", "priority": 1}])
    reason = can_start_task(ctx, ctx.state["tasks"][0])
    assert reason is None


def test_can_start_global_wip():
    tasks = [
        {"id": "a", "status": "in-progress", "lane": "api", "priority": 1},
        {"id": "b", "status": "in-progress", "lane": "web", "priority": 1},
        {"id": "c", "status": "in-progress", "lane": "data", "priority": 1},
        {"id": "d", "status": "not-started", "lane": "api", "priority": 1},
    ]
    ctx = _make_ctx(tasks=tasks)
    reason = can_start_task(ctx, tasks[3])
    assert reason is not None
    assert "WIP limit" in reason


def test_can_start_lane_wip():
    tasks = [
        {"id": "a", "status": "in-progress", "lane": "api", "priority": 1},
        {"id": "b", "status": "not-started", "lane": "api", "priority": 2},
    ]
    ctx = _make_ctx(tasks=tasks)
    reason = can_start_task(ctx, tasks[1])
    assert reason is not None
    assert "already has" in reason


# ── start / complete ─────────────────────────────────────────────────


def test_start_task():
    ctx = _make_ctx(tasks=[{"id": "nx-1", "status": "not-started", "lane": "api", "priority": 1}])
    rc = start_task(ctx, "nx-1")
    assert rc == 0
    assert ctx.state["tasks"][0]["status"] == "in-progress"


def test_complete_task():
    ctx = _make_ctx(tasks=[{"id": "nx-1", "status": "in-progress", "lane": "api", "priority": 1}])
    rc = complete_task(ctx, "nx-1")
    assert rc == 0
    assert ctx.state["tasks"][0]["status"] == "done"


def test_start_task_not_found():
    ctx = _make_ctx()
    assert start_task(ctx, "missing") == 1


# ── scorecard / readiness index ──────────────────────────────────────


def test_compute_rri_all_done():
    # 40 (completion) + 20 (flow) + 25 (gate) + 15 (wip) = 100
    assert compute_release_readiness_index(10, 10, 0, 0, 0, 3, {"g1": {"status": "pass"}}) == 100


def test_compute_rri_empty():
    # 0 (completion) + 20 (flow: 0/0 → fallback) + 0 (gate: empty) + 15 (wip) = 35
    assert compute_release_readiness_index(0, 0, 0, 0, 0, 3, {}) == 35


# ── save_json atomicity ─────────────────────────────────────────────


def test_save_json_atomic(tmp_path):
    path = tmp_path / "test.json"
    save_json(path, {"hello": "world"})
    data = json.loads(path.read_text())
    assert data["hello"] == "world"
    # No .tmp left behind
    assert not list(tmp_path.glob("*.tmp"))
