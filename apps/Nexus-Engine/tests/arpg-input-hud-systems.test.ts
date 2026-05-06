import { describe, expect, it } from "vitest";
import {
  applyZoomDelta,
  commandFromKey,
} from "../src/systems/arpg-input-system";
import { getCameraModeLabel } from "../src/systems/arpg-hud-system";

describe("arpg input helpers", () => {
  it("maps keyboard keys to runtime commands", () => {
    expect(commandFromKey("f")).toBe("melee");
    expect(commandFromKey("F")).toBe("melee");
    expect(commandFromKey("Escape")).toBe("clearLock");
    expect(commandFromKey("x")).toBeNull();
  });

  it("applies zoom deltas with hard clamps", () => {
    expect(applyZoomDelta(7, 100, 0.25, 36)).toBeCloseTo(8.5);
    expect(applyZoomDelta(0.3, -1000, 0.25, 36)).toBe(0.25);
    expect(applyZoomDelta(35.5, 1000, 0.25, 36)).toBe(36);
  });
});

describe("arpg hud helpers", () => {
  it("returns readable camera mode labels", () => {
    expect(getCameraModeLabel("first-person")).toBe("First-person");
    expect(getCameraModeLabel("third-person")).toBe("Third-person");
    expect(getCameraModeLabel("isometric")).toBe("Isometric");
  });
});
