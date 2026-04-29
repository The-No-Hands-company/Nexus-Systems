import { describe, expect, test } from "bun:test";
import { existsSync, readFileSync, rmSync } from "node:fs";
import { join } from "node:path";

describe("nexus-guardian persisted state", () => {
  test("decisions and policy survive module restart", async () => {
    const statePath = join(
      process.cwd(),
      "tests",
      `tmp-guardian-state-${Date.now()}-${Math.random().toString(16).slice(2)}.json`,
    );

    process.env.NEXUS_GUARDIAN_STATE_PATH = statePath;

    try {
      const stateA = await import(`../src/state.ts?run=${Date.now()}-a`);
      stateA.clearDecisions();
      stateA.recordDecision("service", "svc-persist", "deny", "guardian-test", "blocked by policy");
      stateA.setPublicExposureDenyRules(["svc-persist", "unsafe"]);

      expect(existsSync(statePath)).toBe(true);

      const persisted = JSON.parse(readFileSync(statePath, "utf-8")) as {
        policy: { publicExposureDenyRules: string[]; version: number };
        decisions: Array<{ subjectId: string }>;
      };

      expect(persisted.decisions.some((d) => d.subjectId === "svc-persist")).toBe(true);
      expect(persisted.policy.publicExposureDenyRules).toContain("unsafe");
      expect(persisted.policy.version).toBeGreaterThanOrEqual(2);

      const stateB = await import(`../src/state.ts?run=${Date.now()}-b`);
      const restored = stateB.getDecision("service", "svc-persist");
      const restoredPolicy = stateB.getPolicy();

      expect(restored).toBeDefined();
      expect(restored?.verdict).toBe("deny");
      expect(restoredPolicy.publicExposureDenyRules).toContain("unsafe");

      stateB.clearDecisions();
    } finally {
      delete process.env.NEXUS_GUARDIAN_STATE_PATH;
      rmSync(statePath, { force: true });
    }
  });
});
