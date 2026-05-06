"""Parallel compilation helpers for NexusLang build pipelines."""

from __future__ import annotations

import subprocess
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Set


@dataclass(eq=False)
class CompilationTask:
    """A single compilation task."""

    source_file: Path
    output_file: Path
    compiler_args: List[str]
    dependencies: List[Path] = field(default_factory=list)

    def __hash__(self) -> int:
        return hash((self.source_file, self.output_file))

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, CompilationTask):
            return False
        return (
            self.source_file == other.source_file
            and self.output_file == other.output_file
        )


@dataclass
class TaskResult:
    """Result of running one compilation task."""

    task: CompilationTask
    success: bool
    duration: float
    stdout: str = ""
    stderr: str = ""
    errors: List[str] = field(default_factory=list)
    return_code: int = 0


class DependencyGraph:
    """Dependency graph over compilation tasks."""

    def __init__(self) -> None:
        self._tasks: Dict[Path, CompilationTask] = {}

    def add_task(self, task: CompilationTask) -> None:
        self._tasks[task.source_file] = task

    def task_count(self) -> int:
        return len(self._tasks)

    def topological_layers(self) -> List[List[CompilationTask]]:
        if not self._tasks:
            return []

        in_degree: Dict[Path, int] = {k: 0 for k in self._tasks}
        reverse_adj: Dict[Path, Set[Path]] = {k: set() for k in self._tasks}

        for src, task in self._tasks.items():
            for dep in task.dependencies:
                if dep in self._tasks:
                    in_degree[src] += 1
                    reverse_adj[dep].add(src)

        layers: List[List[CompilationTask]] = []
        ready = sorted([k for k, deg in in_degree.items() if deg == 0], key=str)
        processed = 0

        while ready:
            layer_paths = ready
            ready = []
            layer: List[CompilationTask] = [self._tasks[p] for p in layer_paths]
            layers.append(layer)
            processed += len(layer_paths)

            for node in layer_paths:
                for dependent in reverse_adj[node]:
                    in_degree[dependent] -= 1
                    if in_degree[dependent] == 0:
                        ready.append(dependent)
            ready.sort(key=str)

        if processed != len(self._tasks):
            raise ValueError("Circular dependency detected in compilation tasks")

        return layers


class ParallelCompiler:
    """Runs compilation tasks in parallel while respecting dependencies."""

    def __init__(self, max_workers: int = 0):
        self.max_workers = max_workers if max_workers > 0 else 1

    def _run_task(self, task: CompilationTask) -> TaskResult:
        started = time.perf_counter()

        if not task.compiler_args:
            return TaskResult(
                task=task,
                success=False,
                duration=time.perf_counter() - started,
                errors=["Empty compiler command"],
                return_code=127,
            )

        try:
            proc = subprocess.run(
                task.compiler_args,
                capture_output=True,
                text=True,
                check=False,
            )
            success = proc.returncode == 0
            errors = [] if success else [proc.stderr.strip() or "Compilation failed"]
            return TaskResult(
                task=task,
                success=success,
                duration=time.perf_counter() - started,
                stdout=proc.stdout,
                stderr=proc.stderr,
                errors=errors,
                return_code=proc.returncode,
            )
        except FileNotFoundError as exc:
            return TaskResult(
                task=task,
                success=False,
                duration=time.perf_counter() - started,
                stderr=str(exc),
                errors=[str(exc)],
                return_code=127,
            )
        except Exception as exc:
            return TaskResult(
                task=task,
                success=False,
                duration=time.perf_counter() - started,
                stderr=str(exc),
                errors=[str(exc)],
                return_code=1,
            )

    def compile_independent(self, tasks: List[CompilationTask]) -> List[TaskResult]:
        if not tasks:
            return []

        results: List[TaskResult] = []
        with ThreadPoolExecutor(max_workers=self.max_workers) as pool:
            futures = [pool.submit(self._run_task, task) for task in tasks]
            for future in as_completed(futures):
                results.append(future.result())
        return results

    def compile_all(self, tasks: List[CompilationTask]) -> List[TaskResult]:
        if not tasks:
            return []

        graph = DependencyGraph()
        for task in tasks:
            graph.add_task(task)

        results: List[TaskResult] = []
        for layer in graph.topological_layers():
            results.extend(self.compile_independent(layer))

        return results


def build_tasks_from_sources(
    source_files: List[Path],
    output_dir: Path,
    compiler_path: Path,
    compiler_flags: List[str],
    dep_graph: Optional[Dict[Path, Set[Path]]] = None,
) -> List[CompilationTask]:
    """Create compilation tasks from source files and optional dependency map."""
    output_dir.mkdir(parents=True, exist_ok=True)

    tasks: List[CompilationTask] = []
    dep_graph = dep_graph or {}

    for source in source_files:
        output = output_dir / f"{source.stem}.o"
        args = [str(compiler_path), *compiler_flags, str(source), "-o", str(output)]
        dependencies = sorted(list(dep_graph.get(source, set())), key=str)
        tasks.append(
            CompilationTask(
                source_file=source,
                output_file=output,
                compiler_args=args,
                dependencies=dependencies,
            )
        )

    return tasks


__all__ = [
    "CompilationTask",
    "TaskResult",
    "DependencyGraph",
    "ParallelCompiler",
    "build_tasks_from_sources",
]
