import { describe, expect, it } from "bun:test";
import { mkdtemp, rm } from "node:fs/promises";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { TerminalAudit } from "../src/audit";

describe("TerminalAudit", () => {
  it("finalizes a session only once across racing cleanup paths", async () => {
    const root = await mkdtemp(join(tmpdir(), "nexus-terminal-audit."));
    const path = join(root, "audit.sqlite");
    const audit = new TerminalAudit(path);

    try {
      audit.begin("session-1", "founder-1", null);
      audit.end("session-1", 15);
      audit.end("session-1", 9);

      const record = audit.recent(1)[0] as { exit_code?: number } | undefined;
      expect(record?.exit_code).toBe(15);
    } finally {
      (audit as unknown as { close?: () => void }).close?.();
      await rm(root, { recursive: true, force: true });
    }
  });
});
