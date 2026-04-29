import { describe, it, expect, beforeEach } from "vitest";
import {
  createArtifact,
  getArtifact,
  deleteArtifact,
  listArtifacts,
  clearArtifacts,
} from "./index.js";

describe("ArtifactStore", () => {
  beforeEach(() => clearArtifacts());

  it("creates an artifact and retrieves it by id", () => {
    const a = createArtifact({ title: "Test Chart", html: "<h1>Hello</h1>" });
    expect(a.id).toBeTruthy();
    expect(a.title).toBe("Test Chart");
    expect(a.html).toBe("<h1>Hello</h1>");
    expect(a.contentType).toBe("text/html");

    const retrieved = getArtifact(a.id);
    expect(retrieved).toBeDefined();
    expect(retrieved!.title).toBe("Test Chart");
  });

  it("returns undefined for unknown id", () => {
    expect(getArtifact("does-not-exist")).toBeUndefined();
  });

  it("lists artifacts without html field", () => {
    createArtifact({ title: "A1", html: "<p>1</p>" });
    createArtifact({ title: "A2", html: "<p>2</p>" });
    const list = listArtifacts();
    expect(list).toHaveLength(2);
    for (const item of list) {
      expect((item as any).html).toBeUndefined();
    }
  });

  it("lists artifacts sorted newest first", async () => {
    const a1 = createArtifact({ title: "First", html: "" });
    await new Promise((r) => setTimeout(r, 5));
    const a2 = createArtifact({ title: "Second", html: "" });
    const list = listArtifacts();
    expect(list[0].id).toBe(a2.id);
    expect(list[1].id).toBe(a1.id);
  });

  it("deletes an artifact", () => {
    const a = createArtifact({ title: "ToDelete", html: "" });
    expect(deleteArtifact(a.id)).toBe(true);
    expect(getArtifact(a.id)).toBeUndefined();
  });

  it("returns false when deleting non-existent artifact", () => {
    expect(deleteArtifact("ghost-id")).toBe(false);
  });

  it("stores agentId and sessionId", () => {
    const a = createArtifact({ title: "T", html: "", agentId: "agent1", sessionId: "sess1" });
    expect(getArtifact(a.id)?.agentId).toBe("agent1");
    expect(getArtifact(a.id)?.sessionId).toBe("sess1");
  });
});
