import type { Database } from "bun:sqlite";
import { beforeEach, describe, expect, it } from "bun:test";
import { HttpError } from "../src/http";
import { openDatabase } from "../src/store/db";
import { createDependency, updateDependency } from "../src/store/dependencies";
import { createProject } from "../src/store/projects";
import { listStatuses, updateStatus } from "../src/store/statuses";
import { createTask } from "../src/store/tasks";
import { createTeamWorkspace, setMember } from "../src/store/workspaces";

/**
 * Access helpers turn "you cannot see that" into a 404 for the resource at
 * hand. Only a 404 may be converted that way: any other failure is a real
 * fault and must reach the server's 500 handler (and the operator's log)
 * rather than being disguised as "not found".
 */
const OWNER = "usr-owner";
const MEMBER = "usr-member";
let db: Database;
let open: string;
let hidden: string;

beforeEach(() => {
  db = openDatabase(":memory:");
  const ws = createTeamWorkspace(db, OWNER, "Team");
  setMember(db, OWNER, ws.id, MEMBER, "member");
  const base = { description: "", startDate: "2026-09-07", scheduleMode: "manual" as const };
  open = createProject(db, OWNER, ws.id, {
    ...base,
    key: "OPEN",
    name: "Open",
    visibility: "workspace",
  }).id;
  hidden = createProject(db, OWNER, ws.id, {
    ...base,
    key: "HID",
    name: "Hidden",
    visibility: "restricted",
  }).id;
  db.exec(
    `INSERT INTO project_members (project_id, subject, created_at, created_by) VALUES ('${hidden}', '${MEMBER}', 'x', 'x')`,
  );
});

/** Breaks the restricted-project lookup so projectAccess fails with a non-HTTP error. */
function breakVisibilityLookup(): void {
  db.exec("DROP TABLE project_members");
}

function expectInternal(fn: () => unknown): void {
  let thrown: unknown;
  try {
    fn();
  } catch (error) {
    thrown = error;
  }
  expect(thrown).toBeDefined();
  expect(thrown instanceof HttpError).toBe(false);
}

describe("access helpers convert only 404s", () => {
  it("a status lookup rethrows a database fault", () => {
    const status = listStatuses(db, MEMBER, hidden)[0]?.id as string;
    breakVisibilityLookup();
    expectInternal(() => updateStatus(db, MEMBER, status, { name: "Renamed" }));
  });

  it("a dependency lookup rethrows a database fault", () => {
    const a = createTask(db, OWNER, hidden, { title: "A", durationDays: 1 });
    const b = createTask(db, OWNER, hidden, { title: "B", durationDays: 1 });
    const link = createDependency(db, OWNER, hidden, {
      predecessorId: a.id,
      successorId: b.id,
      type: "FS",
      lagDays: 0,
    });
    breakVisibilityLookup();
    expectInternal(() => updateDependency(db, MEMBER, link.id, { lagDays: 1 }));
  });

  it("a cross-project link check rethrows a database fault", () => {
    const here = createTask(db, OWNER, open, { title: "Here", durationDays: 1 });
    const there = createTask(db, OWNER, hidden, { title: "There", durationDays: 1 });
    breakVisibilityLookup();
    expectInternal(() =>
      createDependency(db, MEMBER, open, {
        predecessorId: here.id,
        successorId: there.id,
        type: "FS",
        lagDays: 0,
      }),
    );
  });

  it("still answers 404 for what the caller cannot see", () => {
    db.exec(`DELETE FROM project_members WHERE subject = '${MEMBER}'`);
    const status = listStatuses(db, OWNER, hidden)[0]?.id as string;
    expect(() => updateStatus(db, MEMBER, status, { name: "x" })).toThrow("status not found");
  });
});
