#!/usr/bin/env python3
import argparse
import json
import shlex
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional

try:
    from dc_config import load_settings as _load_dc_settings
    _DC_AVAILABLE = True
except ImportError:
    _DC_AVAILABLE = False

__version__ = "2.0.0"

ROOT = Path(__file__).resolve().parents[2]
BASE = Path(__file__).resolve().parent
DEEPCODE_DIR = BASE.parent / ".deepcode"
CONFIG_PATH = BASE / "autopilot.config.json"
BACKLOG_SEED_PATH = BASE / "backlog.seed.json"
STATE_PATH = BASE / "autopilot.state.json"
HANDOFF_DIR = BASE / "handoffs"
DECISIONS_DIR = BASE / "decisions"
SCORECARD_DIR = BASE / "reports" / "scorecards"


@dataclass
class AppContext:
    root: Path
    config: Dict[str, Any]
    state: Dict[str, Any]


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def load_json(path: Path, fallback: Dict[str, Any]) -> Dict[str, Any]:
    if not path.exists():
        return fallback
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def save_json(path: Path, payload: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    with tmp.open("w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2)
        f.write("\n")
    tmp.replace(path)


def slugify(value: str) -> str:
    clean = "".join(ch.lower() if ch.isalnum() else "-" for ch in value)
    return "-".join(part for part in clean.split("-") if part)


def parse_option(args: List[str], name: str) -> Optional[str]:
    if name not in args:
        return None
    idx = args.index(name)
    if idx + 1 >= len(args):
        return None
    return args[idx + 1]


def ensure_state() -> Dict[str, Any]:
    state = load_json(
        STATE_PATH,
        {
            "version": 1,
            "createdAt": utc_now(),
            "updatedAt": utc_now(),
            "tasks": [],
            "history": [],
        },
    )
    if not state.get("tasks"):
        seed = load_json(BACKLOG_SEED_PATH, {"tasks": []})
        state["tasks"] = seed.get("tasks", [])
        state["history"].append({
            "at": utc_now(),
            "event": "seed-imported",
            "detail": f"Imported {len(state['tasks'])} tasks from backlog seed",
        })
        state["updatedAt"] = utc_now()
        save_json(STATE_PATH, state)
    return state


def load_context() -> AppContext:
    config = load_json(CONFIG_PATH, {"wip": {}, "workflows": {}})
    state = ensure_state()
    return AppContext(root=ROOT, config=config, state=state)


def save_state(ctx: AppContext) -> None:
    ctx.state["updatedAt"] = utc_now()
    save_json(STATE_PATH, ctx.state)


def active_tasks(tasks: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    return [t for t in tasks if t.get("status") == "in-progress"]


def completed_ids(tasks: List[Dict[str, Any]]) -> set[str]:
    return {str(t.get("id")) for t in tasks if t.get("status") == "done"}


def unresolved_dependencies(task: Dict[str, Any], done_ids: set[str]) -> List[str]:
    deps = task.get("dependsOn") or []
    return [dep for dep in deps if dep not in done_ids]


def is_task_ready(task: Dict[str, Any], done_ids: set[str]) -> bool:
    return task.get("status") == "not-started" and len(unresolved_dependencies(task, done_ids)) == 0


def sorted_backlog(tasks: List[Dict[str, Any]], lane: Optional[str] = None) -> List[Dict[str, Any]]:
    done_ids = completed_ids(tasks)
    filtered = [
        t for t in tasks
        if t.get("status") == "not-started"
        and (lane is None or t.get("lane") == lane)
        and is_task_ready(t, done_ids)
    ]
    return sorted(filtered, key=lambda t: (int(t.get("priority", 999)), t.get("id", "")))


def sorted_backlog_for_role(tasks: List[Dict[str, Any]], role: str, lane: Optional[str] = None) -> List[Dict[str, Any]]:
    queue = sorted_backlog(tasks, lane)
    return [t for t in queue if t.get("owner") == role]


def print_board(ctx: AppContext) -> int:
    tasks = ctx.state.get("tasks", [])
    done_ids = completed_ids(tasks)
    groups = {
        "not-started": [t for t in tasks if t.get("status") == "not-started"],
        "in-progress": [t for t in tasks if t.get("status") == "in-progress"],
        "done": [t for t in tasks if t.get("status") == "done"],
    }
    for status, rows in groups.items():
        print(f"\n[{status}] {len(rows)}")
        for t in sorted(rows, key=lambda x: (x.get("lane", ""), int(x.get("priority", 999)), x.get("id", ""))):
            blocked = unresolved_dependencies(t, done_ids)
            blocked_str = "ready" if not blocked else f"blocked-by={','.join(blocked)}"
            print(f"- {t.get('id')} | lane={t.get('lane')} | p{t.get('priority')} | {blocked_str} | {t.get('title')}")
    return 0


def print_next(ctx: AppContext, lane: Optional[str] = None) -> int:
    queue = sorted_backlog(ctx.state.get("tasks", []), lane)
    if not queue:
        print("No ready tasks available for selection.")
        return 0
    print("Next recommended tasks:")
    for t in queue[:5]:
        print(f"- {t.get('id')} | lane={t.get('lane')} | p{t.get('priority')} | {t.get('title')}")
    return 0


def find_task(ctx: AppContext, task_id: str) -> Optional[Dict[str, Any]]:
    for t in ctx.state.get("tasks", []):
        if t.get("id") == task_id:
            return t
    return None


def can_start_task(ctx: AppContext, task: Dict[str, Any]) -> Optional[str]:
    wip = ctx.config.get("wip", {})
    max_global = int(wip.get("maxActiveGlobal", 3))
    max_per_lane = int(wip.get("maxActivePerLane", 1))
    actives = active_tasks(ctx.state.get("tasks", []))

    if len(actives) >= max_global:
        return f"WIP limit reached: {len(actives)}/{max_global} active tasks"

    lane = task.get("lane")
    lane_active = [t for t in actives if t.get("lane") == lane]
    if len(lane_active) >= max_per_lane:
        return f"Lane '{lane}' already has {len(lane_active)} active task(s), max is {max_per_lane}"

    done_ids = completed_ids(ctx.state.get("tasks", []))
    blocked = unresolved_dependencies(task, done_ids)
    if blocked:
        return f"Task blocked by incomplete dependencies: {', '.join(blocked)}"

    return None


def start_task(ctx: AppContext, task_id: str) -> int:
    task = find_task(ctx, task_id)
    if not task:
        print(f"Task not found: {task_id}")
        return 1
    if task.get("status") == "done":
        print(f"Task already done: {task_id}")
        return 1

    reason = can_start_task(ctx, task)
    if reason:
        print(reason)
        return 1

    task["status"] = "in-progress"
    task["startedAt"] = utc_now()
    ctx.state["history"].append({"at": utc_now(), "event": "task-started", "taskId": task_id})
    save_state(ctx)
    print(f"Started {task_id}")
    return 0


def complete_task(ctx: AppContext, task_id: str) -> int:
    task = find_task(ctx, task_id)
    if not task:
        print(f"Task not found: {task_id}")
        return 1
    task["status"] = "done"
    task["completedAt"] = utc_now()
    ctx.state["history"].append({"at": utc_now(), "event": "task-completed", "taskId": task_id})
    save_state(ctx)
    print(f"Completed {task_id}")
    return 0


def run_workflow(ctx: AppContext, name: str, dry_run: bool = False) -> int:
    workflow = ctx.config.get("workflows", {}).get(name)
    if not workflow:
        print(f"Unknown workflow: {name}")
        return 1

    cwd = ctx.root / workflow.get("cwd", ".")
    cmd = workflow.get("command", "")
    if not cmd:
        print(f"Workflow '{name}' has no command configured")
        return 1

    timeout_seconds = int(workflow.get("timeoutSeconds", 900))

    print(f"[workflow:{name}] cwd={cwd}")
    print(f"[workflow:{name}] cmd={cmd}")
    print(f"[workflow:{name}] timeout={timeout_seconds}s")

    if dry_run:
        return 0

    try:
        result = subprocess.run(shlex.split(cmd), cwd=str(cwd), timeout=timeout_seconds, capture_output=True)
    except subprocess.TimeoutExpired:
        print(f"[workflow:{name}] timeout after {timeout_seconds}s")
        ctx.state["history"].append(
            {
                "at": utc_now(),
                "event": "workflow-run",
                "workflow": name,
                "exitCode": 124,
                "timeoutSeconds": timeout_seconds,
            }
        )
        save_state(ctx)
        return 124

    ctx.state["history"].append(
        {
            "at": utc_now(),
            "event": "workflow-run",
            "workflow": name,
            "exitCode": result.returncode,
            "timeoutSeconds": timeout_seconds,
        }
    )
    save_state(ctx)
    return int(result.returncode)


def run_pipeline(ctx: AppContext, name: str, dry_run: bool = False) -> int:
    pipelines = ctx.config.get("pipelines", {})
    if name not in pipelines:
        print(f"Unknown pipeline: {name}")
        return 1

    workflows = pipelines[name]
    if not isinstance(workflows, list) or not workflows:
        print(f"Pipeline '{name}' has no workflows configured")
        return 1

    print(f"[pipeline:{name}] workflows={', '.join(workflows)}")
    exit_code = 0
    for workflow in workflows:
        code = run_workflow(ctx, workflow, dry_run=dry_run)
        if code != 0:
            exit_code = code
            print(f"[pipeline:{name}] halted on workflow '{workflow}' with exit code {code}")
            break

    ctx.state["history"].append(
        {
            "at": utc_now(),
            "event": "pipeline-run",
            "pipeline": name,
            "exitCode": exit_code,
            "dryRun": dry_run,
        }
    )
    save_state(ctx)
    return exit_code


def run_quality_gate(ctx: AppContext, gate_name: str, dry_run: bool = False) -> int:
    gates = ctx.config.get("qualityGates", {})
    gate = gates.get(gate_name)
    if not gate:
        print(f"Unknown gate: {gate_name}")
        return 1

    workflows = gate.get("workflows", [])
    if not isinstance(workflows, list) or not workflows:
        print(f"Gate '{gate_name}' has no workflows configured")
        return 1

    print(f"[gate:{gate_name}] {gate.get('description', '')}".strip())
    print(f"[gate:{gate_name}] workflows={', '.join(workflows)}")

    exit_code = 0
    for workflow in workflows:
        code = run_workflow(ctx, workflow, dry_run=dry_run)
        if code != 0:
            exit_code = code
            print(f"[gate:{gate_name}] failed on workflow '{workflow}' with exit code {code}")
            break

    ctx.state["history"].append(
        {
            "at": utc_now(),
            "event": "quality-gate-run",
            "gate": gate_name,
            "exitCode": exit_code,
            "dryRun": dry_run,
        }
    )
    save_state(ctx)
    return exit_code


def print_org(ctx: AppContext) -> int:
    org = ctx.config.get("organization", {})
    levels = org.get("levels", [])

    print(f"company: {org.get('company', 'n/a')}")
    print(f"program: {org.get('program', 'n/a')}")
    print(f"mission: {org.get('mission', 'n/a')}")
    print("levels:")
    for row in levels:
        print(f"- {row.get('level')}: {row.get('role')} | {row.get('purpose')}")

    print("specialist roles:")
    for role, meta in sorted(ctx.config.get("roles", {}).items()):
        seniority = meta.get("seniority", "n/a")
        print(f"- {role} ({seniority}): {meta.get('description', 'n/a')}")
    return 0


def print_plan(ctx: AppContext) -> int:
    tasks = ctx.state.get("tasks", [])
    done_ids = completed_ids(tasks)

    lanes = sorted({str(t.get("lane", "unknown")) for t in tasks})
    print("lane plan:")
    for lane in lanes:
        lane_tasks = [t for t in tasks if t.get("lane") == lane and t.get("status") == "not-started"]
        ready = [t for t in lane_tasks if is_task_ready(t, done_ids)]
        blocked = [t for t in lane_tasks if not is_task_ready(t, done_ids)]
        print(f"- {lane}: ready={len(ready)} blocked={len(blocked)}")
        for t in sorted(ready, key=lambda x: (int(x.get("priority", 999)), x.get("id", ""))):
            print(f"  ready: {t.get('id')} p{t.get('priority')} owner={t.get('owner')} | {t.get('title')}")
        for t in sorted(blocked, key=lambda x: (int(x.get("priority", 999)), x.get("id", ""))):
            blocked_by = unresolved_dependencies(t, done_ids)
            print(f"  blocked: {t.get('id')} by {','.join(blocked_by)}")
    return 0


def print_brief(ctx: AppContext) -> int:
    tasks = ctx.state.get("tasks", [])
    done_ids = completed_ids(tasks)
    active = active_tasks(tasks)
    ready = [t for t in tasks if is_task_ready(t, done_ids)]
    blocked = [t for t in tasks if t.get("status") == "not-started" and not is_task_ready(t, done_ids)]

    print("executive brief:")
    print(f"- total tasks: {len(tasks)}")
    print(f"- active: {len(active)}")
    print(f"- ready: {len(ready)}")
    print(f"- blocked: {len(blocked)}")

    by_role: Dict[str, int] = {}
    for t in ready:
        role = str(t.get("owner", "unassigned"))
        by_role[role] = by_role.get(role, 0) + 1

    print("- ready by role:")
    for role in sorted(by_role.keys()):
        print(f"  - {role}: {by_role[role]}")

    print("- top ready tasks:")
    for t in sorted(ready, key=lambda x: (int(x.get("priority", 999)), x.get("id", "")))[:5]:
        print(f"  - {t.get('id')} | lane={t.get('lane')} | owner={t.get('owner')} | p{t.get('priority')} | {t.get('title')}")

    cadence = ctx.config.get("cadence", {})
    print("- cadence:")
    for period in ["daily", "weekly", "release"]:
        flows = cadence.get(period, [])
        print(f"  - {period}: {', '.join(flows) if flows else 'n/a'}")
    return 0


def dispatch_task(ctx: AppContext, role: str, lane: Optional[str] = None, auto_start: bool = False) -> int:
    queue = sorted_backlog_for_role(ctx.state.get("tasks", []), role, lane)
    if not queue:
        lane_msg = f" in lane '{lane}'" if lane else ""
        print(f"No ready tasks for role '{role}'{lane_msg}.")
        return 0

    task = queue[0]
    print(f"dispatch: {task.get('id')} | lane={task.get('lane')} | p{task.get('priority')} | {task.get('title')}")
    show_work_order(ctx, str(task.get("id")))

    ctx.state["history"].append(
        {
            "at": utc_now(),
            "event": "task-dispatched",
            "taskId": task.get("id"),
            "role": role,
            "lane": lane,
            "autoStart": auto_start,
        }
    )
    save_state(ctx)

    if auto_start:
        return start_task(ctx, str(task.get("id")))
    return 0


def show_work_order(ctx: AppContext, task_id: str) -> int:
    task = find_task(ctx, task_id)
    if not task:
        print(f"Task not found: {task_id}")
        return 1

    role = task.get("owner", "ai")
    role_meta = ctx.config.get("roles", {}).get(role, {})
    done_ids = completed_ids(ctx.state.get("tasks", []))
    blocked = unresolved_dependencies(task, done_ids)

    print(f"task: {task.get('id')} ({task.get('title')})")
    print(f"lane: {task.get('lane')} | priority: {task.get('priority')} | status: {task.get('status')}")
    print(f"role: {role}")
    if role_meta.get("description"):
        print(f"roleContext: {role_meta.get('description')}")
    print(f"blockedBy: {', '.join(blocked) if blocked else 'none'}")
    print("what:")
    print(f"- {task.get('what', 'n/a')}")
    print("why:")
    print(f"- {task.get('why', 'n/a')}")
    print("how:")
    print(f"- {task.get('how', 'n/a')}")
    print("acceptance:")
    print(f"- {task.get('acceptance', 'n/a')}")
    print("rollback:")
    print(f"- {task.get('rollback', 'n/a')}")
    return 0


def write_handoff(ctx: AppContext, task_id: str, out_path: Optional[Path] = None) -> int:
    task = find_task(ctx, task_id)
    if not task:
        print(f"Task not found: {task_id}")
        return 1

    role = str(task.get("owner", "ai"))
    role_card = BASE / "company" / "agents" / f"{role}.md"
    ts = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    default_name = f"handoff-{slugify(task_id)}-{ts}.md"
    target = out_path or (HANDOFF_DIR / default_name)

    done_ids = completed_ids(ctx.state.get("tasks", []))
    blocked = unresolved_dependencies(task, done_ids)
    blocked_text = ", ".join(blocked) if blocked else "none"

    lines = [
        f"# Handoff: {task.get('id')} - {task.get('title')}",
        "",
        "## Specialist Routing",
        f"- Role: {role}",
        f"- Role Card: {role_card.relative_to(ctx.root) if role_card.exists() else 'missing role card'}",
        f"- Lane: {task.get('lane')}",
        f"- Priority: {task.get('priority')}",
        f"- Blocked By: {blocked_text}",
        "",
        "## Prompt Packet",
        f"- Objective: {task.get('what', 'n/a')}",
        f"- Why: {task.get('why', 'n/a')}",
        f"- Implementation Guidance: {task.get('how', 'n/a')}",
        f"- Acceptance: {task.get('acceptance', 'n/a')}",
        f"- Rollback: {task.get('rollback', 'n/a')}",
        "",
        "## Execution Guardrails",
        "- Read the role card before coding.",
        "- Respect dependency ordering and do not bypass blockers.",
        "- Log key decisions via /decision <task-id> <summary>.",
        "",
        "## Suggested Commands",
        f"- python3 dhts/ecosystem-autopilot/autopilot.py /work-order {task.get('id')}",
        f"- python3 dhts/ecosystem-autopilot/autopilot.py /start {task.get('id')}",
        f"- python3 dhts/ecosystem-autopilot/autopilot.py /decision {task.get('id')} \"<decision-summary>\"",
    ]

    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text("\n".join(lines) + "\n", encoding="utf-8")

    ctx.state["history"].append(
        {
            "at": utc_now(),
            "event": "handoff-generated",
            "taskId": task.get("id"),
            "role": role,
            "path": str(target),
        }
    )
    save_state(ctx)
    print(f"Handoff written: {target}")
    return 0


def write_decision_log(ctx: AppContext, task_id: str, summary: str, out_path: Optional[Path] = None) -> int:
    task = find_task(ctx, task_id)
    if not task:
        print(f"Task not found: {task_id}")
        return 1
    if not summary.strip():
        print("Decision summary cannot be empty")
        return 1

    role = str(task.get("owner", "ai"))
    ts = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    default_name = f"decision-{slugify(task_id)}-{ts}.md"
    target = out_path or (DECISIONS_DIR / default_name)
    role_card = BASE / "company" / "agents" / f"{role}.md"

    content = [
        f"# Decision: {task.get('id')}",
        "",
        f"- Time: {utc_now()}",
        f"- Task: {task.get('id')} ({task.get('title')})",
        f"- Role: {role}",
        f"- Role Card: {role_card.relative_to(ctx.root) if role_card.exists() else 'missing role card'}",
        f"- Summary: {summary.strip()}",
        "",
        "## Context",
        f"- Why: {task.get('why', 'n/a')}",
        f"- Acceptance: {task.get('acceptance', 'n/a')}",
        f"- Rollback: {task.get('rollback', 'n/a')}",
    ]

    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text("\n".join(content) + "\n", encoding="utf-8")

    ctx.state["history"].append(
        {
            "at": utc_now(),
            "event": "decision-logged",
            "taskId": task.get("id"),
            "role": role,
            "summary": summary.strip(),
            "path": str(target),
        }
    )
    save_state(ctx)
    print(f"Decision logged: {target}")
    return 0


def latest_gate_health(ctx: AppContext) -> Dict[str, Dict[str, Any]]:
    gates = ctx.config.get("qualityGates", {})
    history = ctx.state.get("history", [])
    latest: Dict[str, Dict[str, Any]] = {}
    for event in history:
        if event.get("event") == "quality-gate-run":
            gate_name = str(event.get("gate"))
            latest[gate_name] = {
                "lastRunAt": event.get("at"),
                "exitCode": int(event.get("exitCode", 1)),
                "status": "pass" if int(event.get("exitCode", 1)) == 0 else "fail",
                "dryRun": bool(event.get("dryRun", False)),
            }

    for gate_name in gates.keys():
        if gate_name not in latest:
            latest[gate_name] = {
                "lastRunAt": None,
                "exitCode": None,
                "status": "unknown",
                "dryRun": None,
            }
    return latest


def workflow_drift_trend(ctx: AppContext) -> Dict[str, Any]:
    history = [h for h in ctx.state.get("history", []) if h.get("event") == "workflow-run"]
    tracked = [h for h in history if h.get("workflow") in {"stack", "porter"}]

    if not tracked:
        return {"status": "unknown", "recentFailures": 0, "previousFailures": 0, "window": 0}

    recent = tracked[-5:]
    previous = tracked[-10:-5]
    recent_failures = sum(1 for h in recent if int(h.get("exitCode", 1)) != 0)
    previous_failures = sum(1 for h in previous if int(h.get("exitCode", 1)) != 0)

    if len(previous) == 0:
        status = "stable" if recent_failures == 0 else "degrading"
    elif recent_failures > previous_failures:
        status = "degrading"
    elif recent_failures < previous_failures:
        status = "improving"
    else:
        status = "stable"

    return {
        "status": status,
        "recentFailures": recent_failures,
        "previousFailures": previous_failures,
        "window": len(recent),
    }


def compute_release_readiness_index(
    total: int,
    done: int,
    ready: int,
    blocked: int,
    active: int,
    max_active: int,
    gate_health: Dict[str, Dict[str, Any]],
) -> int:
    completion_score = 0 if total == 0 else (done / total) * 40
    flow_den = ready + blocked
    flow_score = 20 if flow_den == 0 else (ready / flow_den) * 20
    pass_count = sum(1 for g in gate_health.values() if g.get("status") == "pass")
    gate_score = 0 if not gate_health else (pass_count / len(gate_health)) * 25
    wip_penalty = max(0, active - max_active)
    wip_score = max(0, 15 - (wip_penalty * 5))
    score = int(round(completion_score + flow_score + gate_score + wip_score))
    return max(0, min(100, score))


def generate_scorecard(ctx: AppContext, json_out: Optional[Path] = None, md_out: Optional[Path] = None) -> int:
    tasks = ctx.state.get("tasks", [])
    done_ids = completed_ids(tasks)
    active = active_tasks(tasks)
    ready = [t for t in tasks if is_task_ready(t, done_ids)]
    blocked = [t for t in tasks if t.get("status") == "not-started" and not is_task_ready(t, done_ids)]
    done = [t for t in tasks if t.get("status") == "done"]

    critical_path_blocked: List[Dict[str, Any]] = []
    lanes = sorted({str(t.get("lane", "unknown")) for t in tasks})
    for lane in lanes:
        lane_not_done = [
            t for t in tasks
            if t.get("lane") == lane and t.get("status") in {"not-started", "in-progress"}
        ]
        if not lane_not_done:
            continue
        lane_not_done = sorted(lane_not_done, key=lambda t: (int(t.get("priority", 999)), t.get("id", "")))
        lead = lane_not_done[0]
        if lead.get("status") == "not-started" and not is_task_ready(lead, done_ids):
            critical_path_blocked.append(
                {
                    "lane": lane,
                    "taskId": lead.get("id"),
                    "blockedBy": unresolved_dependencies(lead, done_ids),
                }
            )

    gate_health = latest_gate_health(ctx)
    drift_trend = workflow_drift_trend(ctx)
    max_active = int(ctx.config.get("wip", {}).get("maxActiveGlobal", 3))
    readiness_index = compute_release_readiness_index(
        total=len(tasks),
        done=len(done),
        ready=len(ready),
        blocked=len(blocked),
        active=len(active),
        max_active=max_active,
        gate_health=gate_health,
    )

    payload: Dict[str, Any] = {
        "generatedAt": utc_now(),
        "summary": {
            "total": len(tasks),
            "done": len(done),
            "inProgress": len(active),
            "ready": len(ready),
            "blocked": len(blocked),
        },
        "criticalPath": {
            "blockedCount": len(critical_path_blocked),
            "blocked": critical_path_blocked,
        },
        "gateHealth": gate_health,
        "driftTrend": drift_trend,
        "releaseReadinessIndex": readiness_index,
    }

    if json_out:
        save_json(json_out, payload)
    if md_out:
        lines = [
            "# Nexus Ecosystem Scorecard",
            "",
            f"- Generated: {payload['generatedAt']}",
            f"- Release Readiness Index: {readiness_index}/100",
            "",
            "## Portfolio Summary",
            f"- Total Tasks: {payload['summary']['total']}",
            f"- Done: {payload['summary']['done']}",
            f"- In Progress: {payload['summary']['inProgress']}",
            f"- Ready: {payload['summary']['ready']}",
            f"- Blocked: {payload['summary']['blocked']}",
            "",
            "## Critical Path",
            f"- Blocked Critical Lanes: {payload['criticalPath']['blockedCount']}",
        ]
        for row in critical_path_blocked:
            lines.append(f"- {row['lane']}: {row['taskId']} blocked by {', '.join(row['blockedBy'])}")
        if not critical_path_blocked:
            lines.append("- No blocked critical-path lane leaders.")

        lines.append("")
        lines.append("## Gate Health")
        for gate_name, info in sorted(gate_health.items()):
            lines.append(
                f"- {gate_name}: {info.get('status')} (exit={info.get('exitCode')}, lastRun={info.get('lastRunAt')}, dryRun={info.get('dryRun')})"
            )

        lines.append("")
        lines.append("## Drift Trend")
        lines.append(
            f"- Status: {drift_trend['status']} (recentFailures={drift_trend['recentFailures']}, previousFailures={drift_trend['previousFailures']}, window={drift_trend['window']})"
        )

        md_out.parent.mkdir(parents=True, exist_ok=True)
        md_out.write_text("\n".join(lines) + "\n", encoding="utf-8")

    if json_out:
        print(f"scorecard-json: {json_out}")
    if md_out:
        print(f"scorecard-md: {md_out}")
    print(f"release-readiness-index: {readiness_index}")

    ctx.state["history"].append(
        {
            "at": utc_now(),
            "event": "scorecard-generated",
            "jsonOut": str(json_out) if json_out else None,
            "mdOut": str(md_out) if md_out else None,
            "releaseReadinessIndex": readiness_index,
        }
    )
    save_state(ctx)
    return 0


def show_status(ctx: AppContext) -> int:
    tasks = ctx.state.get("tasks", [])
    by_status = {
        "not-started": sum(1 for t in tasks if t.get("status") == "not-started"),
        "in-progress": sum(1 for t in tasks if t.get("status") == "in-progress"),
        "done": sum(1 for t in tasks if t.get("status") == "done"),
    }

    print(f"root: {ctx.root}")
    print(f"state: {STATE_PATH}")
    print(f"updatedAt: {ctx.state.get('updatedAt')}")
    print(f"tasks: {len(tasks)} total")
    print(f"- not-started: {by_status['not-started']}")
    print(f"- in-progress: {by_status['in-progress']}")
    print(f"- done: {by_status['done']}")
    print("workflows:")
    for name in sorted(ctx.config.get("workflows", {}).keys()):
        print(f"- {name}")
    pipelines = ctx.config.get("pipelines", {})
    print("pipelines:")
    for name in sorted(pipelines.keys()):
        print(f"- {name}: {', '.join(pipelines[name])}")
    gates = ctx.config.get("qualityGates", {})
    print("quality-gates:")
    for name in sorted(gates.keys()):
        print(f"- {name}: {', '.join(gates[name].get('workflows', []))}")
    return 0


def print_cheatsheet() -> int:
    print("CEO quick start:")
    print("- One-command autonomous mode:")
    print("  /ceo \"Build and harden the Nexus ecosystem\"")
    print("  (safe-fast by default: dispatch + planning + scorecard, no long workflow execution)")
    print("- Common variants:")
    print("  /ceo \"Stabilize before merge\" --mode stabilize")
    print("  /ceo \"Prepare release candidate\" --mode release")
    print("  /ceo \"Run operational backbone\" --mode ops")
    print("  /ceo \"Plan only\" --mode plan")
    print("- Options:")
    print("  --execute (run workflows/gates; without this, CEO mode is non-blocking)")
    print("  --dry-run (show actions without executing workflows)")
    print("  --max-dispatch <N> (default 6)")
    print("  --no-start (dispatch recommendations only)")
    print("  --no-heavy (skip heavy network workflows like stack/porter)")
    print("  --force-pipeline (always run full daily-backbone in develop mode)")
    print("- Fallback manual commands:")
    print("  /brief, /next [lane], /dispatch <role> [lane] [--start], /pipeline <name>, /gate <name>, /scorecard")
    return 0


def run_ceo_command(
    ctx: AppContext,
    directive: str,
    mode: str = "develop",
    dry_run: bool = False,
    max_dispatch: int = 6,
    auto_start: bool = True,
    execute: bool = False,
    force_pipeline: bool = False,
    no_heavy: bool = False,
) -> int:
    normalized_mode = mode.lower().strip()
    valid_modes = {"develop", "stabilize", "release", "ops", "plan"}
    if normalized_mode not in valid_modes:
        print(f"Unknown CEO mode: {mode}")
        print("Valid modes: develop, stabilize, release, ops, plan")
        return 1

    print("ceo-mode: autonomous orchestration")
    print(f"directive: {directive}")
    print(
        f"mode: {normalized_mode} | dry-run={dry_run} | max-dispatch={max_dispatch} | "
        f"auto-start={auto_start} | execute={execute} | force-pipeline={force_pipeline} | no-heavy={no_heavy}"
    )

    print_brief(ctx)

    dispatches = 0
    for _ in range(max_dispatch):
        queue = sorted_backlog(ctx.state.get("tasks", []))
        if not queue:
            break

        task = queue[0]
        role = str(task.get("owner", "unassigned"))
        lane = str(task.get("lane", "unknown"))
        rc = dispatch_task(ctx, role=role, lane=lane, auto_start=auto_start)
        if rc != 0:
            break

        dispatches += 1
        if not auto_start:
            break

    print(f"ceo-dispatches: {dispatches}")

    exit_code = 0
    if not execute:
        print("ceo-note: execute not set; skipping workflows/gates for non-blocking CEO pass.")
        if normalized_mode == "plan":
            print_plan(ctx)
    elif normalized_mode == "develop":
        if no_heavy:
            print("ceo-note: no-heavy enabled; running pre-merge stabilization gate.")
            exit_code = run_quality_gate(ctx, "pre-merge", dry_run=dry_run)
        elif dispatches == 0 and not force_pipeline:
            print("ceo-note: no dispatchable tasks; running pre-merge stabilization gate instead of daily-backbone.")
            exit_code = run_quality_gate(ctx, "pre-merge", dry_run=dry_run)
        else:
            pipeline_name = "daily-backbone"
            exit_code = run_pipeline(ctx, pipeline_name, dry_run=dry_run)
    elif normalized_mode == "ops":
        if no_heavy:
            print("ceo-note: no-heavy enabled; running graph + nit only.")
            exit_code = run_workflow(ctx, "graph", dry_run=dry_run)
            if exit_code == 0:
                exit_code = run_workflow(ctx, "nit", dry_run=dry_run)
        else:
            pipeline_name = "daily-backbone"
            exit_code = run_pipeline(ctx, pipeline_name, dry_run=dry_run)
    elif normalized_mode == "stabilize":
        exit_code = run_quality_gate(ctx, "pre-merge", dry_run=dry_run)
    elif normalized_mode == "release":
        exit_code = run_quality_gate(ctx, "release", dry_run=dry_run)
    elif normalized_mode == "plan":
        print_plan(ctx)

    scorecard_rc = generate_scorecard(
        ctx,
        json_out=SCORECARD_DIR / "latest.json",
        md_out=SCORECARD_DIR / "latest.md",
    )
    if exit_code == 0 and scorecard_rc != 0:
        exit_code = scorecard_rc

    ctx.state["history"].append(
        {
            "at": utc_now(),
            "event": "ceo-command",
            "directive": directive,
            "mode": normalized_mode,
            "dryRun": dry_run,
            "maxDispatch": max_dispatch,
            "autoStart": auto_start,
            "execute": execute,
            "forcePipeline": force_pipeline,
            "noHeavy": no_heavy,
            "dispatches": dispatches,
            "exitCode": exit_code,
        }
    )
    save_state(ctx)
    return exit_code


def _build_argparser() -> argparse.ArgumentParser:
    """Construct the argparse-driven CLI."""
    parser = argparse.ArgumentParser(
        prog="autopilot",
        description="Nexus Ecosystem Autopilot — task orchestration and delivery management.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Examples:\n"
               "  autopilot board\n"
               "  autopilot brief\n"
               "  autopilot dispatch backend --lane backbone --start\n"
               "  autopilot work-order NX-BB-001\n"
               "  autopilot start NX-BB-001\n"
               "  autopilot run nit\n"
               "  autopilot pipeline daily-backbone --dry-run\n"
               "  autopilot gate pre-merge\n"
               "  autopilot scorecard\n"
               "  autopilot ceo \"Build and harden the Nexus ecosystem\" --mode develop --execute\n",
    )
    parser.add_argument("--version", action="version", version=f"%(prog)s {__version__}")
    sub = parser.add_subparsers(dest="command", help="Available commands")

    # ── read-only commands ──
    sub.add_parser("status", help="Show state file summary and workflow inventory")
    sub.add_parser("cheatsheet", help="Print CEO quick-start reference")
    sub.add_parser("board", help="Kanban board grouped by status")
    sub.add_parser("org", help="Print organisation chart and role definitions")
    sub.add_parser("plan", help="Lane-by-lane ready/blocked plan")
    sub.add_parser("brief", help="Executive summary with top ready tasks")

    p_next = sub.add_parser("next", help="Show next recommended tasks")
    p_next.add_argument("lane", nargs="?", default=None, help="Filter by lane")

    p_dispatch = sub.add_parser("dispatch", help="Dispatch the next task for a role")
    p_dispatch.add_argument("role", help="Role to dispatch for (e.g. backend)")
    p_dispatch.add_argument("--lane", default=None, help="Limit dispatch to a specific lane")
    p_dispatch.add_argument("--start", action="store_true", help="Auto-start the dispatched task")

    p_wo = sub.add_parser("work-order", help="Show full work order for a task")
    p_wo.add_argument("task_id", help="Task ID")

    p_handoff = sub.add_parser("handoff", help="Generate a handoff markdown for a task")
    p_handoff.add_argument("task_id", help="Task ID")
    p_handoff.add_argument("--out", default=None, help="Custom output path")

    p_decision = sub.add_parser("decision", help="Log a decision against a task")
    p_decision.add_argument("task_id", help="Task ID")
    p_decision.add_argument("summary", help="Decision summary text")
    p_decision.add_argument("--out", default=None, help="Custom output path")

    # ── mutation commands ──
    p_start = sub.add_parser("start", help="Start a task (moves to in-progress)")
    p_start.add_argument("task_id", help="Task ID")

    p_done = sub.add_parser("done", help="Complete a task (moves to done)")
    p_done.add_argument("task_id", help="Task ID")

    # ── execution commands ──
    p_run = sub.add_parser("run", help="Execute a single workflow")
    p_run.add_argument("workflow", help="Workflow name")
    p_run.add_argument("--dry-run", action="store_true", help="Preview without executing")

    p_pipeline = sub.add_parser("pipeline", help="Run a sequence of workflows")
    p_pipeline.add_argument("name", help="Pipeline name")
    p_pipeline.add_argument("--dry-run", action="store_true", help="Preview without executing")

    p_gate = sub.add_parser("gate", help="Run a quality gate")
    p_gate.add_argument("name", help="Gate name")
    p_gate.add_argument("--dry-run", action="store_true", help="Preview without executing")

    p_scorecard = sub.add_parser("scorecard", help="Generate release-readiness scorecard")
    p_scorecard.add_argument("--json-out", default=None, help="JSON output path")
    p_scorecard.add_argument("--md-out", default=None, help="Markdown output path")

    # ── CEO command ──
    p_ceo = sub.add_parser("ceo", help="Autonomous CEO orchestration mode")
    p_ceo.add_argument("directive", help="Natural-language directive")
    p_ceo.add_argument("--mode", default="develop", choices=["develop", "stabilize", "release", "ops", "plan"],
                       help="Orchestration mode (default: develop)")
    p_ceo.add_argument("--execute", action="store_true", help="Actually run workflows/gates")
    p_ceo.add_argument("--dry-run", action="store_true", help="Preview without executing")
    p_ceo.add_argument("--max-dispatch", type=int, default=6, help="Max tasks to dispatch (default: 6)")
    p_ceo.add_argument("--no-start", action="store_true", help="Dispatch recommendations only")
    p_ceo.add_argument("--no-heavy", action="store_true", help="Skip heavy network workflows")
    p_ceo.add_argument("--force-pipeline", action="store_true", help="Always run daily-backbone in develop mode")

    return parser


def dispatch_main(ns: argparse.Namespace) -> int:
    """Route parsed args to the appropriate handler."""
    ctx = load_context()
    cmd = ns.command

    if cmd == "status":
        return show_status(ctx)
    if cmd == "cheatsheet":
        return print_cheatsheet()
    if cmd == "board":
        return print_board(ctx)
    if cmd == "org":
        return print_org(ctx)
    if cmd == "plan":
        return print_plan(ctx)
    if cmd == "brief":
        return print_brief(ctx)
    if cmd == "next":
        return print_next(ctx, ns.lane)
    if cmd == "dispatch":
        return dispatch_task(ctx, role=ns.role, lane=ns.lane, auto_start=ns.start)
    if cmd == "work-order":
        return show_work_order(ctx, ns.task_id)
    if cmd == "handoff":
        out_path = Path(ns.out) if ns.out else None
        return write_handoff(ctx, ns.task_id, out_path=out_path)
    if cmd == "decision":
        out_path = Path(ns.out) if ns.out else None
        return write_decision_log(ctx, ns.task_id, ns.summary, out_path=out_path)
    if cmd == "start":
        return start_task(ctx, ns.task_id)
    if cmd == "done":
        return complete_task(ctx, ns.task_id)
    if cmd == "run":
        return run_workflow(ctx, ns.workflow, dry_run=ns.dry_run)
    if cmd == "pipeline":
        return run_pipeline(ctx, ns.name, dry_run=ns.dry_run)
    if cmd == "gate":
        return run_quality_gate(ctx, ns.name, dry_run=ns.dry_run)
    if cmd == "scorecard":
        json_path = Path(ns.json_out) if ns.json_out else (SCORECARD_DIR / "latest.json")
        md_path = Path(ns.md_out) if ns.md_out else (SCORECARD_DIR / "latest.md")
        return generate_scorecard(ctx, json_out=json_path, md_out=md_path)
    if cmd == "ceo":
        return run_ceo_command(
            ctx,
            directive=ns.directive,
            mode=ns.mode,
            dry_run=ns.dry_run,
            max_dispatch=ns.max_dispatch,
            auto_start=not ns.no_start,
            execute=ns.execute,
            force_pipeline=ns.force_pipeline,
            no_heavy=ns.no_heavy,
        )

    return 0


def main(argv: List[str] | None = None) -> int:
    """Entry point — parse args and dispatch."""
    parser = _build_argparser()

    # Support legacy slash-command syntax: /ceo → ceo, /brief → brief
    if argv is None:
        argv = sys.argv[1:]
    if argv and argv[0].startswith("/"):
        argv = [argv[0][1:]] + argv[1:]

    ns = parser.parse_args(argv)
    if not ns.command:
        parser.print_help()
        return 0

    return dispatch_main(ns)


if __name__ == "__main__":
    raise SystemExit(main())
