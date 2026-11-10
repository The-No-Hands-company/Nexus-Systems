import { describe, expect, it } from "bun:test";
import { TasksEngine } from "../src/tasks-engine";

describe("Nexus Tasks Engine", () => {
  it("creates lists and manages tasks with priorities and completion states", () => {
    const engine = new TasksEngine(":memory:");
    const list = engine.addList("Sprint Punchlist", "Engineering sprint items");
    expect(list.id).toBeDefined();

    const task = engine.addTask("Fix 502 gateway timeouts", "Configure reverse proxy", list.id, "usr-ops", "urgent", "2026-11-12");
    expect(task.title).toBe("Fix 502 gateway timeouts");
    expect(task.completed).toBe(0);

    engine.completeTask(task.id);
    const updated = engine.getTask(task.id);
    expect(updated?.completed).toBe(1);

    const tasks = engine.listTasks(list.id, "urgent");
    expect(tasks.length).toBe(1);
  });
});
