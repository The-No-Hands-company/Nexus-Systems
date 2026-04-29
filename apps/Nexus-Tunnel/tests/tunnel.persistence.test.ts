import { describe, expect, test } from "bun:test";
import { existsSync, readFileSync, rmSync } from "node:fs";
import { join } from "node:path";

describe("nexus-tunnel persisted state", () => {
  test("routes, exposures, and decisions survive module restart", async () => {
    const statePath = join(
      process.cwd(),
      "tests",
      `tmp-tunnel-state-${Date.now()}-${Math.random().toString(16).slice(2)}.json`,
    );

    process.env.NEXUS_TUNNEL_STATE_PATH = statePath;

    try {
      const stateA = await import(`../src/state.ts?run=${Date.now()}-a`);
      stateA.clearRoutes();
      stateA.registerRoute("svc-persist", "http://127.0.0.1:9999", "public");
      stateA.recordExposure("svc-persist", "public", true, "guardian approved");
      stateA.recordDecision("svc-persist", "public", true, "tunnel", "approved for exposure");

      expect(existsSync(statePath)).toBe(true);

      const persisted = JSON.parse(readFileSync(statePath, "utf-8")) as {
        routes: Array<{ toolId: string }>;
        exposures: Array<{ toolId: string }>;
        decisions: Array<{ toolId: string }>;
      };
      expect(persisted.routes.some((r) => r.toolId === "svc-persist")).toBe(true);
      expect(persisted.exposures.some((e) => e.toolId === "svc-persist")).toBe(true);
      expect(persisted.decisions.some((d) => d.toolId === "svc-persist")).toBe(true);

      const stateB = await import(`../src/state.ts?run=${Date.now()}-b`);
      const restoredRoute = stateB.getRoute("svc-persist");
      const restoredExposure = stateB.getExposure("svc-persist");
      const restoredDecisions = stateB.listDecisions();

      expect(restoredRoute).toBeDefined();
      expect(restoredRoute?.targetUrl).toBe("http://127.0.0.1:9999");
      expect(restoredExposure?.guardianApproved).toBe(true);
      expect(restoredDecisions.some((d: { toolId: string }) => d.toolId === "svc-persist")).toBe(true);

      stateB.clearRoutes();
    } finally {
      delete process.env.NEXUS_TUNNEL_STATE_PATH;
      rmSync(statePath, { force: true });
    }
  });
});
