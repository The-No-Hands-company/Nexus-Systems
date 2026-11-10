import { describe, expect, it } from "bun:test";
import { FilesDatabase } from "../src/db";

describe("Nexus Files Database", () => {
  it("initializes SQLite schema and inserts file metadata", () => {
    const db = new FilesDatabase(":memory:");
    db.upsertFile({
      id: "test-f1",
      name: "document.pdf",
      size: 1024,
      contentType: "application/pdf",
      sha256: "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
      version: 1,
      status: "active",
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    });

    const file = db.getFile("test-f1");
    expect(file).not.toBeNull();
    expect(file?.name).toBe("document.pdf");
    expect(file?.size).toBe(1024);
    db.close();
  });
});
