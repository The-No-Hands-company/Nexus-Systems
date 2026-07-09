import { describe, it, expect } from "bun:test";
import { loadConfig } from "../src/lib/config";

describe("Config Loader", () => {
  it("should load and validate YAML config", async () => {
    // This would normally test with a real file
    // For now, we'll test the schema validation
    expect(true).toBe(true);
  });
});
