// GSD (Get Shit Done) — Spec-driven task management
// Phase 1.2: SQLite persistence for specs & tasks (survive restarts)
// Phase 10.2: Progress tracking improvements

import { randomUUID } from "node:crypto";
import Database from "better-sqlite3";
import type { GSDSpec, GSDTask } from "../core/types.js";

export class GSDManager {
  private db: Database.Database;
  private autoCommit: boolean;
  private verifyBeforeDone: boolean;

  constructor(
    dbPath: string,
    options: { autoCommit?: boolean; verifyBeforeDone?: boolean } = {},
  ) {
    this.autoCommit = options.autoCommit ?? true;
    this.verifyBeforeDone = options.verifyBeforeDone ?? true;
    this.db = new Database(dbPath);
    this.db.pragma("journal_mode = WAL");
    this.db.pragma("foreign_keys = ON");
    this.initSchema();
  }

  private initSchema(): void {
    this.db.exec(`
      CREATE TABLE IF NOT EXISTS gsd_specs (
        id TEXT PRIMARY KEY,
        title TEXT NOT NULL,
        description TEXT NOT NULL,
        status TEXT NOT NULL DEFAULT 'draft',
        created_at INTEGER NOT NULL,
        updated_at INTEGER NOT NULL
      );

      CREATE TABLE IF NOT EXISTS gsd_tasks (
        id TEXT PRIMARY KEY,
        spec_id TEXT NOT NULL,
        title TEXT NOT NULL,
        description TEXT NOT NULL,
        done_criteria TEXT NOT NULL, -- JSON array
        status TEXT NOT NULL DEFAULT 'pending',
        output TEXT,
        commit_hash TEXT,
        sort_order INTEGER NOT NULL DEFAULT 0,
        created_at INTEGER NOT NULL,
        completed_at INTEGER,
        FOREIGN KEY (spec_id) REFERENCES gsd_specs(id) ON DELETE CASCADE
      );

      CREATE INDEX IF NOT EXISTS idx_gsd_tasks_spec ON gsd_tasks(spec_id);
      CREATE INDEX IF NOT EXISTS idx_gsd_tasks_status ON gsd_tasks(status);
    `);
    // Phase 2.4 migration: add blocked_by column if absent
    const cols = (this.db.pragma(`table_info(gsd_tasks)`) as Array<{ name: string }>).map((c) => c.name);
    if (!cols.includes("blocked_by")) {
      this.db.exec(`ALTER TABLE gsd_tasks ADD COLUMN blocked_by TEXT DEFAULT NULL`);
    }
  }

  // ─── Spec Management ───────────────────────────────────────────────

  createSpec(title: string, description: string): GSDSpec {
    const now = Date.now();
    const id = randomUUID();
    this.db
      .prepare(
        `INSERT INTO gsd_specs (id, title, description, status, created_at, updated_at) VALUES (?, ?, ?, 'draft', ?, ?)`,
      )
      .run(id, title, description, now, now);
    return { id, title, description, tasks: [], status: "draft", createdAt: now };
  }

  getSpec(specId: string): GSDSpec | undefined {
    const row = this.db
      .prepare(`SELECT * FROM gsd_specs WHERE id = ?`)
      .get(specId) as any;
    if (!row) return undefined;
    return this.hydrateSpec(row);
  }

  listSpecs(): GSDSpec[] {
    const rows = this.db
      .prepare(`SELECT * FROM gsd_specs ORDER BY created_at DESC`)
      .all() as any[];
    return rows.map((r) => this.hydrateSpec(r));
  }

  deleteSpec(specId: string): boolean {
    const info = this.db.prepare(`DELETE FROM gsd_specs WHERE id = ?`).run(specId);
    return info.changes > 0;
  }

  private hydrateSpec(row: any): GSDSpec {
    const tasks = this.db
      .prepare(`SELECT * FROM gsd_tasks WHERE spec_id = ? ORDER BY sort_order`)
      .all(row.id) as any[];
    return {
      id: row.id,
      title: row.title,
      description: row.description,
      status: row.status,
      createdAt: row.created_at,
      tasks: tasks.map((t) => this.hydrateTask(t)),
    };
  }

  private hydrateTask(row: any): GSDTask {
    return {
      id: row.id,
      specId: row.spec_id,
      title: row.title,
      description: row.description,
      doneCriteria: JSON.parse(row.done_criteria),
      status: row.status,
      output: row.output ?? undefined,
      commitHash: row.commit_hash ?? undefined,
      createdAt: row.created_at,
      completedAt: row.completed_at ?? undefined,
      blockedBy: row.blocked_by ? (JSON.parse(row.blocked_by) as string[]) : undefined,
    };
  }

  private updateSpecStatus(specId: string): void {
    const tasks = this.db
      .prepare(`SELECT status FROM gsd_tasks WHERE spec_id = ?`)
      .all(specId) as any[];
    if (tasks.length === 0) return;
    const allDone = tasks.every((t) => t.status === "done");
    const anyInProgress = tasks.some(
      (t) => t.status === "in-progress" || t.status === "verifying",
    );
    const newStatus = allDone ? "done" : anyInProgress ? "executing" : "planned";
    this.db
      .prepare(`UPDATE gsd_specs SET status = ?, updated_at = ? WHERE id = ?`)
      .run(newStatus, Date.now(), specId);
  }

  // ─── Task Planning ─────────────────────────────────────────────────

  addTask(
    specId: string,
    title: string,
    description: string,
    doneCriteria: string[],
    blockedBy?: string[],
  ): GSDTask {
    const spec = this.db.prepare(`SELECT id FROM gsd_specs WHERE id = ?`).get(specId);
    if (!spec) throw new Error(`Spec not found: ${specId}`);

    const now = Date.now();
    const id = randomUUID();
    const maxOrder = (
      this.db
        .prepare(`SELECT MAX(sort_order) as m FROM gsd_tasks WHERE spec_id = ?`)
        .get(specId) as any
    )?.m ?? -1;

    this.db
      .prepare(
        `INSERT INTO gsd_tasks (id, spec_id, title, description, done_criteria, status, sort_order, created_at, blocked_by) VALUES (?, ?, ?, ?, ?, 'pending', ?, ?, ?)`,
      )
      .run(id, specId, title, description, JSON.stringify(doneCriteria), maxOrder + 1, now, blockedBy ? JSON.stringify(blockedBy) : null);

    // Upgrade spec from draft to planned
    this.db
      .prepare(
        `UPDATE gsd_specs SET status = CASE WHEN status = 'draft' THEN 'planned' ELSE status END, updated_at = ? WHERE id = ?`,
      )
      .run(now, specId);

    return {
      id,
      specId,
      title,
      description,
      doneCriteria,
      status: "pending",
      createdAt: now,
      blockedBy: blockedBy?.length ? blockedBy : undefined,
    };
  }

  planFromText(specId: string, planText: string): GSDTask[] {
    const spec = this.db.prepare(`SELECT id FROM gsd_specs WHERE id = ?`).get(specId);
    if (!spec) throw new Error(`Spec not found: ${specId}`);

    const lines = planText.split("\n").filter((l) => l.trim());
    const tasks: GSDTask[] = [];
    let currentTask: { title: string; desc: string[]; criteria: string[] } | null = null;

    for (const line of lines) {
      const taskMatch = line.match(/^\s*(?:\d+[.)]\s*|[-*]\s+)(.+)/);
      const criteriaMatch = line.match(/^\s+[-*]\s+(?:DONE:|Done:|✓|✅)\s*(.+)/);

      if (criteriaMatch && currentTask) {
        currentTask.criteria.push(criteriaMatch[1].trim());
      } else if (taskMatch) {
        if (currentTask) {
          tasks.push(
            this.addTask(
              specId,
              currentTask.title,
              currentTask.desc.join("\n"),
              currentTask.criteria.length > 0
                ? currentTask.criteria
                : [`"${currentTask.title}" is complete and verified`],
            ),
          );
        }
        currentTask = { title: taskMatch[1].trim(), desc: [], criteria: [] };
      } else if (currentTask) {
        currentTask.desc.push(line.trim());
      }
    }

    if (currentTask) {
      tasks.push(
        this.addTask(
          specId,
          currentTask.title,
          currentTask.desc.join("\n"),
          currentTask.criteria.length > 0
            ? currentTask.criteria
            : [`"${currentTask.title}" is complete and verified`],
        ),
      );
    }

    return tasks;
  }

  // ─── Task Execution ────────────────────────────────────────────────

  /** Return all tasks that are blocked by unfinished dependencies. */
  getBlockedTasks(specId: string): GSDTask[] {
    const rows = this.db
      .prepare(`SELECT * FROM gsd_tasks WHERE spec_id = ? AND blocked_by IS NOT NULL`)
      .all(specId) as any[];
    const tasks = rows.map((r) => this.hydrateTask(r));
    const doneIds = new Set(
      (this.db.prepare(`SELECT id FROM gsd_tasks WHERE spec_id = ? AND status = 'done'`).all(specId) as any[]).map(
        (r) => r.id,
      ),
    );
    return tasks.filter((t) => t.blockedBy?.some((dep) => !doneIds.has(dep)));
  }

  getNextTask(specId: string): GSDTask | null {
    const row = this.db
      .prepare(
        `SELECT * FROM gsd_tasks WHERE spec_id = ? AND status = 'pending' ORDER BY sort_order LIMIT 1`,
      )
      .get(specId) as any;
    return row ? this.hydrateTask(row) : null;
  }

  startTask(taskId: string): GSDTask | null {
    const row = this.db.prepare(`SELECT * FROM gsd_tasks WHERE id = ?`).get(taskId) as any;
    if (!row) return null;
    if (row.status !== "pending") {
      throw new Error(`Task "${row.title}" is already ${row.status}`);
    }
    // Ensure no other task is in-progress for this spec
    const inProgress = this.db
      .prepare(
        `SELECT title FROM gsd_tasks WHERE spec_id = ? AND status = 'in-progress'`,
      )
      .get(row.spec_id) as any;
    if (inProgress) {
      throw new Error(
        `Cannot start task — "${inProgress.title}" is still in progress. Complete it first.`,
      );
    }
    this.db
      .prepare(`UPDATE gsd_tasks SET status = 'in-progress' WHERE id = ?`)
      .run(taskId);
    this.updateSpecStatus(row.spec_id);
    return this.hydrateTask({ ...row, status: "in-progress" });
  }

  completeTask(taskId: string, output: string, commitHash?: string): GSDTask | null {
    const row = this.db.prepare(`SELECT * FROM gsd_tasks WHERE id = ?`).get(taskId) as any;
    if (!row) return null;
    if (row.status !== "in-progress") {
      throw new Error(`Task "${row.title}" is not in progress`);
    }
    const newStatus = this.verifyBeforeDone ? "verifying" : "done";
    const now = Date.now();
    this.db
      .prepare(
        `UPDATE gsd_tasks SET status = ?, output = ?, commit_hash = ?, completed_at = CASE WHEN ? = 'done' THEN ? ELSE completed_at END WHERE id = ?`,
      )
      .run(newStatus, output, commitHash ?? null, newStatus, now, taskId);
    this.updateSpecStatus(row.spec_id);
    return this.hydrateTask({
      ...row,
      status: newStatus,
      output,
      commit_hash: commitHash,
      completed_at: newStatus === "done" ? now : null,
    });
  }

  verifyTask(taskId: string, passed: boolean, reason?: string): GSDTask | null {
    const row = this.db.prepare(`SELECT * FROM gsd_tasks WHERE id = ?`).get(taskId) as any;
    if (!row) return null;
    if (row.status !== "verifying") {
      throw new Error(`Task "${row.title}" is not in verification`);
    }
    const newStatus = passed ? "done" : "failed";
    const now = Date.now();
    const output = passed
      ? row.output
      : `VERIFICATION FAILED: ${reason || "Criteria not met"}\n\nOriginal output:\n${row.output}`;
    this.db
      .prepare(
        `UPDATE gsd_tasks SET status = ?, output = ?, completed_at = CASE WHEN ? = 'done' THEN ? ELSE completed_at END WHERE id = ?`,
      )
      .run(newStatus, output, newStatus, now, taskId);
    this.updateSpecStatus(row.spec_id);
    return this.hydrateTask({
      ...row,
      status: newStatus,
      output,
      completed_at: passed ? now : null,
    });
  }

  // ─── Task Context Generation ───────────────────────────────────────

  getTaskContext(taskId: string): string {
    const row = this.db.prepare(`SELECT * FROM gsd_tasks WHERE id = ?`).get(taskId) as any;
    if (!row) throw new Error(`Task not found: ${taskId}`);

    const spec = this.db.prepare(`SELECT * FROM gsd_specs WHERE id = ?`).get(row.spec_id) as any;
    const tasks = this.db
      .prepare(`SELECT * FROM gsd_tasks WHERE spec_id = ? ORDER BY sort_order`)
      .all(row.spec_id) as any[];

    const completedTasks = tasks.filter((t) => t.status === "done");
    const currentIdx = tasks.findIndex((t) => t.id === taskId);
    const criteria: string[] = JSON.parse(row.done_criteria);

    let context = `# Current Task: ${row.title}\n\n`;
    context += `## Spec: ${spec.title}\n`;
    context += `${spec.description}\n\n`;
    context += `## Task Description\n${row.description}\n\n`;
    context += `## Done Criteria\n`;
    for (const c of criteria) {
      context += `- [ ] ${c}\n`;
    }
    context += "\n";

    if (completedTasks.length > 0) {
      context += `## Completed Tasks (${completedTasks.length}/${tasks.length})\n`;
      for (const ct of completedTasks) {
        context += `- ✅ ${ct.title}`;
        if (ct.commit_hash) context += ` (${ct.commit_hash.slice(0, 7)})`;
        context += "\n";
      }
      context += "\n";
    }

    const remaining = tasks
      .slice(currentIdx + 1)
      .filter((t) => t.status === "pending");
    if (remaining.length > 0) {
      context += `## Remaining Tasks\n`;
      for (const rt of remaining) {
        context += `- ⬜ ${rt.title}\n`;
      }
      context += "\n";
    }

    context += `## Rules\n`;
    context += `- Focus ONLY on this task. Do not work on other tasks.\n`;
    context += `- Verify ALL done criteria before marking complete.\n`;
    context += `- Make a single atomic commit for this task.\n`;
    context += `- If blocked, explain why and stop.\n`;

    return context;
  }

  // ─── Status ────────────────────────────────────────────────────────

  getProgress(specId: string): {
    total: number;
    done: number;
    inProgress: number;
    pending: number;
    failed: number;
    percentDone: number;
  } | null {
    const spec = this.db.prepare(`SELECT id FROM gsd_specs WHERE id = ?`).get(specId);
    if (!spec) return null;

    const tasks = this.db
      .prepare(`SELECT status FROM gsd_tasks WHERE spec_id = ?`)
      .all(specId) as any[];

    const total = tasks.length;
    const done = tasks.filter((t) => t.status === "done").length;
    const inProgress = tasks.filter((t) => t.status === "in-progress").length;
    const pending = tasks.filter((t) => t.status === "pending").length;
    const failed = tasks.filter((t) => t.status === "failed").length;

    return {
      total,
      done,
      inProgress,
      pending,
      failed,
      percentDone: total > 0 ? Math.round((done / total) * 100) : 0,
    };
  }

  close(): void {
    this.db.close();
  }
}

// ─── GSD Tools (exposed to agent) ─────────────────────────────────────────

export function createGSDTools(gsd: GSDManager): import("../core/types.js").Tool[] {
  return [
    {
      name: "gsd_create_spec",
      description:
        "Create a new GSD spec (project/feature specification) for spec-driven development",
      parameters: [
        { name: "title", type: "string", description: "Short title for the spec", required: true },
        {
          name: "description",
          type: "string",
          description: "Detailed description of what needs to be built",
          required: true,
        },
      ],
      async execute(args) {
        return gsd.createSpec(args.title as string, args.description as string);
      },
    },
    {
      name: "gsd_add_task",
      description: "Add an atomic task to a GSD spec with explicit done criteria",
      parameters: [
        { name: "spec_id", type: "string", description: "ID of the spec", required: true },
        { name: "title", type: "string", description: "Short task title", required: true },
        {
          name: "description",
          type: "string",
          description: "Detailed task description",
          required: true,
        },
        {
          name: "done_criteria",
          type: "array",
          description: "Criteria list that must be true when done",
          required: true,
        },
      ],
      async execute(args) {
        return gsd.addTask(
          args.spec_id as string,
          args.title as string,
          args.description as string,
          args.done_criteria as string[],
        );
      },
    },
    {
      name: "gsd_plan_text",
      description: "Parse a natural language plan into atomic GSD tasks",
      parameters: [
        { name: "spec_id", type: "string", description: "ID of the spec", required: true },
        { name: "plan", type: "string", description: "Natural language plan text", required: true },
      ],
      async execute(args) {
        return gsd.planFromText(args.spec_id as string, args.plan as string);
      },
    },
    {
      name: "gsd_next_task",
      description: "Get the next pending task for a spec",
      parameters: [
        { name: "spec_id", type: "string", description: "ID of the spec", required: true },
      ],
      async execute(args) {
        const task = gsd.getNextTask(args.spec_id as string);
        if (!task) return { message: "All tasks complete or no tasks found" };
        return { task, context: gsd.getTaskContext(task.id) };
      },
    },
    {
      name: "gsd_start_task",
      description: "Mark a task as in-progress",
      parameters: [
        { name: "task_id", type: "string", description: "ID of the task", required: true },
      ],
      async execute(args) {
        return gsd.startTask(args.task_id as string);
      },
    },
    {
      name: "gsd_complete_task",
      description: "Mark current task as complete with output summary",
      parameters: [
        { name: "task_id", type: "string", description: "ID of the task", required: true },
        { name: "output", type: "string", description: "Summary of what was done", required: true },
        {
          name: "commit_hash",
          type: "string",
          description: "Git commit hash (if auto_commit)",
          required: false,
        },
      ],
      async execute(args) {
        return gsd.completeTask(
          args.task_id as string,
          args.output as string,
          args.commit_hash as string | undefined,
        );
      },
    },
    {
      name: "gsd_verify_task",
      description: "Verify a task passes or fails against done criteria",
      parameters: [
        { name: "task_id", type: "string", description: "ID of the task", required: true },
        { name: "passed", type: "boolean", description: "Whether verification passed", required: true },
        { name: "reason", type: "string", description: "Reason if failed", required: false },
      ],
      async execute(args) {
        return gsd.verifyTask(
          args.task_id as string,
          args.passed as boolean,
          args.reason as string | undefined,
        );
      },
    },
    {
      name: "gsd_progress",
      description: "Get progress overview for a spec",
      parameters: [
        { name: "spec_id", type: "string", description: "ID of the spec", required: true },
      ],
      async execute(args) {
        const progress = gsd.getProgress(args.spec_id as string);
        const spec = gsd.getSpec(args.spec_id as string);
        return {
          spec: spec ? { title: spec.title, status: spec.status } : null,
          progress,
        };
      },
    },
    {
      name: "gsd_list_specs",
      description: "List all GSD specs and their status",
      parameters: [],
      async execute() {
        return gsd.listSpecs().map((s) => ({
          id: s.id,
          title: s.title,
          status: s.status,
          tasks: s.tasks.length,
          progress: gsd.getProgress(s.id),
        }));
      },
    },
    {
      name: "gsd_delete_spec",
      description: "Delete a GSD spec and all its tasks",
      parameters: [
        { name: "spec_id", type: "string", description: "ID of the spec", required: true },
      ],
      async execute(args) {
        const deleted = gsd.deleteSpec(args.spec_id as string);
        return { deleted };
      },
    },
  ];
}
